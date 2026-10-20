# 模型兼容性

## 默认模型

`models/depth-anything-v2-small/q4f16/model_q4f16.onnx`，Depth Anything V2 Small 的 q4f16 量化版本，Apache-2.0。

默认模型随构建产物分发，约 19 MB；程序不会联网下载。目录按量化档位分层，便于以后增加 `/base`、`/large` 或其他精度：

```text
models/depth-anything-v2-small/
├─ LICENSE
└─ q4f16/
   ├─ model_q4f16.onnx
   └─ preprocessor_config.json
```

## 模型信息与精度

设置 → 模型会在路径下方显示所选 ONNX 的模型类型（fp32 / fp16 / q4 / q4f16 / bnb4 / int8）。量化文件常保留浮点输入输出，因此类型优先参考文件名中的 q4/q4f16/int8 标识，再回退到 initializer 数据类型。

判断优先看权重 initializer 的存储精度；即使模型图的输入/输出仍是 FP32，只要权重主体是 FP16，也会显示 fp16。

- 支持 **FLOAT32** 与 **FLOAT16** 输入/输出
- FLOAT16 输入会自动从归一化 float32 转换；输出会转回 float32
- 转换使用 round-to-nearest-even，处理 NaN、Inf、正规数、次正规数与下溢
- **q4 / q4f16 / bnb4** 量化导出通常保持浮点输入输出，可直接尝试；当前随包的 q4f16 已作为默认模型验证
- **int8** 导出如果包含 `ConvInteger` 等 ONNX Runtime CPU 未实现的算子，会明确报加载错误；设置页仍会显示识别到的 `int8` 类型，不会静默假装可用
- 固定输入尺寸、多输入/多输出模型仍需要额外适配

## 预处理配置

优先从模型同目录或其上一级的 `preprocessor_config.json` 读取；默认 q4f16 配置位于 `q4f16/`。配置可以写在顶层，也可以放在 `"preprocess": { ... }` 子对象中；两者同时存在时优先使用子对象。

支持的字段：

| 字段 | 默认值 | 说明 |
|---|---|---|
| `size` / `height` / `width` | 518 | 输入尺寸；对象形式 `{"height": ..., "width": ...}` 也可 |
| `ensure_multiple_of` | 14 | 输入尺寸取整倍数 |
| `do_resize` | `true` | 是否缩放；关闭时输入尺寸保持原图 |
| `resample` | 3（bicubic） | PIL/Hugging Face 整数：0 nearest、1 lanczos、2 bilinear、3 bicubic、4 box、5 hamming |
| `do_rescale` | `true` | 是否应用 `rescale_factor` |
| `rescale_factor` | `1/255` | 像素缩放系数，仅接受有限正数 |
| `do_normalize` | `true` | 是否按 mean/std 归一化 |
| `image_mean` / `image_std` | ImageNet | 归一化参数 |
| `output.invert` | 由模型配置决定 | 深度近远是否反转 |

`resample` 使用 stb_image_resize2 的滤波器近似：Lanczos 与 bicubic 都映射到 Catmull-Rom，Hamming 映射到 Mitchell。这样避免引入新的缩放依赖，但重采样结果与 PIL 不会逐像素一致。

找不到配置文件时回退到 518 / 14 / ImageNet mean/std / 1/255。

## 当前运行时假设

- ONNX 只有 1 个输入、1 个输出。
- 输入为 NCHW，`1×3×H×W`。
- 输入/输出名称从模型查询，不硬编码名称。
- 输出为单通道浮点深度；按最后一维为宽度、倒数第二维为高度读取。
- 输出做 min-max 归一化，越大越近；再翻成 bottom-up 供 GL 使用。
- 模型路径可在设置中选择，默认路径不写死在推理层。
- 外部数据与模型文件同目录，由 ONNX Runtime 自动加载。

## ONNX 外部数据

大模型可能以两个文件分发：

```text
model.onnx
model.onnx_data
```

`model.onnx` 与 `model.onnx_data` 是 ONNX 外部数据格式的两个组成部分，应保持在同一目录并保留原文件名。选择 `model.onnx` 后，ONNX Runtime 会自动读取对应的外部权重。

## 可用/可尝试的模型

同一预处理和输出假设下，优先选择：

- Depth Anything V2 Small / Base / Large 的 ONNX 导出（推荐）
- Depth Anything V1 ONNX 导出
- 其他 Depth Anything 兼容 ONNX 导出

可尝试但需要验证：

- MiDaS / DPT-Hybrid ONNX：归一化通常不同，可能需要额外预处理支持
- DepthPro、Sapiens Depth：输入尺寸/输出格式可能不同
- Depth Anything V3：当前版本不支持；其多输入/多输出结构会在加载时被拒绝
- 任何多输入、非 float32 输出、固定尺寸不匹配的模型都不保证可用

选择模型文件后若加载或推理失败，应用会显示 ONNX Runtime 的错误；当前不会自动转换模型。

## 模型加载失败排查

- 确认 `onnxruntime.dll` 与 `DirectML.dll` 位于 exe 同级目录。
- 确认模型文件完整，外部数据文件同名且同目录。
- DirectML 不可用时切换到 CPU，或在设置中选择 Auto。
- 模型检查会显示 ORT 返回的错误；若只是推理失败，检查输入/输出数量和类型是否符合上述假设。
