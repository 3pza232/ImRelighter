#include "ui/ui.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "app/app.h"
#include "io/native_dialog.h"
#include "io/paths.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#ifndef IMRELIGHTER_VERSION
#define IMRELIGHTER_VERSION "0.1.1"
#endif

namespace ui {

namespace {

constexpr float kTopBarHeight = 58.0f;
constexpr float kStatusHeight = 36.0f;
constexpr float kSidebarWidth = 236.0f;
constexpr float kRightWidth = 360.0f;
constexpr float kLabelWidth = 160.0f;
constexpr float kCollapsedSidebarWidth = 38.0f;
constexpr float kCanvasBarHeight = 44.0f;
constexpr float kCanvasBottomGap = 6.0f;
constexpr float kTaskRowHeight = 84.0f;
constexpr float kTaskThumbSize = 64.0f;
constexpr float kTaskCloseWidth = 30.0f;
constexpr float kTaskCloseGap = 6.0f;
constexpr float kSettingsLabelWidth = 150.0f;
constexpr float kPositionBoxHeight = 80.0f;

const ImGuiWindowFlags kRootFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;


bool sliderRow(app::App& app, const char* labelKey, float& value, float min,
               float max, const char* format) {
    ImGui::PushID(labelKey);
    formLabel(app.tr(labelKey).c_str(), kLabelWidth);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::SliderFloat("##value", &value, min, max, format);
    ImGui::PopID();
    return changed;
}

bool checkboxRow(app::App& app, const char* labelKey, bool& value) {
    ImGui::PushID(labelKey);
    const bool changed = ImGui::Checkbox(app.tr(labelKey).c_str(), &value);
    ImGui::PopID();
    return changed;
}

ImVec4 hexToVec4(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return theme::text;
    auto component = [&](int index) {
        const unsigned value = static_cast<unsigned>(
            std::strtoul(hex.substr(index, 2).c_str(), nullptr, 16));
        return static_cast<float>(value) / 255.0f;
    };
    return ImVec4(component(1), component(3), component(5), 1.0f);
}

std::string vec4ToHex(const float rgb[3]) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
                  static_cast<int>(std::lround(std::clamp(rgb[0], 0.0f, 1.0f) * 255.0f)),
                  static_cast<int>(std::lround(std::clamp(rgb[1], 0.0f, 1.0f) * 255.0f)),
                  static_cast<int>(std::lround(std::clamp(rgb[2], 0.0f, 1.0f) * 255.0f)));
    return buffer;
}

bool colorRow(app::App& app, const char* labelKey, std::string& color) {
    ImGui::PushID(labelKey);
    formLabel(app.tr(labelKey).c_str(), kLabelWidth);

    ImVec4 value = hexToVec4(color);
    float rgb[3] = {value.x, value.y, value.z};
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::ColorEdit3("##color", rgb,
                                           ImGuiColorEditFlags_NoInputs |
                                               ImGuiColorEditFlags_NoLabel);
    if (changed) color = vec4ToHex(rgb);
    ImGui::PopID();
    return changed;
}

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


std::string modelPathLabel(const std::filesystem::path& path) {
    if (path.empty()) return {};
    std::error_code error;
    const auto relative = std::filesystem::relative(path, paths::exeDir(), error);
    if (!error && !relative.empty() &&
        relative.native().compare(0, 2, L"..") != 0) {
        return paths::toUtf8(relative);
    }
    return paths::toUtf8(path);
}
std::string backendLabel(app::App& app, core::Backend backend) {
    switch (backend) {
        case core::Backend::Cpu: return app.tr("settings.cpu");
        case core::Backend::Dml: return app.tr("settings.dml");
        case core::Backend::Auto: return app.tr("settings.auto");
    }
    return app.tr("settings.auto");
}

std::string formatMegabytes(std::uintmax_t bytes) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f MB",
                  static_cast<double>(bytes) / (1024.0 * 1024.0));
    return buffer;
}

