# 模型兼容性

## 内置模型

APK 内置 Depth Anything V2 Small 的 q4 量化模型：

```text
models/depth-anything-v2-small/
├─ preprocessor_config.json
└─ q4/
   └─ model_q4.onnx
```

模型约 27 MB，随 APK 分发，不联网下载。默认选择 q4，设置中提供“恢复默认 Q4”。

## 推理后端

当前 Android 版本只使用 **ONNX Runtime CPU**。没有 DirectML；NNAPI 尚未启用。`q4` 使用 4-bit 权重、FP32 激活，CPU 上可直接运行。

## 自定义模型

设置 → 模型：

1. 选择“导入模型文件夹”（SAF 目录选择器）。
2. 应用保留读取权限，并扫描该目录中的 `.onnx` 与 `preprocessor_config.json`，复制/镜像到应用私有存储。
3. 自定义模型按父文件夹名称显示；类型由文件名中的量化标记和 ONNX initializer 数据类型综合判断。
4. 选择模型后保持设置面板打开；可用“恢复默认 Q4”回到内置模型。

模型类型支持识别：

- `q4` / `q4f16` / `fp16` / `fp32` / `int8` / `bf16`（以文件名提示优先，再检查权重 initializer）。
- 模型图输入/输出建议为单输入、单输出、NCHW `1×3×H×W`，输出单通道深度。
- 固定输入尺寸模型需要与预处理配置匹配；动态尺寸模型按原图/预处理尺寸推理。

## 预处理配置

优先在模型同目录或上一级目录查找 `preprocessor_config.json`。支持桌面版相同的字段语义：

| 字段 | 说明 |
|---|---|
| `size` / `height` / `width` | 输入尺寸 |
| `ensure_multiple_of` | 尺寸取整倍数（通常 14） |
| `do_resize` | 是否缩放 |
| `resample` | 重采样方式（nearest/bilinear/bicubic 等） |
| `do_rescale` / `rescale_factor` | 像素缩放 |
| `do_normalize` / `image_mean` / `image_std` | 归一化参数 |
| `output.invert` | 深度方向反转 |

模型缺少配置时使用内置默认值；配置解析失败会在状态栏/设置中显示错误。

## 深度后处理

- ONNX 输出转换为 float32。
- 取有效深度范围归一化到 `0..1`，保持 top-down 方向。
- 按原图尺寸缩放（bilinear）。
- 生成 `depth_rgba`（可视化）和 `normal_rgba`（CPU 回退法线）。
- Android 深度 Pass 使用 `normalized_depth` 重建 R32F 高度；方向与法线已在 GPU 路径验证。

## 不支持/未验证

- 多输入/多输出模型（如 Depth Anything V3）未适配。
- `int8` 模型若依赖 ONNX Runtime CPU 未实现的算子（如 `ConvInteger`）会加载失败。
- 模型自动下载、转换、量化不提供。
- 自定义模型只验证结构与类型，不保证所有第三方导出都能运行。
