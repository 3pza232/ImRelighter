#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ml {

struct PreprocessResult {
    int width = 0;
    int height = 0;
    std::vector<float> tensor; // NCHW, 1x3xHxW
};

bool preprocess(const uint8_t* rgba, int width, int height,
                PreprocessResult& result, std::string* error);

bool postprocess(const std::vector<float>& raw, int rawWidth, int rawHeight,
                 int targetWidth, int targetHeight,
                 std::vector<float>& depth, std::string* error);

} // namespace ml