void drawTopBar(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel topbar("##topbar", ImVec2(0.0f, kTopBarHeight), 0.0f, false,
                     ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse);
        if (topbar) {
            const float lineHeight = ImGui::GetTextLineHeight();
            ImGui::SetCursorPos(ImVec2(20.0f, (kTopBarHeight - lineHeight) * 0.5f));
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
                       (kTopBarHeight - ImGui::GetFrameHeight()) * 0.5f));
            if (ImGui::Button(settingsLabel.c_str())) app.ui.settingsOpen = true;
        }
    }
    ImGui::PopStyleColor();
}
void drawTaskCard(app::App& app, size_t index, uint64_t& activateId,
                  uint64_t& closeId) {
    const core::Document* doc = app.document(index);
    if (!doc) return;

    const bool active = app.activeIndex() == static_cast<int>(index);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float rowWidth = ImGui::GetContentRegionAvail().x;
    const float barWidth = std::max(
        0.0f, rowWidth - kTaskCloseWidth - kTaskCloseGap);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const bool hovered = ImGui::IsMouseHoveringRect(
        rowStart, ImVec2(rowStart.x + barWidth, rowStart.y + kTaskRowHeight));
    if (active || hovered) {
        draw->AddRectFilled(rowStart,
                            ImVec2(rowStart.x + barWidth, rowStart.y + kTaskRowHeight),
                            theme::u32(active ? theme::panel2 : theme::taskHover),
                            7.0f);
    }
    if (active) {
        draw->AddRectFilled(rowStart,
                            ImVec2(rowStart.x + 2.0f, rowStart.y + kTaskRowHeight),
                            theme::u32(theme::accent), 7.0f);
    }

    ImGui::PushID(static_cast<int>(doc->id));
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x + 12.0f, rowStart.y + 10.0f));
    ImGui::InvisibleButton("##thumbnail", ImVec2(kTaskThumbSize, kTaskThumbSize));
    if (ImGui::IsItemClicked()) activateId = doc->id;

    const ImVec2 thumbPos = ImVec2(rowStart.x + 12.0f, rowStart.y + 10.0f);
    draw->AddRectFilled(thumbPos,
                        ImVec2(thumbPos.x + kTaskThumbSize, thumbPos.y + kTaskThumbSize),
                        theme::u32(theme::canvas), 4.0f);
    if (gpu::Texture* texture = app.thumbnail(doc->id);
        texture && texture->id() != 0 && texture->width() > 0 &&
        texture->height() > 0) {
        const float scale = std::min(
            kTaskThumbSize / static_cast<float>(texture->width()),
            kTaskThumbSize / static_cast<float>(texture->height()));
        const ImVec2 drawSize(texture->width() * scale, texture->height() * scale);
        const ImVec2 offset((kTaskThumbSize - drawSize.x) * 0.5f,
                            (kTaskThumbSize - drawSize.y) * 0.5f);
        draw->AddImage(static_cast<ImTextureID>(texture->id()),
                       ImVec2(thumbPos.x + offset.x, thumbPos.y + offset.y),
                       ImVec2(thumbPos.x + offset.x + drawSize.x,
                              thumbPos.y + offset.y + drawSize.y));
    }
    draw->AddRect(thumbPos,
                  ImVec2(thumbPos.x + kTaskThumbSize, thumbPos.y + kTaskThumbSize),
                  ImGui::ColorConvertFloat4ToU32(theme::border), 4.0f);

    ImGui::PushClipRect(
        ImVec2(rowStart.x + 88.0f, rowStart.y),
        ImVec2(rowStart.x + barWidth - 4.0f, rowStart.y + kTaskRowHeight), true);
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x + 88.0f, rowStart.y + 20.0f));
    ImGui::BeginGroup();
    if (active) {
        ImGui::TextColored(theme::accent, "%s", doc->name.c_str());
    } else {
        ImGui::TextUnformatted(doc->name.c_str());
    }
    ImGui::TextDisabled("%d × %d", doc->width, doc->height);
    ImGui::EndGroup();
    ImGui::PopClipRect();

    ImGui::SetCursorScreenPos(
        ImVec2(rowStart.x + barWidth + kTaskCloseGap, rowStart.y));
    if (closeButton("##remove", ImVec2(kTaskCloseWidth, kTaskRowHeight))) {
        closeId = doc->id;
    }

    ImGui::SetCursorScreenPos(
        ImVec2(rowStart.x, rowStart.y + kTaskRowHeight + 2.0f));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopID();
}
void drawSidebar(app::App& app) {
    const bool open = app.ui.sidebarOpen;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel sidebar("##sidebar",
                      ImVec2(open ? kSidebarWidth : kCollapsedSidebarWidth, 0),
                      0.0f, true, ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse);

        if (!open) {
            if (sidebar) {
                ImGui::SetCursorPos(ImVec2(8.0f, 12.0f));
                if (iconButton("##expand", "»", ImVec2(22.0f, 26.0f))) {
                    app.ui.sidebarOpen = true;
                    app.ui.fitPending = true;
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
                app.ui.fitPending = true;
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
                        centeredText("PNG / JPEG");
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
    if (ImGui::Button(buttonLabel.c_str(), ImVec2(buttonWidth, 30.0f))) {
        app.requestOpenImage();
    }
}

void drawImageCanvas(app::App& app) {
    const core::Document* doc = app.activeDocument();
    if (!doc) return;

    constexpr float kPadding = 24.0f;
    const ImVec2 areaMin = ImGui::GetCursorScreenPos();
    const ImVec2 areaSize = ImGui::GetContentRegionAvail();
    const float fitScale = std::max(
        0.001f, std::min((areaSize.x - kPadding * 2.0f) / static_cast<float>(doc->width),
                         (areaSize.y - kPadding * 2.0f) / static_cast<float>(doc->height)));

    if (app.ui.fitPending) {
        app.ui.zoom = 1.0f;
        app.ui.panX = 0.0f;
        app.ui.panY = 0.0f;
        app.ui.fitPending = false;
    }

    const ImVec2 areaCenter(areaMin.x + areaSize.x * 0.5f,
                            areaMin.y + areaSize.y * 0.5f);

    auto imageLayout = [&](float& scale, ImVec2& topLeft) {
        scale = fitScale * app.ui.zoom;
        const ImVec2 size(doc->width * scale, doc->height * scale);
        topLeft = ImVec2(areaCenter.x + app.ui.panX - size.x * 0.5f,
                         areaCenter.y + app.ui.panY - size.y * 0.5f);
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
            app.ui.zoom * std::pow(1.12f, io.MouseWheel), 0.05f, 8.0f);
        const float newScale = fitScale * newZoom;
        const ImVec2 newSize(doc->width * newScale, doc->height * newScale);
        const ImVec2 newTopLeft(cursor.x - imagePoint.x * newScale,
                                cursor.y - imagePoint.y * newScale);
        app.ui.panX = newTopLeft.x - areaCenter.x + newSize.x * 0.5f;
        app.ui.panY = newTopLeft.y - areaCenter.y + newSize.y * 0.5f;
        app.ui.zoom = newZoom;
        imageLayout(scale, topLeft);
    }

    if (ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        app.ui.panX += io.MouseDelta.x;
        app.ui.panY += io.MouseDelta.y;
        imageLayout(scale, topLeft);
    }

    const bool lightView = app.ui.view == core::View::Original ||
                           app.ui.view == core::View::Lit ||
                           app.ui.view == core::View::Compare ||
                           app.ui.view == core::View::Lighting ||
                           app.ui.view == core::View::Shadow;

    if (lightView && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        float bestDistance = 16.0f;
        int bestLight = -1;
        for (const core::Light& light : app.ui.lights) {
            const ImVec2 marker = lightScreenPosition(*doc, light, topLeft, scale);
            const float distance = std::hypot(marker.x - io.MousePos.x,
                                              marker.y - io.MousePos.y);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestLight = light.id;
            }
        }
        if (bestLight >= 0) app.ui.selectedLight = bestLight;
    }

    if (app.ui.view == core::View::Compare && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const float u = std::clamp(
            (io.MousePos.x - topLeft.x) /
                std::max(1.0f, static_cast<float>(doc->width) * scale),
            0.0f, 1.0f);
        app.ui.settings.comparison = u;
    }

    if (lightView && app.ui.view != core::View::Compare && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const float aspect = static_cast<float>(doc->width) / static_cast<float>(doc->height);
        const float u = (io.MousePos.x - topLeft.x) /
                        (static_cast<float>(doc->width) * scale);
        const float v = (io.MousePos.y - topLeft.y) /
                        (static_cast<float>(doc->height) * scale);
        for (core::Light& light : app.ui.lights) {
            if (light.id != app.ui.selectedLight) continue;
            light.position[0] = std::clamp((u * 2.0f - 1.0f) * aspect, -100.0f, 100.0f);
            light.position[1] = std::clamp(1.0f - v * 2.0f, -100.0f, 100.0f);
            app.ui.animate = false;
            break;
        }
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
        draw->AddText(ImVec2(areaMin.x + 24.0f, areaMin.y + 24.0f),
                      ImGui::ColorConvertFloat4ToU32(theme::muted),
                      "Image texture is not ready.");
    }

    if (app.ui.view == core::View::Compare) {
        const float splitX =
            topLeft.x + doc->width * scale * app.ui.settings.comparison;
        const ImVec2 top(splitX, topLeft.y);
        const ImVec2 bottom(splitX, topLeft.y + doc->height * scale);
        draw->AddLine(top, bottom, theme::u32(theme::accent), 2.0f);
        draw->AddCircleFilled(
            ImVec2(splitX, (top.y + bottom.y) * 0.5f), 5.0f,
            theme::u32(theme::accent), 16);
    }

    if (lightView) {
        for (const core::Light& light : app.ui.lights) {
            const ImVec2 marker = lightScreenPosition(*doc, light, topLeft, scale);
            const bool selected = light.id == app.ui.selectedLight;
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
                          theme::u32(theme::ink, (light.enabled ? 240.0f : 140.0f) / 255.0f),
                          label.c_str());
        }
    }

    if (hovered) {
        ImGui::SetMouseCursor(app.ui.view == core::View::Compare
                                  ? ImGuiMouseCursor_ResizeEW
                                  : ImGuiMouseCursor_Hand);
    }
}

