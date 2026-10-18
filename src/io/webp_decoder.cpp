#include "io/webp_decoder.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

#include "io/image_limits.h"
#include "io/paths.h"

namespace io {
namespace {

using WebPGetInfoFn = int (*)(const uint8_t*, size_t, int*, int*);
using WebPDecodeRGBAFn = uint8_t* (*)(const uint8_t*, size_t, int*, int*);
using WebPFreeFn = void (*)(void*);

struct WebPApi {
    HMODULE module = nullptr;
    WebPGetInfoFn getInfo = nullptr;
    WebPDecodeRGBAFn decodeRgba = nullptr;
    WebPFreeFn free = nullptr;

    bool ready() const { return getInfo && decodeRgba && free; }
};

// GetProcAddress returns FARPROC, which cannot be cast to a function pointer
// with a different signature in portable C++. Copying the bits expresses the
// same conversion without -Wcast-function-type diagnostics.
template <typename Function>
Function loadFunction(HMODULE module, const char* name) {
    static_assert(sizeof(Function) == sizeof(FARPROC));
    const FARPROC address = GetProcAddress(module, name);
    Function function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

WebPApi& api() {
    static WebPApi value = [] {
        WebPApi result;
        const auto besideExe = paths::exeDir() / L"libwebp.dll";
        result.module = LoadLibraryExW(
            besideExe.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!result.module) result.module = LoadLibraryW(L"libwebp.dll");
        if (!result.module) return result;
        result.getInfo = loadFunction<WebPGetInfoFn>(result.module, "WebPGetInfo");
        result.decodeRgba =
            loadFunction<WebPDecodeRGBAFn>(result.module, "WebPDecodeRGBA");
        result.free = loadFunction<WebPFreeFn>(result.module, "WebPFree");
        return result;
    }();
    return value;
}

} // namespace

bool decodeWebp(const uint8_t* bytes, size_t size, int& width, int& height,
                std::vector<uint8_t>& rgba, std::string* error) {
    const WebPApi& webp = api();
    if (!webp.ready()) {
        if (error) *error = "libwebp.dll is missing or incompatible.";
        return false;
    }
    if (!bytes || size == 0) {
        if (error) *error = "WebP file is empty.";
        return false;
    }

    int headerWidth = 0;
    int headerHeight = 0;
    if (!webp.getInfo(bytes, size, &headerWidth, &headerHeight) ||
        headerWidth <= 0 || headerHeight <= 0) {
        if (error) *error = "Invalid WebP image.";
        return false;
    }
    if (exceedsPixelLimit(headerWidth, headerHeight)) {
        if (error) *error = "Image is larger than the 24 megapixel limit.";
        return false;
    }

    int decodedWidth = 0;
    int decodedHeight = 0;
    uint8_t* pixels =
        webp.decodeRgba(bytes, size, &decodedWidth, &decodedHeight);
    if (!pixels || decodedWidth != headerWidth || decodedHeight != headerHeight) {
        if (pixels) webp.free(pixels);
        if (error) *error = "WebP decoding failed.";
        return false;
    }

    width = decodedWidth;
    height = decodedHeight;
    rgba.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
    webp.free(pixels);
    return true;
}

} // namespace io
