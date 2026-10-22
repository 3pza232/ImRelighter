#pragma once

#include <android/asset_manager.h>

#include <string>

namespace imrelighter::ml {

// Codes mirror the Hugging Face / PIL resample values.
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
    bool doResize = true;
    bool invertDepth = false;
    Resample resample = Resample::Default;
    bool loaded = false;
};

ModelConfig defaultModelConfig();

// Resolves preprocessor_config.json from the model directory, then its parent,
// matching the desktop implementation. The model path may be an APK asset path
// (for bundled q4) or an absolute local filesystem path (custom ONNX).
ModelConfig loadModelConfig(AAssetManager* assets, const std::string& modelPath);

}  // namespace imrelighter::ml