void drawCanvas(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::canvas);
    {
        Panel column("##canvas_column", ImVec2(-kRightWidth, 0.0f), 0.0f,
                     false, ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoScrollWithMouse);
        if (column) {
            const core::Document* doc = app.activeDocument();
            const bool hasDepth = app.hasTask() && doc && doc->hasDepth;
            if (app.hasTask() && !hasDepth &&
                app.ui.view != core::View::Original) {
                app.ui.view = core::View::Original;
            }

            const float columnHeight = ImGui::GetContentRegionAvail().y;
            const float areaHeight = std::max(
                0.0f, columnHeight - kCanvasBarHeight * 2.0f - kCanvasBottomGap);

            {
                Panel tabs("##view_tabs", ImVec2(0.0f, kCanvasBarHeight), 0.0f, false,
                           ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoScrollWithMouse);
                if (tabs) {
                    const core::View views[] = {
                        core::View::Lit,     core::View::Compare,
                        core::View::Original, core::View::Normal,
                        core::View::Depth,   core::View::Lighting,
                        core::View::Shadow,
                    };
                    ImGui::SetCursorPos(ImVec2(12.0f, 7.0f));
                    for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); ++i) {
                        if (i > 0) ImGui::SameLine(0.0f, 4.0f);
                        const bool active = app.ui.view == views[i];
                        const bool enabled =
                            app.hasTask() &&
                            (views[i] == core::View::Original || hasDepth);
                        if (!enabled) ImGui::BeginDisabled();
                        if (active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
                            ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
                        }
                        if (ImGui::Button(app.tr(core::viewKey(views[i])).c_str())) {
                            app.ui.view = views[i];
                        }
                        if (active) ImGui::PopStyleColor(2);
                        if (!enabled) ImGui::EndDisabled();
                    }

                    const std::string exportLabel = app.tr("topbar.export");
                    const float exportWidth =
                        ImGui::CalcTextSize(exportLabel.c_str()).x +
                        ImGui::GetStyle().FramePadding.x * 2.0f;
                    ImGui::SetCursorPos(
                        ImVec2(ImGui::GetWindowWidth() - exportWidth - 16.0f,
                               (kCanvasBarHeight - ImGui::GetFrameHeight()) * 0.5f));
                    if (app.exportBusy()) ImGui::BeginDisabled();
                    if (ImGui::Button(exportLabel.c_str())) {
                        app.ui.exportPopupRequested = true;
                    }
                    if (app.exportBusy()) ImGui::EndDisabled();
                }
            }

            ImGui::SetCursorPosY(kCanvasBarHeight);
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

            ImGui::SetCursorPosY(columnHeight - kCanvasBarHeight - kCanvasBottomGap);
            {
                Panel zoom("##zoom_tools", ImVec2(0.0f, kCanvasBarHeight), 0.0f, false,
                           ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoScrollWithMouse);
                if (zoom) {
                    const float y = (kCanvasBarHeight - ImGui::GetFrameHeight()) * 0.5f;
                    ImGui::SetCursorPos(
                        ImVec2(ImGui::GetWindowWidth() - 216.0f, y));
                    if (!app.hasTask()) ImGui::BeginDisabled();
                    if (ImGui::Button("-", ImVec2(28.0f, 0.0f))) {
                        app.ui.zoom =
                            std::clamp(app.ui.zoom / 1.2f, 0.05f, 8.0f);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%.0f%%", app.ui.zoom * 100.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("+", ImVec2(28.0f, 0.0f))) {
                        app.ui.zoom =
                            std::clamp(app.ui.zoom * 1.2f, 0.05f, 8.0f);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(app.tr("canvas.fit").c_str(),
                                      ImVec2(64.0f, 0.0f))) {
                        app.ui.zoom = 1.0f;
                        app.ui.panX = 0.0f;
                        app.ui.panY = 0.0f;
                        app.ui.fitPending = true;
                    }
                    if (!app.hasTask()) ImGui::EndDisabled();
                }
            }
        }
    }
    ImGui::PopStyleColor();
}
void drawImageSection(app::App& app) {
    sectionTitle(app.tr("panel.image").c_str());

    ImGui::SeparatorText(app.tr("panel.image.model").c_str());
    const bool modelReady = app.modelExists();
    ImGui::TextColored(modelReady ? theme::ok : theme::danger, "%s",
                       app.tr(modelReady ? "model.ready" : "model.missing").c_str());

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("panel.image.depth").c_str());
    ImGui::TextUnformatted(app.tr("panel.image.backend").c_str());
    ImGui::SameLine(kLabelWidth);
    ImGui::TextDisabled("%s", backendLabel(app, app.config.backend).c_str());

    ImGui::TextUnformatted(app.tr("panel.image.depth").c_str());
    ImGui::SameLine(kLabelWidth);
    const core::Document* active = app.activeDocument();
    const bool hasDepth = active && active->hasDepth;
    const bool running = app.inferenceBusy();
    ImGui::TextDisabled("%s",
                        app.tr(running ? "depth.running"
                                       : hasDepth ? "depth.ready"
                                                  : "depth.not_generated")
                            .c_str());

    ImGui::Spacing();
    const bool canGenerate = app.hasTask() && !running;
    if (canGenerate) {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accentHover);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
    }
    ImGui::BeginDisabled(!canGenerate);
    if (ImGui::Button(app.tr("panel.image.generate").c_str(), ImVec2(-1.0f, 34.0f))) {
        app.generateDepth();
    }
    ImGui::EndDisabled();
    if (canGenerate) ImGui::PopStyleColor(3);

    if (running) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, theme::danger);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::dangerHover);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
        if (ImGui::Button(app.tr("panel.image.cancel").c_str(), ImVec2(-1.0f, 30.0f))) {
            app.cancelInference();
        }
        ImGui::PopStyleColor(3);
    }

    if (!app.hasTask()) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", app.tr("panel.image.no_task").c_str());
    }
}

