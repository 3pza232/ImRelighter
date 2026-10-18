#pragma once

#include "imgui.h"

namespace theme {

enum class Mode { Dark, Light };

// Palette tokens are refreshed by setMode(). UI code only reads these names,
// never raw color literals.
inline ImVec4 bg;
inline ImVec4 canvas;
inline ImVec4 panel;
inline ImVec4 panel2;
inline ImVec4 border;
inline ImVec4 text;
inline ImVec4 muted;
inline ImVec4 accent;
inline ImVec4 accentCool;
inline ImVec4 accentHover;
inline ImVec4 danger;
inline ImVec4 dangerHover;
inline ImVec4 ok;
inline ImVec4 ink;
inline ImVec4 closeText;
inline ImVec4 taskHover;
inline ImVec4 glowCore;
inline ImVec4 overlay;

inline ImVec4 fieldHover;
inline ImVec4 fieldActive;
inline ImVec4 buttonHover;
inline ImVec4 buttonActive;
inline ImVec4 header;
inline ImVec4 headerHover;
inline ImVec4 headerActive;
inline ImVec4 tabHover;
inline ImVec4 tabDimSelected;
inline ImVec4 sliderActive;
inline ImVec4 plotHover;
inline ImVec4 textSelected;
inline ImVec4 modalDim;
inline ImVec4 scrollbarBg;
inline ImVec4 scrollbarGrab;
inline ImVec4 scrollbarHover;
inline ImVec4 scrollbarActive;
inline ImVec4 axisX;
inline ImVec4 axisY;
inline ImVec4 axisZ;

inline ImU32 u32(const ImVec4& color, float alpha = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(color.x, color.y, color.z, color.w * alpha));
}

void setMode(Mode mode);
void setScale(float scale);
void apply();

} // namespace theme