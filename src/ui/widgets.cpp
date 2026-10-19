#include "ui/widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/color.h"
#include "ui/theme.h"

namespace ui {

Panel::Panel(const char* id, const ImVec2& size, float padding, bool border,
             ImGuiWindowFlags flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));

    ImGuiChildFlags childFlags = border ? ImGuiChildFlags_Borders : 0;
    if (padding > 0.0f) childFlags |= ImGuiChildFlags_AlwaysUseWindowPadding;

    open_ = ImGui::BeginChild(id, size, childFlags, flags);
}

Panel::~Panel() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

bool iconButton(const char* id, const char* glyph, const ImVec2& size) {
    const bool clicked = ImGui::InvisibleButton(id, size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 background = theme::u32(held ? theme::accent
                                   : hovered ? theme::panel2
                                             : theme::panel);
    draw->AddRectFilled(min, max, background, 6.0f);
    draw->AddRect(min, max, theme::u32(theme::border), 6.0f);

    const ImVec2 textSize = ImGui::CalcTextSize(glyph);
    const ImVec2 textPos(min.x + (size.x - textSize.x) * 0.5f,
                         min.y + (size.y - textSize.y) * 0.5f);
    draw->AddText(textPos,
                  theme::u32(held ? theme::ink : theme::text), glyph);
    return clicked;
}

bool centeredButton(const char* label, const ImVec2& size) {
    ImGuiStyle& style = ImGui::GetStyle();
    const float paddingY =
        std::max(style.FramePadding.y,
                 (size.y - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x, paddingY));
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    return clicked;
}

bool closeButton(const char* id, const ImVec2& size) {
    const bool clicked = ImGui::InvisibleButton(id, size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max,
                        theme::u32(hovered || held ? theme::dangerHover
                                                   : theme::danger),
                        7.0f);

    // Draw the cross from the exact center instead of relying on the glyph
    // bounding box, which sits slightly high inside tall buttons.
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const float arm = std::min(size.x, size.y) * 0.22f;
    const float thickness = std::max(1.0f, ImGui::GetFontSize() * 0.09f);
    const ImU32 color = theme::u32(theme::closeText);
    draw->AddLine(ImVec2(center.x - arm, center.y - arm),
                  ImVec2(center.x + arm, center.y + arm), color, thickness);
    draw->AddLine(ImVec2(center.x - arm, center.y + arm),
                  ImVec2(center.x + arm, center.y - arm), color, thickness);
    return clicked;
}

void sectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(theme::accent, "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}

void formLabel(const char* label, float labelWidth) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
}

void centeredText(const char* text) {
    const float width = ImGui::GetContentRegionAvail().x;
    const float textWidth = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textWidth) * 0.5f);
    ImGui::TextUnformatted(text);
}

bool segmentButton(const char* label, bool active, const ImVec2& size) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::accent);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
    }
    const bool clicked = ImGui::Button(label, size);
    if (active) ImGui::PopStyleColor(4);
    return clicked;
}

bool labelValueRow(const char* label, const char* value, float labelWidth) {
    formLabel(label, labelWidth);
    ImGui::PushID(label);
    const bool hovered = readOnlyField("##value", value);
    ImGui::PopID();
    return hovered;
}

