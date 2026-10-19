#pragma once

#include <imgui.h>

#include <string>

#include "core/types.h"

namespace app {
class App;
}

namespace ui {

// Shared layout metrics. Values are in unscaled pixels; multiply them with
// scaled() before use so the whole interface follows the display DPI.
inline constexpr float kTopBarHeight = 58.0f;
inline constexpr float kStatusHeight = 36.0f;
inline constexpr float kSidebarWidth = 258.0f;
inline constexpr float kRightWidth = 404.0f;
inline constexpr float kLabelWidth = 160.0f;
inline constexpr float kCollapsedSidebarWidth = 38.0f;
inline constexpr float kCanvasBarHeight = 44.0f;
inline constexpr float kCanvasBottomGap = 6.0f;
inline constexpr float kTaskRowHeight = 88.0f;
inline constexpr float kTaskThumbSize = 64.0f;
inline constexpr float kTaskPadding = 12.0f;
inline constexpr float kTaskCloseWidth = 30.0f;
inline constexpr float kTaskCloseGap = 6.0f;
inline constexpr float kSettingsLabelWidth = 150.0f;

extern float uiScale;

float scaled(float value);

// Converts a #rrggbb string to an opaque ImGui color. Invalid input falls
// back to the theme text color.
ImVec4 hexToVec4(const std::string& hex);

std::string backendLabel(app::App& app, core::Backend backend);

void drawTopBar(app::App& app);
void drawSidebar(app::App& app);
void drawCanvas(app::App& app);
void drawRightPanel(app::App& app);
void drawStatusBar(app::App& app);
void drawSettings(app::App& app);
void drawExportMenu(app::App& app);

} // namespace ui
