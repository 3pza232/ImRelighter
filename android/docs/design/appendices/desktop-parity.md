# 附录 A · 桌面与 Android 能力对照

本表用于判断哪些桌面能力可以复用、哪些必须重写。状态说明：

- **可复用**：算法/数据/纯 C++ 逻辑可直接移植；
- **需适配**：语义保留，但依赖平台 API 或 GPU 特性；
- **不复用**：Android 使用不同实现或 MVP 暂不支持。

| 能力 | 桌面实现 | Android 方案 | 状态 |
|---|---|---|---|
| 文档/设置模型 | `src/core` | 移植到 native core | 可复用 |
| i18n | JSON + core | 同一套 JSON，加载自 assets | 可复用 |
| 图片解码 | stb + libwebp | stb + libwebp（NDK） | 可复用/需构建 |
| 深度推理 | ORT + DirectML/CPU | ORT Android CPU/NNAPI | 需适配 |
| 预处理/后处理 | `src/ml` | 移植，输入尺寸 518/14 对齐 | 可复用 |
| 默认模型 | q4 ONNX 27.4 MB | 同一模型，来自 root `models/` | 可复用 |
| 深度纹理 | GL_R32F | GLES R32F + 回退 | 需适配 |
| 深度平滑 | `depth_smooth.frag` | GLES 版本，MVP 可选 | 需适配 |
| 法线生成 | `normal.frag` | GLES 版本 + CPU 回退 | 需适配 |
| 光照 | `lighting.frag` PBR | GLES 版本，减少灯数 | 需适配 |
| 阴影 | 多灯 shadow maps | 移动端降分辨率/数量 | 需适配 |
| 导出 | GL readback + PNG | FBO + glReadPixels + PNG | 需适配 |
| 文件选择 | Windows 对话框 | SAF | 不复用 |
| 拖放 | Win32/GLFW | 分享/打开 Intent | 不复用 |
| 窗口/输入 | GLFW + Win32 | Kotlin host + SurfaceView + EGL | 不复用 |
| UI | ImGui 桌面布局 | ImGui 移动布局/触控 | 需适配 |
| 安装 | Inno Setup | APK/AAB + Play/侧载 | 不复用 |

## 桌面功能优先级

**MVP**

- 单图打开；
- q4 深度；
- 原图/深度/法线/光照视图；
- 单灯/双灯，基础材质；
- PNG 导出；
- 中英 UI。

**MVP 后**

- 4 灯与阴影质量档；
- 预设、复制外观；
- 撤销/重做；
- 多图任务队列；
- 分享/批量导出。

**暂缓**

- 8 灯完整桌面级阴影；
- Vulkan 后端；
- 云端推理；
- 与桌面共享同一构建系统。
