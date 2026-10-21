#pragma once

#include <android/asset_manager.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/types.h"

namespace imrelighter::io {

bool readAsset(AAssetManager* assets, const std::string& path,
               std::vector<uint8_t>& bytes, std::string& error);

bool decodeImage(const uint8_t* bytes, size_t size, ImageData& image,
                 std::string& error);

bool loadAssetImage(AAssetManager* assets, const std::string& path,
                    ImageData& image, std::string& error);

}  // namespace imrelighter::io
