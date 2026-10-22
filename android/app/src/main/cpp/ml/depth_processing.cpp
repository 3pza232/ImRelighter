#include "ml/depth_processing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <android/log.h>

#include "stb_image_resize2.h"

namespace imrelighter::ml {
namespace {

constexpr const char* kTag = "ImRelighter";

std::vector<float> medianFilterDepth(const std::vector<float>& input, int width,
                                     int height, int radius) {
    std::vector<float> output(static_cast<size_t>(width) * height);
    std::vector<float> values(
        static_cast<size_t>((radius * 2 + 1) * (radius * 2 + 1)));
    const size_t center = values.size() / 2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = 0;
            for (int oy = -radius; oy <= radius; ++oy) {
                const int sy = std::clamp(y + oy, 0, height - 1);
                for (int ox = -radius; ox <= radius; ++ox) {
                    const int sx = std::clamp(x + ox, 0, width - 1);
                    values[index++] = input[static_cast<size_t>(sy) * width + sx];
                }
            }
            std::nth_element(values.begin(), values.begin() + center,
                             values.end());
            output[static_cast<size_t>(y) * width + x] = values[center];
        }
    }
    return output;
}

}  // namespace

bool buildDepthAndNormal(const std::vector<float>& raw_depth, int raw_width,
                         int raw_height, int target_width, int target_height,
                         DepthResult& result, std::string& error,
                         bool invert_depth) {
    result = DepthResult{};
    if (raw_depth.empty() || raw_width <= 0 || raw_height <= 0 ||
        target_width <= 0 || target_height <= 0 ||
        raw_depth.size() < static_cast<size_t>(raw_width) * raw_height) {
        error = "invalid depth tensor";
        return false;
    }

    std::vector<float> sanitized(
        raw_depth.begin(),
        raw_depth.begin() + static_cast<size_t>(raw_width) * raw_height);
    for (float& value : sanitized) {
        if (!std::isfinite(value)) {
            value = 0.0f;
        }
    }

    const std::vector<float> filtered =
        medianFilterDepth(sanitized, raw_width, raw_height, 2);

    float minimum = std::numeric_limits<float>::max();
    float maximum = -std::numeric_limits<float>::max();
    for (const float value : filtered) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    const float span = maximum - minimum;
    if (span < 1e-8f) {
        error = "depth output has no variation";
        return false;
    }

    std::vector<float> normalized_raw(filtered.size());
    for (size_t i = 0; i < filtered.size(); ++i) {
        float value = std::clamp((filtered[i] - minimum) / span, 0.0f, 1.0f);
        normalized_raw[i] = invert_depth ? 1.0f - value : value;
    }

    std::vector<float> resized(static_cast<size_t>(target_width) * target_height);
    if (!stbir_resize_float_linear(normalized_raw.data(), raw_width, raw_height,
                                   0, resized.data(), target_width, target_height,
                                   0, STBIR_1CHANNEL)) {
        error = "depth resize failed";
        return false;
    }

    result.width = target_width;
    result.height = target_height;
    result.depth_min = minimum;
    result.depth_max = maximum;
    result.normalized_depth = resized;
    result.depth_rgba.resize(resized.size() * 4);
    for (size_t i = 0; i < resized.size(); ++i) {
        const uint8_t value = static_cast<uint8_t>(
            std::clamp(resized[i] * 255.0f, 0.0f, 255.0f));
        result.depth_rgba[i * 4 + 0] = value;
        result.depth_rgba[i * 4 + 1] = value;
        result.depth_rgba[i * 4 + 2] = value;
        result.depth_rgba[i * 4 + 3] = 255;
    }

    std::vector<float> median_raw(normalized_raw.size());
    for (int y = 0; y < raw_height; ++y) {
        for (int x = 0; x < raw_width; ++x) {
            std::array<float, 9> values{};
            int index = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    const int sx = std::clamp(x + ox, 0, raw_width - 1);
                    const int sy = std::clamp(y + oy, 0, raw_height - 1);
                    values[index++] =
                        normalized_raw[static_cast<size_t>(sy) * raw_width + sx];
                }
            }
            std::sort(values.begin(), values.end());
            median_raw[static_cast<size_t>(y) * raw_width + x] = values[4];
        }
    }

    std::vector<uint8_t> normal_raw(
        static_cast<size_t>(raw_width) * raw_height * 4);
    const float normal_scale = static_cast<float>(raw_height) * 0.5f;
    for (int y = 0; y < raw_height; ++y) {
        for (int x = 0; x < raw_width; ++x) {
            const int left = std::max(0, x - 1);
            const int right = std::min(raw_width - 1, x + 1);
            const int down = std::max(0, y - 1);
            const int up = std::min(raw_height - 1, y + 1);
            const size_t row = static_cast<size_t>(y) * raw_width;
            const float dx =
                (median_raw[row + right] - median_raw[row + left]) * 0.5f;
            const float dy =
                (median_raw[static_cast<size_t>(up) * raw_width + x] -
                 median_raw[static_cast<size_t>(down) * raw_width + x]) *
                0.5f;
            const float nx = -dx * normal_scale;
            const float ny = dy * normal_scale;
            const float nz = 1.0f;
            const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            const size_t i = (row + x) * 4;
            normal_raw[i + 0] = static_cast<uint8_t>(std::clamp(
                (nx / length * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            normal_raw[i + 1] = static_cast<uint8_t>(std::clamp(
                (ny / length * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            normal_raw[i + 2] = static_cast<uint8_t>(std::clamp(
                (nz / length * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            normal_raw[i + 3] = 255;
        }
    }

    result.normal_rgba.resize(static_cast<size_t>(target_width) * target_height *
                              4);
    if (!stbir_resize(normal_raw.data(), raw_width, raw_height, 0,
                      result.normal_rgba.data(), target_width, target_height, 0,
                      STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP,
                      STBIR_FILTER_TRIANGLE)) {
        error = "normal resize failed";
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "depth normal: %dx%d -> %dx%d", raw_width, raw_height,
                        target_width, target_height);
    return true;
}

}  // namespace imrelighter::ml
