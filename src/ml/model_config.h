#pragma once

#include <filesystem>
#include <string>

namespace ml {

struct ModelConfig {
    int inputSize = 518;
    int sizeMultiple = 14;
    float mean[3] = {0.485f, 0.456f, 0.406f};
    float stddev[3] = {0.229f, 0.224f, 0.225f};
    bool rescale = true;
    bool normalize = true;
    bool invertDepth = false;
    bool loaded = false;
};

ModelConfig defaults();

ModelConfig loadModelConfig(const std::filesystem::path& modelPath);

} // namespace ml
