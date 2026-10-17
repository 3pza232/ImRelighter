# ImRelighter · third-party dependencies

本目录只保留 Windows + MinGW-w64 构建所需的第三方源码/二进制。所有版本固定，不随系统更新漂移。

> 目标工具链：MinGW-w64 GCC 13.x、CMake、Ninja。当前不保证 MSVC / macOS / Linux 构建。

| 目录 | 版本 / 来源 | 许可证 | 用途 | 备注 |
|---|---|---|---|---|
| `glad/` | glad 0.1.36，生成于 2026-10-17，`gl=4.5 core` | MIT（生成器） | OpenGL 4.5 函数加载 | 仅 `include/` + `src/glad.c`；使用 `gladLoadGLLoader()`；未生成扩展，`KHR_debug` 已包含在 4.3+ core 中 |
| `glfw/` | GLFW 3.5.1，MinGW-w64 预编译包 | zlib/libpng | 窗口、输入、GL 上下文 | 只保留 `include/`、`lib-mingw-w64/`、LICENSE、README；静态库为 `libglfw3.a`，动态库为 `glfw3.dll` |
| `imgui/` | Dear ImGui 1.92.5（master，非 docking 分支） | MIT | 全部 UI | 只保留核心源码、`backends/glfw`、`backends/opengl3`、`misc/cpp/imgui_stdlib`；|
| `stb/` | stb 快照（vendored） | MIT / public domain | PNG/JPEG 解码、PNG 写入、图像缩放 | 只保留 `stb_image.h`、`stb_image_write.h`、`stb_image_resize2.h` |
| `json/` | nlohmann/json 3.11.3（单头） | MIT | 语言、配置、模型 manifest | `extern/json/nlohmann/json.hpp` |
| `libwebp/` | libwebp 1.5.x x64 + libsharpyuv | BSD-3 | WebP 解码 | `libwebp.dll`、`libsharpyuv.dll` 随构建复制 |
| `directml/` | DirectML 1.15.4 x64 | Microsoft 许可 | DirectML 推理 | `DirectML.dll` 随构建复制；CPU 不依赖 |
| `onnxruntime/` | ONNX Runtime 1.20.1 x64 DirectML build | MIT | 深度推理 | 动态加载 C API；CPU + DirectML EP |

## 许可证与再分发

- 各目录保留上游 LICENSE 文件；发布时在根目录 `THIRD_PARTY_NOTICES.md` 汇总。
- `glad` 生成代码本身没有附带 LICENSE 文件；在 notices 中注明 glad 0.1.36 / MIT。
- 模型权重放在 `models/depth-anything-v2-small/`，随项目/构建产物分发；保留其 Apache-2.0 LICENSE 与 manifest。
