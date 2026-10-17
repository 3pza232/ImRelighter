#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "core/document.h"
#include "gpu/shader.h"

namespace gpu {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(const std::filesystem::path& shaderDir,
                    std::string* error = nullptr);
    void shutdown();

    bool ready() const { return ready_; }
    unsigned int previewTexture() const { return preview_.texture; }

    bool renderPreview(const core::Document& doc, std::string* error = nullptr);
    bool renderExport(const core::Document& doc, core::View view,
                      std::vector<uint8_t>& rgba, std::string* error = nullptr);

private:
    struct Target {
        unsigned int framebuffer = 0;
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
    };

    struct MrtTarget {
        unsigned int framebuffer = 0;
        unsigned int texture[2] = {0, 0};
        int width = 0;
        int height = 0;
    };

    bool createTarget(Target& target, unsigned int internalFormat, int width,
                      int height);
    bool createMrtTarget(MrtTarget& target, unsigned int internalFormat,
                         int width, int height);
    void destroyTarget(Target& target);
    void destroyMrtTarget(MrtTarget& target);

    bool ensureSource(const core::Document& doc, std::string* error);
    bool ensureTargets(const core::Document& doc, int width, int height,
                       std::string* error);
    bool ensurePreviewTargets(const core::Document& doc, std::string* error);
    void renderPasses(const core::Document& doc, core::View view);

    void bindLights(const Shader& shader, const core::Document& doc);
    int selectedLightIndex(const core::Document& doc) const;

    static std::size_t stateHash(const core::Document& doc);

    Shader depthShader_;
    Shader normalShader_;
    Shader shadowShader_;
    Shader blurShader_;
    Shader lightingShader_;

    unsigned int vao_ = 0;
    unsigned int colorTexture_ = 0;
    unsigned int depthTexture_ = 0;
    int sourceWidth_ = 0;
    int sourceHeight_ = 0;
    uint64_t sourceDocument_ = 0;
    uint64_t sourceDepthRevision_ = 0;

    Target height_;
    Target normal_;
    Target preview_;
    MrtTarget shadow_;
    MrtTarget blur_;

    std::size_t lastStateHash_ = 0;
    bool hasRendered_ = false;
    bool ready_ = false;
};

} // namespace gpu