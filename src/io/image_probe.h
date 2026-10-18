#pragma once

#include <cstddef>
#include <cstdint>

namespace io {

// Reads dimensions from PNG or JPEG headers without decoding pixel data.
// Returns false for other formats or malformed headers.
bool probeImageDimensions(const std::uint8_t* data, std::size_t size,
                          int& width, int& height);

} // namespace io
