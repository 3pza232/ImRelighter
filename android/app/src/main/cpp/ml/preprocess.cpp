#include "ml/preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "stb_image_resize2.h"

namespace imrelighter::ml {
namespace {

constexpr int kInputSize = 518;
constexpr int kSizeMultiple = 14;
constexpr float kMean[3] = {0.485f, 0.456f, 0.406f};
constexpr float kStd[3] = {0.229f, 0.224f, 0.225f};
constexpr float kRescale = 1.0f / 255.0f;

int roundToMultiple(float value, int multiple) {
    const int unit = std::max(1, multiple);
    const int steps = static_cast<int>(std::nearbyint(value / unit));
    return std::max(unit, steps * unit);
}

}  // namespace

bool preprocessImage(const ImageData& image, std::vector<float>& tensor,
                     int& input_width, int& input_height, std::string& error) {
    if (!image.valid()) {
        error = "invalid image";
        return false;
    }

    const float scale_width =
        static_cast<float>(kInputSize) / static_cast<float>(image.width);
    const float scale_height =
        static_cast<float>(kInputSize) / static_cast<float>(image.height);
    float scale_x = scale_width;
    float scale_y = scale_height;
    if (std::abs(1.0f - scale_width) < std::abs(1.0f - scale_height)) {
        scale_y = scale_width;
    } else {
        scale_x = scale_height;
    }

    input_width = roundToMultiple(scale_x * static_cast<float>(image.width),
                                  kSizeMultiple);
    input_height = roundToMultiple(scale_y * static_cast<float>(image.height),
                                   kSizeMultiple);
    std::vector<uint8_t> resized(static_cast<size_t>(input_width) *
                                 static_cast<size_t>(input_height) * 4);
    if (!stbir_resize(image.rgba.data(), image.width, image.height, 0,
                      resized.data(), input_width, input_height, 0, STBIR_RGBA,
                      STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP,
                      STBIR_FILTER_CATMULLROM)) {
        error = "image resize failed";
        return false;
    }

    const size_t plane =
        static_cast<size_t>(input_width) * static_cast<size_t>(input_height);
    tensor.assign(plane * 3, 0.0f);
    for (int channel = 0; channel < 3; ++channel) {
        for (size_t i = 0; i < plane; ++i) {
            const float value =
                static_cast<float>(resized[i * 4 + static_cast<size_t>(channel)]) *
                kRescale;
            tensor[static_cast<size_t>(channel) * plane + i] =
                (value - kMean[channel]) / kStd[channel];
        }
    }
    return true;
}

}  // namespace imrelighter::ml
