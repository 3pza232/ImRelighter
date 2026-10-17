# 模型兼容性

## 默认模型

`models/depth-anything-v2-small/model.onnx`，Depth Anything V2 Small，Apache-2.0。

## 当前运行时假设

- ONNX 只有 1 个输入、1 个输出。
- 输入为 NCHW `float32`，`1×3×H×W`；H/W 由预处理决定。
- 预处理固定：最长边约 518、尺寸为 14 的倍数、ImageNet mean/std、双线性 resize。
- 输出按最后一维为宽度、倒数第二维为高度读取，并按单通道 `float32` 深度处理。
- 输出做 min-max 归一化，越大越近；再翻成 bottom-up 供 GL 使用。
- 输入/输出名称从模型查询，不硬编码名称。
- 模型路径可在设置中选择，默认路径不写死在推理层。

## 可用/可尝试的模型

同一预处理和输出假设下，优先选择：

- Depth Anything V2 Small / Base / Large 的 ONNX 导出（推荐）
- Depth Anything V1 ONNX 导出
- 其他 Depth Anything 兼容 ONNX 导出

可尝试但需要验证：

- MiDaS / DPT-Hybrid ONNX：归一化通常不同，可能需要额外预处理支持
- DepthPro、Depth Anything V3、Sapiens Depth：输入尺寸/输出格式可能不同
- 任何多输入、多输出、非 float32 输出、固定非 518 输入的模型都不保证可用

选择模型文件后若加载或推理失败，应用会显示 ONNX Runtime 的错误；当前不会自动转换模型。
