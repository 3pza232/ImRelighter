#include "ui/widgets.h"

#include "ui/theme.h"
#include "imgui_internal.h"

#include <algorithm>
#include <vector>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace imrelighter::ui {
namespace {

ImU32 color(const ImVec4& value) {
    return ImGui::GetColorU32(value);
}

}  // namespace

Panel::Panel(const char* id, const ImVec2& size, float padding, bool border,
             ImGuiWindowFlags flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    if (border) {
        child_flags |= ImGuiChildFlags_Borders;
    }
    open_ = ImGui::BeginChild(id, size, child_flags, flags);
}

Panel::~Panel() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

Card::Card(const char* id, float padding, const ImVec4& background) {
    if (background.w > 0.0f) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
        pushed_background_ = true;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    const ImGuiChildFlags flags = ImGuiChildFlags_Borders |
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_AlwaysUseWindowPadding;
    open_ = ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), flags,
                              ImGuiWindowFlags_NoScrollbar);
}

Card::~Card() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    if (pushed_background_) {
        ImGui::PopStyleColor();
    }
}

DisabledScope::DisabledScope(bool disabled) {
    ImGui::BeginDisabled(disabled);
}

DisabledScope::~DisabledScope() {
    ImGui::EndDisabled();
}

FontScale::FontScale(float factor) {
    const float size = ImGui::GetStyle().FontSizeBase * std::max(factor, 0.1f);
    ImGui::PushFont(nullptr, size);
}

FontScale::~FontScale() {
    ImGui::PopFont();
}

bool centeredButton(const char* label, const ImVec2& size) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = std::max(size.y, ImGui::GetTextLineHeight() + style.FramePadding.y * 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x,
                               std::max(style.FramePadding.y,
                                        (height - ImGui::GetTextLineHeight()) * 0.5f)));
    const bool pressed = ImGui::Button(label, ImVec2(size.x, height));
    ImGui::PopStyleVar();
    return pressed;
}

bool primaryButton(const char* label, const ImVec2& size) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = std::max(size.y, ImGui::GetTextLineHeight() + style.FramePadding.y * 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x,
                               std::max(style.FramePadding.y,
                                        (height - ImGui::GetTextLineHeight()) * 0.5f)));
    ImGui::PushStyleColor(ImGuiCol_Button, palette().accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(std::min(palette().accent.x + 0.06f, 1.0f),
                                 std::min(palette().accent.y + 0.04f, 1.0f),
                                 std::min(palette().accent.z + 0.02f, 1.0f), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette().accent);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().ink);
    const bool pressed = ImGui::Button(label, ImVec2(size.x, height));
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    return pressed;
}

bool iconButton(const char* id, const char* glyph, const ImVec2& size) {
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec4 fill = active ? palette().accent
                               : (hovered ? palette().panel2
                                          : palette().panel);
    draw->AddRectFilled(min, max, color(fill), 6.0f);
    draw->AddRect(min, max, color(palette().border), 6.0f);
    const ImVec2 text_size = ImGui::CalcTextSize(glyph);
    const ImVec2 text_pos(min.x + (size.x - text_size.x) * 0.5f,
                          min.y + (size.y - text_size.y) * 0.5f);
    draw->AddText(text_pos, color(active ? palette().ink : palette().text), glyph);
    return pressed;
}

bool closeButton(const char* id, const ImVec2& size) {
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max,
                        color(palette().danger),
                        7.0f);
    const float arm = std::min(size.x, size.y) * 0.22f;
    const float thickness = std::max(1.0f, ImGui::GetFontSize() * 0.09f);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const ImU32 cross = color(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    draw->AddLine(ImVec2(center.x - arm, center.y - arm),
                  ImVec2(center.x + arm, center.y + arm), cross, thickness);
    draw->AddLine(ImVec2(center.x - arm, center.y + arm),
                  ImVec2(center.x + arm, center.y - arm), cross, thickness);
    return pressed;
}

void sectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(palette().accent, "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}

void formLabel(const char* label, float labelWidth) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
}

void centeredText(const char* text) {
    const float width = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f));
    ImGui::TextUnformatted(text);
}

