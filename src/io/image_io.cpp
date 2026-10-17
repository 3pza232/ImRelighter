#include "io/image_io.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

#include "io/paths.h"
#include "io/webp_decoder.h"
#include "stb_image.h"

namespace io {

namespace {
constexpr int kMaxPixels = 24'000'000;
}

std::optional<ImageData> loadImage(const std::filesystem::path& path,
                                   std::string* error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        if (error) *error = "Cannot open image file: " + path.string();
        return std::nullopt;
    }

    const std::streamsize size = stream.tellg();
    if (size <= 0) {
        if (error) *error = "Image file is empty: " + path.string();
        return std::nullopt;
    }
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) {
        if (error) *error = "Failed to read image file: " + path.string();
        return std::nullopt;
    }

    std::string extension = paths::toUtf8(path.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    ImageData result;
    if (extension == ".webp") {
        if (!decodeWebp(bytes.data(), bytes.size(), result.width, result.height,
                        result.rgba, error)) {
            return std::nullopt;
        }
        return result;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                            &width, &height, &channels, 4);
    if (!pixels) {
        if (error) {
            const char* reason = stbi_failure_reason();
            *error = std::string("Unsupported or corrupt image") +
                     (reason ? std::string(": ") + reason : std::string());
        }
        return std::nullopt;
    }

    if (width <= 0 || height <= 0 || width * height > kMaxPixels) {
        stbi_image_free(pixels);
        if (error) *error = "Image is larger than the 24 megapixel limit.";
        return std::nullopt;
    }

    result.width = width;
    result.height = height;
    result.rgba.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
    stbi_image_free(pixels);
    return result;
}

bool makeThumbnail(const ImageData& image, int maxSize, std::vector<uint8_t>& out,
                   int& thumbWidth, int& thumbHeight) {
    if (image.width <= 0 || image.height <= 0 ||
        image.rgba.size() != static_cast<size_t>(image.width) * image.height * 4) {
        return false;
    }

    const float scale = std::min(
        1.0f, static_cast<float>(maxSize) / static_cast<float>(std::max(image.width, image.height)));
    thumbWidth = std::max(1, static_cast<int>(std::lround(image.width * scale)));
    thumbHeight = std::max(1, static_cast<int>(std::lround(image.height * scale)));
    out.assign(static_cast<size_t>(thumbWidth) * thumbHeight * 4, 0);

    for (int y = 0; y < thumbHeight; ++y) {
        const float srcY = (static_cast<float>(y) + 0.5f) * image.height / thumbHeight - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(srcY)), 0, image.height - 1);
        const int y1 = std::min(y0 + 1, image.height - 1);
        const float fy = srcY - std::floor(srcY);

        for (int x = 0; x < thumbWidth; ++x) {
            const float srcX = (static_cast<float>(x) + 0.5f) * image.width / thumbWidth - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(srcX)), 0, image.width - 1);
            const int x1 = std::min(x0 + 1, image.width - 1);
            const float fx = srcX - std::floor(srcX);

            for (int c = 0; c < 4; ++c) {
                const auto sample = [&](int sx, int sy) {
                    return static_cast<float>(image.rgba[(static_cast<size_t>(sy) * image.width + sx) * 4 + c]);
                };
                const float top = sample(x0, y0) * (1.0f - fx) + sample(x1, y0) * fx;
                const float bottom = sample(x0, y1) * (1.0f - fx) + sample(x1, y1) * fx;
                out[(static_cast<size_t>(y) * thumbWidth + x) * 4 + c] =
                    static_cast<uint8_t>(std::lround(top * (1.0f - fy) + bottom * fy));
            }
        }
    }
    return true;
}

} // namespace io
