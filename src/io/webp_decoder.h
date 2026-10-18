#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "io/image_limits.h"

namespace io {

bool decodeWebp(
    const uint8_t* bytes, size_t size, int& width, int& height,
    std::vector<uint8_t>& rgba, std::string* error,
    std::int64_t maxPixels = megapixelsToPixels(kDefaultImageMegapixels));

} // namespace io
