#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace io {

// Returns the EXIF orientation (1..8) from a JPEG byte stream, or 0 when the
// stream has no usable orientation tag. Parsing is bounds-checked and accepts
// both little- and big-endian TIFF headers.
int readJpegOrientation(const std::uint8_t* data, std::size_t size);

// Applies an EXIF orientation to top-down RGBA pixels. Orientation 1 is a
// no-op. Returns false for invalid orientation values or mismatched sizes.
bool applyExifOrientation(std::vector<std::uint8_t>& rgba, int& width,
                          int& height, int orientation);

} // namespace io
