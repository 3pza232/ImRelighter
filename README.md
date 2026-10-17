# ImRelighter

> 让一张图拥有可编辑的光。

Windows 本地的单图重打光工具：本地深度估计、OpenGL 实时光照、多视图预览与 PNG 导出。

- 左侧任务栏、中间画布、右侧属性区、顶栏、状态栏已搭好。
- 思源黑体 CN 中英文 UI、黑夜/白天主题、应用图标已完成。
- 设置面板已可用：语言（中/英）、主题、推理后端（Auto / CPU / DirectML）、模型文件选择、关于/许可证。
- **PNG / JPEG / WebP 图片加载、缩略图任务栏、多任务切换/关闭、滚轮缩放、中键平移、光源拖动** 已接入。
- **OpenGL 4.5 渲染管线**：深度平滑、法线、8 灯阴影 MRT、模糊、最终光照、7 视图、对比线。
- **本地 ONNX 深度推理**：ONNX Runtime C API、动态加载、CPU / DirectML、预处理/后处理、后台线程。
- 模型、`assets/`（含 `assets/shaders/`）、ONNX Runtime、DirectML、libwebp 会在构建时复制到产物目录。
- **PNG 导出**：光照、法线、仅光照、阴影 RGBA8；深度 16 位灰度 PNG；全分辨率渲染回读。
- **自动环绕、12 任务 / 2 GB 资源上限、GPU 金图像回归、DirectML、异步导出** 已接入。

## 构建

需要 MinGW-w64 GCC 13.x、CMake 3.24+、Ninja。

```powershell
cmake --preset mingw-debug
cmake --build --preset mingw-debug
```

产物：

```text
build/mingw-debug/bin/ImRelighter.exe
```

直接运行即可。Release/RelWithDebInfo：

```powershell
cmake --preset mingw-release
cmake --build --preset mingw-release
ctest --test-dir build/mingw-release --output-on-failure
```

性能基准（本机，512×512 合成图，模型 99 MB）：

```text
CPU  529.8 ms
DML   96.5 ms（首次 warmup 1313.6 ms）
```

GPU 金图像位于 `tests/golden/`；重新生成需在 build 目录放置 `update-golden.flag` 后运行 `imrelighter_gpu_tests`。DirectML 基准：`build/mingw-release/bin/imrelighter_inference_bench.exe`。

## 当前可用操作

- `Ctrl+O` 或「打开图片」：打开原生文件对话框，支持多选；每张图成为一个任务。
- 也可以把图片路径作为命令行参数：`ImRelighter.exe a.png b.jpg`。
- 点击任务缩略图切换；右上角 `×` 关闭任务。
- 滚轮缩放画布（以鼠标为中心）；中键拖动平移；左键拖动光源。
- 右侧属性区可调灯光位置/颜色/强度/范围、明暗、表面参数。
- `Ctrl+,` 或「设置」：打开设置面板；切换语言、查看模型状态。\n- `Ctrl+G`：生成深度与法线。
- `Ctrl+E` 或「导出」：导出当前任务的光照、法线、深度、仅光照、阴影 PNG。
- `Esc`：关闭设置面板。

## 目录

```text
assets/      字体、图标、i18n
docs/        项目文档（build/usage/models/testing/packaging）
extern/      GLAD / GLFW / ImGui / stb / nlohmann/json / ONNX Runtime
models/      本地 ONNX 模型（随构建复制）
src/         应用、核心、I/O、UI
tests/       core / GPU 单元测试（CTest）
```

## 说明

- 主题支持黑夜 / 白天，可在设置中切换并写入 `config.json`。
- AI 深度：右栏「生成深度与法线」或 `Ctrl+G`。
- 配置写入 exe 同级的 `config.json`；模型在 exe 同级的 `models/`，不写 AppData。
- 不联网、不上传图片、不下载模型。
- 模型：`models/depth-anything-v2-small/model.onnx`（Apache-2.0）。
- 第三方许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
- 项目文档见 [docs/README.md](docs/README.md)。
