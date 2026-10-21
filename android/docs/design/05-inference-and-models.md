# 05 · 推理、模型与图像处理

## 模型策略

| 模型 | 路径 | 大小 | Android 定位 |
|---|---|---:|---|
| **q4（默认）** | `models/depth-anything-v2-small/q4/model_q4.onnx` | 27,404,416 B | 默认模型；4-bit 权重 + FP32 激活 |
| q4f16 | 旧模型，已从仓库默认移除 | 19.1 MB | 仅兼容手动选择；arm64 真机法线噪点，不作为 Android 默认 |
| FP32 | `models/depth-anything-v2-small/fp32/model.onnx` | 99.1 MB | 质量回退/对照，不默认打包 |
| bnb4 | `.../bnb4/model_bnb4.onnx` | 26.1 MB | 可选实验，需重新验证 |

模型文件只保留在仓库根 `models/`。Android 构建通过 Gradle 任务同步到生成 assets，**不在 `android/` 再复制一份受版本控制的模型**。

预处理器配置位于 `models/depth-anything-v2-small/preprocessor_config.json`（顶层，不在量化子目录）。量化目录只放各自模型文件。

## 推理运行时

- ONNX Runtime Android 1.20.0，C/C++ API；
- 直接链接/打包 `libonnxruntime.so`（arm64-v8a、x86_64）；
- MVP 使用 CPU Execution Provider，4 个 intra-op 线程；
- NNAPI 作为后续可选后端，必须保留 CPU 回退；
- 不使用 DirectML（Windows only）；
- ORT AAR/头文件/so 位于 `android/third_party/onnxruntime-android-1.20.0/`。

## 推理流程

1. Android SAF 返回图片 URI；
2. Kotlin 读取字节或文件描述符，交给 native；
3. stb_image 解码为 RGBA8，限制最大像素数；
4. 预处理：
   - 目标短边 518，保持比例；
   - 宽高向上取 14 的倍数；
   - Catmull-Rom resize；
   - ImageNet mean/std；
   - CHW float32；
5. 从 APK assets 读取 q4 模型到内存，`CreateSessionFromArray`；
6. 执行推理，获得 `[1,1,H,W]` 深度；
7. 深度后处理：5×5 中值 → min/max 归一化 → 缩放/方向约定；
8. 法线在深度分辨率生成，再放大到目标尺寸；
9. 结果交给 GL 线程上传纹理。

## 线程与生命周期

- UI/GL 线程不执行推理；
- 推理线程拥有 session 或由 session 池管理；
- 每次任务带 generation ID；文档切换后旧结果必须丢弃；
- 取消采用原子标志，不强杀线程；
- session 初始化较慢，可在应用启动后预热一次；
- 内存/降级：低内存时释放 FP32 缓存，只保留当前模型和必要纹理。

## 已验证性能

设备：V2243A，arm64-v8a，Adreno 740，Android API 36。

| 模型 | 推理耗时（4 线程） | 结果 |
|---|---:|---|
| q4 | 约 1.95 s | 法线干净，默认选择 |
| q4f16 | 约 2.3 s | 轮廓密集噪点，弃用 |
| FP32 | 约 1.95 s | 干净但模型 99 MB |

桌面 q4 benchmark（512×512）：CPU 556.1 ms，DML 73.7 ms；仅作性能参考，不代表手机。

## 精度与验收

- q4 模型在真机上的深度范围约 0.0000–4.4122；
- 轮廓法线不得复现 q4f16 的密集点状噪声；
- 深度/法线方向与桌面一致；
- 推理失败必须显示可读错误，不允许黑屏或静默使用旧深度；
- 切换模型后必须校验输入名、shape、元素类型；
- 每个模型变体至少保留一张真机 golden 截图。

## 后续优化

- FP16 或更适合 arm64 的量化模型，目标 APK 明显小于 q4 的约 88–92 MB；
- NNAPI/QNN 实验，必须与 CPU 结果对比；
- 模型按 ABI 分包/按需下载（Play Asset Delivery）；
- 量化后处理/缓存，避免重复 resize 和 normalize。
