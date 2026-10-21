#include "ui/theme.h"

#include <android/asset_manager.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace imrelighter::ui {
namespace {

constexpr const char* kLogTag = "ImRelighterAndroid";

const Palette kDarkPalette = {
    .bg = ImVec4(0.067f, 0.075f, 0.086f, 1.0f),
    .canvas = ImVec4(0.043f, 0.051f, 0.059f, 1.0f),
    .panel = ImVec4(0.094f, 0.106f, 0.122f, 1.0f),
    .panel2 = ImVec4(0.125f, 0.141f, 0.165f, 1.0f),
    .border = ImVec4(0.165f, 0.188f, 0.220f, 1.0f),
    .text = ImVec4(0.906f, 0.914f, 0.925f, 1.0f),
    .muted = ImVec4(0.541f, 0.569f, 0.612f, 1.0f),
    .accent = ImVec4(0.910f, 0.702f, 0.416f, 1.0f),
    .accent_cool = ImVec4(0.416f, 0.663f, 0.910f, 1.0f),
    .ok = ImVec4(0.498f, 0.753f, 0.541f, 1.0f),
    .danger = ImVec4(0.878f, 0.416f, 0.416f, 1.0f),
    .ink = ImVec4(0.070f, 0.070f, 0.070f, 1.0f),
};

const Palette kLightPalette = {
    .bg = ImVec4(0.949f, 0.953f, 0.961f, 1.0f),
    .canvas = ImVec4(0.898f, 0.906f, 0.918f, 1.0f),
    .panel = ImVec4(1.000f, 1.000f, 1.000f, 1.0f),
    .panel2 = ImVec4(0.965f, 0.969f, 0.976f, 1.0f),
    .border = ImVec4(0.827f, 0.843f, 0.867f, 1.0f),
    .text = ImVec4(0.118f, 0.133f, 0.153f, 1.0f),
    .muted = ImVec4(0.420f, 0.447f, 0.490f, 1.0f),
    .accent = ImVec4(0.690f, 0.435f, 0.176f, 1.0f),
    .accent_cool = ImVec4(0.184f, 0.435f, 0.682f, 1.0f),
    .ok = ImVec4(0.176f, 0.541f, 0.294f, 1.0f),
    .danger = ImVec4(0.784f, 0.278f, 0.278f, 1.0f),
    .ink = ImVec4(1.000f, 1.000f, 1.000f, 1.0f),
};

Palette g_palette = kDarkPalette;
ThemeMode g_theme_mode = ThemeMode::Dark;
Language g_language = Language::Chinese;
float g_density = 1.0f;

// stb_truetype (used internally by Dear ImGui) only supports TrueType
// outlines. Some Android vendors ship their CJK collection as CFF (OTTO),
// which is valid OpenType but cannot be parsed by stb_truetype and would
// trigger an ImGui assertion. Inspect the SFNT table directory ourselves so
// that only TrueType-outline fonts reach ImGui.
uint32_t readU32(const std::vector<unsigned char>& data, size_t offset) {
    if (offset + 4 > data.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

bool hasTable(const std::vector<unsigned char>& data, size_t font_offset,
              uint32_t wanted_tag) {
    if (font_offset + 12 > data.size()) {
        return false;
    }
    const uint16_t table_count =
        static_cast<uint16_t>((data[font_offset + 4] << 8) | data[font_offset + 5]);
    const size_t records = font_offset + 12;
    if (records + static_cast<size_t>(table_count) * 16 > data.size()) {
        return false;
    }
    for (uint16_t i = 0; i < table_count; ++i) {
        if (readU32(data, records + static_cast<size_t>(i) * 16) == wanted_tag) {
            return true;
        }
    }
    return false;
}

bool isTrueTypeOutlineFont(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const std::streamoff length = file.tellg();
    if (length < 12) {
        return false;
    }
    const size_t header_size = std::min<size_t>(static_cast<size_t>(length), 64 * 1024);
    std::vector<unsigned char> header(header_size);
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(header.data()),
                   static_cast<std::streamsize>(header.size()))) {
        return false;
    }

    const uint32_t sfnt_tag = readU32(header, 0);
    size_t font_offset = 0;
    if (sfnt_tag == 0x74746366u) {  // 'ttcf'
        const uint32_t font_count = readU32(header, 8);
        if (font_count == 0) {
            return false;
        }
        font_offset = readU32(header, 12);
    } else {
        font_offset = 0;
    }

    if (font_offset + 12 > header.size()) {
        return false;
    }
    const uint32_t outline_tag = readU32(header, font_offset);
    if (outline_tag != 0x00010000u && outline_tag != 0x74727565u) {
        return false;  // OTTO/CFF or unsupported outline format
    }
    constexpr uint32_t kCmapTag = 0x636D6170u;  // 'cmap'
    constexpr uint32_t kGlyfTag = 0x676C7966u;  // 'glyf'
    return hasTable(header, font_offset, kCmapTag) &&
           hasTable(header, font_offset, kGlyfTag);
}

}  // namespace

