#include "ui/ui_internal.h"

#include <imgui.h>

#include <string>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

void drawStatusBar(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel status("##statusbar", ImVec2(0.0f, scaled(kStatusHeight)), 0.0f, false,
                     ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse);
        if (status) {
            const float y = (scaled(kStatusHeight) - ImGui::GetTextLineHeight()) * 0.5f;
            ImGui::SetCursorPos(ImVec2(20.0f, y));
            ImGui::TextDisabled("%s", app.statusText().c_str());

            const std::string right =
                "OpenGL 4.5  ·  " + backendLabel(app, app.config.backend);
            const float rightWidth = ImGui::CalcTextSize(right.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - rightWidth - 20.0f);
            ImGui::TextDisabled("%s", right.c_str());
        }
    }
    ImGui::PopStyleColor();
}

} // namespace ui
