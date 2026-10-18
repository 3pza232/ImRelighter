#pragma once

#include <filesystem>
#include <string>

namespace ml {

// Resampling filters follow the integer codes used by PIL/Pillow, which is
// what Hugging Face image processors store in "resample".
enum class Resample {
    Default = -1,
    Nearest = 0,
    Lanczos = 1,
    Bilinear = 2,
    Bicubic = 3,
    Box = 4,
    Hamming = 5,
};

struct ModelConfig {
    int inputSize = 518;
    int sizeMultiple = 14;
    float mean[3] = {0.485f, 0.456f, 0.406f};
    float stddev[3] = {0.229f, 0.224f, 0.225f};
    float rescaleFactor = 1.0f / 255.0f;
    bool rescale = true;
    bool normalize = true;
    bool invertDepth = false;
    bool doResize = true;
    Resample resample = Resample::Default;
    bool loaded = false;
};

ModelConfig defaults();

ModelConfig loadModelConfig(const std::filesystem::path& modelPath);

} // namespace ml
