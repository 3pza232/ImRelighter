#include "gpu/renderer.h"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

#include "core/color.h"

namespace gpu {
namespace {

// Light and ambient colors are authored in sRGB, while the lighting shader
// accumulates radiance in linear space.
float srgbToLinear(float value) {
    return value <= 0.04045f ? value / 12.92f
                             : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

std::array<float, 3> parseLinearColor(const std::string& hex) {
    constexpr std::array<float, 3> kFallback{1.0f, 1.0f, 1.0f};
    const auto srgb = core::parseHexColor(hex).value_or(kFallback);
    return {srgbToLinear(srgb[0]), srgbToLinear(srgb[1]),
            srgbToLinear(srgb[2])};
}

size_t combine(size_t seed, size_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

size_t hashFloat(float value) {
    return std::hash<float>{}(value);
}

// Every rendering input must be folded into this hash; otherwise changing a
// setting can leave a stale preview on screen.
std::size_t renderStateHash(const core::Document& doc);

} // namespace

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(const std::filesystem::path& shaderDir,
                          std::string* error) {
    shutdown();

    const auto vertex = shaderDir / L"fullscreen.vert";
    if (!depthShader_.load(vertex, shaderDir / L"depth_smooth.frag", error) ||
        !normalShader_.load(vertex, shaderDir / L"normal.frag", error) ||
        !shadowShader_.load(vertex, shaderDir / L"shadow.frag", error) ||
        !blurShader_.load(vertex, shaderDir / L"shadow_blur.frag", error) ||
        !lightingShader_.load(vertex, shaderDir / L"lighting.frag", error)) {
        return false;
    }

    glGenVertexArrays(1, &vao_);
    ready_ = true;
    return true;
}

void Renderer::shutdown() {
    destroyMrtTarget(blur_);
    destroyMrtTarget(shadow_);
    destroyTarget(preview_);
    destroyTarget(normal_);
    destroyTarget(height_);

    if (colorTexture_ != 0) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (depthTexture_ != 0) {
        glDeleteTextures(1, &depthTexture_);
        depthTexture_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    depthShader_.destroy();
    normalShader_.destroy();
    shadowShader_.destroy();
    blurShader_.destroy();
    lightingShader_.destroy();

    sourceWidth_ = 0;
    sourceHeight_ = 0;
    sourceDocument_ = 0;
    sourceDepthRevision_ = 0;
    lastStateHash_ = 0;
    hasRendered_ = false;
    ready_ = false;
}

bool Renderer::createTarget(Target& target, unsigned int internalFormat,
                            int width, int height) {
    destroyTarget(target);
    if (width <= 0 || height <= 0) return false;

    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    if (internalFormat == GL_R32F) {
        format = GL_RED;
        type = GL_FLOAT;
    } else if (internalFormat == GL_RGBA16F) {
        format = GL_RGBA;
        type = GL_FLOAT;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width,
                 height, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, target.texture, 0);
    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        destroyTarget(target);
        return false;
    }

    target.width = width;
    target.height = height;
    return true;
}

bool Renderer::createMrtTarget(MrtTarget& target, unsigned int internalFormat,
                               int width, int height) {
    destroyMrtTarget(target);
    if (width <= 0 || height <= 0) return false;

    glGenTextures(2, target.texture);
    for (unsigned int texture : target.texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat),
                     width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, target.texture[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, target.texture[1], 0);
    const GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, buffers);
    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        destroyMrtTarget(target);
        return false;
    }

