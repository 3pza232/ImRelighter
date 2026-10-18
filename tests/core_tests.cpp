#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "core/i18n.h"
#include "core/config.h"
#include "io/exif.h"
#include "io/hash.h"
#include "io/image_io.h"
#include "io/image_limits.h"
#include "io/image_probe.h"
#include "io/png_export.h"
#include "ml/half.h"
#include "ml/model_config.h"
#include "ml/model_metadata.h"
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

void testModelConfig() {
    const ml::ModelConfig processor =
        ml::loadModelConfig(IMRELIGHTER_PREPROCESSOR_CONFIG);
    check(processor.loaded && processor.inputSize == 518 &&
              processor.sizeMultiple == 14 && processor.rescale &&
              processor.normalize,
          "preprocessor_config values load");
    check(processor.doResize && processor.resample == ml::Resample::Bicubic,
          "preprocessor resize mode loads");
    check(near(processor.rescaleFactor, 1.0f / 255.0f),
          "preprocessor rescale factor loads");

    const auto directory =
        std::filesystem::temp_directory_path() / "imrelighter-preprocess-test";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    const auto configPath = directory / L"preprocessor_config.json";
    {
        std::ofstream stream(configPath, std::ios::binary);
        stream << R"({"preprocess": {"size": 640, "ensure_multiple_of": 32,
                    "resample": 2, "rescale_factor": 0.5}})";
    }
    const ml::ModelConfig nested =
        ml::loadModelConfig(directory / L"model.onnx");
    check(nested.loaded && nested.inputSize == 640 &&
              nested.sizeMultiple == 32 &&
              nested.resample == ml::Resample::Bilinear &&
              near(nested.rescaleFactor, 0.5f),
          "nested preprocess values load");

    {
        std::ofstream stream(configPath, std::ios::binary);
        stream << R"({"do_resize": false, "resample": 0})";
    }
    const ml::ModelConfig noResize =
        ml::loadModelConfig(directory / L"model.onnx");
    check(noResize.loaded && !noResize.doResize &&
              noResize.resample == ml::Resample::Nearest,
          "preprocessor disable-resize values load");
    std::filesystem::remove_all(directory, ec);
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

    ml::ModelConfig noResize = ml::defaults();
    noResize.doResize = false;
    ml::PreprocessResult passthrough;
    check(ml::preprocess(rgba.data(), 100, 50, passthrough, &error, noResize),
          "preprocess without resize succeeds");
    check(passthrough.width == 100 && passthrough.height == 50,
          "preprocess without resize preserves dimensions");
    check(passthrough.tensor.size() == 100u * 50u * 3u,
          "preprocess without resize keeps tensor shape");

    const std::vector<uint8_t> single = {10, 0, 0, 255};
    ml::ModelConfig scaled = ml::defaults();
    scaled.doResize = false;
    scaled.rescaleFactor = 0.5f;
    scaled.normalize = false;
    ml::PreprocessResult scaledResult;
    check(ml::preprocess(single.data(), 1, 1, scaledResult, &error, scaled),
          "preprocess applies custom rescale factor");
    check(scaledResult.tensor.size() == 3 &&
              near(scaledResult.tensor[0], 5.0f),
          "preprocess uses configured rescale factor");
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