void drawLightSection(app::App& app) {
    sectionTitle(app.tr("panel.light").c_str());

    ImGui::SeparatorText(app.tr("section.light_list").c_str());
    ImGui::TextDisabled("%d / 8", static_cast<int>(app.ui.lights.size()));

    for (auto& light : app.ui.lights) {
        ImGui::PushID(light.id);
        ImGui::PushStyleColor(ImGuiCol_Text, hexToVec4(light.color));
        ImGui::TextUnformatted("●");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Selectable(light.name.c_str(), app.ui.selectedLight == light.id)) {
            app.ui.selectedLight = light.id;
        }
        ImGui::PopID();
    }

    ImGui::BeginDisabled(app.ui.lights.size() >= 8);
    if (ImGui::Button(app.tr("light.add").c_str())) {
        int nextId = 1;
        for (const auto& light : app.ui.lights) nextId = std::max(nextId, light.id + 1);
        app.ui.lights.push_back(core::makeLight(nextId, {0.35f, 0.2f, 1.4f}));
        app.ui.selectedLight = nextId;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(app.ui.lights.size() <= 1);
    if (ImGui::Button(app.tr("light.delete").c_str())) {
        app.ui.lights.erase(
            std::remove_if(app.ui.lights.begin(), app.ui.lights.end(),
                           [&](const core::Light& light) {
                               return light.id == app.ui.selectedLight;
                           }),
            app.ui.lights.end());
        if (!app.ui.lights.empty()) app.ui.selectedLight = app.ui.lights.front().id;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(app.tr("light.toggle").c_str())) {
        for (auto& light : app.ui.lights) {
            if (light.id == app.ui.selectedLight) light.enabled = !light.enabled;
        }
    }

    auto selected = std::find_if(app.ui.lights.begin(), app.ui.lights.end(),
                                 [&](const core::Light& light) {
                                     return light.id == app.ui.selectedLight;
                                 });
    if (selected == app.ui.lights.end() && !app.ui.lights.empty()) {
        selected = app.ui.lights.begin();
        app.ui.selectedLight = selected->id;
    }

    if (selected != app.ui.lights.end()) {
        ImGui::Spacing();
        ImGui::SeparatorText(app.tr("section.selected_light").c_str());
        ImGui::TextUnformatted(app.tr("light.position").c_str());
        const float positionMin[3] = {-2.0f, -2.0f, 0.1f};
        const float positionMax[3] = {2.0f, 2.0f, 4.0f};
        {
            Box positionBox("##position_box",
                            ImVec2(0.0f, kPositionBoxHeight), 10.0f, true);
            if (positionBox) {
                vector3Control("##position", selected->position.data(),
                               positionMin, positionMax);
            }
        }

        ImGui::Spacing();
        colorRow(app, "light.color", selected->color);
        sliderRow(app, "light.intensity", selected->intensity, 0.0f, 8.0f, "%.2f");
        sliderRow(app, "light.range", selected->range, 0.2f, 8.0f, "%.2f");
    }

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.motion").c_str());
    checkboxRow(app, "light.orbit", app.ui.animate);

}

