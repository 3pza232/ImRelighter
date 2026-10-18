#include "io/native_dialog.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <array>
#include <cwchar>

namespace native_dialog {

std::vector<std::filesystem::path> openImagesDialog(void* owner,
                                                    const wchar_t* title) {
    std::array<wchar_t, 32768> file{};
    const wchar_t filter[] =
        L"Images (*.png;*.jpg;*.jpeg;*.webp)\0*.png;*.jpg;*.jpeg;*.webp\0"
        L"PNG (*.png)\0*.png\0"
        L"JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
        L"WebP (*.webp)\0*.webp\0"
        L"All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(owner);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&dialog)) return {};

    std::vector<std::filesystem::path> result;
    const wchar_t* cursor = file.data();
    const std::wstring first(cursor);
    cursor += first.size() + 1;

    if (*cursor == L'\0') {
        result.emplace_back(first);
        return result;
    }

    const std::filesystem::path directory(first);
    while (*cursor != L'\0') {
        const std::wstring name(cursor);
        result.push_back(directory / name);
        cursor += name.size() + 1;
    }
    return result;
}

std::optional<std::filesystem::path> savePngDialog(
    void* owner, const wchar_t* title, const wchar_t* defaultName) {
    std::array<wchar_t, 32768> file{};
    if (defaultName) {
        wcsncpy_s(file.data(), file.size(), defaultName, _TRUNCATE);
    }

    const wchar_t filter[] = L"PNG (*.png)\0*.png\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(owner);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrTitle = title;
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT |
                   OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(file.data());
}
std::optional<std::filesystem::path> pickOnnxFile(void* owner,
                                                    const wchar_t* title) {
    std::array<wchar_t, 32768> file{};
    const wchar_t filter[] =
        L"ONNX model (*.onnx)\0*.onnx\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(owner);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrTitle = title;
    dialog.lpstrDefExt = L"onnx";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(file.data());
}

void openInExplorer(const std::filesystem::path& path) {
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
}

} // namespace native_dialog