void testSha256() {
    const std::string digest = io::sha256Model(IMRELIGHTER_TEST_WEBP);
    check(digest ==
              "4559c622d464194eed630fc3aa6ece8202dde886e030a223edc7412ac806991c",
          "SHA-256 matches the test asset");
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

void testExifOrientation() {
    const int width = 2;
    const int height = 3;
    std::vector<uint8_t> source(static_cast<size_t>(width) * height * 4);
    for (int i = 0; i < width * height; ++i) {
        source[static_cast<size_t>(i) * 4 + 0] = static_cast<uint8_t>(i);
        source[static_cast<size_t>(i) * 4 + 3] = 255;
    }

    struct Case {
        int orientation;
        int width;
        int height;
        std::vector<int> expected;
    };
    const Case cases[] = {
        {1, 2, 3, {0, 1, 2, 3, 4, 5}},
        {2, 2, 3, {1, 0, 3, 2, 5, 4}},
        {3, 2, 3, {5, 4, 3, 2, 1, 0}},
        {4, 2, 3, {4, 5, 2, 3, 0, 1}},
        {5, 3, 2, {0, 2, 4, 1, 3, 5}},
        {6, 3, 2, {4, 2, 0, 5, 3, 1}},
        {7, 3, 2, {5, 3, 1, 4, 2, 0}},
        {8, 3, 2, {1, 3, 5, 0, 2, 4}},
    };

    for (const Case& item : cases) {
        auto pixels = source;
        int w = width;
        int h = height;
        check(io::applyExifOrientation(pixels, w, h, item.orientation),
              ("apply EXIF orientation " +
               std::to_string(item.orientation)).c_str());
        check(w == item.width && h == item.height,
              ("EXIF orientation dimensions " +
               std::to_string(item.orientation)).c_str());
        for (size_t i = 0; i < item.expected.size(); ++i) {
            check(pixels[i * 4] == item.expected[i],
                  ("EXIF pixel mapping " +
                   std::to_string(item.orientation)).c_str());
        }
    }

    auto invalid = source;
    int invalidWidth = width;
    int invalidHeight = height;
    check(!io::applyExifOrientation(invalid, invalidWidth, invalidHeight, 9),
          "invalid EXIF orientation is rejected");
}

std::vector<uint8_t> makeExifJpeg(bool littleEndian, int orientation) {
    const auto push16 = [&](std::vector<uint8_t>& out, int value) {
        if (littleEndian) {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        } else {
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(value & 0xFF));
        }
    };
    const auto push32 = [&](std::vector<uint8_t>& out, uint32_t value) {
        if (littleEndian) {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        } else {
            out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(value & 0xFF));
        }
    };

    std::vector<uint8_t> tiff;
    if (littleEndian) {
        tiff.push_back('I');
        tiff.push_back('I');
    } else {
        tiff.push_back('M');
        tiff.push_back('M');
    }
    push16(tiff, 42);
    push32(tiff, 8);
    push16(tiff, 1);          // one IFD0 entry
    push16(tiff, 0x0112);     // Orientation
    push16(tiff, 3);          // SHORT
    push32(tiff, 1);
    push16(tiff, orientation);
    push16(tiff, 0);          // inline value padding
    push32(tiff, 0);          // no next IFD

    std::vector<uint8_t> jpeg = {0xFF, 0xD8, 0xFF, 0xE1};
    const size_t length = 2 + 6 + tiff.size();
    jpeg.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    jpeg.push_back(static_cast<uint8_t>(length & 0xFF));
    jpeg.insert(jpeg.end(), {'E', 'x', 'i', 'f', 0, 0});
    jpeg.insert(jpeg.end(), tiff.begin(), tiff.end());
    jpeg.insert(jpeg.end(), {0xFF, 0xD9});
    return jpeg;
}

void testExifParsing() {
    const auto little = makeExifJpeg(true, 6);
    check(io::readJpegOrientation(little.data(), little.size()) == 6,
          "EXIF little-endian orientation");

    const auto big = makeExifJpeg(false, 8);
    check(io::readJpegOrientation(big.data(), big.size()) == 8,
          "EXIF big-endian orientation");

    auto truncated = little;
    truncated.resize(10);
    check(io::readJpegOrientation(truncated.data(), truncated.size()) == 0,
          "truncated EXIF is rejected");

    const uint8_t notJpeg[] = {0x89, 'P', 'N', 'G'};
    check(io::readJpegOrientation(notJpeg, sizeof(notJpeg)) == 0,
          "non-JPEG has no EXIF orientation");
}

