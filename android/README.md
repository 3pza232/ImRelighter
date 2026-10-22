# ImRelighter Android

Independent Android implementation of ImRelighter. All Android source, build
files, tests and release artifacts live under this directory. The desktop
project must not reference Android code, and Android must not include desktop
`src/` or desktop CMake targets.

## Status

The first functional MVP is implemented and the production-oriented portrait UI
is now in place. The APK contains both `arm64-v8a` and `x86_64`; the current
portrait build has been exercised on the x86_64 emulator, while earlier builds
were validated on an arm64 physical phone. Physical startup issues were
resolved with 16 KB page-aligned native libraries, a static C++ runtime, a
bundled Source Han Sans font, and validated system-font fallbacks.

Implemented:

- Kotlin activity + `SurfaceView` host, native C++ EGL/OpenGL ES render thread.
- Dear ImGui mobile UI with the desktop dark slate/amber design language.
- Portrait layout: top bar, default-collapsed left `任务` rail, optional task
  drawer with bottom import action, upper canvas, view strip, lower inspector.
- Empty canvas state with a centered gallery import action.
- Six view modes: `原图 / 光照 / 法线 / 深度 / 阴影 / 仅光照`.
- Desktop-style lower inspector cards for image/depth status, lighting,
  material, shadow controls, model selection, and reset.
- Top/bottom bars; the bottom bar uses a larger wrapped status font so long
  image names and error messages can occupy two lines.
- Android document picker (`ACTION_OPEN_DOCUMENT`) with high-resolution downscaling and SurfaceView recreation recovery.
- q4 depth inference via ONNX Runtime Android 1.20.0, CPU provider.
- Depth and normal previews copied from the validated desktop-equivalent
  pipeline: 5x5 depth median, min/max normalization, resize to source size,
  central-difference normals at depth resolution, then upscale.
- Model settings with **q4** as the bundled default, **恢复默认 Q4**, persisted
  selection, multiple custom ONNX models, and a bounded scrollable model list.
  Users choose a model storage folder via SAF; the app keeps read permission,
  mirrors all ONNX/config files into app storage, and displays each custom model
  by its parent folder name. Model types are detected from the exported variant
  name plus ONNX initializer data types (q4/q4f16/fp16/fp32/int8/bf16), and
  selecting a model keeps Settings open.
- The phone status bar is visible; the native top bar is shifted below its inset
  and the app bottom bar is compacted to 52dp to preserve canvas space.
- English mode covers native status text, model descriptions, settings labels,
  errors, busy indicators, export menu and Kotlin toasts.
- Preprocessing follows the desktop JSON processor semantics (`size`,
  `ensure_multiple_of`, resize/filter, rescale, normalize, mean/std and
  `output.invert`); q4 reads the bundled top-level config.
- After a successful manual inference, the first result switches an Original
  view to Lit; an explicitly selected other view is preserved.
- Bundled Source Han Sans CJK font, with validated system-font fallbacks.
- Single-finger canvas pan, pinch zoom, light-marker dragging, thumb-only sliders,
  press-capture scrolling with cancellation, and basic inertial scrolling.

## Stability Pass (2026-10-22)

- The render thread and `AndroidApp` now live for the whole process. Home,
  the document picker and other surface losses detach only the `EGLSurface`;
  the `EGLContext`, textures, ImGui context, tasks, depth, selected material,
  lighting and scroll position are retained. A pbuffer keeps the context valid
  while no window is attached. Force-stop / finishing the activity still starts
  a clean session.
- Removed the deep-copy task snapshot/restore path. Surface recreation no longer
  copies all images/depth or rebuilds the model; this removes the black gap and
  the high-memory path that could crash with many tasks.
- Image import decodes on a worker thread and passes the `Bitmap` to native code
  through `nativeOpenBitmap` (`AndroidBitmap_lockPixels`), eliminating the
  per-pixel Kotlin ARGB-to-RGBA loop and the large temporary JVM arrays.
- Task drawer rows have disjoint activate/close hit rectangles (the close button
  is no longer underneath the row button), the list scrolls with the drawer, and
  deletions are deferred until after the current frame.
- Task limit defaults to 8 and is configurable in Settings → 通用 → 任务.
  At the limit, import is blocked before opening the picker and the status
  message explains how to continue.
- Lower-panel scrolling uses a monotonic release velocity and no longer applies
  a stale scroll target after release. Settings scroll is isolated to the modal.
- Slider knobs have a matching hit/accept area, the color control opens an inline
  picker that registers as a direct touch target, and the ambient/light color
  picker no longer scrolls the panel behind it.
- Light rows use the light color as their background with luminance-based
  inverted text and a selection border; canvas marker numerals use the same
  contrast rule.
- The depth generation button is a primary amber button; the obsolete
  “性能与交互” block was removed from Settings.
- Final interaction polish: scroll gestures clear any active ImGui control so
  widgets passing under the finger do not show hover/active state; scrollbars
  are a subtle 6dp rounded track; Settings is auto-height with 12dp corners and
  no scrollbar; the bottom status text is larger and wraps to two lines.