void drawShadingSection(app::App& app) {
    sectionTitle(app.tr("panel.shading").c_str());

    ImGui::SeparatorText(app.tr("section.environment").c_str());
    colorRow(app, "shading.ambient_color", app.ui.settings.ambientColor);
    sliderRow(app, "shading.ambient", app.ui.settings.ambient, 0.0f, 2.0f, "%.2f");
    sliderRow(app, "shading.wrap", app.ui.settings.wrap, 0.0f, 1.0f, "%.2f");

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.shadows").c_str());
    checkboxRow(app, "shading.shadows", app.ui.settings.shadows);
    ImGui::BeginDisabled(!app.ui.settings.shadows);
    sliderRow(app, "shading.bias", app.ui.settings.bias, 0.0001f, 0.1f, "%.4f");
    sliderRow(app, "shading.softness", app.ui.settings.softness, 0.0f, 0.25f, "%.3f");
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader((app.tr("shading.highlights") + "###highlights").c_str())) {
        sliderRow(app, "shading.specular", app.ui.settings.specular, 0.0f, 2.0f, "%.2f");
        sliderRow(app, "shading.shininess", app.ui.settings.shininess, 1.0f, 256.0f, "%.0f");
        sliderRow(app, "shading.rim", app.ui.settings.rim, 0.0f, 2.0f, "%.2f");
    }
}

