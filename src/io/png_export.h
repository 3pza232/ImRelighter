#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace io {

bool writeRgba8Png(const std::filesystem::path& path,
                   const std::vector<uint8_t>& rgba, int width, int height,
                   std::string* error);

bool writeDepth16Png(const std::filesystem::path& path,
                     const std::vector<float>& depth, int width, int height,
                     bool invert, std::string* error);

} // namespace io