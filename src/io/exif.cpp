#include "io/exif.h"

#include <algorithm>
#include <cstring>

namespace io {

namespace {

std::uint16_t read16(const std::uint8_t* data, bool littleEndian) {
    if (littleEndian) {
        return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    }
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

std::uint32_t read32(const std::uint8_t* data, bool littleEndian) {
    if (littleEndian) {
        return static_cast<std::uint32_t>(data[0]) |
               (static_cast<std::uint32_t>(data[1]) << 8) |
               (static_cast<std::uint32_t>(data[2]) << 16) |
               (static_cast<std::uint32_t>(data[3]) << 24);
    }
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::uint16_t readBe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

int orientationFromTiff(const std::uint8_t* tiff, std::size_t size) {
    if (size < 8) return 0;

    bool littleEndian = false;
    if (tiff[0] == 'I' && tiff[1] == 'I') {
        littleEndian = true;
    } else if (tiff[0] == 'M' && tiff[1] == 'M') {
        littleEndian = false;
    } else {
        return 0;
    }

    if (read16(tiff + 2, littleEndian) != 42) return 0;
    const std::uint32_t ifdOffset = read32(tiff + 4, littleEndian);
    if (ifdOffset > size - 2) return 0;

    const std::uint16_t entryCount = read16(tiff + ifdOffset, littleEndian);
    const std::size_t entriesBegin = static_cast<std::size_t>(ifdOffset) + 2;
    if (entryCount > (size - entriesBegin) / 12) return 0;

    for (std::uint16_t i = 0; i < entryCount; ++i) {
        const std::uint8_t* entry = tiff + entriesBegin + i * 12;
        const std::uint16_t tag = read16(entry, littleEndian);
        if (tag != 0x0112) continue;

        const std::uint16_t type = read16(entry + 2, littleEndian);
        const std::uint32_t count = read32(entry + 4, littleEndian);
        if (type != 3 || count < 1) return 0;

        std::uint16_t value = 0;
        if (count == 1) {
            value = read16(entry + 8, littleEndian);
        } else {
            const std::uint32_t valueOffset = read32(entry + 8, littleEndian);
            if (valueOffset > size - 2) return 0;
            value = read16(tiff + valueOffset, littleEndian);
        }
        return (value >= 1 && value <= 8) ? value : 0;
    }
    return 0;
}

} // namespace

int readJpegOrientation(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 4 || data[0] != 0xFF || data[1] != 0xD8) return 0;

    std::size_t pos = 2;
    while (pos + 2 <= size) {
        if (data[pos] != 0xFF) {
            ++pos;
            continue;
        }
        while (pos < size && data[pos] == 0xFF) ++pos;
        if (pos >= size) return 0;

        const std::uint8_t marker = data[pos++];
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) continue;
        if (marker == 0xD9 || marker == 0xDA) return 0;

        if (pos + 2 > size) return 0;
        const std::uint16_t length = readBe16(data + pos);
        if (length < 2 || length > size - pos) return 0;

        if (marker == 0xE1 && length >= 8 &&
            std::memcmp(data + pos + 2, "Exif\0\0", 6) == 0) {
            const int orientation =
                orientationFromTiff(data + pos + 8, length - 8);
            if (orientation != 0) return orientation;
        }
        pos += length;
    }
    return 0;
}

bool applyExifOrientation(std::vector<std::uint8_t>& rgba, int& width,
                          int& height, int orientation) {
    if (orientation < 1 || orientation > 8) return false;
    if (width <= 0 || height <= 0 ||
        rgba.size() != static_cast<std::size_t>(width) * height * 4) {
        return false;
    }
    if (orientation == 1) return true;

    const bool swapped = orientation >= 5;
    const int outWidth = swapped ? height : width;
    const int outHeight = swapped ? width : height;
    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(outWidth) * outHeight * 4);

    for (int y = 0; y < outHeight; ++y) {
        for (int x = 0; x < outWidth; ++x) {
            int sx = x;
            int sy = y;
            switch (orientation) {
            case 2: sx = width - 1 - x; break;
            case 3: sx = width - 1 - x; sy = height - 1 - y; break;
            case 4: sy = height - 1 - y; break;
            case 5: sx = y; sy = x; break;
            case 6: sx = y; sy = height - 1 - x; break;
            case 7: sx = width - 1 - y; sy = height - 1 - x; break;
            case 8: sx = width - 1 - y; sy = x; break;
            default: break;
            }

            const auto source =
                (static_cast<std::size_t>(sy) * width + sx) * 4;
            const auto target =
                (static_cast<std::size_t>(y) * outWidth + x) * 4;
            std::memcpy(output.data() + target, rgba.data() + source, 4);
        }
    }

    rgba.swap(output);
    width = outWidth;
    height = outHeight;
    return true;
}

} // namespace io
