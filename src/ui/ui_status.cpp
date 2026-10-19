#include "ui/ui_internal.h"

#include <imgui.h>

#include <algorithm>
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
            const float y =
                (scaled(kStatusHeight) - ImGui::GetTextLineHeight()) * 0.5f;
            const std::string right =
                backendLabel(app, app.config.backend);
            const float rightWidth = ImGui::CalcTextSize(right.c_str()).x;
            const float rightX =
                ImGui::GetWindowWidth() - rightWidth - scaled(20.0f);
            ImGui::SetCursorPos(ImVec2(rightX, y));
            ImGui::TextDisabled("%s", right.c_str());

            const ImVec2 origin = ImGui::GetWindowPos();
            const float leftX = scaled(20.0f);
            const float leftEnd = std::max(leftX, rightX - scaled(12.0f));
            ImGui::PushClipRect(
                ImVec2(origin.x + leftX, origin.y + y),
                ImVec2(origin.x + leftEnd,
                       origin.y + y + ImGui::GetTextLineHeight()),
                true);
            ImGui::SetCursorPos(ImVec2(leftX, y));
            ImGui::TextDisabled("%s", app.statusText().c_str());
            const bool statusHovered = ImGui::IsItemHovered();
            ImGui::PopClipRect();
            if (statusHovered &&
                ImGui::CalcTextSize(app.statusText().c_str()).x >
                    leftEnd - leftX) {
                ImGui::SetTooltip("%s", app.statusText().c_str());
            }
        }
    }
    ImGui::PopStyleColor();
}

} // namespace ui
