# Android ImRelighter — 设计文档

> 状态：设计准备阶段（Draft）
> 适用范围：`android/` 工作区
> 最后更新：2026-10-21
>
> 这些文档用于启动 Android 版本的设计与实现。正式版本完成后，本目录会被
> 重新整理为面向用户和维护者的正式文档；当前内容优先保证决策可追溯、边界清晰、
> 新接手者可以快速建立完整上下文。

## 1. 目标

在 `android/` 内交付一个独立、可构建、可测试、可发布的 Android 应用，同时：

- 不破坏桌面端 `src/`、CMake、安装器和发布流程；
- 尽可能复用已经验证过的算法、着色器、模型和 UI 组件；
- 保持优秀的模块边界、专业注释和可验证的构建链；
- 让后续 Agent 或开发者从文档即可恢复上下文，而不是依赖口头历史。

## 2. 非目标

- 本阶段不把 Android 代码混入桌面端 `src/`。
- 不试图让桌面端 CMake 直接构建 Android 目标。
- 不在第一版追求与桌面端完全相同的功能数量；优先保证单图重打光的核心闭环。
- 不把测试探针 `C:\Users\86136\Desktop\output\test` 作为正式工程；它只作为验证记录。

## 3. 文档地图

| 文档 | 内容 |
| --- | --- |
| [01-product-and-scope.md](01-product-and-scope.md) | 产品目标、MVP 范围、非目标、验收标准 |
| [02-architecture.md](02-architecture.md) | 分层架构、模块职责、数据流、线程与生命周期 |
| [03-platform-runtime.md](03-platform-runtime.md) | Activity/EGL、输入、文件访问、资源与配置 |
| [04-rendering.md](04-rendering.md) | GLES 渲染管线、深度/法线/灯光/阴影、精度与兼容性 |
| [05-inference-and-models.md](05-inference-and-models.md) | ORT Android、q4 模型、预处理/后处理、性能与模型管理 |
| [06-ui-ux.md](06-ui-ux.md) | 移动端布局、触控、画布、面板、i18n 与可访问性 |
| [07-build-test-release.md](07-build-test-release.md) | Gradle/CMake、ABI、测试、构建产物与签名 |
| [08-risks-roadmap.md](08-risks-roadmap.md) | 风险、缓解措施、阶段计划与里程碑 |
| [appendices/desktop-parity.md](appendices/desktop-parity.md) | 桌面端能力与 Android 目标对照 |
| [appendices/validation-results.md](appendices/validation-results.md) | 已验证事实、设备数据、失败结论 |
| [decisions/](decisions/) | 架构决策记录（ADR） |

## 4. 设计原则

1. **边界优先**：Android 的所有源码、构建、文档、产物都在 `android/` 内。
2. **单向依赖**：Android 可以复制/移植桌面算法，但不得反向依赖桌面 `src/` 或桌面构建产物。
3. **先验证再抽象**：已验证的结论写入附录；未验证的假设明确标记。
4. **平台层薄**：Android/Kotlin 只做生命周期、窗口、输入、文件选择等平台服务；
   核心渲染与推理保持在可移植 C++ 中。
5. **可回退**：模型、法线路径、渲染目标精度都保留可控回退，不把设备兼容性押在单一扩展上。
6. **文档与代码同步**：新增渲染参数、模型类型、构建步骤必须同时更新文档和测试。

## 5. 硬约束

- 工作区：`android/`；桌面端主仓库其他目录不因 Android 开发而改写。
- 默认模型：**q4**（4-bit 权重、FP32 激活），路径
  `models/depth-anything-v2-small/q4/model_q4.onnx`，约 27.4 MB。
- 模型配置：仓库根 `models/depth-anything-v2-small/preprocessor_config.json`。
- Android 最低图形能力：OpenGL ES 3.1；以 ES 3.2 设备为主要目标，3.1 设备提供回退。
- 默认 ABI：`arm64-v8a`；模拟器/开发机可加 `x86_64`。
- 默认 UI 技术：Dear ImGui（复用桌面 UI 组件套件），平台窗口层由 Android Host 承担。
- 语言：C++20；Android 平台层 Kotlin/Java 仅限必要部分。
- 不新增未经评估的第三方依赖；依赖清单见 `android/third_party/README.md`。

## 6. 已验证基线（来自 test 探针）

- NativeActivity + EGL + OpenGL ES 3.1 + ImGui 1.92.5 可运行。
- ONNX Runtime Android 1.20.0 CPU 可加载 q4/q4f16/FP32 模型。
- q4 在 arm64 真机（V2243A，Adreno 740）上推理约 **1.95 s**，法线 pass 约 3.7 ms。
- q4 的法线结果干净；q4f16 在 arm64 上会出现轮廓密集噪点，**不作为 Android 默认模型**。
- FP32 画质最好但体积大（约 99 MB），仅作为质量回退。
- 深度图 518×714，法线在深度分辨率计算后再放大，比在 878×1200 上直接中心差分更稳定。

## 7. 待确认决策

以下决策在 ADR 中记录；未标记 Accepted 前，实现不应视为最终：

- 正式 App 的 Host 形态：NativeActivity 直连，还是 Kotlin Activity + SurfaceView + JNI。
- 是否在首版启用阴影；阴影分辨率与 PCF 方案。
- 法线最终走 GPU 还是 CPU；是否保留双路径。
- 导出格式与分享入口。
- 模型是否随 APK 打包，还是首次启动下载/导入。

## 8. 阅读顺序

新接手者建议按以下顺序阅读：

1. 本文件；
2. `01-product-and-scope.md`；
3. `02-architecture.md`；
4. `05-inference-and-models.md`；
5. `04-rendering.md`；
6. `03-platform-runtime.md`；
7. `06-ui-ux.md`；
8. 附录与 ADR。

## 9. 维护规则

- 每个设计决策变更都应新增或更新 ADR，而不是只改实现。
- 验证数据必须注明设备、ABI、模型、输入尺寸和测量条件。
- 不要把临时调试代码、绝对路径或本机专属配置写入设计文档。
- 正式实现开始后，本目录仍保留为设计历史；发布文档另行整理。
