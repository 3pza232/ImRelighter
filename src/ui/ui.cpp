#include "ui/ui.h"

#include <imgui.h>

#include <algorithm>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/ui_internal.h"
#include "ui/widgets.h"

namespace ui {

float uiScale = 1.0f;

float scaled(float value) { return value * uiScale; }

namespace {

const ImGuiWindowFlags kRootFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

} // namespace

void setUiScale(float scale) {
    uiScale = std::clamp(scale, 1.0f, 3.0f);
    theme::setScale(uiScale);
}

void draw(app::App& app) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    // The settings dialog is modal: disable the workspace behind it so clicks
    // and keyboard focus cannot reach the canvas or side panels.
    ImGui::BeginDisabled(app.ui.settingsOpen);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##root", nullptr, kRootFlags);

    drawTopBar(app);

    const float mainHeight =
        std::max(0.0f, display.y - scaled(kTopBarHeight) - scaled(kStatusHeight));
    ImGui::SetCursorPos(ImVec2(0.0f, scaled(kTopBarHeight)));
    {
        Panel main("##main_area", ImVec2(0.0f, mainHeight), 0.0f, false,
                   ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_NoScrollWithMouse);
        if (main) {
            drawSidebar(app);
            ImGui::SameLine(0.0f, 0.0f);
            drawCanvas(app);
            ImGui::SameLine(0.0f, 0.0f);
            drawRightPanel(app);
        }
    }

    ImGui::SetCursorPos(ImVec2(0.0f, display.y - scaled(kStatusHeight)));
    drawStatusBar(app);
    ImGui::End();
    ImGui::PopStyleVar(2);

    drawExportMenu(app);
    ImGui::EndDisabled();

    drawSettings(app);
}

} // namespace ui