bool formSliderFloat(const char* id, const char* label, float& value, float min,
                     float max, const char* format, float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::SliderFloat("##value", &value, min, max,
                                            format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    return changed;
}

bool formSliderInt(const char* id, const char* label, int& value, int min,
                   int max, float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    ImGui::SetNextItemWidth(-1.0f);
    float dragged = static_cast<float>(value);
    const bool changed = ImGui::SliderFloat(
        "##value", &dragged, static_cast<float>(min), static_cast<float>(max),
        "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (changed) {
        const int rounded = std::clamp(
            static_cast<int>(std::lround(dragged)), min, max);
        if (rounded != value) {
            value = rounded;
            ImGui::PopID();
            return true;
        }
    }
    ImGui::PopID();
    return false;
}

bool formCheckbox(const char* id, const char* label, bool& value) {
    ImGui::PushID(id);
    const bool changed = ImGui::Checkbox(label, &value);
    ImGui::PopID();
    return changed;
}

bool formColor(const char* id, const char* label, std::string& hex,
               float labelWidth) {
    const auto parsed = core::parseHexColor(hex);
    float rgb[3] = {parsed ? (*parsed)[0] : 1.0f,
                    parsed ? (*parsed)[1] : 1.0f,
                    parsed ? (*parsed)[2] : 1.0f};
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::ColorEdit3(
        "##color", rgb,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    if (changed) {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
                      static_cast<int>(std::lround(
                          std::clamp(rgb[0], 0.0f, 1.0f) * 255.0f)),
                      static_cast<int>(std::lround(
                          std::clamp(rgb[1], 0.0f, 1.0f) * 255.0f)),
                      static_cast<int>(std::lround(
                          std::clamp(rgb[2], 0.0f, 1.0f) * 255.0f)));
        hex = buffer;
    }
    ImGui::PopID();
    return changed;
}

bool formCombo(const char* id, const char* label, int& index,
               const char* const* items, int count, float labelWidth) {
    ImGui::PushID(id);
    formLabel(label, labelWidth);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::Combo("##value", &index, items, count);
    ImGui::PopID();
    return changed;
}

DisabledScope::DisabledScope(bool disabled) : enabled_(disabled) {
    if (enabled_) ImGui::BeginDisabled();
}

DisabledScope::~DisabledScope() {
    if (enabled_) ImGui::EndDisabled();
}

bool readOnlyField(const char* id, const char* text, const ImVec2& size) {
    ImGuiStyle& style = ImGui::GetStyle();
    const float width =
        size.x > 0.0f ? size.x : ImGui::GetContentRegionAvail().x;
    const float height =
        size.y > 0.0f ? size.y : ImGui::GetFrameHeight();
    ImGui::InvisibleButton(id, ImVec2(width, height));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max, theme::u32(theme::panel2),
                        style.FrameRounding);
    draw->AddRect(min, max, theme::u32(theme::border), style.FrameRounding);
    draw->PushClipRect(
        ImVec2(min.x + style.FramePadding.x, min.y),
        ImVec2(max.x - style.FramePadding.x, max.y), true);
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    draw->AddText(
        ImVec2(min.x + style.FramePadding.x,
               min.y + (height - textSize.y) * 0.5f),
        theme::u32(theme::muted), text);
    draw->PopClipRect();
    return false;
}

bool vector3Control(const char* id, float values[3], const float mins[3],
                    const float maxs[3], float speed, const char* format) {
    ImGui::PushID(id);
    const char* axes[3] = {"X", "Y", "Z"};
    const ImVec4 colors[3] = {theme::axisX, theme::axisY, theme::axisZ};
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float width = std::max(
        24.0f, (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f);
    const float labelHeight = ImGui::GetTextLineHeight();
    const float fieldGap = ImGui::GetStyle().ItemSpacing.y * 0.4f;
    const float fieldHeight = ImGui::GetFrameHeight();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    bool changed = false;

    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        const float x = start.x + static_cast<float>(i) * (width + gap);

        ImGui::SetCursorScreenPos(ImVec2(x, start.y));
        ImGui::TextColored(colors[i], "%s", axes[i]);

        ImGui::SetCursorScreenPos(
            ImVec2(x, start.y + labelHeight + fieldGap));
        ImGui::SetNextItemWidth(width);
        changed |= ImGui::DragFloat("##axis", &values[i], speed, mins[i],
                                    maxs[i], format, ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(
        ImVec2(start.x, start.y + labelHeight + fieldGap + fieldHeight));
    // Submit a zero-size item so ImGui records the new cursor position as
    // content instead of treating it as an out-of-bounds cursor move.
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopID();
    return changed;
}

Card::Card(const char* id, float padding, const ImVec4& background,
           bool border, float indent) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    if (indent > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    }
    ImGuiChildFlags flags = ImGuiChildFlags_AutoResizeY |
                            ImGuiChildFlags_AlwaysUseWindowPadding;
    if (border) flags |= ImGuiChildFlags_Borders;
    indent_ = indent;
    open_ = ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), flags);
    if (open_ && indent_ > 0.0f) ImGui::Indent(indent_);
}

Card::~Card() {
    if (indent_ > 0.0f) ImGui::Unindent(indent_);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
Box::Box(const char* id, const ImVec2& size, float padding, bool border)
    : panel_(id, size, padding, border) {}

FontScale::FontScale(float factor) {
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * factor);
}

FontScale::~FontScale() {
    ImGui::PopFont();
}

Dialog::Dialog(const char* id, const ImVec2& size, float padding,
               ImGuiWindowFlags flags) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    // A background-list overlay is hidden by the opaque root window. Draw the
    // dim layer as a separate, non-interactive window so it sits above the main
    // UI but below the dialog created below.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin(
        "##dialog_overlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(0.0f, 0.0f), display, theme::u32(theme::modalDim));
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::panel);
    open_ = ImGui::Begin(id, nullptr,
                         flags | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings);
}

Dialog::~Dialog() {
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

} // namespace ui