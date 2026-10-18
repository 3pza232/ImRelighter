#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace native_dialog {

std::vector<std::filesystem::path> openImagesDialog(
    void* owner, const wchar_t* title = L"Open Image");

std::optional<std::filesystem::path> savePngDialog(
    void* owner, const wchar_t* title, const wchar_t* defaultName);

std::optional<std::filesystem::path> pickOnnxFile(
    void* owner, const wchar_t* title = L"Select ONNX Model");

void openInExplorer(const std::filesystem::path& path);

} // namespace native_dialog
