#include "io/image_loader.h"

#include <android/log.h>

#include <limits>

#include "stb_image.h"

namespace imrelighter::io {

namespace {

constexpr const char* kTag = "ImRelighter";
constexpr int64_t kMaxPixels = 12LL * 1024 * 1024;

}  // namespace

bool readAsset(AAssetManager* assets, const std::string& path,
               std::vector<uint8_t>& bytes, std::string& error) {
    if (assets == nullptr || path.empty()) {
        error = "invalid asset request";
        return false;
    }
    AAsset* asset = AAssetManager_open(assets, path.c_str(), AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        error = "asset not found: " + path;
        return false;
    }
    const off_t length = AAsset_getLength(asset);
    if (length <= 0) {
        AAsset_close(asset);
        error = "asset is empty: " + path;
        return false;
    }
    bytes.resize(static_cast<size_t>(length));
    const int read = AAsset_read(asset, bytes.data(), bytes.size());
    AAsset_close(asset);
    if (read != static_cast<int>(bytes.size())) {
        error = "short asset read: " + path;
        return false;
    }
    return true;
}

bool decodeImage(const uint8_t* bytes, size_t size, ImageData& image,
                 std::string& error) {
    if (bytes == nullptr || size == 0) {
        error = "empty image";
        return false;
    }
    if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
        error = "image file is too large";
        return false;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        bytes, static_cast<int>(size), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        error = std::string("image decode failed: ") + stbi_failure_reason();
        return false;
    }
    if (width <= 0 || height <= 0 ||
        static_cast<int64_t>(width) * height > kMaxPixels) {
        stbi_image_free(pixels);
        error = "image is too large (limit 12 MP)";
        return false;
    }
    image.width = width;
    image.height = height;
    image.rgba.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
    stbi_image_free(pixels);
    __android_log_print(ANDROID_LOG_INFO, kTag, "image decoded: %dx%d", width,
                        height);
    return true;
}

bool loadAssetImage(AAssetManager* assets, const std::string& path,
                    ImageData& image, std::string& error) {
    std::vector<uint8_t> bytes;
    if (!readAsset(assets, path, bytes, error)) {
        return false;
    }
    return decodeImage(bytes.data(), bytes.size(), image, error);
}

}  // namespace imrelighter::io