    target.width = width;
    target.height = height;
    return true;
}

void Renderer::destroyTarget(Target& target) {
    if (target.framebuffer != 0) {
        glDeleteFramebuffers(1, &target.framebuffer);
        target.framebuffer = 0;
    }
    if (target.texture != 0) {
        glDeleteTextures(1, &target.texture);
        target.texture = 0;
    }
    target.width = 0;
    target.height = 0;
}

void Renderer::destroyMrtTarget(MrtTarget& target) {
    if (target.framebuffer != 0) {
        glDeleteFramebuffers(1, &target.framebuffer);
        target.framebuffer = 0;
    }
    if (target.texture[0] != 0) {
        glDeleteTextures(2, target.texture);
        target.texture[0] = 0;
        target.texture[1] = 0;
    }
    target.width = 0;
    target.height = 0;
}

bool Renderer::ensureSource(const core::Document& doc, std::string* error) {
    if (doc.color.empty() || doc.depth.empty()) {
        if (error) *error = "Renderer source channels are empty.";
        return false;
    }
    if (doc.id == sourceDocument_ &&
        doc.depthRevision == sourceDepthRevision_ &&
        doc.width == sourceWidth_ && doc.height == sourceHeight_ &&
        colorTexture_ != 0 && depthTexture_ != 0) {
        return true;
    }

    if (colorTexture_ == 0) glGenTextures(1, &colorTexture_);
    if (depthTexture_ == 0) glGenTextures(1, &depthTexture_);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, doc.width, doc.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, doc.color.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, doc.width, doc.height, 0, GL_RED,
                 GL_FLOAT, doc.depth.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    sourceDocument_ = doc.id;
    sourceDepthRevision_ = doc.depthRevision;
    sourceWidth_ = doc.width;
    sourceHeight_ = doc.height;
    return true;
}

bool Renderer::ensurePreviewTargets(const core::Document& doc,
                                    std::string* error) {
    const int requested = std::clamp(doc.settings.previewLongEdge, 512, 4096);
    const int longEdge = std::min(requested, std::max(doc.width, doc.height));
    const int width = std::max(
        1, static_cast<int>(std::lround(static_cast<double>(doc.width) *
                                        longEdge /
                                        std::max(doc.width, doc.height))));
    const int height = std::max(
        1, static_cast<int>(std::lround(static_cast<double>(doc.height) *
                                        longEdge /
                                        std::max(doc.width, doc.height))));
    return ensureTargets(doc, width, height, error);
}

bool Renderer::ensureTargets(const core::Document& doc, int width, int height,
                             std::string* error) {
    if (width <= 0 || height <= 0) {
        if (error) *error = "Invalid render target size.";
        return false;
    }
    const int divisor = std::clamp(doc.settings.shadowDivisor, 1, 4);
    const int shadowWidth = std::max(1, (width + divisor - 1) / divisor);
    const int shadowHeight = std::max(1, (height + divisor - 1) / divisor);
    if (preview_.width == width && preview_.height == height &&
        shadow_.width == shadowWidth && shadow_.height == shadowHeight &&
        height_.texture != 0 && normal_.texture != 0 && preview_.texture != 0 &&
        shadow_.texture[0] != 0 && blur_.texture[0] != 0) {
        return true;
    }

    if (!createTarget(height_, GL_R32F, width, height) ||
        !createTarget(normal_, GL_RGBA16F, width, height) ||
        !createTarget(preview_, GL_RGBA8, width, height)) {
        if (error) *error = "Failed to create preview render targets.";
        return false;
    }

    if (!createMrtTarget(shadow_, GL_RGBA8, shadowWidth, shadowHeight) ||
        !createMrtTarget(blur_, GL_RGBA8, shadowWidth, shadowHeight)) {
        if (error) *error = "Failed to create shadow render targets.";
        return false;
    }

    hasRendered_ = false;
    return true;
}

int Renderer::selectedLightIndex(const core::Document& doc) const {
    for (size_t i = 0; i < doc.lights.size() && i < 8; ++i) {
        if (doc.lights[i].id == doc.selectedLight) {
            return static_cast<int>(i);
        }
    }
    return doc.lights.empty() ? -1 : 0;
}

void Renderer::bindLights(const Shader& shader, const core::Document& doc) {
    std::array<float, 24> positions{};
    std::array<float, 24> colors{};
    std::array<float, 8> intensities{};
    std::array<float, 8> ranges{};
    std::array<int, 8> enabled{};
    const int count = std::min(8, static_cast<int>(doc.lights.size()));

    for (int i = 0; i < count; ++i) {
        const core::Light& light = doc.lights[static_cast<size_t>(i)];
        for (int axis = 0; axis < 3; ++axis) {
            positions[static_cast<size_t>(i * 3 + axis)] =
                light.position[static_cast<size_t>(axis)];
        }
        const auto color = parseLinearColor(light.color);
        for (int axis = 0; axis < 3; ++axis) {
            colors[static_cast<size_t>(i * 3 + axis)] =
                color[static_cast<size_t>(axis)];
        }
        intensities[static_cast<size_t>(i)] = light.intensity;
        ranges[static_cast<size_t>(i)] = light.range;
        enabled[static_cast<size_t>(i)] = light.enabled ? 1 : 0;
    }

    shader.setInt("uLightCount", count);
    shader.setVec3Array("uLightPos", positions.data(), count);
    shader.setVec3Array("uLightColor", colors.data(), count);
    shader.setFloatArray("uLightIntensity", intensities.data(), count);
    shader.setFloatArray("uLightRange", ranges.data(), count);
    shader.setIntArray("uLightEnabled", enabled.data(), count);
}

void Renderer::renderPasses(const core::Document& doc, core::View view) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(vao_);