void drawSurfaceSection(app::App& app) {
    sectionTitle(app.tr("panel.surface").c_str());

    ImGui::SeparatorText(app.tr("section.base").c_str());
    ImGui::TextUnformatted(app.tr("surface.base_mode").c_str());
    const float baseWidth = ImGui::GetContentRegionAvail().x;
    if (segmentButton(app.tr("surface.base_keep").c_str(),
                      !app.ui.settings.baseMode, ImVec2(baseWidth, 0.0f))) {
        app.ui.settings.baseMode = false;
    }
    if (segmentButton(app.tr("surface.base_clean").c_str(),
                      app.ui.settings.baseMode, ImVec2(baseWidth, 0.0f))) {
        app.ui.settings.baseMode = true;
    }

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.depth_surface").c_str());
    sliderRow(app, "surface.height", app.ui.settings.height, 0.01f, 2.0f, "%.2f");
    sliderRow(app, "surface.normal_strength", app.ui.settings.normalStrength, 0.1f, 3.0f, "%.2f");
    sliderRow(app, "surface.smooth", app.ui.settings.smooth, 0.0f, 4.0f, "%.1f");
    checkboxRow(app, "surface.line_assist", app.ui.settings.lineAssist);
    checkboxRow(app, "surface.invert", app.ui.settings.invert);

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.composite").c_str());
    sliderRow(app, "surface.mix", app.ui.settings.mix, 0.0f, 1.0f, "%.2f");
    sliderRow(app, "surface.exposure", app.ui.settings.exposure, -3.0f, 3.0f, "%.2f");

    if (ImGui::CollapsingHeader((app.tr("surface.advanced") + "###advanced").c_str())) {
        checkboxRow(app, "surface.flip_x", app.ui.settings.flipX);
        checkboxRow(app, "surface.flip_y", app.ui.settings.flipY);
        checkboxRow(app, "surface.flip_z", app.ui.settings.flipZ);
        checkboxRow(app, "surface.auto_depth", app.ui.settings.autoDepthRange);
        ImGui::BeginDisabled(app.ui.settings.autoDepthRange);
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputFloat(app.tr("surface.depth_min").c_str(),
                          &app.ui.settings.depthMin, 0.0f, 0.0f, "%.3f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputFloat(app.tr("surface.depth_max").c_str(),
                          &app.ui.settings.depthMax, 0.0f, 0.0f, "%.3f");
        ImGui::EndDisabled();
        if (app.ui.settings.depthMax <= app.ui.settings.depthMin) {
            app.ui.settings.depthMax = app.ui.settings.depthMin + 0.001f;
        }
    }
}

