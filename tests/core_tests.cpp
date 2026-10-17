#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/i18n.h"
#include "io/image_io.h"
#include "io/png_export.h"
#include "ml/preprocess.h"
#include "nlohmann/json.hpp"

#ifndef IMRELIGHTER_I18N_DIR
#define IMRELIGHTER_I18N_DIR "assets/i18n"
#endif
#ifndef IMRELIGHTER_TEST_WEBP
#define IMRELIGHTER_TEST_WEBP "tests/data/test.webp"
#endif

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float a, float b, float epsilon = 1e-4f) {
    return std::abs(a - b) <= epsilon;
}

void testPreprocessShape() {
    std::vector<uint8_t> rgba(100 * 50 * 4, 128);
    ml::PreprocessResult result;
    std::string error;
    check(ml::preprocess(rgba.data(), 100, 50, result, &error),
          "preprocess succeeds");
    check(result.width % 14 == 0 && result.height % 14 == 0,
          "preprocess target dimensions are multiples of 14");
    check(result.tensor.size() == static_cast<size_t>(result.width) *
                                     static_cast<size_t>(result.height) * 3,
          "preprocess tensor is NCHW 3-plane");
    check(std::isfinite(result.tensor.front()), "preprocess tensor is finite");
}

void testPostprocessOrientation() {
    const std::vector<float> raw = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> depth;
    std::string error;
    check(ml::postprocess(raw, 2, 2, 2, 2, depth, &error), "2x2 postprocess");
    check(depth.size() == 4, "2x2 depth size");
    if (depth.size() != 4) return;

    // Source is top-down; doc.depth is deliberately bottom-up for GL/FBO parity.
    check(near(depth[0], 2.0f / 3.0f) && near(depth[1], 1.0f),
          "postprocess flips source rows to bottom-up");
    check(near(depth[2], 0.0f) && near(depth[3], 1.0f / 3.0f),
          "postprocess second row");
}

void testPostprocessFlatAndNaN() {
    std::vector<float> depth;
    std::string error;
    check(!ml::postprocess({2.0f, 2.0f, 2.0f, 2.0f}, 2, 2, 2, 2, depth,
                          &error),
          "flat depth is rejected");
    check(!error.empty(), "flat depth reports no variation");

    depth.clear();
    error.clear();
    check(ml::postprocess({std::nanf(""), 1.0f, 2.0f, 3.0f}, 2, 2, 2, 2,
                          depth, &error),
          "NaN postprocess succeeds");
    check(depth.size() == 4 && std::isfinite(depth[0]), "NaN becomes finite");
}

std::vector<uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});
}