    passDepth(doc);
    passNormal(doc);
    passShadow(doc);
    passBlur(doc);
    passLighting(doc, view);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::passDepth(const core::Document& doc) {
    glBindFramebuffer(GL_FRAMEBUFFER, height_.framebuffer);
    glViewport(0, 0, preview_.width, preview_.height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    depthShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    depthShader_.setInt("uColor", 0);
    depthShader_.setInt("uDepth", 1);
    depthShader_.setVec2("uImageSize",
                         static_cast<float>(preview_.width),
                         static_cast<float>(preview_.height));
    const float depthLow =
        doc.settings.autoDepthRange ? 0.0f : doc.settings.depthMin;
    const float depthHigh =
        doc.settings.autoDepthRange ? 1.0f : doc.settings.depthMax;
    depthShader_.setFloat("uLow", depthLow);
    depthShader_.setFloat("uSpan", std::max(0.001f, depthHigh - depthLow));
    depthShader_.setFloat("uSmooth", doc.settings.smooth);
    depthShader_.setInt("uInvert", doc.settings.invert ? 1 : 0);
    depthShader_.setInt("uLineAssist", doc.settings.lineAssist ? 1 : 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::passNormal(const core::Document& doc) {
    const float imageWidth = static_cast<float>(doc.width);
    const float imageHeight = static_cast<float>(doc.height);

    glBindFramebuffer(GL_FRAMEBUFFER, normal_.framebuffer);
    glViewport(0, 0, preview_.width, preview_.height);
    glClear(GL_COLOR_BUFFER_BIT);
    normalShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, height_.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    normalShader_.setInt("uHeight", 0);
    normalShader_.setInt("uColor", 1);
    normalShader_.setVec2("uTexel", 1.0f / static_cast<float>(preview_.width),
                          1.0f / static_cast<float>(preview_.height));
    normalShader_.setVec2("uImageSize", imageWidth, imageHeight);
    normalShader_.setFloat(
        "uPixelScale",
        static_cast<float>(doc.width) / static_cast<float>(preview_.width));
    normalShader_.setFloat("uMaxTilt", 1.5f);
    normalShader_.setFloat("uDetailStrength", doc.settings.detail);
    normalShader_.setFloat("uAmplitude",
                           std::max(doc.settings.height, 0.001f));
    normalShader_.setFloat("uNormalStrength", doc.settings.normalStrength);
    normalShader_.setVec3("uFlips",
                          doc.settings.flipX ? -1.0f : 1.0f,
                          doc.settings.flipY ? -1.0f : 1.0f,
                          doc.settings.flipZ ? -1.0f : 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::passShadow(const core::Document& doc) {
    const float imageWidth = static_cast<float>(doc.width);
    const float imageHeight = static_cast<float>(doc.height);
    const GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_.framebuffer);
    glViewport(0, 0, shadow_.width, shadow_.height);
    glDrawBuffers(2, buffers);
    glClear(GL_COLOR_BUFFER_BIT);
    shadowShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, height_.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    shadowShader_.setInt("uHeight", 0);
    shadowShader_.setInt("uColor", 1);
    shadowShader_.setVec2("uTexel", 1.0f / static_cast<float>(preview_.width),
                          1.0f / static_cast<float>(preview_.height));
    shadowShader_.setVec2("uImageSize", imageWidth, imageHeight);
    shadowShader_.setFloat("uAmplitude",
                           std::max(doc.settings.height, 0.001f));
    shadowShader_.setFloat("uBias", doc.settings.bias);
    shadowShader_.setFloat("uSoftness", doc.settings.softness);
    shadowShader_.setInt("uSteps", std::clamp(doc.settings.shadowSteps, 16, 192));
    shadowShader_.setInt(
        "uSamples", doc.settings.softness > 0.005f
                        ? std::clamp(doc.settings.shadowSamples, 1, 8)
                        : 1);
    shadowShader_.setInt("uShadowsEnabled", doc.settings.shadows ? 1 : 0);
    bindLights(shadowShader_, doc);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::passBlur(const core::Document& doc) {
    const GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glBindFramebuffer(GL_FRAMEBUFFER, blur_.framebuffer);
    glViewport(0, 0, shadow_.width, shadow_.height);
    glDrawBuffers(2, buffers);
    glClear(GL_COLOR_BUFFER_BIT);
    blurShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_.texture[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadow_.texture[1]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, height_.texture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    blurShader_.setInt("uShadow0", 0);
    blurShader_.setInt("uShadow1", 1);
    blurShader_.setInt("uHeight", 2);
    blurShader_.setInt("uColor", 3);
    blurShader_.setVec2("uTexel", 1.0f / static_cast<float>(shadow_.width),
                         1.0f / static_cast<float>(shadow_.height));
    blurShader_.setFloat("uAmplitude",
                         std::max(doc.settings.height, 0.001f));
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::passLighting(const core::Document& doc, core::View view) {
    const float imageWidth = static_cast<float>(doc.width);
    const float imageHeight = static_cast<float>(doc.height);

    glBindFramebuffer(GL_FRAMEBUFFER, preview_.framebuffer);
    glViewport(0, 0, preview_.width, preview_.height);
    glClear(GL_COLOR_BUFFER_BIT);
    lightingShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_.texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, height_.texture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, blur_.texture[0]);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, blur_.texture[1]);
    lightingShader_.setInt("uColor", 0);
    lightingShader_.setInt("uNormal", 1);
    lightingShader_.setInt("uHeight", 2);
    lightingShader_.setInt("uBlur0", 3);
    lightingShader_.setInt("uBlur1", 4);
    lightingShader_.setVec2("uImageSize", imageWidth, imageHeight);
    lightingShader_.setFloat("uAmplitude",
                             std::max(doc.settings.height, 0.001f));
    const auto ambient = parseLinearColor(doc.settings.ambientColor);
    lightingShader_.setVec3("uAmbientColor", ambient.data());
    lightingShader_.setFloat("uAmbient", doc.settings.ambient);
    lightingShader_.setFloat("uWrap", doc.settings.wrap);
    lightingShader_.setFloat("uSpecular", doc.settings.specular);
    lightingShader_.setFloat("uShininess", doc.settings.shininess);
    lightingShader_.setFloat("uRim", doc.settings.rim);
    lightingShader_.setFloat("uExposure", doc.settings.exposure);
    lightingShader_.setFloat("uMix", doc.settings.mix);
    lightingShader_.setFloat("uComparison", doc.settings.comparison);
    lightingShader_.setInt("uBaseMode", doc.settings.baseMode ? 1 : 0);
    lightingShader_.setInt("uViewMode", static_cast<int>(view));
    lightingShader_.setInt("uSelectedLight", selectedLightIndex(doc));
    lightingShader_.setInt("uShadowsEnabled", doc.settings.shadows ? 1 : 0);
    bindLights(lightingShader_, doc);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

bool Renderer::renderExport(const core::Document& doc, core::View view,
                            std::vector<uint8_t>& rgba, std::string* error) {
    if (!ready_ || !doc.hasDepth || doc.depth.empty()) {
        if (error) *error = "Depth is not ready.";
        return false;
    }
    if (!ensureSource(doc, error) ||
        !ensureTargets(doc, doc.width, doc.height, error)) {
        return false;
    }

    renderPasses(doc, view);
    const size_t rowBytes = static_cast<size_t>(doc.width) * 4;
    std::vector<uint8_t> bottomUp(rowBytes * static_cast<size_t>(doc.height));
    glBindTexture(GL_TEXTURE_2D, preview_.texture);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    rgba.resize(bottomUp.size());
    for (int y = 0; y < doc.height; ++y) {
        const size_t source = static_cast<size_t>(doc.height - 1 - y) * rowBytes;
        const size_t target = static_cast<size_t>(y) * rowBytes;
        std::memcpy(rgba.data() + target, bottomUp.data() + source, rowBytes);
    }

    hasRendered_ = false;
    return true;
}

bool Renderer::renderPreview(const core::Document& doc, std::string* error) {
    if (!ready_ || !doc.hasDepth || doc.depth.empty()) return false;
    if (!ensureSource(doc, error) || !ensurePreviewTargets(doc, error)) {
        return false;
    }

    const std::size_t hash = renderStateHash(doc);
    if (hasRendered_ && hash == lastStateHash_) return true;

    renderPasses(doc, doc.view);
    lastStateHash_ = hash;
    hasRendered_ = true;
    return true;
}

namespace {

std::size_t renderStateHash(const core::Document& doc) {
    std::size_t seed = 0;
    const auto addFloat = [&](float value) {
        seed = combine(seed, hashFloat(value));
    };
    const auto addInt = [&](int value) {
        seed = combine(seed, std::hash<int>{}(value));
    };
    const auto addText = [&](const std::string& value) {
        seed = combine(seed, std::hash<std::string>{}(value));
    };

    seed = combine(seed, std::hash<uint64_t>{}(doc.id));
    seed = combine(seed, std::hash<uint64_t>{}(doc.depthRevision));
    addInt(doc.width);
    addInt(doc.height);
    addInt(static_cast<int>(doc.view));
    addInt(doc.selectedLight);
    addFloat(doc.settings.comparison);
    addFloat(doc.settings.ambient);
    addText(doc.settings.ambientColor);
    addInt(doc.settings.baseMode ? 1 : 0);
    addFloat(doc.settings.mix);
    addFloat(doc.settings.wrap);
    addInt(doc.settings.shadows ? 1 : 0);
    addFloat(doc.settings.bias);
    addFloat(doc.settings.softness);
    addFloat(doc.settings.specular);
    addFloat(doc.settings.shininess);
    addFloat(doc.settings.rim);
    addFloat(doc.settings.exposure);
    addFloat(doc.settings.height);
    addInt(doc.settings.invert ? 1 : 0);
    addFloat(doc.settings.smooth);
    addInt(doc.settings.lineAssist ? 1 : 0);
    addFloat(doc.settings.normalStrength);
    addInt(doc.settings.flipX ? 1 : 0);
    addInt(doc.settings.flipY ? 1 : 0);
    addInt(doc.settings.flipZ ? 1 : 0);
    addFloat(doc.settings.depthMin);
    addFloat(doc.settings.depthMax);
    addInt(doc.settings.autoDepthRange ? 1 : 0);
    addInt(doc.settings.shadowSteps);
    addInt(doc.settings.shadowSamples);
    addInt(doc.settings.shadowDivisor);
    addInt(doc.settings.previewLongEdge);

    seed = combine(seed, doc.lights.size());
    for (const core::Light& light : doc.lights) {
        addInt(light.id);
        addText(light.color);
        addFloat(light.intensity);
        addFloat(light.range);
        addInt(light.enabled ? 1 : 0);
        for (float value : light.position) addFloat(value);
    }
    return seed;
}

} // namespace

} // namespace gpu