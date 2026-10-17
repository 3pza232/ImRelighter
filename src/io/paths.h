#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace paths {

std::filesystem::path exeDir();
std::filesystem::path assetDir();
std::filesystem::path assetPath(std::string_view relative);

std::filesystem::path modelDir();
std::filesystem::path modelFile();
std::filesystem::path modelManifest();

std::filesystem::path configFile();

std::string toUtf8(const std::filesystem::path& path);
std::filesystem::path fromUtf8(std::string_view text);
std::string fileNameUtf8(const std::filesystem::path& path);
bool fileExists(const std::filesystem::path& path);
std::uintmax_t fileSize(const std::filesystem::path& path);

} // namespace paths
