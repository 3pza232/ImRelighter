# 架构与数据流

## 目标

ImRelighter 是单进程 Windows 桌面应用，核心约束是：

- 完全离线，图片与深度推理不离开本机。
- UI 线程不阻塞在模型加载、推理或 PNG 编码上。
- 渲染、推理、图像 I/O 之间保持单向依赖，避免 UI 细节渗入核心逻辑。
- 资源（任务、内存、GPU 纹理、模型）有明确的所有者和上限。

## 分层

```text
app  ──────────────► ui
 │                    │
 ├──► core ◄──────────┤
 ├──► io              │
 ├──► ml              │
 └──► gpu             │
```

| 模块 | 职责 | 不依赖 |
|---|---|---|
| `src/core` | `Document`、灯光与渲染参数、配置、i18n、颜色解析 | GL / ImGui / ONNX Runtime |
| `src/io` | 图片探测与解码、EXIF、WebP、PNG 导出、路径、哈希、原生对话框 | UI / 渲染 |
| `src/ml` | ONNX Runtime 动态加载、模型配置与元数据、预处理/后处理、推理线程 | 渲染 / UI |
| `src/gpu` | Shader、Texture、Renderer 与 5 个渲染 Pass | UI / 推理 |
| `src/app` | 生命周期、任务集合、模型选择、导出调度、状态栏 | — |
| `src/ui` | ImGui 布局、画布交互、右栏、设置、导出菜单 | — |

`core` 是共享数据层：`Document` 同时被 UI、渲染和推理使用。UI 不再维护一份影子设置；所有任务参数都直接读写 `Document`。

## 目录

```text
src/
  main.cpp
  app/
    app.cpp / app.h              生命周期与协调
    document_store.cpp / .h      任务集合、缩略图与 active 纹理
    model_manager.cpp / .h       模型路径、检查、SHA-256、DML
    export_service.cpp / .h      异步 PNG 编码
  core/
    document.h                   单任务状态与参数
    types.h                      灯光、视图、后端等基础类型
    config.cpp / .h              config.json
    i18n.cpp / .h                中英词典
    color.h                      #rrggbb 解析
  io/
    image_io.cpp / .h            图片解码入口
    image_probe.cpp / .h         PNG/JPEG 头尺寸探测
    image_limits.h               像素上限（默认 10 MP）
    exif.cpp / .h                JPEG EXIF 方向
    webp_decoder.cpp / .h        动态加载 libwebp
    png_export.cpp / .h          RGBA8 / 16 位 PNG
    hash.cpp / .h                SHA-256
    paths.cpp / .h               exe 目录、UTF-8 路径
    native_dialog.cpp / .h       打开文件对话框
  ml/
    onnx.cpp / .h                ORT 动态加载与会话
    inference.cpp / .h           后台推理与取消
    model_config.cpp / .h        preprocessor_config.json
    model_metadata.cpp / .h      权重精度统计
    preprocess.cpp / .h          resize / rescale / normalize
    half.h                       FP16 转换
  gpu/
    renderer.cpp / .h            Pass 编排、FBO、状态哈希
    shader.cpp / .h              GLSL 编译与 uniform
    texture.cpp / .h             纹理
  ui/
    ui.cpp                       顶层布局
    ui_internal.h                内部布局常量与共享声明
    ui_topbar / sidebar / canvas / inspector / settings / status / export
    ui_helpers.cpp               i18n 标签、颜色转换
    theme.cpp / .h               主题 token
    widgets.cpp / .h             表单与对话框组件
```

## 数据流

### 打开图片

```text
文件路径
  → io::loadImage
      → 扩展名判断
      → JPEG: 读取 EXIF Orientation
      → io::probeImageDimensions（解码前像素上限检查，默认 10 MP）
      → stb / libwebp 解码为 top-down RGBA8
      → 应用 EXIF 1~8 方向变换
      → 解码后再次检查像素上限
  → DocumentStore::add
      → 生成缩略图纹理
      → 上传 active 纹理
```

任务数量与总像素内存由 `DocumentStore::Limits` 控制，超限时返回可本地化的错误信息。

### 深度推理

