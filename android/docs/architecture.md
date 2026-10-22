# 架构与数据流

## 目标

ImRelighter Android 是一个单进程、单 Activity 的本地应用，核心约束：

- 图片、深度和模型不离开设备。
- UI 与推理不在渲染线程阻塞；深度推理在独立工作线程执行。
- GPU 资源、任务状态和参数有明确所有者，任务切换不能串数据。
- 触摸输入、渲染和文件选择器在 Activity 重建/Surface 重建后保持可用。

## 分层

```text
MainActivity.kt ──JNI──► AndroidApp
                         ├─ ui/ (Dear ImGui)
                         ├─ io/ (图片解码)
                         ├─ ml/ (ONNX Runtime + 预处理)
                         ├─ core/ (类型/设置/模型)
                         └─ platform/ (EGL/GLES)
```

| 层 | 目录 | 职责 |
|---|---|---|
| Kotlin 宿主 | `app/src/main/java/com/imrelighter/android` | Activity、SurfaceView、系统栏、SAF 文件选择、相册导出、JNI 声明 |
| 应用层 | `app/src/main/cpp/app` | `AndroidApp` 生命周期、渲染循环、ImGui 布局、任务与参数、Pass 调度 |
| 核心数据 | `app/src/main/cpp/core` | `ViewMode`、`Light`、`LightingSettings`、`DepthResult`、模型目录 |
| 推理 | `app/src/main/cpp/ml` | ONNX Runtime、模型配置、预处理、深度归一化/缩放 |
| 图像 I/O | `app/src/main/cpp/io` | stb_image 解码、Asset 读取 |
| 平台 | `app/src/main/cpp/platform` | JNI 桥、EGL 上下文、Surface 生命周期 |
| UI 工具 | `app/src/main/cpp/ui` | 主题、字体、`widgets.*` 可复用控件与触摸目标注册 |

## 线程模型

- **渲染线程**：持有 EGL Context、全部 OpenGL 对象和 `AndroidApp` 的 UI/任务状态。每帧执行 `render()`：处理队列 → 轮询推理结果 → ImGui NewFrame/Draw → GL 绘制。
- **推理线程**：由 `InferenceService` 管理工作线程；结果通过线程安全队列交回渲染线程。回调只写结果，不直接碰 GL。
- **Kotlin 主线程**：处理 Activity、系统栏、SAF 选择器和 MediaStore；图片解码在后台线程完成后通过 JNI 排队给原生层。
- **Surface 生命周期**：Activity 使用常驻 `SurfaceView`；Surface 销毁时仅 detach `EGLSurface`，Context 和资源保留；重建时重新 attach。任务与 GPU 资源不因 Home/返回而丢失。

## 任务模型

`AndroidApp` 内维护 `std::vector<Task>`：

```cpp
struct Task {
    ImageData image;
    DepthResult depth;
    std::vector<Light> lights;
    LightingSettings lighting;
    ViewMode view;
    float canvas_zoom, canvas_pan_x, canvas_pan_y;
    GLuint thumbnail_texture;
    bool depth_ready, busy;
    uint64_t generation;
};
```

活动任务直接使用 `image_`、`depth_`、`lights_`、`lighting_` 等成员；`store_active_task()` 把当前状态写回 `Task`，`activate_task()` 恢复另一个任务。切换任务时：

1. 保存旧任务。
2. 复制新任务的图片/深度/灯光/参数/视图/缩放。
3. 上传该任务的原图/深度纹理。
4. 将 `height_dirty_`、`normal_dirty_`、`shadow_dirty_`、`render_dirty_` 全部置位，确保共享 GPU 高度/法线/阴影/光照纹理不会残留上一个任务的数据。

任务只存在进程内存；进程死亡后从空状态开始。

## 渲染管线

```text
原图 (RGBA8, top-down)
  │
  ├─► depth_smooth.frag (R32F height FBO)
  │        uLow/uSpan, uSmooth, uInvert, uLineAssist
  │
  ├─► normal.frag (RGBA16F / fallback RGBA8)
  │        uAmplitude, uNormalStrength, uDetailStrength,
  │        uDenoise, uFlips
  │
  ├─► shadow.frag ×2 (RGBA8, lights 0–3 / 4–7)
  │        ray-march + uSteps/uSamples/uBias/uSoftness
  │        └─► shadow_blur.frag → blur textures
  │
  └─► lighting.frag (RGBA8 preview / full-res export)
           classic / physical, point / spot, soft / ACES,
           Lit / Normal / Depth / Shadow / Lighting views
```

### 渲染目标

| 目标 | 格式 | 说明 |
|---|---|---|
| `depth_source_texture_` | R32F | 归一化深度输入 |
| `height_texture_` | R32F | 高度场，深度 Pass 输出 |
| `normal_texture_` | RGBA16F（回退 RGBA8） | 法线 + alpha 边缘遮罩 |
| `shadow_texture_[2]` | RGBA8 | 原始光线步进可见性，每张 4 盏灯 |
| `blur_texture_[2]` | RGBA8 | 边缘感知模糊后的可见性 |
| `lit_texture_` | RGBA8 | 预览/导出结果 |

## 脏状态与性能

所有 Pass 都有独立脏标记：

- `height_dirty_`：平滑、反转、线条辅助、自动/手动深度范围。
- `normal_dirty_`：高度变化，或法线强度、细节、深度幅度、翻转。
- `shadow_dirty_`：高度变化、阴影参数、灯光位置/启用状态。
- `render_dirty_`：视图、灯光、材质或任一 `LightingSettings` 变化。

`draw_canvas` 只有 `render_dirty_` 时才调用 `render_lighting()`；静止画面不再重复执行 GPU Pass。拖动参数时：

- 高度使用 `uInteractive` 的 3x3 快速核，保持平滑/反转/线条辅助即时反馈；
- 法线/光照即时更新；
- 阴影光线步进保持全步数、采样数减半实时刷新；松手后恢复完整采样。

`preview_long_edge` 仅限制显示用的光照目标尺寸；导出调用 `render_lighting(true)`，按原图分辨率渲染后再读取像素。

## 法线与阴影方向约定

- Android 的原图和深度均为 top-down；`depth_smooth` 不做桌面版的 V 翻转。
- `lighting.frag` 直接使用 `normalUv = vUv` 采样法线；不要重新引入 `1.0 - vUv.y`，否则法线上下会再次反转。
- 桌面法线目标是 bottom-up，因此桌面 shader 的翻转逻辑不能原样复制到 Android。
- 阴影 Pass 使用与法线一致的 top-down UV；边缘模糊同时参考原图 alpha 和高度差。

## 关键文件

| 文件 | 作用 |
|---|---|
| `android_app.cpp/.h` | 应用、UI、任务、FBO、Pass 调度 |
| `depth_shader.h` | `depth_smooth.frag` GLES 移植 |
| `normal_shader.h` | `normal.frag` GLES 移植 |
| `shadow_shader.h` | 光线步进与边缘感知模糊 |
| `lighting_shader.h` | `lighting.frag` GLES 移植 |
| `core/types.h` | 视图/灯光/参数默认值 |
| `ml/inference_service.cpp` | ORT 推理与深度结果 |
| `ml/depth_processing.cpp` | 归一化、缩放、方向处理 |
| `platform/jni_bridge.cpp` | Kotlin 与原生层接口 |
| `ui/widgets.*` | 表单控件与触摸命中注册 |
