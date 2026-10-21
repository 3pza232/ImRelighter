# Android 架构设计

状态：Draft（验证阶段设计，正式文档在实现完成后重写）

## 1. 目标

在 `android/` 内建立独立、可构建、可测试的 Android 应用，复用 ImRelighter 的算法与交互理念，但**不直接依赖桌面源码**。Android 与桌面保持清晰的代码边界，允许算法按需移植。

## 2. 分层

```text
┌─────────────────────────────────────────┐
│ app (Kotlin/Java host, Activity, SAF)   │
│  - lifecycle, file picker, share, JNI   │
└──────────────────┬──────────────────────┘
                   │ JNI
┌──────────────────▼──────────────────────┐
│ native/app (C++ orchestration)          │
│  - AppState, task scheduling, settings  │
├───────────────┬───────────────┬─────────┤
│ native/core   │ native/ml     │ native/io│
│ document/i18n │ ORT/preprocess│ decode/  │
│ settings      │ inference     │ export   │
├───────────────┴───────────────┴─────────┤
│ native/gpu (GLES renderer, shaders)      │
│ native/ui  (ImGui widgets/screens)       │
└─────────────────────────────────────────┘
                   │
             third_party (ImGui, stb, ORT)
```

### 模块职责

- **native/app**：应用状态、打开/关闭文档、推理任务、导出任务、设置持久化、错误呈现。
- **native/core**：`Document`、`RenderSettings`、`ModelConfig`、i18n、类型与纯算法；不得包含 Android UI 或 OpenGL 调用。
- **native/io**：图片解码（stb_image + libwebp）、EXIF 方向、PNG 导出、Android asset/文件路径适配。
- **native/ml**：ONNX Runtime C++ API 封装、q4 模型加载、预处理、推理线程、深度后处理。
- **native/gpu**：GLES 上下文资源、纹理/FBO、shader、渲染 Pass、readback。
- **native/ui**：ImGui 组件、画布、检查器、设置、导出、触控映射。
- **app host**：Kotlin Activity/SurfaceView 或 NativeActivity；只做平台服务与 JNI，不承载业务算法。

## 3. 依赖规则

1. `native/core` 不得依赖 `gpu/ui/ml/io`。
2. `native/gpu` 不得直接调用 Android 文件 API；纹理输入由 `io/app` 提供。
3. `native/ml` 的输入输出使用普通 `std::vector` / POD，不暴露 ORT 类型到 UI。
4. 平台特定代码通过接口隔离：`PlatformFiles`、`ImageDecoder`、`InferenceSession`、`Renderer`。
5. 禁止 Android 代码 include 桌面 `src/`；需要复用时复制/重写并记录来源。
6. 第三方代码只放在 `android/third_party/`，业务代码不得直接 patch 上游文件。

## 4. 主数据流

```text
SAF picker -> Kotlin uri -> native read bytes
  -> decode RGBA + EXIF orientation
  -> create Document (color, width, height)
  -> preprocess 518x714 (or multiple of 14)
  -> ORT q4 inference (worker thread)
  -> depth resize + normalize + median
  -> generate normal map at depth resolution
  -> upload textures -> GLES lighting pass -> ImGui canvas
  -> export PNG via SAF create-document
```

推理与导出在后台任务执行；UI 线程只做状态切换、纹理上传和绘制。任务带 generation id，取消/新图后旧结果不得覆盖新文档。

## 5. 线程与生命周期

- **UI/render thread**：拥有 EGL 上下文、ImGui、所有 GL 资源。
- **inference thread**：只计算，不碰 GL；完成后通过线程安全队列交回 UI 线程上传纹理。
- **export thread**：读只读快照，写 PNG；不得持有 UI 锁。
- **生命周期**：`onResume` 创建/恢复 EGL；`onPause` 停止渲染并保存状态；surface 丢失时销毁 GL 资源，恢复时重建；推理任务可取消。
- 禁止在 GL 线程等待推理完成；显示 busy 状态和取消按钮。
- 内存上限：一次只保留当前文档、深度、法线、预览纹理；导出临时缓冲用后释放。

## 6. 状态与持久化

- `Document`：id、RGBA 原图、宽高、深度、法线、设置、修改状态。
- 设置：光参数、材质、视图、导出选项；使用 Android 私有目录 JSON，不依赖 exe 同级路径。
- 模型：APK assets 或首次启动复制到 `filesDir`；默认 `models/depth-anything-v2-small/q4/model_q4.onnx` 的构建期同步版本。
- 最近文件：仅保存 content URI 权限与显示名，不复制原图。
- i18n：与桌面 key 对齐，新增文案必须中英同步。

## 7. 错误与可观测性

- 统一 `Result<T, AppError>`；错误码 + 用户可读文案（i18n）+ 技术日志。
- 日志 tag：`ImRelighter`；关键节点：图片尺寸、模型路径/大小、推理耗时、GL 版本、FBO 完整性、导出路径。
- Debug 构建显示 GL/ORT/ABI/模型信息；Release 关闭详细日志。
- 崩溃/黑屏保护：模型缺失、FBO 不完整、上下文丢失时回退到明确错误页，不显示空白。

## 8. 关键接口（概念）

```cpp
class InferenceSession {
  virtual bool load(const ModelSource&, std::string& error) = 0;
  virtual bool run(const float* input, int w, int h,
                   std::vector<float>& depth, std::string& error) = 0;
};

class Renderer {
  virtual bool initialize(std::string& error) = 0;
  virtual void resize(int w, int h) = 0;
  virtual void render(const Document&, const View&) = 0;
  virtual bool readRgba(std::vector<uint8_t>&, std::string& error) = 0;
};

class PlatformFiles {
  virtual bool readUri(const std::string& uri, std::vector<uint8_t>&) = 0;
  virtual bool writePng(const std::string& uri, const uint8_t*, int, int) = 0;
};
```

## 9. 明确不采用

- 不在 Android 工程内引用桌面 `CMakeLists.txt`、GLFW、Win32、DirectML、Inno Setup。
- 不把 ImGui 替换为 Compose 作为 MVP 前提；先用 ImGui 验证画质与交互，再评估移动端 UI 重构。
- 不默认引入 SDL2；host 方案见 ADR-0002。
