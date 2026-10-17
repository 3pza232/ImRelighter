#pragma once

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

// Three-axis vector editor with per-axis colors and consistent spacing.
bool vector3Control(const char* id, float values[3], const float mins[3],
                    const float maxs[3], float speed = 0.01f,
                    const char* format = "%.2f");

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