#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "io/image_limits.h"

namespace io {

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // top-down
};

std::optional<ImageData> loadImage(
    const std::filesystem::path& path, std::string* error = nullptr,
    std::int64_t maxPixels =
        megapixelsToPixels(kDefaultImageMegapixels));

bool makeThumbnail(const ImageData& image, int maxSize, std::vector<uint8_t>& out,
                   int& thumbWidth, int& thumbHeight);

} // namespace io