void drawRightPanel(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel panel("##right_panel", ImVec2(kRightWidth, 0.0f), 0.0f, true);
        if (panel) {
            Panel content("##right_scroll", ImVec2(0.0f, 0.0f), 18.0f);
            if (content) {
                drawImageSection(app);
                ImGui::BeginDisabled(!app.hasTask());
                drawLightSection(app);
                drawShadingSection(app);
                drawSurfaceSection(app);
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::BeginDisabled(!app.hasTask());
                if (ImGui::Button(app.tr("panel.reset").c_str(),
                                  ImVec2(-1.0f, 0.0f))) {
                    app.resetTask();
                }
                ImGui::EndDisabled();
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }
        }
    }
    ImGui::PopStyleColor();
}
void drawStatusBar(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel status("##statusbar", ImVec2(0.0f, kStatusHeight), 0.0f, false,
                     ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse);
        if (status) {
            const float y = (kStatusHeight - ImGui::GetTextLineHeight()) * 0.5f;
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
void drawSettings(app::App& app) {
    if (!app.ui.settingsOpen) return;

    Dialog settings("##settings_window", ImVec2(680.0f, 520.0f), 22.0f);
    if (settings) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        {
            FontScale titleScale(1.12f);
            ImGui::TextUnformatted(app.tr("settings.title").c_str());
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 48.0f);
        if (closeButton("##settings_close", ImVec2(26.0f, 26.0f))) {
            app.ui.settingsOpen = false;
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        ImGui::Separator();

        if (ImGui::BeginTabBar("##settings_tabs")) {
            if (ImGui::BeginTabItem(
                    (app.tr("settings.general") + "###general").c_str())) {
                sectionTitle(app.tr("settings.appearance").c_str());

                formLabel(app.tr("settings.theme").c_str(), kSettingsLabelWidth);
                if (segmentButton(app.tr("settings.theme.dark").c_str(),
                                  app.theme() == core::Theme::Dark,
                                  ImVec2(80.0f, 0.0f))) {
                    app.setTheme(core::Theme::Dark);
                }
                ImGui::SameLine();
                if (segmentButton(app.tr("settings.theme.light").c_str(),
                                  app.theme() == core::Theme::Light,
                                  ImVec2(80.0f, 0.0f))) {
                    app.setTheme(core::Theme::Light);
                }

                ImGui::Spacing();
                formLabel(app.tr("settings.language").c_str(), kSettingsLabelWidth);
                if (segmentButton("简体中文", app.language() == core::Language::ZhCN,
                                  ImVec2(110.0f, 0.0f))) {
                    app.setLanguage(core::Language::ZhCN);
                }
                ImGui::SameLine();
                if (segmentButton("English", app.language() == core::Language::EnUS,
                                  ImVec2(100.0f, 0.0f))) {
                    app.setLanguage(core::Language::EnUS);
                }

                ImGui::Spacing();
                formLabel(app.tr("settings.backend").c_str(), kSettingsLabelWidth);
                if (segmentButton(app.tr("settings.auto").c_str(),
                                  app.config.backend == core::Backend::Auto,
                                  ImVec2(80.0f, 0.0f))) {
                    app.setBackend(core::Backend::Auto);
                }
                ImGui::SameLine();
                if (segmentButton(app.tr("settings.cpu").c_str(),
                                  app.config.backend == core::Backend::Cpu,
                                  ImVec2(70.0f, 0.0f))) {
                    app.setBackend(core::Backend::Cpu);
                }
                ImGui::SameLine();
                const bool dmlReady = app.directmlAvailable();
                if (!dmlReady) ImGui::BeginDisabled();
                if (segmentButton(app.tr("settings.dml").c_str(),
                                  app.config.backend == core::Backend::Dml,
                                  ImVec2(100.0f, 0.0f)) && dmlReady) {
                    app.setBackend(core::Backend::Dml);
                }
                if (!dmlReady) ImGui::EndDisabled();
                if (!dmlReady) {
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.dml_unavailable").c_str());
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(
                    (app.tr("settings.model") + "###model").c_str())) {
                sectionTitle(app.tr("settings.model").c_str());
                const bool modelReady = app.modelExists();
                {
                    Box info("##model_info", ImVec2(0.0f, 0.0f), 12.0f, true);
                    if (info) {
                    const bool noModel = app.modelPath().empty();
                    ImGui::TextColored(modelReady ? theme::ok : theme::danger,
                                       "%s",
                                       app.tr(noModel
                                                  ? "model.not_selected"
                                                  : modelReady ? "model.ready"
                                                               : "model.missing")
                                           .c_str());
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.model_path").c_str());
                    const std::string modelPathText = modelPathLabel(app.modelPath());
                    ImGui::TextWrapped("%s", modelPathText.c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", paths::toUtf8(app.modelPath()).c_str());
                    }
                    if (!app.modelPath().empty()) {
                        app.inspectModelInfo();
                        ImGui::TextDisabled("%s",
                                            app.tr("settings.model_type").c_str());
                        ImGui::TextUnformatted(app.modelType().c_str());
                    }
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.model_size").c_str());
                    ImGui::TextUnformatted(formatMegabytes(app.modelSize()).c_str());
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.model_sha").c_str());
                    app.refreshModelSha();
                    if (app.modelShaPending()) {
                        ImGui::TextDisabled("%s", app.tr("settings.model_sha_calculating").c_str());
                    } else {
                        const std::string sha = app.modelSha();
                        ImGui::TextWrapped("%s", sha.empty() ? "—" : sha.c_str());
                    }
                    ImGui::Spacing();

                    if (ImGui::Button(
                            app.tr("settings.choose_model_file").c_str(),
                            ImVec2(-1.0f, 0.0f))) {
                        app.chooseModelFile();
                    }
                    const float modelButtonWidth =
                        (ImGui::GetContentRegionAvail().x -
                         ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                    if (ImGui::Button(
                            app.tr("settings.open_model_dir").c_str(),
                            ImVec2(modelButtonWidth, 0.0f))) {
                        native_dialog::openInExplorer(app.modelFolder());
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, theme::danger);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::dangerHover);
                    if (ImGui::Button(
                            app.tr("settings.clear_model").c_str(),
                            ImVec2(modelButtonWidth, 0.0f))) {
                        app.clearModelSelection();
                    }
                    ImGui::PopStyleColor(2);
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(
                    (app.tr("settings.about") + "###about").c_str())) {
                sectionTitle(app.tr("settings.about").c_str());
                ImGui::Text("%s %s", app.tr("settings.version").c_str(),
                            IMRELIGHTER_VERSION);
                ImGui::TextDisabled("by pza");
                ImGui::Text("%s: %s", app.tr("settings.opengl").c_str(),
                            app.glVersion().c_str());
                ImGui::Text("%s: %s", app.tr("settings.gpu").c_str(),
                            app.glRenderer().c_str());
                ImGui::Spacing();
                ImGui::TextUnformatted(app.tr("settings.shortcuts").c_str());
                ImGui::TextDisabled("%s",
                                    app.tr("settings.shortcuts_text").c_str());
                ImGui::Spacing();
                ImGui::TextUnformatted(app.tr("settings.licenses").c_str());
                ImGui::TextDisabled(
                    "GLFW · Dear ImGui · GLAD · stb · nlohmann/json");
                ImGui::TextDisabled(
                    "ONNX Runtime (MIT) · DirectML (Microsoft) · libwebp (BSD-3)");
                ImGui::TextDisabled("Depth Anything V2 (Apache-2.0) · Source Han Sans (OFL-1.1)");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
void drawExportMenu(app::App& app) {
    if (app.ui.exportPopupRequested) {
        ImGui::OpenPopup("##export_menu");
        app.ui.exportPopupRequested = false;
    }

    if (ImGui::BeginPopup("##export_menu")) {
        const core::Document* doc = app.activeDocument();
        const bool enabled = doc && doc->hasDepth && !app.exportBusy();
        if (ImGui::MenuItem(app.tr("view.lit").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Lit);
        }
        if (ImGui::MenuItem(app.tr("view.normal").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Normal);
        }
        if (ImGui::MenuItem(app.tr("view.depth").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Depth);
        }
        if (ImGui::MenuItem(app.tr("view.lighting").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Lighting);
        }
        if (ImGui::MenuItem(app.tr("view.shadow").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Shadow);
        }
        ImGui::EndPopup();
    }
}

} // namespace

void draw(app::App& app) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##root", nullptr, kRootFlags);

    drawTopBar(app);

    const float mainHeight =
        std::max(0.0f, display.y - kTopBarHeight - kStatusHeight);
    ImGui::SetCursorPos(ImVec2(0.0f, kTopBarHeight));
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

    ImGui::SetCursorPos(ImVec2(0.0f, display.y - kStatusHeight));
    drawStatusBar(app);
    ImGui::End();
    ImGui::PopStyleVar(2);

    drawExportMenu(app);
    drawSettings(app);
}

} // namespace ui
