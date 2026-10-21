# 附录 · 验证结果与已知数据

本附录记录 Android 可行性验证阶段得到的真实数据，供后续实现、回归和设计评审使用。验证代码位于 `C:\Users\86136\Desktop\output\test`，不属于正式 Android 工程；正式代码从 `android/app` 开始。

## 1. 环境

| 项 | 值 |
|---|---|
| 开发机 | Windows 11 |
| Android SDK | `D:\Android\SDK`，platform 36，build-tools 36.0.0 |
| NDK | 26.3.11579264 |
| CMake | 3.22.1（SDK 组件） |
| Gradle | 9.1.0 |
| AGP | 9.0.1 |
| JDK | 17.0.17 |
| 模拟器 | `device01`，Pixel 7，API 34，x86_64，SwiftShader |
| 真机 | V2243A，arm64-v8a，Android API 36，Adreno 740 |

验证工程使用 NativeActivity + EGL + OpenGL ES + Dear ImGui 1.92.5，仅用于快速验证；正式工程采用薄 Kotlin host + SurfaceView + native EGL。

## 2. 阶段结果

### Phase 0：图形/UI 栈

- OpenGL ES 3.1（模拟器 SwiftShader；真机 3.2）。
- ImGui 1.92.5 正常初始化并渲染。
- EGL surface 模拟器 1784×1080；真机 3200×1440。
- 多帧 `eglSwapBuffers` 成功，`glGetError() == 0`。
- 结论：Android 上 ImGui + GLES 可行。

### Phase 1：q4f16 推理

- 模型：q4f16，19,126,267 B。
- 输入：`pixel_values`，float32，`[1,3,518,518]`。
- 输出：`predicted_depth`，float32，518×518 / 518×714。
- x86_64 模拟器 warm inference：约 1.49–3.0 s。
- 结论：ORT Android CPU + q4f16 可运行，但真机存在深度/法线噪声，不能作为 Android 默认。

### 实际图片 `mzm.jpg`

- 原图：878×1200。
- 预处理：518 目标、14 倍数、Catmull-Rom、ImageNet 均值/方差。
- 模型输入：`[1,3,714,518]`。
- 输出：518×714。
- 深度归一化后用于法线/显示。

### 真机模型对比（V2243A）

| 模型 | 模型大小 | 真机推理 | 轮廓法线 |
|---|---:|---:|---|
| q4f16 | 19.1 MB | ~2.3 s | 密集点状噪点 |
| q4 | 27.4 MB | ~1.95 s | 干净，轻微边缘锯齿 |
| FP32 | 99.1 MB | ~1.95 s | 干净 |

结论：q4（4-bit 权重、FP32 激活）是 Android 默认模型。q4f16 仅保留为桌面/手动兼容类型，不作为 Android 默认。FP32 作为质量回退。

### 法线处理结论

- 仅做 denoise/mipmap 不能消除 q4f16 真机噪点。
- 将法线改为 CPU 计算也不能消除 q4f16 噪点，说明问题在深度数据。
- 最终验证管线：
  - 深度在 518×714 做 5×5 中值滤波；
  - 法线在 518×714 计算；
  - 上采样到 878×1200；
  - 法线纹理 mipmap；
  - CPU 法线作为稳定显示基线，GPU normal pass 保留诊断。
- q4 + 上述管线在真机上消除了密集点状噪点，剩余轻微边缘锯齿可接受。

## 3. 性能数据

| 指标 | 环境 | 结果 |
|---|---|---|
| q4 推理 | V2243A arm64，4 线程 | ~1.95 s |
| FP32 推理 | V2243A arm64，4 线程 | ~1.95 s |
| q4f16 推理 | V2243A arm64，4 线程 | ~2.3 s |
| GPU normal pass | V2243A | ~3.7 ms |
| 桌面 q4 CPU | 512×512 合成输入 | 556.1 ms（warmup 556.5 ms） |
| 桌面 q4 DML | 512×512 合成输入 | 73.7 ms（warmup 348.7 ms） |

这些数据只代表当前设备/模型，正式应用需要建立自己的性能基线。

## 4. 模型/依赖哈希

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `models/depth-anything-v2-small/q4/model_q4.onnx` | 27,404,416 | `5D55B02762E1907589158AF3E366BD61DDF648155852A07BBF5E3A074639FCF8` |
| `android/third_party/onnxruntime-android-1.20.0/jni/arm64-v8a/libonnxruntime.so` | 17,571,160 | `52329B7C8E5FCD5C1AA88B01093215FAAEF362E3F3D0E374CB8B10D1E4704677` |
| `.../jni/x86_64/libonnxruntime.so` | 20,290,904 | `047705AB42981A4884EEC435F60CC1A311E40414CD52FA2F252DA2B473756844` |

## 5. 验证截图

测试目录中保留以下参考图：

- `phase0-probe.png`：GLES + ImGui 基线；
- `phase1-probe.png`：q4f16 合成输入指标；
- `phase2-image-final.png`：原图/深度/CPU 法线；
- `normal-final3.png`：GPU 法线 pass 诊断；
- `phone-fp32-final.png`：真机 FP32 结果；
- `phone-fp32-final-crop.png`：真机 FP32 法线局部；
- `phone-lownormal-crop.png`：q4f16 真机噪点对照；
- `phone-q4.png` / `phone-q4-crop.png`：q4 真机结果；
- `desktop-mzm-depth3.png`：桌面法线参考。

正式工程接入 golden 测试时，应将这些图片重新生成/裁剪为固定视口和固定参数，不直接依赖测试工程路径。

## 6. 未完成验证

- 中端 arm64 真机（Mali/Adreno 6xx/7xx 低端）上的 GLES 3.1/FBO 支持；
- NNAPI 执行 provider、FP16 模型和 Play Asset Delivery；
- 4 灯/阴影/大图导出在真机上的性能与热稳定性；
- SAF、分享、后台恢复、屏幕旋转；
- 与桌面端的数值/像素级对比。