void testHalfConversion() {
    check(ml::floatToHalf(0.0f) == 0x0000, "half +0");
    check(ml::floatToHalf(-0.0f) == 0x8000, "half -0");
    check(ml::floatToHalf(1.0f) == 0x3c00, "half 1.0");
    check(ml::floatToHalf(-2.0f) == 0xc000, "half -2.0");
    check(ml::floatToHalf(65504.0f) == 0x7bff, "half max finite");
    check(ml::floatToHalf(65519.0f) == 0x7bff, "half rounds below overflow");
    check(ml::floatToHalf(65520.0f) == 0x7c00, "half overflow tie");
    check(ml::floatToHalf(std::numeric_limits<float>::infinity()) == 0x7c00,
          "half +infinity");
    check(ml::floatToHalf(-std::numeric_limits<float>::infinity()) == 0xfc00,
          "half -infinity");

    const std::uint16_t nan = ml::floatToHalf(
        std::numeric_limits<float>::quiet_NaN());
    check((nan & 0x7c00u) == 0x7c00u && (nan & 0x03ffu) != 0,
          "half NaN");

    check(ml::floatToHalf(std::ldexp(1.0f, -24)) == 0x0001,
          "half minimum subnormal");
    check(ml::floatToHalf(std::ldexp(1.0f, -25)) == 0x0000,
          "half subnormal tie rounds to even");

    for (std::uint32_t bits = 0; bits <= 0x7bffu; ++bits) {
        check(ml::floatToHalf(ml::halfToFloat(static_cast<std::uint16_t>(bits))) ==
                  bits,
              "half positive round-trip");
        const std::uint16_t negative =
            static_cast<std::uint16_t>(bits | 0x8000u);
        check(ml::floatToHalf(ml::halfToFloat(negative)) == negative,
              "half negative round-trip");
    }
}

void testImageProbe() {
    const std::uint8_t png[24] = {0x89, 'P', 'N',  'G',  0x0d, 0x0a, 0x1a,
                                  0x0a, 0x00, 0x00, 0x00, 0x0d, 'I',  'H',
                                  'D',  'R',  0x00, 0x00, 0x0a, 0x00, 0x00,
                                  0x00, 0x14, 0x00};
    int width = 0;
    int height = 0;
    check(io::probeImageDimensions(png, sizeof(png), width, height),
          "PNG header probe succeeds");
    check(width == 2560 && height == 5120, "PNG header dimensions");

    const std::uint8_t jpeg[] = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x04, 0x00, 0x00, 0xff, 0xc0,
        0x00, 0x0b, 0x08, 0x00, 0x0a, 0x00, 0x0c, 0x01, 0x01, 0x11,
        0x00};
    check(io::probeImageDimensions(jpeg, sizeof(jpeg), width, height),
          "JPEG SOF probe succeeds");
    check(width == 12 && height == 10, "JPEG SOF dimensions");

    std::uint8_t oversized[24] = {};
    std::copy(png, png + sizeof(png), oversized);
    oversized[18] = 0x17;
    oversized[19] = 0x70;
    oversized[22] = 0x13;
    oversized[23] = 0x88;
    check(io::probeImageDimensions(oversized, sizeof(oversized), width, height),
          "oversized PNG header probe succeeds");
    check(io::exceedsPixelLimit(width, height),
          "oversized header is rejected by the pixel limit");

    check(!io::probeImageDimensions(nullptr, 0, width, height),
          "empty header probe is rejected");
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

