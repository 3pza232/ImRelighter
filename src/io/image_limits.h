#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace io {

// Decoding larger images would allocate hundreds of megabytes before the
// model or renderer ever sees them, so enforce the limit from headers first.
inline constexpr int kMinImageMegapixels = 4;
inline constexpr int kMaxImageMegapixels = 200;
inline constexpr int kDefaultImageMegapixels = 30;

inline constexpr std::int64_t megapixelsToPixels(int megapixels) {
    return static_cast<std::int64_t>(megapixels) * 1'000'000;
}

inline bool exceedsPixelLimit(
    std::int64_t width, std::int64_t height,
    std::int64_t maxPixels = megapixelsToPixels(kDefaultImageMegapixels)) {
    return width <= 0 || height <= 0 || width * height > maxPixels;
}

inline std::string pixelLimitMessage(int width, int height,
                                     std::int64_t maxPixels) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer),
                  "%d × %d (%.1f MP) exceeds the %lld MP limit.", width,
                  height, static_cast<double>(width) * height / 1'000'000.0,
                  static_cast<long long>(maxPixels / 1'000'000));
    return buffer;
}

} // namespace io
