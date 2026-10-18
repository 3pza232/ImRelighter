#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace app {

// Tracks the single background PNG encode. Encoding is asynchronous while the
// resulting status message remains the caller's responsibility.
class ExportService {
public:
    struct Result {
        bool done = false;
        std::string error;
    };

    bool busy() const { return future_.valid(); }

    void startDepth(const std::filesystem::path& path, std::vector<float> depth,
                    int width, int height, bool invert);
    void startRgba(const std::filesystem::path& path, std::vector<std::uint8_t> rgba,
                   int width, int height);

    Result poll();
    void wait();

private:
    std::future<std::string> future_;
};

} // namespace app
