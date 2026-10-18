#include "io/image_probe.h"

#include <climits>
#include <cstring>

namespace io {
namespace {

std::uint32_t readBigEndian32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

bool probePng(const std::uint8_t* data, std::size_t size, int& width,
              int& height) {
    static constexpr std::uint8_t signature[8] = {0x89, 'P', 'N', 'G',
                                                  0x0d, 0x0a, 0x1a, 0x0a};
    if (size < 24 || std::memcmp(data, signature, sizeof(signature)) != 0) {
        return false;
    }
    if (std::memcmp(data + 12, "IHDR", 4) != 0 ||
        readBigEndian32(data + 8) != 13) {
        return false;
    }

    const std::uint32_t w = readBigEndian32(data + 16);
    const std::uint32_t h = readBigEndian32(data + 20);
    if (w == 0 || h == 0 || w > INT_MAX || h > INT_MAX) return false;
    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

bool isStartOfFrame(std::uint8_t marker) {
    // SOF0-SOF15 except DHT (0xC4), JPG (0xC8), and DAC (0xCC).
    return marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 &&
           marker != 0xc8 && marker != 0xcc;
}

bool probeJpeg(const std::uint8_t* data, std::size_t size, int& width,
               int& height) {
    if (size < 4 || data[0] != 0xff || data[1] != 0xd8) return false;

    std::size_t pos = 2;
    while (pos + 1 < size) {
        if (data[pos] != 0xff) return false;
        while (pos < size && data[pos] == 0xff) ++pos;
        if (pos >= size) return false;

        const std::uint8_t marker = data[pos++];
        if (marker == 0xd8 || marker == 0x01 ||
            (marker >= 0xd0 && marker <= 0xd7)) {
            continue;
        }
        if (marker == 0xd9 || marker == 0xda) return false;
        if (pos + 2 > size) return false;

        const std::size_t segmentLength =
            (static_cast<std::size_t>(data[pos]) << 8) | data[pos + 1];
        if (segmentLength < 2 || pos + segmentLength > size) return false;
        if (isStartOfFrame(marker)) {
            if (segmentLength < 7) return false;
            const int h = (static_cast<int>(data[pos + 3]) << 8) |
                          data[pos + 4];
            const int w = (static_cast<int>(data[pos + 5]) << 8) |
                          data[pos + 6];
            if (w <= 0 || h <= 0) return false;
            width = w;
            height = h;
            return true;
        }
        pos += segmentLength;
    }
    return false;
}

} // namespace

bool probeImageDimensions(const std::uint8_t* data, std::size_t size,
                          int& width, int& height) {
    if (!data || size == 0) return false;
    if (probePng(data, size, width, height)) return true;
    return probeJpeg(data, size, width, height);
}

} // namespace io
