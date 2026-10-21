# 03 Android 平台与运行时设计

状态：设计稿（验证阶段结论已吸收，正式实现后可替换）
范围：Android 宿主、生命周期、输入、文件、资源、存储与日志。渲染与推理分别见 `04-rendering.md`、`05-inference-and-models.md`。

## 1. 宿主选择

验证阶段使用 `NativeActivity + EGL + GLES + ImGui`，证明 native 渲染链路可用。
正式版本推荐采用**薄 Kotlin 宿主 + SurfaceView + C++ EGL 渲染线程**：

- Kotlin 只负责 Activity 生命周期、权限、SAF/Photo Picker、分享、设置系统栏。
- C++ 负责 EGL/GLES、ImGui、渲染、推理调度和文档状态。
- JNI 接口保持窄：`surfaceCreated/surfaceChanged/surfaceDestroyed`、`touch`、`key`、`openDocumentResult`、`exportResult`。
- 不在 Kotlin 层复制业务逻辑，也不把渲染放进 `GLSurfaceView` 自带的 renderer 线程。
- 不直接使用 GLFW；SDL2 不是 MVP 依赖。若后续需要成熟输入/音频，再评估 SDL2 或 GameActivity。

这样做的原因是：SAF 与 Activity Result 在 Kotlin 侧最可靠，而 ImGui/GLES 上下文仍由 C++ 独占，避免 Android UI 框架和 native UI 两套状态。

## 2. 生命周期

```
Activity onCreate
  -> 加载 native library
  -> 创建 SurfaceView / 设置 JNI 回调
Activity onStart/onResume
  -> surfaceCreated: 创建 EGLDisplay/Context/Surface
  -> 初始化 ImGui、GPU 资源、字体、模型会话（可延迟）
Activity onPause
  -> 停止动画帧循环
  -> 取消未完成推理/导出（保留已提交结果）
  -> 保存窗口/文档轻量状态
Activity onStop/onDestroy
  -> 释放 GL 资源、EGL、native 对象
  -> 释放 ORT session
```

### EGL 丢失

Android 可能销毁 Surface 但进程仍存活：

- `surfaceDestroyed` 后不得继续 `eglSwapBuffers`。
- 所有 GL 资源标记为“上下文相关”，在重建后重新创建纹理、FBO、shader、VAO。
- CPU 侧 `Document`（RGBA、depth、settings）独立于 GL 上下文，可跨 surface 重建复用。
- 重建后不重复推理；只重新上传纹理并重绘。
- 推理线程不得直接持有 GL 对象；结果回主线程后再上传。

## 3. 输入

### 触摸

- 单指拖动：旋转灯光方向/移动灯光标记（由当前模式决定）。
- 双指：缩放/平移画布；双指旋转暂不做。
- 长按：上下文菜单或进入拖动。
- 轻点：选择灯光/面板控件。
- 侧边滑入：打开/关闭 Inspector 抽屉。
- 所有触控先转换为 ImGui IO 事件，再由 UI 层决定行为；渲染层不直接解析手势。

### 键盘与返回键

- 外接键盘可选支持；不作为 MVP 验收条件。
- Android 返回键：先关闭弹窗/抽屉，再退出；由 Kotlin 宿主转给 native UI 决策。

### 触控目标

- 最小可点击区域 44 dp。
- ImGui 全局缩放跟随 `density`，但画布纹理坐标保持像素级。
- 不使用鼠标 hover 作为必需交互；所有 hover 功能必须有触摸替代。

## 4. 文件与 SAF

### 打开图片

- 使用 `ActivityResultContracts.OpenDocument`，MIME 限定 `image/*`。
- 读取 `content://` URI 到内存字节，再交给 `stb_image`/libwebp 解码；不依赖真实文件路径。
- 限制：最大像素数、最大文件字节数、解码失败提示。
- 读取后立即释放文件描述符；不长期持有 URI 权限，除非用户选择“最近文件”。

### 导出

- 使用 `ActivityResultContracts.CreateDocument("image/png")` 或 `ACTION_CREATE_DOCUMENT`。
- native 返回 RGBA 字节，Kotlin 写入 `ContentResolver` 输出流。
- 导出线程独立于 UI；写完后通过 JNI 回调显示成功/失败。
- 分享：使用 `FileProvider` 将临时文件转为 `content://`，再 `ACTION_SEND`；临时文件放 `cacheDir`，定期清理。

### 模型与配置

- q4 模型来自仓库根目录 `models/depth-anything-v2-small/q4/model_q4.onnx`，构建时同步到 APK assets，不在 Android 目录重复提交二进制。
- 预处理器配置来自 `models/depth-anything-v2-small/preprocessor_config.json`。
- 运行时首次启动可选择：
  1. 直接把 asset 读入内存，交给 ORT `CreateSessionFromArray`（简单，MVP 采用）；
  2. 首次复制到 `filesDir/models/`，后续从文件加载（便于热更新/替换）。
- MVP 采用方案 1，避免可写存储和权限；模型更新随 APK 发布。
- 若后续支持用户模型：复制到 `filesDir/models/`，记录来源与校验和，不直接执行外部路径。

### 配置与缓存

- 设置、最近文件、窗口状态存 `SharedPreferences` 或 `filesDir/config.json`；MVP 用 JSON 文件，便于与桌面结构对齐。
- 缩略图/导出临时文件放 `cacheDir`，超过配额清理。
- 不申请 `MANAGE_EXTERNAL_STORAGE`；不写公共 Pictures 目录，统一走 SAF。

## 5. 资源与路径

- 着色器、i18n、默认配置从 APK assets 读取；开发期可用 `AAssetManager` 直接读。
- 不允许 native 代码假设 `exeDir()` 或当前工作目录。
- 平台路径接口：
  ```cpp
  class PlatformFiles {
  public:
      virtual std::vector<uint8_t> readAsset(std::string_view name) = 0;
      virtual std::string cacheDir() const = 0;
      virtual std::string filesDir() const = 0;
  };
  ```
- 图标/字体：ImGui 默认字体可用于验证；正式版可打包 Noto Sans SC 子集，控制体积与许可证。

## 6. 日志与错误

- native 日志统一走 `__android_log_print`，tag `ImRelighter`；debug 构建可输出更多上下文。
- 用户可见错误必须可操作：文件打不开、模型缺失、GLES 版本不足、显存不足、导出失败。
- 崩溃边界：Kotlin 捕获 JNI 初始化失败；native 捕获 C++ 异常并转错误码，不让异常越过 JNI。
- 性能日志：模型加载、推理、normal pass、首帧、导出耗时；release 默认关闭详细日志。

## 7. 屏幕、方向与安全区

- MVP 默认横屏（`sensorLandscape`），与桌面画布比例接近；竖屏作为后续适配项。
- 使用 `WindowInsets` 处理刘海/手势条；ImGui 布局扣除安全区。
- 全屏沉浸模式可切换；系统栏颜色跟随主题。
- 屏幕常亮仅在推理/导出时启用，完成后恢复。

## 8. 平台验收

- 冷启动到可交互 < 3 s（不含模型首次加载）。
- 模型加载 + 首次推理 < 8 s（中端 arm64）。
- surface 销毁/恢复后不崩溃、不重复推理、画面正确。
- 从相册打开 12 MP 图片、导出 PNG、分享均成功。
- 返回键/手势不误退出正在编辑的文档。