Validated on the x86_64 SwiftShader emulator: import/recovery, Home/resume with
identical pre/post screenshots, task deletion, 8-task drawer scroll, slider drag,
inline color picker, settings isolation, and primary button behavior. Physical
arm64 regression remains required.

## Directory Layout

```text
android/
  README.md
  build.gradle.kts
  settings.gradle.kts
  gradle.properties
  gradlew / gradlew.bat
  app/
    build.gradle.kts
    src/main/
      AndroidManifest.xml
      java/com/imrelighter/android/MainActivity.kt
      cpp/
        app/        AndroidApp: lifecycle, UI, textures, lighting
        core/       shared types and model catalog
        io/         stb image loading, asset access
        ml/         preprocessing, depth processing, inference service
        platform/   JNI/EGL bridge
        ui/         theme, bundled font loading, reusable widget kit
      assets/      generated models and bundled resources
      res/          manifest resources
  docs/design/      design notes and ADRs
  third_party/      ImGui, stb, ONNX Runtime Android
```

Runtime models are synchronized from the repository root by the Gradle
`syncModels` task; they are not stored in this directory:

- `models/depth-anything-v2-small/q4/model_q4.onnx` (bundled default)
- `models/depth-anything-v2-small/preprocessor_config.json`

## Build

```powershell
cd C:\Users\86136\Desktop\output\ImRelighter\android
$env:GRADLE_USER_HOME = 'D:\Android\.gradle'
.\gradlew.bat assembleDebug --offline --no-daemon
```

Toolchain: JDK 17, Gradle 9.1.0, AGP 9.0.1, compile/target SDK 36, minSdk 26,
NDK 26.3.11579264, CMake 3.22.1, C++20, GLES 3.1.

Output: `app/build/outputs/apk/debug/app-debug.apk`. The current debug APK is
about 110 MB because it packages the Q4 model, ONNX Runtime and the bundled
Source Han Sans font; release work should split ABIs and subset the font.

## Run

```powershell
adb install -r app\build\outputs\apk\debug\app-debug.apk
adb shell am start -n com.imrelighter.android/.MainActivity
adb logcat -s ImRelighterAndroid AndroidRuntime
```

On the local emulator, use SwiftShader if the host-GPU ANGLE backend stalls:

```powershell
D:\Android\SDK\emulator\emulator.exe -avd device01 `
  -gpu swiftshader_indirect -no-window -no-audio -no-boot-anim `
  -wipe-data -no-snapshot
```

## Verified Results

Emulator: x86_64 API 34, SwiftShader, 1784x1080 surface.

- q4 model: 27,404,416 bytes; inference ~5,856 ms on SwiftShader; clean normal.
- Q4 is the only bundled Android model; restore-default Q4 works and persists.
- Multiple tasks are supported with thumbnails, per-task depth/lights/view.
  Tasks are session-only: Home/resume with the process alive preserves them,
  while a fresh process starts clean.
- Dark/light themes and Chinese/English language selection persist across restarts.
- Import from the system document picker now survives the SurfaceView/activity
  recreation caused by the picker.
- Export to the system gallery works for 光照 / 法线 / 深度 / 阴影 / 仅光照.
- APK includes arm64-v8a and x86_64 native libraries.

The emulator is much slower than the tested arm64 phone: validation on the
physical phone previously measured q4 at about 1.95 s. Physical production-app
benchmarking is still pending.

## Device Compatibility

- Native libraries are linked with 16 KB page alignment and the C++ runtime is
  linked statically. `libimrelighter.so` and `libonnxruntime.so` both show
  `LOAD` alignment `0x4000` on arm64.
- The APK no longer packages `libc++_shared.so`.
- The launcher uses the desktop ImRelighter artwork, with legacy and adaptive
  launcher icons.
- Startup library loading is wrapped so an `UnsatisfiedLinkError` shows a
  readable error instead of a silent crash.
- Font loading validates the SFNT table directory before passing a system font
  to ImGui. Vendor builds often ship `/system/fonts/NotoSansCJK-Regular.ttc` as
  a CFF/OTTO collection, which stb_truetype cannot parse; those files are
  skipped in favor of a TrueType CJK fallback.

## Known Issues

- The shadow view is currently a normal/contact-based preview, not a true
  light-space shadow map. `光照` remains somewhat flatter than the desktop
  physical renderer.
- Normal maps show a small amount of silhouette fringe on the q4 model.
- Status text may log `就绪` while the background inference is still running;
  the busy overlay remains the authoritative state.
- True desktop-parity height-field shadows and full desktop settings parity are
  still pending; current shadow view remains an approximation.
- The portrait build has been exercised on the x86_64 emulator; the previous
  landscape APK was validated on an arm64 phone. Re-test the portrait APK on
  physical hardware before release.
- The debug APK is large because it includes the Q4 model and the bundled CJK
  font; release packaging should use ABI splits and a subset font.

## Boundaries

- Desktop `CMakeLists.txt`, `src/`, `installer/`, and release scripts are not
  modified by Android work.
- Android third-party code lives under `third_party/`; see its README for
  versions, hashes, and update rules.
- Design docs are working documents and may be replaced by final user-facing
  documentation after the Android release stabilizes.
