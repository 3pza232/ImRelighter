#pragma once

#include <string>

#include "imgui.h"

namespace ui {

// RAII wrapper around BeginChild/EndChild with explicit content padding.
class Panel {
public:
    Panel(const char* id, const ImVec2& size, float padding,
          bool border = false, ImGuiWindowFlags flags = 0);
    ~Panel();

    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    explicit operator bool() const { return open_; }

private:
    bool open_ = false;
};

// Button whose label is vertically centered when an explicit height is used.
bool centeredButton(const char* label, const ImVec2& size);

// Centered glyph button used by the sidebar and other compact controls.
bool iconButton(const char* id, const char* glyph, const ImVec2& size);

// Full-height danger button used for task removal / dialog close.
bool closeButton(const char* id, const ImVec2& size);

// Shared text / form primitives.
void sectionTitle(const char* title);
void formLabel(const char* label, float labelWidth);
void centeredText(const char* text);

// Segmented choice control used by settings and view options.
bool segmentButton(const char* label, bool active,
                   const ImVec2& size = ImVec2(0.0f, 0.0f));

// Read-only value field with an input-like frame. It deliberately has no
// combo arrow: a single-item or disabled choice should look like a value,
// not like a control the user can open.
bool readOnlyField(const char* id, const char* text,
                   const ImVec2& size = ImVec2(0.0f, 0.0f));

// Reusable form primitives. Labels are passed in (already localized) and the
// value widget uses the full remaining width, matching the inspector layout.
bool labelValueRow(const char* label, const char* value, float labelWidth);
bool formSliderFloat(const char* id, const char* label, float& value, float min,
                     float max, const char* format, float labelWidth);
bool formSliderInt(const char* id, const char* label, int& value, int min,
                   int max, float labelWidth);
bool formCheckbox(const char* id, const char* label, bool& value);
bool formColor(const char* id, const char* label, std::string& hex,
               float labelWidth);
bool formCombo(const char* id, const char* label, int& index,
               const char* const* items, int count, float labelWidth);

// RAII disabled-state scope. Useful for read-only property groups without
// repeating BeginDisabled/EndDisabled on every control.
class DisabledScope {
public:
    explicit DisabledScope(bool disabled = true);
    ~DisabledScope();

    DisabledScope(const DisabledScope&) = delete;
    DisabledScope& operator=(const DisabledScope&) = delete;

private:
    bool enabled_ = false;
};

// Three-axis vector editor with per-axis colors and consistent spacing.
bool vector3Control(const char* id, float values[3], const float mins[3],
                    const float maxs[3], float speed = 0.01f,
                    const char* format = "%.2f");

// Auto-height card used by the right inspector hierarchy.
class Card {
public:
    Card(const char* id, float padding, const ImVec4& background,
         bool border = true, float indent = 0.0f);
    ~Card();

    Card(const Card&) = delete;
    Card& operator=(const Card&) = delete;

    explicit operator bool() const { return open_; }

private:
    bool open_ = false;
    float indent_ = 0.0f;
};

// Bordered content box for grouping related fields.
class Box {
public:
    Box(const char* id, const ImVec2& size, float padding = 10.0f,
        bool border = true);

    explicit operator bool() const { return static_cast<bool>(panel_); }

private:
    Panel panel_;
};

// Temporarily scales the current font; restores it at end of scope.
class FontScale {
public:
    explicit FontScale(float factor);
    ~FontScale();

    FontScale(const FontScale&) = delete;
    FontScale& operator=(const FontScale&) = delete;
};

// Floating dialog with dimmed backdrop, centered placement and body padding.
class Dialog {
public:
    Dialog(const char* id, const ImVec2& size, float padding = 20.0f,
           ImGuiWindowFlags flags = 0);
    ~Dialog();

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;

    explicit operator bool() const { return open_; }

private:
    bool open_ = false;
};

} // namespace ui