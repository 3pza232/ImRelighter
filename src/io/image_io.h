#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace io {

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // top-down
};

std::optional<ImageData> loadImage(const std::filesystem::path& path,
                                   std::string* error = nullptr);

bool makeThumbnail(const ImageData& image, int maxSize, std::vector<uint8_t>& out,
                   int& thumbWidth, int& thumbHeight);

} // namespace io
