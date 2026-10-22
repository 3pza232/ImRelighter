#pragma once

#include <string>
#include <vector>

#include "core/types.h"
#include "ml/model_config.h"

namespace imrelighter::ml {

bool preprocessImage(const ImageData& image, const ModelConfig& config,
                     std::vector<float>& tensor, int& input_width,
                     int& input_height, std::string& error);

bool preprocessImage(const ImageData& image, std::vector<float>& tensor,
                     int& input_width, int& input_height, std::string& error);

}  // namespace imrelighter::ml
