#include "ui/ui_internal.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "app/app.h"
#include "core/document.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

namespace {

ImVec2 lightScreenPosition(const core::Document& doc, const core::Light& light,
                           const ImVec2& topLeft, float scale) {
    const float aspect = static_cast<float>(doc.width) / static_cast<float>(doc.height);
    const float u = light.position[0] / aspect * 0.5f + 0.5f;
    const float v = 0.5f - light.position[1] * 0.5f;
    return ImVec2(topLeft.x + u * doc.width * scale,
                  topLeft.y + v * doc.height * scale);
}

ImU32 lightColorU32(const std::string& hex, int alpha) {
    const ImVec4 color = hexToVec4(hex);
    return IM_COL32(static_cast<int>(color.x * 255.0f),
                    static_cast<int>(color.y * 255.0f),
                    static_cast<int>(color.z * 255.0f), alpha);
}

ImU32 lightLabelU32(const std::string& hex, int alpha) {
    // The marker fill is the light color, not the panel background. Choose a
    // neutral number color from its luminance so bright lights get dark digits
    // and dark lights get light digits in both themes.
    const ImVec4 color = hexToVec4(hex);
    const float luma = std::clamp(0.2126f * color.x + 0.7152f * color.y +
                                      0.0722f * color.z,
                                  0.0f, 1.0f);
    const int value =
        static_cast<int>(std::lround(255.0f - luma * 235.0f));
    return IM_COL32(value, value, value, alpha);
}

} // namespace

void drawEmptyCanvas(app::App& app) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const ImVec2 center(min.x + size.x * 0.5f, min.y + size.y * 0.47f);

    for (int i = 4; i >= 1; --i) {
        const float radius = 40.0f + static_cast<float>(i) * 24.0f;
        const int alpha = 5 + i * 3;
        draw->AddCircleFilled(center, radius,
                              theme::u32(theme::accent, alpha / 255.0f), 64);
    }
    draw->AddCircle(center, 31.0f, theme::u32(theme::accent, 160.0f / 255.0f), 64, 1.0f);
    draw->AddCircleFilled(center, 22.0f, theme::u32(theme::accent, 70.0f / 255.0f), 64);
    draw->AddCircleFilled(center, 9.0f, theme::u32(theme::glowCore, 230.0f / 255.0f), 32);

    const float textWidth = ImGui::CalcTextSize(app.tr("canvas.empty.title").c_str()).x;
    ImGui::SetCursorPos(ImVec2((size.x - textWidth) * 0.5f, size.y * 0.47f + 66.0f));
    ImGui::TextUnformatted(app.tr("canvas.empty.title").c_str());

    const float subtitleWidth = ImGui::CalcTextSize(app.tr("canvas.empty.subtitle").c_str()).x;
    ImGui::SetCursorPos(ImVec2((size.x - subtitleWidth) * 0.5f, size.y * 0.47f + 92.0f));
    ImGui::TextUnformatted(app.tr("canvas.empty.subtitle").c_str());

    const float hintWidth = ImGui::CalcTextSize(app.tr("canvas.empty.hint").c_str()).x;
    ImGui::SetCursorPos(ImVec2((size.x - hintWidth) * 0.5f, size.y * 0.47f + 116.0f));
    ImGui::TextDisabled("%s", app.tr("canvas.empty.hint").c_str());

    const std::string buttonLabel = app.tr("topbar.open");
    const float buttonWidth = ImGui::CalcTextSize(buttonLabel.c_str()).x +
                              ImGui::GetStyle().FramePadding.x * 2.0f + 24.0f;
    ImGui::SetCursorPos(ImVec2((size.x - buttonWidth) * 0.5f, size.y * 0.47f + 152.0f));
    if (centeredButton(buttonLabel.c_str(),
                       ImVec2(buttonWidth, scaled(32.0f)))) {
        app.requestOpenImage();
    }
}

