#include "app/export_service.h"

#include <chrono>
#include <utility>

#include "io/png_export.h"

namespace app {

void ExportService::startDepth(const std::filesystem::path& path,
                               std::vector<float> depth, int width, int height,
                               bool invert) {
    future_ = std::async(
        std::launch::async,
        [path, depth = std::move(depth), width, height, invert]() {
            std::string error;
            const bool ok =
                io::writeDepth16Png(path, depth, width, height, invert, &error);
            return ok ? std::string() : error;
        });
}

void ExportService::startRgba(const std::filesystem::path& path,
                              std::vector<std::uint8_t> rgba, int width,
                              int height) {
    future_ = std::async(
        std::launch::async, [path, rgba = std::move(rgba), width, height]() {
            std::string error;
            const bool ok =
                io::writeRgba8Png(path, rgba, width, height, &error);
            return ok ? std::string() : error;
        });
}

ExportService::Result ExportService::poll() {
    Result result;
    if (!future_.valid() ||
        future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return result;
    }
    result.done = true;
    result.error = future_.get();
    return result;
}

void ExportService::wait() {
    if (future_.valid()) future_.wait();
}

} // namespace app
