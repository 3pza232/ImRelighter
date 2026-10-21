# Android Architecture Decisions

Design-phase ADRs. These records explain why the Android implementation is shaped this way. They will be replaced by formal documentation after implementation stabilizes.

## ADR-001 · Android code stays inside `android/`

**Status:** Accepted

**Context:** The desktop app is a Windows C++20/GLFW/OpenGL 4.5 application. Android needs a different host, build system, renderer backend, and release artifact. Mixing both under `src/` would make the desktop CMake graph and installer depend on Android-only tools.

**Decision:** All Android source, Gradle files, design documents, third-party copies, tests, and release artifacts live under `android/`. Desktop CMake, installer, and runtime never reference `android/`. Android may port or copy algorithms from desktop `src/`, but must not include desktop sources or link desktop targets directly.

**Consequences:** Some duplication is accepted initially. Shared algorithms can later be extracted into a platform-neutral library only after both implementations are stable and tests exist.

## ADR-002 · Thin Kotlin host + native renderer

**Status:** Accepted for production; NativeActivity was validation-only

**Context:** The validation probe used `NativeActivity` + EGL + ImGui. That was sufficient to prove GLES/ImGui/ORT, but Android file picking, sharing, lifecycle, and permissions are substantially easier in Kotlin.

**Decision:** The production app uses a thin Kotlin host (`Activity`/`SurfaceView`) and an EGL/GLES renderer in C++. Kotlin owns Activity lifecycle, SAF/URI access, intents, share sheet, and platform dialogs. C++ owns rendering, ImGui, document state, inference, and export. JNI surface stays narrow.

**Consequences:** No GLFW or SDL2 dependency in the MVP. NativeActivity remains useful for minimal probes, not the product shell. ImGui remains the UI toolkit, with touch input translated in C++.

## ADR-003 · q4 is the Android default model

**Status:** Accepted

**Context:** q4f16 is small but produced dense normal-map noise on an arm64 phone (V2243A, Adreno 740), while the x86_64 emulator was clean. FP32 fixed the noise but is 99 MB. q4 uses 4-bit weights with FP32 activations and was clean on the same phone.

**Decision:** Android defaults to `models/depth-anything-v2-small/q4/model_q4.onnx` (27,404,416 bytes). q4f16 remains a legacy/manual model type and is not the Android default. FP32 remains a quality fallback. The preprocessor config lives at `models/depth-anything-v2-small/preprocessor_config.json`.

**Consequences:** APK/model size is larger than q4f16 but much smaller than FP32. Android does not duplicate the model in git; Gradle syncs it from the root `models/` tree. FP16/NNAPI/Play Asset Delivery remain optimization work, not correctness requirements.

## ADR-004 · OpenGL ES 3.1+ and ImGui

**Status:** Accepted

**Context:** Desktop uses OpenGL 4.5, GLFW, and ImGui. Android lacks desktop GL and GLFW. The validation probe proved GLES 3.1 + ImGui + ORT on both x86_64 emulator and arm64 hardware.

**Decision:** Use OpenGL ES 3.1 as the minimum, with 3.2 capabilities when available. Use the existing ImGui widget model and `imgui_impl_opengl3` in ES3 mode. Do not introduce Vulkan or a second UI toolkit for the MVP.

**Consequences:** Shaders must be ported to GLSL ES; `glGetTexImage` is unavailable; float FBO support must be feature-detected with fallbacks. A future Vulkan renderer is possible behind the same renderer interface but is explicitly deferred.

## ADR-005 · Normals at depth resolution with CPU fallback

**Status:** Accepted for MVP

**Context:** Full-resolution central differences on bilinearly upscaled depth amplify model quantization noise and create silhouette artifacts. The validated path computes normals at the 518×714 model resolution after a 5×5 depth median, then upscales the normal map.

**Decision:** MVP normal generation follows the validated depth-resolution path. A GPU normal shader remains available for diagnostics/optimization. The renderer keeps a CPU normal fallback so a device-specific GL precision issue cannot make the app unusable.

**Consequences:** Slightly softer normal detail than a full-resolution desktop shader, but stable results on the tested arm64 device. GPU normal output must pass golden-image validation before becoming the default.

## ADR-006 · Models are synced, not duplicated

**Status:** Accepted

**Context:** The root repository already owns the q4 model and preprocessor config. Copying a second model into `android/app/src/main/assets` would create drift and repository bloat.

**Decision:** The Android Gradle build copies/syncs the q4 model and preprocessor config from the root `models/` tree into generated assets at build time. Source assets remain in the root tree.

**Consequences:** Android builds require the root model folder. CI must fetch LFS/large files before building. The APK still contains the model, but the repository stores one source copy.