const Palette& palette() {
    return g_palette;
}

ThemeMode themeMode() {
    return g_theme_mode;
}

void setThemeMode(ThemeMode mode) {
    if (g_theme_mode == mode) {
        return;
    }
    g_theme_mode = mode;
    g_palette = mode == ThemeMode::Light ? kLightPalette : kDarkPalette;
    applyTheme(g_density);
}

Language language() {
    return g_language;
}

void setLanguage(Language language) {
    g_language = language;
}

const char* tr(const char* chinese, const char* english) {
    return g_language == Language::English ? english : chinese;
}

void applyTheme(float density) {
    ImGuiStyle& style = ImGui::GetStyle();
    const float scale = density > 0.0f ? density : 1.0f;
    g_density = scale;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f * scale;
    style.FrameRounding = 8.0f * scale;
    style.PopupRounding = 8.0f * scale;
    style.ScrollbarRounding = 4.0f * scale;
    style.GrabRounding = 4.0f * scale;
    style.TabRounding = 7.0f * scale;
    style.WindowBorderSize = 1.0f * scale;
    style.ChildBorderSize = 1.0f * scale;
    style.FrameBorderSize = 1.0f * scale;
    style.PopupBorderSize = 1.0f * scale;
    style.WindowPadding = ImVec2(16.0f * scale, 16.0f * scale);
    style.FramePadding = ImVec2(10.0f * scale, 6.0f * scale);
    style.ItemSpacing = ImVec2(10.0f * scale, 10.0f * scale);
    style.ItemInnerSpacing = ImVec2(8.0f * scale, 6.0f * scale);
    style.ScrollbarSize = 6.0f * scale;
    style.GrabMinSize = 18.0f * scale;
    style.DisabledAlpha = 0.48f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = g_palette.text;
    colors[ImGuiCol_TextDisabled] = g_palette.muted;
    colors[ImGuiCol_WindowBg] = g_palette.panel;
    colors[ImGuiCol_ChildBg] = g_palette.panel;
    colors[ImGuiCol_PopupBg] = g_palette.panel2;
    colors[ImGuiCol_Border] = g_palette.border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = g_palette.panel2;
    colors[ImGuiCol_FrameBgHovered] = g_palette.border;
    colors[ImGuiCol_FrameBgActive] = g_palette.panel2;
    colors[ImGuiCol_TitleBg] = g_palette.bg;
    colors[ImGuiCol_TitleBgActive] = g_palette.bg;
    colors[ImGuiCol_TitleBgCollapsed] = g_palette.bg;
    colors[ImGuiCol_MenuBarBg] = g_palette.panel;
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] =
        ImVec4(g_palette.border.x, g_palette.border.y, g_palette.border.z, 0.55f);
    colors[ImGuiCol_ScrollbarGrabHovered] =
        ImVec4(g_palette.border.x, g_palette.border.y, g_palette.border.z, 0.85f);
    colors[ImGuiCol_ScrollbarGrabActive] = g_palette.accent;
    colors[ImGuiCol_CheckMark] = g_palette.accent;
    colors[ImGuiCol_SliderGrab] = g_palette.accent;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.80f, 0.52f, 1.0f);
    colors[ImGuiCol_Button] = g_palette.panel2;
    colors[ImGuiCol_ButtonHovered] = g_palette.border;
    colors[ImGuiCol_ButtonActive] = g_palette.accent;
    colors[ImGuiCol_Header] = g_palette.panel2;
    colors[ImGuiCol_HeaderHovered] = g_palette.border;
    colors[ImGuiCol_HeaderActive] = g_palette.accent;
    colors[ImGuiCol_Separator] = g_palette.border;
    colors[ImGuiCol_SeparatorHovered] = g_palette.accent;
    colors[ImGuiCol_SeparatorActive] = g_palette.accent;
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_Tab] = g_palette.panel;
    colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);
    colors[ImGuiCol_TabSelected] = g_palette.panel2;
    colors[ImGuiCol_TabSelectedOverline] = g_palette.accent;
    colors[ImGuiCol_TabDimmed] = g_palette.bg;
    colors[ImGuiCol_TabDimmedSelected] = g_palette.panel;
    colors[ImGuiCol_PlotLines] = g_palette.accent_cool;
    colors[ImGuiCol_PlotHistogram] = g_palette.accent;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.910f, 0.702f, 0.416f, 0.25f);
    colors[ImGuiCol_NavCursor] = g_palette.accent;
}

