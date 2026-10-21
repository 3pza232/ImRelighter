#include "io/paths.h"

#include <windows.h>

#include <system_error>
#include <vector>

namespace paths {

namespace {

std::filesystem::path g_exeDir;

} // namespace

std::filesystem::path exeDir() {
    if (!g_exeDir.empty()) return g_exeDir;

    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            g_exeDir = std::filesystem::current_path();
            return g_exeDir;
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            g_exeDir = std::filesystem::path(std::wstring(buffer.data(), buffer.size()))
                           .parent_path();
            return g_exeDir;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path assetDir() {
    const auto besideExe = exeDir() / L"assets";
    std::error_code ec;
    if (std::filesystem::exists(besideExe, ec)) return besideExe;

    // Developer convenience when running from the repository root.
    const auto fromCwd = std::filesystem::current_path() / L"assets";
    if (std::filesystem::exists(fromCwd, ec)) return fromCwd;
    return besideExe;
}

std::filesystem::path assetPath(std::string_view relative) {
    return assetDir() / std::filesystem::path(std::string(relative));
}

std::filesystem::path modelDir() {
    return exeDir() / L"models" / L"depth-anything-v2-small" / L"q4";
}

std::filesystem::path modelFile() {
    return modelDir() / L"model_q4.onnx";
}

std::filesystem::path configFile() {
    return exeDir() / L"config.json";
}

std::string toUtf8(const std::filesystem::path& path) {
    const std::wstring wide = path.wstring();
    if (wide.empty()) return {};

    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                           static_cast<int>(wide.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};

    std::string result(static_cast<size_t>(needed), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                            static_cast<int>(wide.size()),
                                            result.data(), needed, nullptr, nullptr);
    if (written <= 0) return {};
    return result;
}

std::filesystem::path fromUtf8(std::string_view text) {
    if (text.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        wide.data(), needed);
    return std::filesystem::path(wide);
}

std::string fileNameUtf8(const std::filesystem::path& path) {
    return toUtf8(path.filename());
}

bool fileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

std::uintmax_t fileSize(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

} // namespace paths
