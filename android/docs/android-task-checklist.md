# Android task checklist

Status: active implementation backlog. Keep this file updated so later sessions do
not lose requirements.

## Current build

- Source: `android/`
- APK: `android/dist/ImRelighterAndroid-inset-model-delete.apk`
- SHA-256: `2B4A81FF98A8830B668386716D6868F3B2B0CF9F4984187F99F7C0618C230886`
- Build: `$env:GRADLE_USER_HOME='D:\Android\.gradle'; .\gradlew.bat assembleDebug --offline --no-daemon`

## Fixes in the current build

- Process-scoped render thread/AndroidApp and EGL context with pbuffer:
  Home/resume detaches only the window surface; tasks, depth, material preset,
  lighting, settings and scroll position survive without serialization.
- Removed deep-copy task snapshot/restore (previous black gap and memory spike).
- Import decode off the UI thread; native `AndroidBitmap_lockPixels` copy.
- Task drawer close hit rect is disjoint from row activation; drawer list scrolls.
- Task cap default 8, configurable in Settings; import blocked at cap with status.
- Monotonic lower-panel fling; settings scroll isolated to modal.
- Slider knob hit/accept areas aligned; inline color picker registered as direct
  touch target; no background scrolling while adjusting color.
- Light rows use light color background + contrast text; marker numerals match.
- Primary amber generate button; obsolete 性能与交互 settings section removed.

## User requirements

| # | Requirement | Status |
| --- | --- | --- |
| 1 | Theme and language switching in Settings | Implemented and persisted (`theme.txt`, `language.txt`); main UI translated, status strings still mostly Chinese |
| 2 | Lower-panel sliders must drag by grabbing the knob | Implemented; emulator-tested with ambient slider |
| 3 | Lighting/normal/depth/shadow/lighting-only must follow desktop algorithms | Partial; multi-light shader exists, true height-field shadow pass not ported |
| 4 | Export must not be vertically flipped and must show success feedback | Implemented; verified export path previously, needs final orientation regression |
| 5 | Only manual "generate depth & normals"; no auto inference after install/resume | Implemented; tasks restore without auto inference |
| 6 | Multi-task list like desktop; importing a new image must not replace the previous task | Implemented: task vector, thumbnails, switching, per-task depth/lights/material/view; close button and 8-task scroll validated on emulator; session-only |
| 7 | Settings layout and controls close to desktop | Partial; General/Model/About, theme/language, model picker, restore Q4 |
| 8 | Light list styling and vertical centering of "灯光 1" | Implemented: light-color row background, luminance contrast text, selection border, centered labels; marker numerals use same contrast rule |
| 9 | Remove explanatory paragraphs at bottom of General/About; close Settings on outside tap | Implemented |
| 10 | General tab aligned with desktop options | Partial |
| 11 | Do not build/deliver through `C:\Users\86136\Desktop\output\test` | Implemented: use `android/dist` |
| 12 | Scrollbar must be easy to drag | Reworked: 6dp subtle rounded scrollbar, grab 18dp; task drawer list scrolls with drag |
| 13 | Add scroll inertia/smooth scrolling | Monotonic release velocity, decay and clamping for lower panel/settings; no sign reversal/target feedback |

## Latest interaction work

- Press-capture touch handling:
  - direct control (slider knob/canvas) keeps normal press/drag;
  - scroll starts only after a movement threshold;
  - scrolling does not send pointer movement to crossed buttons;
  - tapping a button activates on release inside;
  - settings scroll is routed to the modal and never to the page behind;
  - a lower-panel drag keeps tracking after the finger crosses into the canvas, until release.
- Slider knob hit targets registered in `ui/widgets.*`.
- Multi-task session state:
  - tasks (image, depth, lights, lighting, view, zoom/pan) live in process memory only;
  - Home/resume with the process alive restores the active task and generated data;
  - process death / force-stop starts a clean empty session; legacy `tasks.bin` is deleted;
  - JNI retains only a pending imported image that arrives during surface recreation;
  - inference is still manual only.
- Task rail restored to a compact 44dp width; toggle, 任务, count and + share the
  same center line.
- Theme: dark/light palettes, widget colors read from palette, light theme fixed
  for light list/selectable rows.
- Language: `zh/en` toggle and persisted language; top bar, view tabs, task rail,
  lower-panel controls, Settings tabs and major labels translated.
- Model handling: custom model names come from the parent folder; multiple
  custom models persist; the model list is bounded and scrollable; Settings
  shows a short relative path and custom type instead of raw ids.
- Model storage: SAF folder picker persists read permission and mirrors every
  ONNX/config file in the chosen tree; the folder name is shown in Settings.
- System UI: the phone status bar is visible, content is inset below it, and the
  app bottom status bar is reduced to 52dp.
- English mode: native statuses, busy/error/export strings, model descriptions,
  settings labels and Kotlin toasts are localized; legacy Chinese statuses are
  translated when the language is switched at runtime.
- Preprocessing: `preprocessor_config.json` is resolved from the model folder,
  then its parent, for both bundled assets and local custom models, using the
  same JSON semantics as desktop.
- After a successful manual inference, an Original view switches to Lit while
  an explicitly selected other view is preserved.

## Verification needed

- Physical arm64 phone: final regression of multi-task switching, task restore,
  slider drag, scroll inertia, theme/language persistence, export orientation,
  Settings outside-tap, custom ONNX file selection.
- Emulator only verifies layout; do not treat it as final device validation.

## Next implementation order

1. Desktop lighting/shadow parity, including true height-field shadow pass.
2. Finish language coverage for status messages and remaining labels.
3. Settings parity and remaining desktop parameters.
4. Verify final arm64 device build and removal of any stale installed task store.
5. Optional NNAPI/provider validation and arm64 performance profiling.
