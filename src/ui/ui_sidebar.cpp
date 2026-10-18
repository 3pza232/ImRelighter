#include "ui/ui_internal.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "app/app.h"
#include "gpu/texture.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

void drawTaskCard(app::App& app, size_t index, uint64_t& activateId,
                  uint64_t& closeId) {
    const core::Document* doc = app.document(index);
    if (!doc) return;

    const bool active = app.activeIndex() == static_cast<int>(index);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float rowWidth = ImGui::GetContentRegionAvail().x;
    const float barWidth = std::max(
        0.0f, rowWidth - scaled(kTaskCloseWidth) - scaled(kTaskCloseGap));

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const bool hovered = ImGui::IsMouseHoveringRect(
        rowStart, ImVec2(rowStart.x + barWidth, rowStart.y + scaled(kTaskRowHeight)));
    if (active || hovered) {
        draw->AddRectFilled(rowStart,
                            ImVec2(rowStart.x + barWidth, rowStart.y + scaled(kTaskRowHeight)),
                            theme::u32(active ? theme::panel2 : theme::taskHover),
                            7.0f);
    }
    if (active) {
        draw->AddRectFilled(rowStart,
                            ImVec2(rowStart.x + 2.0f, rowStart.y + scaled(kTaskRowHeight)),
                            theme::u32(theme::accent), 7.0f);
    }

    const float padding = scaled(kTaskPadding);
    const float thumbSize = scaled(kTaskThumbSize);
    // Keep the text column clear of the thumbnail at every DPI scale. These
    // offsets used to be raw pixels while the thumbnail followed scaled().
    const float textX = rowStart.x + padding + thumbSize + scaled(10.0f);
    const float top = rowStart.y + (scaled(kTaskRowHeight) - thumbSize) * 0.5f;

    ImGui::PushID(static_cast<int>(doc->id));
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x + padding, top));
    ImGui::InvisibleButton("##thumbnail", ImVec2(thumbSize, thumbSize));
    if (ImGui::IsItemClicked()) activateId = doc->id;

    const ImVec2 thumbPos = ImVec2(rowStart.x + padding, top);
    draw->AddRectFilled(thumbPos,
                        ImVec2(thumbPos.x + thumbSize, thumbPos.y + thumbSize),
                        theme::u32(theme::canvas), 4.0f);
    if (gpu::Texture* texture = app.thumbnail(doc->id);
        texture && texture->id() != 0 && texture->width() > 0 &&
        texture->height() > 0) {
        const float scale = std::min(
            thumbSize / static_cast<float>(texture->width()),
            thumbSize / static_cast<float>(texture->height()));
        const ImVec2 drawSize(texture->width() * scale, texture->height() * scale);
        const ImVec2 offset((thumbSize - drawSize.x) * 0.5f,
                            (thumbSize - drawSize.y) * 0.5f);
        draw->AddImage(static_cast<ImTextureID>(texture->id()),
                       ImVec2(thumbPos.x + offset.x, thumbPos.y + offset.y),
                       ImVec2(thumbPos.x + offset.x + drawSize.x,
                              thumbPos.y + offset.y + drawSize.y));
    }
    draw->AddRect(thumbPos,
                  ImVec2(thumbPos.x + thumbSize, thumbPos.y + thumbSize),
                  ImGui::ColorConvertFloat4ToU32(theme::border), 4.0f);

    ImGui::PushClipRect(
        ImVec2(textX - scaled(2.0f), rowStart.y),
        ImVec2(rowStart.x + barWidth - scaled(4.0f),
               rowStart.y + scaled(kTaskRowHeight)),
        true);
    ImGui::SetCursorScreenPos(
        ImVec2(textX, rowStart.y + scaled(20.0f)));
    ImGui::BeginGroup();
    if (active) {
        ImGui::TextColored(theme::accent, "%s", doc->name.c_str());
    } else {
        ImGui::TextUnformatted(doc->name.c_str());
    }
    ImGui::TextDisabled("%d × %d", doc->width, doc->height);
    ImGui::EndGroup();
    ImGui::PopClipRect();

    const float closeSize = scaled(28.0f);
    ImGui::SetCursorScreenPos(
        ImVec2(rowStart.x + barWidth + scaled(kTaskCloseGap),
               rowStart.y + (scaled(kTaskRowHeight) - closeSize) * 0.5f));
    if (closeButton("##remove", ImVec2(closeSize, closeSize))) {
        closeId = doc->id;
    }

    ImGui::SetCursorScreenPos(
        ImVec2(rowStart.x, rowStart.y + scaled(kTaskRowHeight) + 2.0f));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopID();
}
void drawSidebar(app::App& app) {
    const bool open = app.ui.sidebarOpen;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel sidebar("##sidebar",
                      ImVec2(open ? scaled(kSidebarWidth) : scaled(kCollapsedSidebarWidth), 0),
                      0.0f, true, ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse);

        if (!open) {
            if (sidebar) {
                ImGui::SetCursorPos(ImVec2(8.0f, 12.0f));
                if (iconButton("##expand", "»", ImVec2(22.0f, 26.0f))) {
                    app.ui.sidebarOpen = true;
                    if (core::Document* doc = app.activeDocument()) {
                        doc->fitPending = true;
                    }
                }
                ImGui::SetCursorPos(ImVec2(8.0f, 46.0f));
                if (iconButton("##open", "+", ImVec2(22.0f, 26.0f))) {
                    app.requestOpenImage();
                }
                const std::string countText =
                    std::to_string(static_cast<int>(app.documentCount()));
                const float countWidth = ImGui::CalcTextSize(countText.c_str()).x;
                ImGui::SetCursorPos(
                    ImVec2(8.0f + (22.0f - countWidth) * 0.5f, 84.0f));
                ImGui::TextDisabled("%s", countText.c_str());
            }
            ImGui::PopStyleColor();
            return;
        }

        if (sidebar) {
            const float lineHeight = ImGui::GetTextLineHeight();
            ImGui::SetCursorPos(ImVec2(16.0f, 14.0f));
            ImGui::TextUnformatted(app.tr("sidebar.title").c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%d", static_cast<int>(app.documentCount()));
            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 32.0f, 14.0f));
            if (iconButton("##collapse", "«", ImVec2(22.0f, 24.0f))) {
                app.ui.sidebarOpen = false;
                if (core::Document* doc = app.activeDocument()) {
                    doc->fitPending = true;
                }
            }

            ImGui::SetCursorPos(ImVec2(0.0f, 14.0f + lineHeight + 12.0f));
            ImGui::Separator();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);

            const float footer = ImGui::GetFrameHeight() + 28.0f;
            {
                Panel tasklist("##tasklist", ImVec2(0.0f, -footer), 12.0f);
                if (tasklist) {
                    if (app.documentCount() == 0) {
                        const float available = ImGui::GetContentRegionAvail().y;
                        ImGui::SetCursorPosY(
                            ImGui::GetCursorPosY() +
                            std::max(0.0f, available * 0.30f));
                        centeredText(app.tr("sidebar.empty").c_str());
                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Text, theme::muted);
                        centeredText(app.tr("canvas.empty.hint").c_str());
                        ImGui::PopStyleColor();
                    } else {
                        uint64_t activateId = 0;
                        uint64_t closeId = 0;
                        for (size_t i = 0; i < app.documentCount(); ++i) {
                            drawTaskCard(app, i, activateId, closeId);
                        }
                        if (closeId != 0) {
                            app.closeDocument(closeId);
                        } else if (activateId != 0) {
                            app.activateDocument(activateId);
                        }
                    }
                }
            }

            ImGui::Separator();
            ImGui::SetCursorPosX(16.0f);
            if (ImGui::Button(app.tr("sidebar.open").c_str(),
                              ImVec2(ImGui::GetContentRegionAvail().x - 16.0f, 0.0f))) {
                app.requestOpenImage();
            }
        }
    }
    ImGui::PopStyleColor();
}

} // namespace ui
