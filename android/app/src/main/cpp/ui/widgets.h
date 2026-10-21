#pragma once

#include <imgui.h>

namespace imrelighter::ui {

// Bordered scrolling child with consistent padding.
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

// Auto-height card used by the mobile inspector.
class Card {
public:
    Card(const char* id, float padding, const ImVec4& background = ImVec4(0, 0, 0, 0));
    ~Card();
    Card(const Card&) = delete;
    Card& operator=(const Card&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_ = false;
    bool pushed_background_ = false;
};

class DisabledScope {
public:
    explicit DisabledScope(bool disabled = true);
    ~DisabledScope();
    DisabledScope(const DisabledScope&) = delete;
    DisabledScope& operator=(const DisabledScope&) = delete;
};

class FontScale {
public:
    explicit FontScale(float factor);
    ~FontScale();
    FontScale(const FontScale&) = delete;
    FontScale& operator=(const FontScale&) = delete;
};

bool centeredButton(const char* label, const ImVec2& size);
bool primaryButton(const char* label, const ImVec2& size);
bool iconButton(const char* id, const char* glyph, const ImVec2& size);
bool closeButton(const char* id, const ImVec2& size);

void sectionTitle(const char* title);
void formLabel(const char* label, float labelWidth);
void centeredText(const char* text);
void mutedText(const char* text);

bool segmentButton(const char* label, bool active, const ImVec2& size = ImVec2(0, 0));
bool readOnlyField(const char* id, const char* text, const ImVec2& size = ImVec2(0, 0));
bool labelValueRow(const char* label, const char* value, float labelWidth);

bool formSliderFloat(const char* id, const char* label, float& value, float min_value,
                     float max_value, const char* format, float labelWidth);
bool formSliderInt(const char* id, const char* label, int& value, int min_value,
                   int max_value, float labelWidth);

// Touch hit registration for mobile press-capture. Sliders register their
// draggable knob so a touch that starts on a knob is delivered immediately,
// while a touch that starts elsewhere stays a potential scroll gesture.
void clearTouchTargets();
void registerTouchTarget(const ImVec2& min, const ImVec2& max);
bool touchHitsTarget(float x, float y);

// Drop any ImGui active item and move the virtual pointer outside the
// viewport. A scroll gesture owns the touch from that point on, so widgets
// passing under the original press position must not show hover/active state.
void cancelActiveControl();
bool formCheckbox(const char* id, const char* label, bool& value);
bool formColor(const char* id, const char* label, float color[3], float labelWidth);
bool formCombo(const char* id, const char* label, int& index, const char* const* items,
               int count, float labelWidth);

}  // namespace imrelighter::ui
