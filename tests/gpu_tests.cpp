#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "core/document.h"
#include "gpu/renderer.h"

#ifndef IMRELIGHTER_SHADER_DIR
#define IMRELIGHTER_SHADER_DIR "shaders"
#endif
#ifndef IMRELIGHTER_GOLDEN_DIR
#define IMRELIGHTER_GOLDEN_DIR "tests/golden"
#endif

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

core::Document makeDocument(bool flat = false) {
    constexpr int width = 64;
    constexpr int height = 64;
    core::Document doc;
    doc.id = 1;
    doc.width = width;
    doc.height = height;
    doc.hasDepth = true;
    doc.color.resize(static_cast<size_t>(width) * height * 4);
    doc.depth.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = (x + 0.5f) / width;
            const float v = (y + 0.5f) / height;
            const size_t i = static_cast<size_t>(y) * width + x;
            doc.color[i * 4 + 0] = static_cast<uint8_t>(40.0f + 180.0f * u);
            doc.color[i * 4 + 1] = static_cast<uint8_t>(40.0f + 180.0f * v);
            doc.color[i * 4 + 2] = 150;
            doc.color[i * 4 + 3] = (x < width / 8) ? 0 : 255;
            const float dx = u - 0.5f;
            const float dy = v - 0.5f;
            const float bump = std::exp(-(dx * dx + dy * dy) * 50.0f);
            // doc.depth is bottom-up: the top image row is at the end.
            const int sourceY = height - 1 - y;
            doc.depth[static_cast<size_t>(sourceY) * width + x] =
                flat ? 0.5f : (0.15f + 0.8f * bump);
        }
    }
    doc.settings.height = 0.6f;
    doc.settings.smooth = 0.0f;
    doc.settings.lineAssist = false;
    doc.settings.ambient = 0.18f;
    doc.settings.ambientColor = "#8899bb";
    doc.settings.specular = 0.0f;
    doc.settings.rim = 0.0f;
    doc.settings.softness = 0.02f;
    doc.settings.bias = 0.007f;
    doc.settings.shadows = true;
    doc.lights = {core::makeLight(1, {-0.35f, 0.45f, 1.6f})};
    doc.selectedLight = 1;
    return doc;
}

std::vector<uint8_t> render(gpu::Renderer& renderer, const core::Document& doc,
                            core::View view) {
    std::vector<uint8_t> rgba;
    std::string error;
    if (!renderer.renderExport(doc, view, rgba, &error)) {
        std::cerr << "render failed: " << error << '\n';
        return {};
    }
    return rgba;
}

double psnr(const std::vector<uint8_t>& actual,
            const std::vector<uint8_t>& reference) {
    if (actual.size() != reference.size() || actual.empty()) return 0.0;
    double squared = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < actual.size(); i += 4) {
        for (int c = 0; c < 3; ++c) {
            const double delta = static_cast<double>(actual[i + c]) -
                                 static_cast<double>(reference[i + c]);
            squared += delta * delta;
            ++count;
        }
    }
    const double mse = squared / static_cast<double>(std::max<size_t>(1, count));
    if (mse <= 1e-12) return 99.0;
    return 10.0 * std::log10(255.0 * 255.0 / mse);
}

std::vector<uint8_t> readBinary(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});
}

void writeBinary(const std::filesystem::path& path,
                 const std::vector<uint8_t>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
}

void testFlatShadow(gpu::Renderer& renderer) {
    core::Document doc = makeDocument(true);
    const auto image = render(renderer, doc, core::View::Shadow);
    if (image.empty()) return;
    uint8_t minimum = 255;
    for (size_t i = 0; i < image.size(); i += 4) {
        minimum = std::min(minimum, image[i]);
    }
    check(minimum >= 250, "flat depth has no self-shadow");
}

