#pragma once

#include <filesystem>
#include <string>

namespace ml {

struct WeightInfo {
    int fp16 = 0;
    int fp32 = 0;
    int other = 0;
};

bool inspectModelWeights(const std::filesystem::path& modelPath,
                         WeightInfo& info, std::string* error);

} // namespace ml