bool loadFont(ImGuiIO& io, AAssetManager* assets, float density, std::string& error) {
    const float scale = density > 0.0f ? density : 1.0f;
    const float font_size = 15.0f * scale;

    // Bundle the same Source Han Sans face used by the desktop build. This
    // avoids ROM-specific font availability and keeps the UI consistent.
    if (assets != nullptr) {
        AAsset* asset = AAssetManager_open(assets, "fonts/SourceHanSansCN-Normal.otf",
                                           AASSET_MODE_BUFFER);
        if (asset != nullptr) {
            const off_t length = AAsset_getLength(asset);
            static std::vector<unsigned char> bundled_font;
            bundled_font.resize(static_cast<size_t>(length));
            const int read = AAsset_read(asset, bundled_font.data(), bundled_font.size());
            AAsset_close(asset);
            if (length > 0 && read == static_cast<int>(bundled_font.size())) {
                ImFontConfig config;
                config.FontNo = 0;
                config.OversampleH = 2;
                config.OversampleV = 2;
                config.FontDataOwnedByAtlas = false;
                ImFont* font = io.Fonts->AddFontFromMemoryTTF(
                    bundled_font.data(), static_cast<int>(bundled_font.size()), font_size,
                    &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
                if (font != nullptr) {
                    io.FontDefault = font;
                    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                                        "loaded bundled font: SourceHanSansCN-Normal.otf");
                    return true;
                }
                __android_log_print(ANDROID_LOG_WARN, kLogTag,
                                    "bundled CJK font could not be parsed");
            } else {
                __android_log_print(ANDROID_LOG_WARN, kLogTag,
                                    "failed to read bundled CJK font");
            }
        }
    }

    // Prefer TrueType system CJK fonts. NotoSansCJK on many vendor builds is a
    // CFF collection (OTTO) and must be skipped: stb_truetype would assert in
    // ImGui during font initialization.
    const std::array<const char*, 6> candidates = {
        "/system/fonts/DroidSansFallbackBBK.ttf",
        "/system/fonts/DroidSansFallbackMonster.ttf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansCJKsc-Regular.otf",
    };

    for (const char* path : candidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            continue;
        }
        if (!isTrueTypeOutlineFont(path)) {
            __android_log_print(ANDROID_LOG_WARN, kLogTag,
                                "skipping unusable CJK font: %s", path);
            continue;
        }
        ImFontConfig config;
        config.FontNo = 0;
        config.OversampleH = 2;
        config.OversampleV = 2;
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            path, font_size, &config,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (font != nullptr) {
            io.FontDefault = font;
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "loaded font: %s", path);
            return true;
        }
    }

    io.Fonts->AddFontDefault();
    error = "No CJK system font found; using the ImGui default font.";
    __android_log_print(ANDROID_LOG_WARN, kLogTag, "%s", error.c_str());
    return false;
}

}  // namespace imrelighter::ui
