#include "io/png_export.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <system_error>

#include "stb_image_write.h"

namespace io {
namespace {

uint32_t crc32(const uint8_t* data, size_t length) {
    static std::array<uint32_t, 256> table{};
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t value = i;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1u) ? (0xedb88320u ^ (value >> 1)) : (value >> 1);
            }
            table[i] = value;
        }
        initialized = true;
    }
    uint32_t result = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        result = table[(result ^ data[i]) & 0xffu] ^ (result >> 8);
    }
    return result ^ 0xffffffffu;
}

uint32_t adler32(const std::vector<uint8_t>& data) {
    uint32_t a = 1;
    uint32_t b = 0;
    for (uint8_t value : data) {
        a = (a + value) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void appendChunk(std::vector<uint8_t>& out, const char* type,
                 const std::vector<uint8_t>& data) {
    appendU32(out, static_cast<uint32_t>(data.size()));
    const size_t typeStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    appendU32(out, crc32(out.data() + typeStart, out.size() - typeStart));
}

std::vector<uint8_t> deflateStored(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out;
    out.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    out.push_back(0x78);
    out.push_back(0x01);

    size_t offset = 0;
    while (offset < raw.size()) {
        const size_t length = std::min<size_t>(65535, raw.size() - offset);
        const bool finalBlock = offset + length == raw.size();
        out.push_back(finalBlock ? 0x01 : 0x00);
        out.push_back(static_cast<uint8_t>(length));
        out.push_back(static_cast<uint8_t>(length >> 8));
        out.push_back(static_cast<uint8_t>(~length));
        out.push_back(static_cast<uint8_t>((~length) >> 8));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                   raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
    }
    appendU32(out, adler32(raw));
    return out;
}

bool writeAtomic(const std::filesystem::path& path,
                 const std::vector<uint8_t>& bytes, std::string* error) {
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
        if (error) *error = "Cannot create output file.";
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        if (error) *error = "Cannot write output file.";
        return false;
    }

    std::error_code code;
    std::filesystem::rename(temporary, path, code);
    if (code) {
        std::filesystem::remove(path, code);
        code.clear();
        std::filesystem::rename(temporary, path, code);
    }
    if (code) {
        std::filesystem::remove(temporary, code);
        if (error) *error = "Cannot replace output file.";
        return false;
    }
    return true;
}

} // namespace

bool writeRgba8Png(const std::filesystem::path& path,
                   const std::vector<uint8_t>& rgba, int width, int height,
                   std::string* error) {
    if (width <= 0 || height <= 0 ||
        rgba.size() < static_cast<size_t>(width) * height * 4) {
        if (error) *error = "Invalid RGBA image.";
        return false;
    }
    std::vector<uint8_t> bytes;
    const auto writer = [](void* context, void* data, int size) {
        auto* output = static_cast<std::vector<uint8_t>*>(context);
        const auto* bytes = static_cast<const uint8_t*>(data);
        output->insert(output->end(), bytes, bytes + size);
    };
    if (!stbi_write_png_to_func(writer, &bytes, width, height, 4, rgba.data(),
                                width * 4) ||
        bytes.empty()) {
        if (error) *error = "PNG encoding failed.";
        return false;
    }
    return writeAtomic(path, bytes, error);
}

bool writeDepth16Png(const std::filesystem::path& path,
                     const std::vector<float>& depth, int width, int height,
                     bool invert, std::string* error) {
    if (width <= 0 || height <= 0 ||
        depth.size() < static_cast<size_t>(width) * height) {
        if (error) *error = "Invalid depth image.";
        return false;
    }

    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 2 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        for (int x = 0; x < width; ++x) {
            // doc.depth is bottom-up; PNG rows are top-down.
            const int sourceY = height - 1 - y;
            float value = depth[static_cast<size_t>(sourceY) * width + x];
            if (invert) value = 1.0f - value;
            value = std::clamp(value, 0.0f, 1.0f);
            const uint16_t gray = static_cast<uint16_t>(std::lround(value * 65535.0f));
            raw.push_back(static_cast<uint8_t>(gray >> 8));
            raw.push_back(static_cast<uint8_t>(gray));
        }
    }

    const std::vector<uint8_t> compressed = deflateStored(raw);
    std::vector<uint8_t> png = {0x89, 0x50, 0x4e, 0x47,
                                0x0d, 0x0a, 0x1a, 0x0a};
    std::vector<uint8_t> header;
    appendU32(header, static_cast<uint32_t>(width));
    appendU32(header, static_cast<uint32_t>(height));
    header.push_back(16);
    header.push_back(0);
    header.push_back(0);
    header.push_back(0);
    header.push_back(0);
    appendChunk(png, "IHDR", header);
    appendChunk(png, "IDAT", compressed);
    appendChunk(png, "IEND", {});
    return writeAtomic(path, png, error);
}

} // namespace io