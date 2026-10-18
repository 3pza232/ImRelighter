# Changelog

本项目遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 的结构，版本号与 `CMakeLists.txt` 保持一致。

## [0.2.0] - 2026-10-18

稳定性、架构与文档整理版本。核心功能与文件格式保持兼容。

### Added

- JPEG EXIF Orientation 支持，手机照片按拍摄方向显示。
- 解码前图片尺寸探测与统一的 24 MP 上限。
- 深度平滑回归测试、FP16 边界/往返测试、PNG round-trip、EXIF 8 方向、模型元数据、配置 round-trip 与图像头探测测试。
- 未使用 i18n 键检查脚本与 GitHub Actions Windows CI。
- `.editorconfig`、`.clang-format`、架构文档、限制说明、CHANGELOG 与界面截图。
- 高 DPI 布局缩放；设置对话框改为模态；灯光需从标记开始拖动。
- 模型检查异步化，模型页显示「检查中…」与错误信息。
- 配置原子写入、保存失败可见提示、滑条松开后写盘。
- 预处理支持 `do_resize`、`resample`、`rescale_factor` 与嵌套 `preprocess` 配置。
- 简体中文/English Setup 安装程序：per-user 安装、开始菜单与可选桌面快捷方式、卸载保留设置、可选附带 VC++ 运行库。
- `scripts/build-installer.ps1`：构建应用、编译安装器并生成 SHA-256。

### Changed

- `App` 拆分出 `DocumentStore`、`ModelManager`、`ExportService`；移除 `UiState` 影子设置。
- `Renderer` 拆分为 Depth / Normal / Shadow / Blur / Lighting Pass，渲染状态哈希集中维护。
- `ui.cpp` 拆分为 topbar / sidebar / canvas / inspector / settings / status / export 等翻译单元。
- 中英文文案统一：补 WebP 提示、灯光名称本地化、修正快捷键说明。
- 源码目标统一启用 `-Wall -Wextra`，第三方头改为 SYSTEM include。
- 构建预设不再写死本机 MinGW/Ninja 路径，改为依赖 `PATH`。
- 发布包补充根目录 MIT LICENSE。
- 调整主界面比例：左侧任务栏加宽，右侧参数栏收窄约四分之一，中间画布获得更多宽度。
- 任务卡片按 DPI 缩放缩略图、文本与关闭按钮的间距，长文件名在独立列内裁剪。

### Fixed

- 深度平滑 `uImageSize` 未设置导致平滑实际无效。
- 非 ASCII/中文路径下字体和窗口图标加载失败。
- 取消推理与模型加载/释放之间的跨线程竞态。
- FP16 转换移位未定义行为与次正规数指数错误。
- 状态栏临时消息不会过期、推理/导出状态长期残留。
- 新模型检查失败时仍显示旧模型类型。
- 设置背景仍可交互、窗口最小尺寸在高 DPI 下超出屏幕。
- 任务卡片的缩略图与文件名/尺寸在 125% 等高 DPI 下重叠。
- 任务卡片的关闭按钮高度与任务行不一致。
- 设置中任务数量与内存上限的滑钮尺寸不一致。
- ImGui `SetCursorScreenPos` 未提交 item 导致的断言。

### Removed

- 未使用的 `webpAvailable()`、`pickFolder()`、`WeightInfo::hasFp16/hasFp32()`、`theme::mode()` 与 14 个未引用 i18n 键。
- 重复的十六进制颜色解析与 24 MP 常量。
- 硬编码的本机构建工具路径。

### Known limitations

- 安装器未签名；SmartScreen 可能提示未知发布者。
- 未提供 MSIX 或系统级安装包。
- 不支持 GIF/动画 WebP/APNG、多输入/多输出模型、INT8/Q4 量化模型。
- 配置写在 exe 同级目录；安装到只读目录时保存设置会失败并显示错误。

## [0.1.0]

初始版本：本地 ONNX 深度推理、OpenGL 多灯重打光、多任务、7 视图、PNG 导出与基础测试。
