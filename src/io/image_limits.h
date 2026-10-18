#pragma once

#include <cstdint>

namespace io {

// Decoding larger images would allocate hundreds of megabytes before the
// model or renderer ever sees them, so enforce the limit from headers first.
inline constexpr int kMaxImagePixels = 24'000'000;

inline bool exceedsPixelLimit(std::int64_t width, std::int64_t height) {
    return width <= 0 || height <= 0 ||
           width * height > static_cast<std::int64_t>(kMaxImagePixels);
}

} // namespace io
