#pragma once

#include <android/asset_manager.h>

#include <string>

#include "imgui.h"

namespace imrelighter::ui {

struct Palette {
    ImVec4 bg;
    ImVec4 canvas;
    ImVec4 panel;
    ImVec4 panel2;
    ImVec4 border;
    ImVec4 text;
    ImVec4 muted;
    ImVec4 accent;
    ImVec4 accent_cool;
    ImVec4 ok;
    ImVec4 danger;
    ImVec4 ink;
};

enum class ThemeMode {
    Dark = 0,
    Light = 1,
};

enum class Language {
    Chinese = 0,
    English = 1,
};

const Palette& palette();
ThemeMode themeMode();
void setThemeMode(ThemeMode mode);
Language language();
void setLanguage(Language language);
const char* tr(const char* chinese, const char* english);

// Applies the shared desktop visual language, scaled for Android density.
void applyTheme(float density);

// Loads the bundled CJK font, falling back to a usable system font.
bool loadFont(ImGuiIO& io, AAssetManager* assets, float density, std::string& error);

}  // namespace imrelighter::ui
