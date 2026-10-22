#include "ml/preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "stb_image_resize2.h"

namespace imrelighter::ml {
namespace {

int roundToMultiple(float value, int multiple) {
    const int unit = std::max(1, multiple);
    const int steps = static_cast<int>(std::nearbyint(value / unit));
    return std::max(unit, steps * unit);
}

stbir_filter filterFor(Resample resample) {
    switch (resample) {
        case Resample::Nearest:
            return STBIR_FILTER_POINT_SAMPLE;
        case Resample::Lanczos:
            return STBIR_FILTER_CATMULLROM;
        case Resample::Bilinear:
            return STBIR_FILTER_TRIANGLE;
        case Resample::Bicubic:
            return STBIR_FILTER_CATMULLROM;
        case Resample::Box:
            return STBIR_FILTER_BOX;
        case Resample::Hamming:
            return STBIR_FILTER_MITCHELL;
        case Resample::Default:
            break;
    }
    return STBIR_FILTER_DEFAULT;
}

}  // namespace

bool preprocessImage(const ImageData& image, const ModelConfig& config,
                     std::vector<float>& tensor, int& input_width,
                     int& input_height, std::string& error) {
    if (!image.valid()) {
        error = "invalid image";
        return false;
    }
    if (config.inputSize <= 0 || config.sizeMultiple <= 0) {
        error = "invalid preprocessor configuration";
        return false;
    }

    const float target = static_cast<float>(config.inputSize);
    const float scale_width = target / static_cast<float>(image.width);
    const float scale_height = target / static_cast<float>(image.height);
    float scale_x = scale_width;
    float scale_y = scale_height;
    if (std::abs(1.0f - scale_width) < std::abs(1.0f - scale_height)) {
        scale_y = scale_width;
    } else {
        scale_x = scale_height;
    }

    input_width = config.doResize
                      ? roundToMultiple(scale_x * static_cast<float>(image.width),
                                        config.sizeMultiple)
                      : image.width;
    input_height = config.doResize
                       ? roundToMultiple(scale_y * static_cast<float>(image.height),
                                         config.sizeMultiple)
                       : image.height;
    if (input_width <= 0 || input_height <= 0) {
        error = "invalid resize dimensions";
        return false;
    }

    const size_t plane =
        static_cast<size_t>(input_width) * static_cast<size_t>(input_height);
    std::vector<uint8_t> resized(plane * 4);
    if (config.doResize) {
        if (!stbir_resize(image.rgba.data(), image.width, image.height, 0,
                          resized.data(), input_width, input_height, 0, STBIR_RGBA,
                          STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP,
                          filterFor(config.resample))) {
            error = "image resize failed";
            return false;
        }
    } else {
        if (image.rgba.size() != plane * 4) {
            error = "image size does not match its dimensions";
            return false;
        }
        std::copy(image.rgba.begin(), image.rgba.end(), resized.begin());
    }

    tensor.assign(plane * 3, 0.0f);
    for (int channel = 0; channel < 3; ++channel) {
        for (size_t i = 0; i < plane; ++i) {
            float value =
                static_cast<float>(resized[i * 4 + static_cast<size_t>(channel)]);
            if (config.rescale) {
                value *= config.rescaleFactor;
            }
            if (config.normalize) {
                value = (value - config.mean[channel]) / config.stddev[channel];
            }
            tensor[static_cast<size_t>(channel) * plane + i] = value;
        }
    }
    return true;
}

bool preprocessImage(const ImageData& image, std::vector<float>& tensor,
                     int& input_width, int& input_height, std::string& error) {
    return preprocessImage(image, defaultModelConfig(), tensor, input_width,
                           input_height, error);
}

}  // namespace imrelighter::ml
