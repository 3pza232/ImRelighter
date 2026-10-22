# Android Third-Party Dependencies

This directory vendors only dependencies required by the Android application.
Nothing here may depend on desktop-only code or build outputs.

## Contents

| Dependency | Version | Location | License |
|---|---|---|---|
| Dear ImGui | 1.92.5 | `imgui/` | `imgui/LICENSE.txt` |
| stb_image | bundled header | `stb/stb_image.h` | public domain / MIT |
| stb_image_resize2 | bundled header | `stb/stb_image_resize2.h` | public domain / MIT |
| nlohmann/json | bundled single header | `json/nlohmann/json.hpp` | `json/LICENSE.MIT` |
| ONNX Runtime Android | 1.20.0 | `onnxruntime-android-1.20.0/` | `onnxruntime-android-1.20.0/LICENSE` |

### ONNX Runtime native libraries

| ABI | File | Size | SHA-256 |
|---|---|---:|---|
| arm64-v8a | `jni/arm64-v8a/libonnxruntime.so` | 17,571,160 | `52329B7C8E5FCD5C1AA88B01093215FAAEF362E3F3D0E374CB8B10D1E4704677` |
| x86_64 | `jni/x86_64/libonnxruntime.so` | 20,290,904 | `047705AB42981A4884EEC435F60CC1A311E40414CD52FA2F252DA2B473756844` |

The ORT tree is extracted from `onnxruntime-android-1.20.0.aar`. It includes the
C/C++ headers and the native libraries for the two supported ABIs. The Java/Kotlin
classes are not vendored until the application needs a Java ORT API.

## Rules

- Do not patch third-party sources locally without an ADR. Prefer upstream fixes
  or a documented, minimal compatibility patch.
- Keep the license files next to the dependency.
- Do not add ABI libraries that the application does not package.
- Keep the dependency list small; Android already carries a large model.
- Update this file when a dependency version changes.

## Updating

1. Replace the dependency tree with the upstream release.
2. Update the table and hashes above.
3. Run the Android native build for both `arm64-v8a` and `x86_64`.
4. Re-run the device smoke test and inference benchmark.
5. Update `docs/design/06-build-test-release.md` if the toolchain changes.