```text
Document::color (top-down RGBA8)
  → ml::preprocess
      → 按配置 resize（nearest/bilinear/bicubic/lanczos 等）
      → rescale（默认 1/255）
      → normalize（ImageNet mean/std）
      → NCHW float32
  → InferenceService worker
      → OnnxSession::run（CPU 或 DirectML）
      → FP16 输入/输出自动转换
  → ml::postprocess
      → min-max 归一化
      → 翻转成 bottom-up 的 Document::depth
  → UI 线程取回结果
      → 仅在 document id 未过期时写回
      → 切换到光照视图
```

推理在独立线程执行。`InferenceService::cancel()` 先置取消标志，再在会话互斥锁内请求 ORT terminate；模型加载与释放也由同一把锁保护，避免取消与 `release()` 竞争。

### 渲染

```text
Document::depth (bottom-up)
  → Depth Pass        → 深度/高度纹理
  → Normal Pass       → 法线
  → Shadow Pass ×8    → MRT 可见性
  → Blur Pass         → 柔化阴影
  → Lighting Pass     → 预览或导出 FBO
```

`Renderer::renderPreview()` 用 `renderStateHash(doc)` 判断参数是否变化；哈希必须覆盖所有会影响画面的输入。新增渲染参数时，要同时更新 Pass 中的 uniform 和哈希。

预览使用受限分辨率，导出时按原图尺寸重新渲染并回读。

### 导出

```text
Renderer 全分辨率渲染 → glReadPixels RGBA8
  → 光照 / 法线 / 仅光照 / 阴影：io::writeRgba8Png
  → 深度：深度缓冲 → 16 位灰度 PNG
  → ExportService 在线程中编码，UI 轮询结果
```

导出期间按钮禁用；应用退出时等待未完成的导出，避免 `std::future` 析构阻塞在不确定的线程状态。

## 线程模型

| 线程 | 工作 |
|---|---|
| UI / 主线程 | GLFW、ImGui、OpenGL 渲染、图片解码、GL 回读 |
| 推理线程 | 模型加载、预处理、`Ort::Run`、后处理 |
| `std::async` | 模型检查、SHA-256、PNG 编码 |

跨线程数据通过值拷贝或 `std::future` 传递；`Document` 只在 UI 线程修改。取消路径由 `InferenceService` 的互斥锁和原子标志协调。

## 关键约定

- **方向**：图片解码后为 top-down；`Document::depth` 与 GL 上传使用 bottom-up；导出深度 PNG 时翻回 top-down。相关约定写在 `document.h`、`preprocess.cpp`、`png_export.cpp` 和 shader 注释中。
- **颜色**：十六进制颜色统一由 `core::parseHexColor` 解析，非法值由调用方决定回退。
- **路径**：内部统一 `std::filesystem::path`；与 ImGui/对话框交互时使用 UTF-8。字体与图标通过内存加载，支持中文和空格路径。
- **配置**：`config.json` 写入 exe 同级目录；先写临时文件再原子替换，失败会显示状态栏错误。
- **i18n**：所有 UI 文案来自 `assets/i18n/*.json`；新增键必须同步中英文，`tests/check_i18n_keys.py` 检查未使用键。
- **资源上限**：单图默认 10 MP，可在设置中调整为 4–100 MP；任务数默认 100（最大 500）；内存默认 4 GB（最大 16 GB）。

## 扩展指南

### 新增渲染参数

1. 在 `core::Document` / `core::Settings` 中增加字段。
2. 在对应 Pass 中设置 uniform。
3. 把字段加入 `renderStateHash`。
4. 如需持久化，更新 `config.cpp` 与 `document` 的序列化逻辑。
5. 若视觉结果应变化，更新 GPU 金图像。

### 新增 UI 文案

1. 同时在 `assets/i18n/en-US.json` 与 `zh-CN.json` 添加键。
2. 通过 `app.tr("key")` 或表单 helper 使用。
3. 运行 `ctest`，确认 i18n parity 与未使用键检查通过。

### 新增图片格式

1. 在 `io::loadImage` 中识别扩展名并探测尺寸。
2. 解码为 top-down RGBA8，必要时应用方向信息。
3. 复用 `exceedsPixelLimit`，不要绕过可配置的像素上限。
4. 增加 round-trip 或最小样本测试。
