# Android UI Kit

The Android UI is built on Dear ImGui, but screen code does not use raw ImGui
controls for common patterns. Reusable pieces live in
`app/src/main/cpp/ui/widgets.h/.cpp` and `ui/theme.h/.cpp`.

## Components

- `Panel`: bordered scrolling child with consistent padding.
- `Card`: auto-height inspector card.
- `DisabledScope`: scoped disabled state.
- `FontScale`: temporary relative font size.
- `centeredButton` / `primaryButton` / `iconButton` / `closeButton`.
- `sectionTitle`, `formLabel`, `centeredText`, `mutedText`.
- `segmentButton`, `readOnlyField`, `labelValueRow`.
- `formSliderFloat`, `formSliderInt`: thumb-only sliders. Tapping the track does
  not change the value; dragging starts only on the knob.
- `formCheckbox`, `formColor`, `formCombo`.
- `clearTouchTargets`, `registerTouchTarget`, `touchHitsTarget`: per-frame hit
  registry used by the press-capture touch router.
- `cancelActiveControl`: clears ImGui's active item and parks the virtual pointer
  outside the viewport when a scroll gesture takes ownership.

## Touch contract

1. A touch that starts on a registered direct control (slider knob or canvas)
   is delivered to ImGui immediately.
2. A touch that starts elsewhere is a potential scroll gesture. No mouse-down
   is sent, so buttons crossed during a scroll do not flash or activate.
3. Once movement exceeds the slop threshold, `cancelActiveControl()` is called;
   the previous slider loses hover/active styling and only the scroll host moves.
4. A tap activates on release inside, not on press.
5. The scroll host keeps ownership until release, including when the finger
   crosses into the canvas. It is cancelled only if the gesture leaves its
   originating panel.

## Scroll and modal rules

- Scrollbars are 6dp with a subtle translucent grab, rounded ends, and an accent
  active state; they remain draggable.
- Settings is an auto-height modal with 12dp rounding and no scrollbar. Its
  content must stay compact.
- While Settings is open, lower-panel and canvas gestures are disabled; a tap on
  the dimmed backdrop closes Settings.

## Theme

`ui::palette()` is the single color source. Widgets must not hard-code dark-theme
colors. `ui::setThemeMode` switches the dark/light palette; `ui::tr` supplies
Chinese/English labels. Theme and language are persisted by the app layer.

## Current boundary

The component layer is reusable, but screen composition still lives in
`AndroidApp::draw_*`. A future cleanup can extract a `SettingsDialog` shell and
separate scroll hosts from the app class; until then, new screens should use the
kit and follow the touch contract rather than adding raw ImGui controls.
