#pragma once

#include <string>
#include <vector>

#include "core/types.h"

namespace imrelighter::ml {

bool buildDepthAndNormal(const std::vector<float>& raw_depth, int raw_width,
                         int raw_height, int target_width, int target_height,
                         DepthResult& result, std::string& error);

}  // namespace imrelighter::ml