void drawImageCanvas(app::App& app) {
    core::Document* doc = app.activeDocument();
    if (!doc) return;

    constexpr float kPadding = 24.0f;
    const ImVec2 areaMin = ImGui::GetCursorScreenPos();
    const ImVec2 areaSize = ImGui::GetContentRegionAvail();
    const float fitScale = std::max(
        0.001f, std::min((areaSize.x - kPadding * 2.0f) / static_cast<float>(doc->width),
                         (areaSize.y - kPadding * 2.0f) / static_cast<float>(doc->height)));

    if (doc->fitPending) {
        doc->zoom = 1.0f;
        doc->panX = 0.0f;
        doc->panY = 0.0f;
        doc->fitPending = false;
    }

    if (app.ui.focusLight != 0) {
        const auto focused = std::find_if(
            doc->lights.begin(), doc->lights.end(),
            [&](const core::Light& light) {
                return light.id == app.ui.focusLight;
            });
        if (focused != doc->lights.end()) {
            const float aspect =
                static_cast<float>(doc->width) / static_cast<float>(doc->height);
            const float u = focused->position[0] / aspect * 0.5f + 0.5f;
            const float v = 0.5f - focused->position[1] * 0.5f;
            const float focusScale = fitScale * 2.0f;
            doc->zoom = 2.0f;
            doc->panX = (0.5f - u) * doc->width * focusScale;
            doc->panY = (0.5f - v) * doc->height * focusScale;
            doc->fitPending = false;
        }
        app.ui.focusLight = 0;
    }

    const ImVec2 areaCenter(areaMin.x + areaSize.x * 0.5f,
                            areaMin.y + areaSize.y * 0.5f);

    auto imageLayout = [&](float& scale, ImVec2& topLeft) {
        scale = fitScale * doc->zoom;
        const ImVec2 size(doc->width * scale, doc->height * scale);
        topLeft = ImVec2(areaCenter.x + doc->panX - size.x * 0.5f,
                         areaCenter.y + doc->panY - size.y * 0.5f);
    };

    float scale = 1.0f;
    ImVec2 topLeft;
    imageLayout(scale, topLeft);

    ImGui::SetCursorScreenPos(areaMin);
    ImGui::InvisibleButton("##image_input", areaSize,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    if (hovered && !app.ui.settingsOpen && io.MouseWheel != 0.0f) {
        const ImVec2 cursor = io.MousePos;
        const ImVec2 imagePoint((cursor.x - topLeft.x) / scale,
                                (cursor.y - topLeft.y) / scale);
        const float newZoom = std::clamp(
            doc->zoom * std::pow(1.12f, io.MouseWheel), 0.05f, 8.0f);
        const float newScale = fitScale * newZoom;
        const ImVec2 newSize(doc->width * newScale, doc->height * newScale);
        const ImVec2 newTopLeft(cursor.x - imagePoint.x * newScale,
                                cursor.y - imagePoint.y * newScale);
        doc->panX = newTopLeft.x - areaCenter.x + newSize.x * 0.5f;
        doc->panY = newTopLeft.y - areaCenter.y + newSize.y * 0.5f;
        doc->zoom = newZoom;
        imageLayout(scale, topLeft);
    }

    if (ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        doc->panX += io.MouseDelta.x;
        doc->panY += io.MouseDelta.y;
        imageLayout(scale, topLeft);
    }

    const bool lightView = doc->view == core::View::Original ||
                           doc->view == core::View::Lit ||
                           doc->view == core::View::Compare ||
                           doc->view == core::View::Lighting ||
                           doc->view == core::View::Shadow;

    const auto placeLight = [&](int lightId) {
        // Allow the light to sit outside the image so it can graze the
        // subject from the side. The range is bounded to keep the marker
        // reachable after zooming out.
        const float u = std::clamp(
            (io.MousePos.x - topLeft.x) /
                std::max(1.0f, static_cast<float>(doc->width) * scale),
            -1.0f, 2.0f);
        const float v = std::clamp(
            (io.MousePos.y - topLeft.y) /
                std::max(1.0f, static_cast<float>(doc->height) * scale),
            -1.0f, 2.0f);
        const float aspect =
            static_cast<float>(doc->width) / static_cast<float>(doc->height);
        for (core::Light& light : doc->lights) {
            if (light.id != lightId) continue;
            light.position[0] = (u * 2.0f - 1.0f) * aspect;
            light.position[1] = 1.0f - v * 2.0f;
            doc->animate = false;
            return true;
        }
        return false;
    };

    if (lightView && doc->view != core::View::Compare &&
        ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        float bestDistance = scaled(16.0f);
        int bestLight = -1;
        for (const core::Light& light : doc->lights) {
            const ImVec2 marker = lightScreenPosition(*doc, light, topLeft, scale);
            const float distance = std::hypot(marker.x - io.MousePos.x,
                                              marker.y - io.MousePos.y);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestLight = light.id;
            }
        }
        if (bestLight >= 0) {
            doc->selectedLight = bestLight;
            app.ui.draggingLight = bestLight;
        } else if (doc->selectedLight != 0 &&
                   placeLight(doc->selectedLight)) {
            app.ui.draggingLight = doc->selectedLight;
        }
    }

    if (doc->view == core::View::Compare && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const float u = std::clamp(
            (io.MousePos.x - topLeft.x) /
                std::max(1.0f, static_cast<float>(doc->width) * scale),
            0.0f, 1.0f);
        doc->settings.comparison = u;
    }

    if (lightView && doc->view != core::View::Compare &&
        app.ui.draggingLight != 0 && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        placeLight(app.ui.draggingLight);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        app.ui.draggingLight = 0;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const unsigned int displayTexture = app.displayTextureId();
    if (displayTexture != 0) {
        const ImVec2 br(topLeft.x + doc->width * scale,
                        topLeft.y + doc->height * scale);
        const bool flipped = app.displayTextureFlipped();
        draw->AddImage(static_cast<ImTextureID>(displayTexture), topLeft, br,
                       flipped ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f),
                       flipped ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f));
        draw->AddRect(topLeft, br,
                      ImGui::ColorConvertFloat4ToU32(theme::border), 2.0f, 0, 1.0f);
    } else {
        draw->AddText(ImVec2(areaMin.x + scaled(24.0f),
                             areaMin.y + scaled(24.0f)),
                      ImGui::ColorConvertFloat4ToU32(theme::muted),
                      app.tr("canvas.texture_not_ready").c_str());
    }

    if (doc->view == core::View::Compare) {
        const float splitX =
            topLeft.x + doc->width * scale * doc->settings.comparison;
        const ImVec2 top(splitX, topLeft.y);
        const ImVec2 bottom(splitX, topLeft.y + doc->height * scale);
        draw->AddLine(top, bottom, theme::u32(theme::accent), 2.0f);
        draw->AddCircleFilled(
            ImVec2(splitX, (top.y + bottom.y) * 0.5f), 5.0f,
            theme::u32(theme::accent), 16);
    }

    if (lightView) {
        for (const core::Light& light : doc->lights) {
            const ImVec2 marker = lightScreenPosition(*doc, light, topLeft, scale);
            const bool selected = light.id == doc->selectedLight;
            const int alpha = light.enabled ? 235 : 95;
            draw->AddCircleFilled(marker, 11.0f, lightColorU32(light.color, alpha), 24);
            draw->AddCircle(marker, 12.5f,
                            ImGui::ColorConvertFloat4ToU32(selected ? theme::accent
                                                                    : theme::border),
                            24, selected ? 2.0f : 1.0f);
            const std::string label = std::to_string(light.id);
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            draw->AddText(ImVec2(marker.x - textSize.x * 0.5f,
                                 marker.y - textSize.y * 0.5f),
                          lightLabelU32(light.color,
                                        light.enabled ? 240 : 140),
                          label.c_str());
        }
    }

    if (hovered) {
        ImGui::SetMouseCursor(doc->view == core::View::Compare
                                  ? ImGuiMouseCursor_ResizeEW
                                  : ImGuiMouseCursor_Hand);
    }
}

