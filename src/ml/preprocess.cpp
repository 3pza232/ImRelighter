#include "ml/preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "stb_image_resize2.h"

namespace ml {
namespace {

int roundToMultiple(float value, int multiple) {
    const int unit = std::max(1, multiple);
    const int steps = static_cast<int>(std::nearbyint(value / unit));
    return std::max(unit, steps * unit);
}

} // namespace

bool preprocess(const uint8_t* rgba, int width, int height,
                PreprocessResult& result, std::string* error,
                const ModelConfig& config) {
    if (!rgba || width <= 0 || height <= 0 ||
        config.inputSize <= 0 || config.sizeMultiple <= 0) {
        if (error) *error = "Invalid image or model preprocessing configuration.";
        return false;
    }

    const float target = static_cast<float>(config.inputSize);
    float scaleWidth = target / static_cast<float>(width);
    float scaleHeight = target / static_cast<float>(height);
    if (std::abs(1.0f - scaleWidth) < std::abs(1.0f - scaleHeight)) {
        scaleHeight = scaleWidth;
    } else {
        scaleWidth = scaleHeight;
    }

    const int newWidth =
        roundToMultiple(scaleWidth * static_cast<float>(width), config.sizeMultiple);
    const int newHeight =
        roundToMultiple(scaleHeight * static_cast<float>(height), config.sizeMultiple);

    std::vector<uint8_t> resized(static_cast<size_t>(newWidth) * newHeight * 4);
    if (!stbir_resize_uint8_linear(rgba, width, height, 0, resized.data(),
                                   newWidth, newHeight, 0, STBIR_RGBA)) {
        if (error) *error = "Image resize failed.";
        return false;
    }

    result.width = newWidth;
    result.height = newHeight;
    const size_t plane = static_cast<size_t>(newWidth) * newHeight;
    result.tensor.resize(plane * 3);

    const float* mean = config.mean;
    const float* stddev = config.stddev;
    for (int channel = 0; channel < 3; ++channel) {
        for (size_t i = 0; i < plane; ++i) {
            float value =
                static_cast<float>(resized[i * 4 + static_cast<size_t>(channel)]);
            if (config.rescale) value /= 255.0f;
            if (config.normalize) value = (value - mean[channel]) / stddev[channel];
            result.tensor[static_cast<size_t>(channel) * plane + i] = value;
        }
    }
    return true;
}

bool postprocess(const std::vector<float>& raw, int rawWidth, int rawHeight,
                 int targetWidth, int targetHeight,
                 std::vector<float>& depth, std::string* error,
                 bool invertDepth) {
    if (raw.empty() || rawWidth <= 0 || rawHeight <= 0 || targetWidth <= 0 ||
        targetHeight <= 0) {
        if (error) *error = "Invalid depth tensor.";
        return false;
    }

    std::vector<float> resized(
        static_cast<size_t>(targetWidth) * targetHeight);
    if (!stbir_resize_float_linear(raw.data(), rawWidth, rawHeight, 0,
                                   resized.data(), targetWidth, targetHeight, 0,
                                   STBIR_1CHANNEL)) {
        if (error) *error = "Depth resize failed.";
        return false;
    }

    float minimum = std::numeric_limits<float>::max();
    float maximum = -std::numeric_limits<float>::max();
    for (float& value : resized) {
        if (!std::isfinite(value)) value = 0.0f;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }

    const float span = maximum - minimum;
    if (span < 1e-8f) {
        if (error) *error = "Depth output has no variation.";
        return false;
    } else {
        for (float& value : resized) {
            value = std::clamp((value - minimum) / span, 0.0f, 1.0f);
        }
    }

    if (invertDepth) {
        for (float& value : resized) value = 1.0f - value;
    }

    const size_t rowWidth = static_cast<size_t>(targetWidth);
    for (int y = 0; y < targetHeight / 2; ++y) {
        auto top = resized.begin() + static_cast<std::ptrdiff_t>(y) *
                                        static_cast<std::ptrdiff_t>(rowWidth);
        auto bottom = resized.begin() +
                      static_cast<std::ptrdiff_t>(targetHeight - 1 - y) *
                          static_cast<std::ptrdiff_t>(rowWidth);
        std::swap_ranges(top, top + static_cast<std::ptrdiff_t>(rowWidth), bottom);
    }

    depth = std::move(resized);
    return true;
}

} // namespace ml