void mutedText(const char* text) {
    ImGui::TextDisabled("%s", text);
}

bool segmentButton(const char* label, bool active, const ImVec2& size) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, palette().accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette().accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette().accent);
        ImGui::PushStyleColor(ImGuiCol_Text, palette().ink);
    }
    const bool pressed = ImGui::Button(label, size);
    if (active) {
        ImGui::PopStyleColor(4);
    }
    return pressed;
}

bool readOnlyField(const char* id, const char* text, const ImVec2& size) {
    ImVec2 field_size = size;
    if (field_size.x <= 0.0f) {
        field_size.x = ImGui::GetContentRegionAvail().x;
    }
    if (field_size.y <= 0.0f) {
        field_size.y = ImGui::GetFrameHeight();
    }
    ImGui::InvisibleButton(id, field_size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max, color(palette().panel2),
                        ImGui::GetStyle().FrameRounding);
    draw->AddRect(min, max, color(palette().border),
                  ImGui::GetStyle().FrameRounding);
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    const ImVec2 pos(min.x + ImGui::GetStyle().FramePadding.x,
                     min.y + (field_size.y - text_size.y) * 0.5f);
    draw->PushClipRect(ImVec2(min.x + ImGui::GetStyle().FramePadding.x, min.y),
                       ImVec2(max.x - ImGui::GetStyle().FramePadding.x, max.y), true);
    draw->AddText(pos, color(palette().muted), text);
    draw->PopClipRect();
    return false;
}

bool labelValueRow(const char* label, const char* value, float labelWidth) {
    ImGui::PushID(label);
    formLabel(label, labelWidth);
    const bool changed = readOnlyField("##value", value);
    ImGui::PopID();
    return changed;
}

namespace {
std::vector<ImVec4> g_touch_targets;
}

void clearTouchTargets() {
    g_touch_targets.clear();
}

void registerTouchTarget(const ImVec2& min, const ImVec2& max) {
    g_touch_targets.emplace_back(min.x, min.y, max.x, max.y);
}

bool touchHitsTarget(float x, float y) {
    for (const ImVec4& target : g_touch_targets) {
        if (x >= target.x && x <= target.z && y >= target.y && y <= target.w) {
            return true;
        }
    }
    return false;
}

void cancelActiveControl() {
    ImGui::ClearActiveID();
    // The touch is now owned by a scroll gesture, not by whatever widget
    // happened to be under the finger when the drag started.
    ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

namespace {

// Mobile-friendly slider: the value changes only when the gesture starts on
// the knob. Tapping or dragging the track must not jump the value.
bool thumbSlider(const char* id, float& value, float min_value, float max_value,
                 const char* format) {
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(width, height));

    const bool hovered = ImGui::IsItemHovered();
    const float track_start = pos.x + height * 0.5f;
    const float track_end = pos.x + std::max(width - height * 0.5f, track_start + 1.0f);
    const float range = std::max(max_value - min_value, 0.000001f);
    const float t = std::clamp((value - min_value) / range, 0.0f, 1.0f);
    const float knob_x = track_start + (track_end - track_start) * t;
    const float knob_radius = height * 0.95f;
    g_touch_targets.emplace_back(knob_x - knob_radius, pos.y,
                                 knob_x + knob_radius, pos.y + height);
    const ImGuiID storage_id = ImGui::GetID(id);
    int* dragging = ImGui::GetStateStorage()->GetIntRef(storage_id, 0);

    if (ImGui::IsItemActivated()) {
        const float mouse_x = ImGui::GetIO().MousePos.x;
        *dragging = std::fabs(mouse_x - knob_x) <= height * 0.95f ? 1 : 0;
    }
    bool changed = false;
    if (ImGui::IsItemActive() && *dragging != 0) {
        const float mouse_x = ImGui::GetIO().MousePos.x;
        const float next_t = std::clamp(
            (mouse_x - track_start) / (track_end - track_start), 0.0f, 1.0f);
        const float next_value = min_value + next_t * range;
        if (std::fabs(next_value - value) > 0.000001f) {
            value = next_value;
            changed = true;
        }
    }
    if (!ImGui::IsItemActive()) {
        *dragging = 0;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min(pos.x, pos.y);
    const ImVec2 max(pos.x + width, pos.y + height);
    const ImU32 frame = ImGui::GetColorU32(
        hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    draw->AddRectFilled(min, max, frame, ImGui::GetStyle().FrameRounding);
    draw->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border),
                  ImGui::GetStyle().FrameRounding);
    const float display_t =
        std::clamp((value - min_value) / std::max(max_value - min_value, 0.000001f),
                   0.0f, 1.0f);
    const float display_x = track_start + (track_end - track_start) * display_t;
    draw->AddRectFilled(ImVec2(track_start, pos.y + height * 0.47f),
                        ImVec2(display_x, pos.y + height * 0.53f),
                        ImGui::GetColorU32(ImGuiCol_SliderGrab),
                        height * 0.03f);
    draw->AddCircleFilled(ImVec2(display_x, pos.y + height * 0.5f), height * 0.36f,
                          ImGui::GetColorU32(*dragging ? ImGuiCol_SliderGrabActive
                                                       : ImGuiCol_SliderGrab),
                          24);
    char text[64];
    std::snprintf(text, sizeof(text), format, value);
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(max.x - text_size.x - ImGui::GetStyle().FramePadding.x,
                         min.y + (height - text_size.y) * 0.5f),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
    return changed;
}

}  // namespace