void drawCanvas(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::canvas);
    {
        Panel column("##canvas_column", ImVec2(-scaled(kRightWidth), 0.0f), 0.0f,
                     false, ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoScrollWithMouse);
        if (column) {
            core::Document fallback;
            core::Document* doc = app.activeDocument();
            if (!doc) doc = &fallback;
            const bool hasDepth = app.hasTask() && doc->hasDepth;
            if (app.hasTask() && !hasDepth &&
                doc->view != core::View::Original) {
                doc->view = core::View::Original;
            }

            const float columnHeight = ImGui::GetContentRegionAvail().y;
            const float areaHeight = std::max(
                0.0f, columnHeight - scaled(kCanvasBarHeight) * 2.0f - scaled(kCanvasBottomGap));

            {
                Panel tabs("##view_tabs", ImVec2(0.0f, scaled(kCanvasBarHeight)), 0.0f, false,
                           ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoScrollWithMouse);
                if (tabs) {
                    const core::View views[] = {
                        core::View::Lit,     core::View::Compare,
                        core::View::Original, core::View::Normal,
                        core::View::Depth,   core::View::Lighting,
                        core::View::Shadow,
                    };
                    const std::string exportLabel = app.tr("topbar.export");
                    const float exportWidth =
                        ImGui::CalcTextSize(exportLabel.c_str()).x +
                        ImGui::GetStyle().FramePadding.x * 2.0f;
                    const float exportX = ImGui::GetWindowWidth() -
                                          exportWidth - scaled(16.0f);
                    const float tabWidth =
                        std::max(scaled(120.0f), exportX - scaled(20.0f));
                    ImGui::SetCursorPos(ImVec2(scaled(10.0f), 0.0f));
                    ImGui::BeginChild("##view_scroll",
                                      ImVec2(tabWidth, scaled(kCanvasBarHeight)),
                                      ImGuiChildFlags_None,
                                      ImGuiWindowFlags_HorizontalScrollbar |
                                          ImGuiWindowFlags_NoBackground);
                    ImGui::SetCursorPos(ImVec2(0.0f, scaled(7.0f)));
                    for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); ++i) {
                        if (i > 0) ImGui::SameLine(0.0f, scaled(4.0f));
                        const bool active = doc->view == views[i];
                        const bool enabled =
                            app.hasTask() &&
                            (views[i] == core::View::Original || hasDepth);
                        if (!enabled) ImGui::BeginDisabled();
                        if (active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
                            ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
                        }
                        if (ImGui::Button(app.tr(core::viewKey(views[i])).c_str())) {
                            doc->view = views[i];
                        }
                        if (active) ImGui::PopStyleColor(2);
                        if (!enabled) ImGui::EndDisabled();
                    }
                    ImGui::EndChild();

                    ImGui::SetCursorPos(
                        ImVec2(exportX,
                               (scaled(kCanvasBarHeight) - ImGui::GetFrameHeight()) * 0.5f));
                    const bool canExport =
                        app.hasTask() && doc->hasDepth && !app.exportBusy();
                    if (!canExport) ImGui::BeginDisabled();
                    if (ImGui::Button(exportLabel.c_str())) {
                        app.ui.exportPopupRequested = true;
                    }
                    if (!canExport) ImGui::EndDisabled();
                }
            }

            ImGui::SetCursorPosY(scaled(kCanvasBarHeight));
            {
                Panel area("##canvas_area", ImVec2(0.0f, areaHeight), 0.0f, false,
                           ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoScrollWithMouse);
                if (area) {
                    if (app.hasTask()) {
                        drawImageCanvas(app);
                    } else {
                        drawEmptyCanvas(app);
                    }
                }
            }

            ImGui::SetCursorPosY(columnHeight - scaled(kCanvasBarHeight) - scaled(kCanvasBottomGap));
            {
                Panel zoom("##zoom_tools", ImVec2(0.0f, scaled(kCanvasBarHeight)), 0.0f, false,
                           ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoScrollWithMouse);
                if (zoom) {
                    const float y =
                        (scaled(kCanvasBarHeight) - ImGui::GetFrameHeight()) * 0.5f;
                    const float buttonWidth = scaled(30.0f);
                    const float fitWidth =
                        ImGui::CalcTextSize(app.tr("canvas.fit").c_str()).x +
                        ImGui::GetStyle().FramePadding.x * 2.0f;
                    const float percentWidth =
                        ImGui::CalcTextSize("100%").x;
                    const float spacing = ImGui::GetStyle().ItemSpacing.x;
                    const float totalWidth = buttonWidth * 2.0f + fitWidth +
                                             percentWidth + spacing * 3.0f;
                    const float startX = std::max(
                        scaled(12.0f),
                        ImGui::GetWindowWidth() - totalWidth - scaled(16.0f));
                    ImGui::SetCursorPos(ImVec2(startX, y));
                    if (!app.hasTask()) ImGui::BeginDisabled();
                    if (ImGui::Button("-", ImVec2(buttonWidth, 0.0f))) {
                        doc->zoom =
                            std::clamp(doc->zoom / 1.2f, 0.05f, 8.0f);
                    }
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%.0f%%", doc->zoom * 100.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("+", ImVec2(buttonWidth, 0.0f))) {
                        doc->zoom =
                            std::clamp(doc->zoom * 1.2f, 0.05f, 8.0f);
                    }
                    ImGui::SameLine();
                    if (centeredButton(app.tr("canvas.fit").c_str(),
                                       ImVec2(fitWidth, 0.0f))) {
                        doc->zoom = 1.0f;
                        doc->panX = 0.0f;
                        doc->panY = 0.0f;
                        doc->fitPending = true;
                    }
                    if (!app.hasTask()) ImGui::EndDisabled();
                }
            }
        }
    }
    ImGui::PopStyleColor();
}

} // namespace ui
