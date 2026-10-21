#pragma once

#include <string>
#include <vector>

#include "core/types.h"

namespace imrelighter::ml {

bool preprocessImage(const ImageData& image, std::vector<float>& tensor,
                     int& input_width, int& input_height, std::string& error);

}  // namespace imrelighter::ml