void testDepthOrientation(gpu::Renderer& renderer) {
    core::Document doc = makeDocument();
    for (int y = 0; y < doc.height; ++y) {
        for (int x = 0; x < doc.width; ++x) {
            const int sourceY = doc.height - 1 - y;
            doc.depth[static_cast<size_t>(sourceY) * doc.width + x] =
                static_cast<float>(y) /
                    static_cast<float>(doc.height - 1);
        }
    }
    ++doc.depthRevision;
    const auto image = render(renderer, doc, core::View::Depth);
    if (image.empty()) return;
    const auto mean = [&](int begin, int end) {
        double sum = 0.0;
        int count = 0;
        for (int y = begin; y < end; ++y) {
            for (int x = 0; x < doc.width; ++x) {
                sum += image[(static_cast<size_t>(y) * doc.width + x) * 4];
                ++count;
            }
        }
        return sum / std::max(1, count);
    };
    check(mean(0, doc.height / 4) < mean(doc.height * 3 / 4, doc.height) - 40.0,
          "depth view is top-down");
}

void testNormalFlip(gpu::Renderer& renderer) {
    core::Document doc = makeDocument();
    const auto before = render(renderer, doc, core::View::Normal);
    doc.settings.flipX = true;
    const auto after = render(renderer, doc, core::View::Normal);
    if (before.empty() || after.empty()) return;
    double difference = 0.0;
    for (size_t i = 0; i < before.size(); i += 4) {
        difference += std::abs(static_cast<int>(before[i]) -
                               static_cast<int>(after[i]));
    }
    check(difference / (before.size() / 4) > 5.0,
          "flipX changes derived normal X channel");
}

void testQualitySettings(gpu::Renderer& renderer) {
    core::Document doc = makeDocument();
    doc.settings.previewLongEdge = 1024;
    doc.settings.shadowDivisor = 4;
    doc.settings.shadowSteps = 32;
    doc.settings.shadowSamples = 1;
    const auto image = render(renderer, doc, core::View::Lit);
    check(!image.empty(), "custom render quality settings render");
}

void testAlpha(gpu::Renderer& renderer) {
    core::Document doc = makeDocument();
    const auto image = render(renderer, doc, core::View::Lit);
    if (image.empty()) return;
    check(image[3] == 0, "transparent source alpha is preserved");
    check(image[(doc.width - 1) * 4 + 3] == 255,
          "opaque source alpha is preserved");
}

} // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "SKIP: GLFW init failed\n";
        return 0;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(32, 32, "gpu-tests", nullptr, nullptr);
    if (!window) {
        std::cerr << "SKIP: no OpenGL 4.5 context\n";
        glfwTerminate();
        return 0;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "SKIP: GLAD load failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

    gpu::Renderer renderer;
    std::string error;
    if (!renderer.initialize(std::filesystem::path(IMRELIGHTER_SHADER_DIR),
                             &error)) {
        std::cerr << "FAIL: renderer init: " << error << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const bool updateGolden = std::filesystem::exists("update-golden.flag");
    const std::filesystem::path goldenDir = IMRELIGHTER_GOLDEN_DIR;
    const core::View views[] = {core::View::Lit, core::View::Normal,
                                core::View::Depth, core::View::Lighting,
                                core::View::Shadow};
    core::Document doc = makeDocument();
    for (core::View view : views) {
        const auto image = render(renderer, doc, view);
        if (image.empty()) {
            ++failures;
            continue;
        }
        const auto path = goldenDir / (std::to_string(static_cast<int>(view)) + ".rgba");
        if (updateGolden) {
            writeBinary(path, image);
            continue;
        }
        const auto golden = readBinary(path);
        if (golden.empty()) {
            std::cerr << "FAIL: missing golden " << path
                      << " (run tests with update-golden.flag)\n";
            ++failures;
            continue;
        }
        const double score = psnr(image, golden);
        std::cout << "view " << static_cast<int>(view) << " PSNR " << score << " dB\n";
        check(score >= 28.0, "golden PSNR for view " + std::to_string(static_cast<int>(view)));
    }

    testFlatShadow(renderer);
    testDepthOrientation(renderer);
    testNormalFlip(renderer);
    testQualitySettings(renderer);
    testAlpha(renderer);

    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    if (failures != 0) {
        std::cerr << failures << " GPU test(s) failed\n";
        return 1;
    }
    std::cout << "all GPU tests passed\n";
    return 0;
}
