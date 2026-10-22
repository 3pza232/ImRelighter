#pragma once

#include <android/asset_manager.h>

#include <cstdint>
#include <string>

namespace imrelighter::ml {

struct WeightInfo {
    int fp16 = 0;
    int fp32 = 0;
    int bf16 = 0;
    int int8 = 0;
    int uint8 = 0;
    int int4 = 0;
    int uint4 = 0;
    int other = 0;
};

// Inspects ONNX initializer data types without creating an ORT session.
// Returns one of q4, q4f16, int8, fp16, bf16, fp32 or unknown.
std::string detectModelType(AAssetManager* assets, const std::string& path);

}  // namespace imrelighter::ml