void testRgbaPngRoundTrip() {
    const auto path =
        std::filesystem::temp_directory_path() / "imrelighter-rgba-test.png";
    const std::vector<uint8_t> pixels = {
        255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0,
        16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192,
    };
    std::string error;
    check(io::writeRgba8Png(path, pixels, 3, 2, &error),
          "write RGBA PNG succeeds");

    const auto loaded = io::loadImage(path, &error);
    check(loaded.has_value(), "RGBA PNG decodes");
    if (loaded) {
        check(loaded->width == 3 && loaded->height == 2,
              "RGBA PNG dimensions round-trip");
        check(loaded->rgba == pixels, "RGBA PNG pixels round-trip");
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

namespace {

void appendVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>(value) | 0x80);
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

void appendTensor(std::vector<uint8_t>& graph, int dataType) {
    std::vector<uint8_t> tensor;
    appendVarint(tensor, 2u << 3); // TensorProto.data_type
    appendVarint(tensor, static_cast<uint64_t>(dataType));
    appendVarint(graph, (5u << 3) | 2u); // GraphProto.initializer
    appendVarint(graph, tensor.size());
    graph.insert(graph.end(), tensor.begin(), tensor.end());
}

} // namespace

void testModelMetadata() {
    std::vector<uint8_t> graph;
    appendTensor(graph, 10); // FLOAT16
    appendTensor(graph, 10);
    appendTensor(graph, 1); // FLOAT

    std::vector<uint8_t> model;
    appendVarint(model, (7u << 3) | 2u); // ModelProto.graph
    appendVarint(model, graph.size());
    model.insert(model.end(), graph.begin(), graph.end());

    const auto path =
        std::filesystem::temp_directory_path() / "imrelighter-metadata-test.onnx";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(model.data()),
                     static_cast<std::streamsize>(model.size()));
    }

    ml::WeightInfo info;
    std::string error;
    check(ml::inspectModelWeights(path, info, &error),
          "model metadata parses minimal ONNX");
    check(info.fp16 == 2 && info.fp32 == 1 && info.other == 0,
          "model metadata counts initializer types");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void testConfigRoundTrip() {
    const auto directory =
        std::filesystem::temp_directory_path() / "imrelighter-config-test";
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    std::filesystem::create_directories(directory, ec);
    const auto path = directory / L"config.json";

    core::UserConfig config;
    config.language = core::Language::ZhCN;
    config.languageSet = true;
    config.backend = core::Backend::Dml;
    config.theme = core::Theme::Light;
    config.modelFile = "selected-model.onnx";
    config.modelSelectionCleared = true;
    config.maxTasks = 7;
    config.memoryLimitGb = 9;

    std::string error;
    check(core::saveUserConfigTo(path, config, &error),
          "config saves atomically");
    check(std::filesystem::exists(path), "config file exists");
    check(!std::filesystem::exists(path.string() + ".tmp"),
          "config temp file is removed");

    core::UserConfig loaded;
    check(core::loadUserConfigFrom(path, loaded, &error), "config loads");
    check(loaded.languageSet && loaded.language == core::Language::ZhCN &&
              loaded.backend == core::Backend::Dml &&
              loaded.theme == core::Theme::Light &&
              loaded.modelFile == config.modelFile &&
              loaded.modelSelectionCleared && loaded.maxTasks == 7 &&
              loaded.memoryLimitGb == 9,
          "config round-trip preserves values");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << "{not valid json";
    }
    error.clear();
    check(!core::loadUserConfigFrom(path, loaded, &error) && !error.empty(),
          "malformed config reports an error");

    error.clear();
    check(!core::saveUserConfigTo(directory / L"missing" / L"config.json",
                                  config, &error) &&
              !error.empty(),
          "unwritable config reports an error");

    std::filesystem::remove_all(directory, ec);
}

} // namespace

int main() {
    testConfigRoundTrip();
    testModelConfig();
    testSha256();
    testWebp();
    testI18nParity();
    testPreprocessShape();
    testPostprocessOrientation();
    testPostprocessFlatAndNaN();
    testExifOrientation();
    testExifParsing();
    testHalfConversion();
    testImageProbe();
    testDepthPngHeader();
    testRgbaPngRoundTrip();
    testModelMetadata();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all core tests passed\n";
    return 0;
}