bool formSliderFloat(const char* id, const char* label, float& value, float min_value,
                     float max_value, const char* format, float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    const bool changed = thumbSlider("##value", value, min_value, max_value, format);
    ImGui::PopID();
    return changed;
}

bool formSliderInt(const char* id, const char* label, int& value, int min_value,
                   int max_value, float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    float float_value = static_cast<float>(value);
    const bool changed =
        thumbSlider("##value", float_value, static_cast<float>(min_value),
                    static_cast<float>(max_value), "%.0f");
    const int next = std::clamp(static_cast<int>(std::lround(float_value)), min_value,
                                max_value);
    if (next != value) {
        value = next;
        ImGui::PopID();
        return true;
    }
    ImGui::PopID();
    return changed;
}

bool formCheckbox(const char* id, const char* label, bool& value) {
    ImGui::PushID(id);
    const bool changed = ImGui::Checkbox(label, &value);
    ImGui::PopID();
    return changed;
}

bool formColor(const char* id, const char* label, float color_value[3], float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    const float height = ImGui::GetFrameHeight();
    const ImVec2 swatch_min = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##swatch", ImVec2(width, height));
    registerTouchTarget(swatch_min, ImVec2(swatch_min.x + width, swatch_min.y + height));
    const bool swatch_hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 swatch_max(swatch_min.x + width, swatch_min.y + height);
    draw->AddRectFilled(swatch_min, swatch_max,
                        ImGui::ColorConvertFloat4ToU32(
                            ImVec4(color_value[0], color_value[1], color_value[2], 1.0f)),
                        ImGui::GetStyle().FrameRounding);
    draw->AddRect(swatch_min, swatch_max,
                  color(swatch_hovered ? palette().accent : palette().border),
                  ImGui::GetStyle().FrameRounding);
    int* open = ImGui::GetStateStorage()->GetIntRef(ImGui::GetID("##open"), 0);
    if (ImGui::IsItemClicked()) {
        *open = *open != 0 ? 0 : 1;
    }

    bool changed = false;
    if (*open != 0) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed = ImGui::ColorPicker3(
            "##picker", color_value,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha |
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_PickerHueBar);
        const ImVec2 picker_min = ImGui::GetItemRectMin();
        const ImVec2 picker_max = ImGui::GetItemRectMax();
        registerTouchTarget(picker_min, picker_max);
    }
    ImGui::PopID();
    return changed;
}

bool formCombo(const char* id, const char* label, int& index, const char* const* items,
               int count, float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = false;
    const int safe_index = std::clamp(index, 0, std::max(0, count - 1));
    if (ImGui::BeginCombo("##value", count > 0 ? items[safe_index] : "")) {
        for (int i = 0; i < count; ++i) {
            const bool selected = i == safe_index;
            if (ImGui::Selectable(items[i], selected)) {
                index = i;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

}  // namespace imrelighter::ui
