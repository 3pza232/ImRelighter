#include "ui/ui_internal.h"

#include <imgui.h>

#include <string>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

void drawTopBar(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel topbar("##topbar", ImVec2(0.0f, scaled(kTopBarHeight)), 0.0f, false,
                     ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse);
        if (topbar) {
            const float lineHeight = ImGui::GetTextLineHeight();
            ImGui::SetCursorPos(ImVec2(20.0f, (scaled(kTopBarHeight) - lineHeight) * 0.5f));
            const ImVec2 brandPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(brandPos.x + 5.0f, brandPos.y + lineHeight * 0.5f), 5.0f,
                theme::u32(theme::accent));
            ImGui::SetCursorPosX(36.0f);
            ImGui::TextUnformatted("ImRelighter");

            const std::string settingsLabel = app.tr("topbar.settings");
            const float buttonWidth =
                ImGui::CalcTextSize(settingsLabel.c_str()).x +
                ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetCursorPos(
                ImVec2(ImGui::GetWindowWidth() - buttonWidth - 20.0f,
                       (scaled(kTopBarHeight) - ImGui::GetFrameHeight()) * 0.5f));
            if (ImGui::Button(settingsLabel.c_str())) app.ui.settingsOpen = true;
        }
    }
    ImGui::PopStyleColor();
}

} // namespace ui