std::vector<uint8_t> inflateStored(const std::vector<uint8_t>& zlib) {
    std::vector<uint8_t> out;
    size_t pos = 2; // zlib header
    for (;;) {
        const uint8_t header = zlib.at(pos++);
        const bool final = (header & 1u) != 0;
        const uint8_t type = (header >> 1u) & 3u;
        if (type != 0) return {};
        const uint16_t length = static_cast<uint16_t>(zlib.at(pos)) |
                                (static_cast<uint16_t>(zlib.at(pos + 1)) << 8);
        pos += 4;
        out.insert(out.end(), zlib.begin() + static_cast<std::ptrdiff_t>(pos),
                   zlib.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
        if (final) break;
    }
    return out;
}

std::vector<uint8_t> collectIdat(const std::vector<uint8_t>& png) {
    std::vector<uint8_t> result;
    size_t pos = 8;
    const auto u32 = [&](size_t offset) {
        return (static_cast<uint32_t>(png[offset]) << 24) |
               (static_cast<uint32_t>(png[offset + 1]) << 16) |
               (static_cast<uint32_t>(png[offset + 2]) << 8) |
               static_cast<uint32_t>(png[offset + 3]);
    };
    while (pos + 12 <= png.size()) {
        const uint32_t length = u32(pos);
        const std::string type(reinterpret_cast<const char*>(png.data() + pos + 4), 4);
        pos += 8;
        if (type == "IDAT") {
            result.insert(result.end(),
                          png.begin() + static_cast<std::ptrdiff_t>(pos),
                          png.begin() + static_cast<std::ptrdiff_t>(pos + length));
        }
        pos += length + 4;
        if (type == "IEND") break;
    }
    return result;
}

void testWebp() {
    std::string error;
    const auto image = io::loadImage(IMRELIGHTER_TEST_WEBP, &error);
    check(image.has_value(), "WebP image loads");
    if (!image) {
        std::cerr << "WebP error: " << error << '\n';
        return;
    }
    check(image->width == 3 && image->height == 2, "WebP dimensions");
    check(image->rgba.size() == 3 * 2 * 4, "WebP RGBA payload");
    check(image->rgba[3] == 255 && image->rgba[0] > 200,
          "WebP red pixel decoded");
}

void testI18nParity() {
    const std::filesystem::path directory = IMRELIGHTER_I18N_DIR;
    const auto read = [&](const char* name) {
        std::ifstream stream(directory / name, std::ios::binary);
        nlohmann::json json;
        stream >> json;
        return json;
    };
    nlohmann::json zh;
    nlohmann::json en;
    try {
        zh = read("zh-CN.json");
        en = read("en-US.json");
    } catch (const std::exception& e) {
        check(false, (std::string("i18n JSON parse: ") + e.what()).c_str());
        return;
    }
    check(zh.is_object() && en.is_object(), "i18n dictionaries are objects");
    for (auto it = zh.begin(); it != zh.end(); ++it) {
        check(en.contains(it.key()), ("i18n key missing in en-US: " + it.key()).c_str());
    }
    for (auto it = en.begin(); it != en.end(); ++it) {
        check(zh.contains(it.key()), ("i18n key missing in zh-CN: " + it.key()).c_str());
    }
    check(zh["settings.model"].is_string() &&
              !zh["settings.model"].get<std::string>().empty(),
          "i18n values are non-empty");
}

void testDepthPngHeader() {
    const auto path = std::filesystem::temp_directory_path() /
                      "imrelighter-depth-test.png";
    const std::vector<float> depth = {0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f};
    std::string error;
    check(io::writeDepth16Png(path, depth, 2, 2, false, &error),
          "write 16-bit depth PNG");

    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(33);
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    check(bytes.size() == 33 && bytes[0] == 0x89 && bytes[1] == 'P' &&
              bytes[2] == 'N' && bytes[3] == 'G',
          "PNG signature");
    const auto u32 = [&](size_t offset) {
        return (static_cast<uint32_t>(bytes[offset]) << 24) |
               (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
               (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
               static_cast<uint32_t>(bytes[offset + 3]);
    };
    check(u32(16) == 2 && u32(20) == 2, "PNG dimensions");
    check(bytes[24] == 16 && bytes[25] == 0, "PNG 16-bit grayscale");

    const auto png = readFile(path);
    const auto raw = inflateStored(collectIdat(png));
    check(raw.size() == 10, "PNG decompressed scanline size");
    if (raw.size() == 10) {
        const auto u16 = [&](size_t offset) {
            return static_cast<uint16_t>(raw[offset] << 8 | raw[offset + 1]);
        };
        check(raw[0] == 0 && raw[5] == 0, "PNG no per-row filters");
        check(std::abs(static_cast<int>(u16(1)) - 43690) <= 1 &&
                  u16(3) == 65535,
              "depth PNG top row is source bottom row");
        check(u16(6) == 0 && std::abs(static_cast<int>(u16(8)) - 21845) <= 1,
              "depth PNG bottom row is source top row");
    }

    std::error_code code;
    std::filesystem::remove(path, code);
}

} // namespace

int main() {
    testWebp();
    testI18nParity();
    testPreprocessShape();
    testPostprocessOrientation();
    testPostprocessFlatAndNaN();
    testDepthPngHeader();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all core tests passed\n";
    return 0;
}
