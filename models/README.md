# Models

项目随包提供一个默认本地模型，不联网下载、不访问 Hugging Face。设置中也可以选择其他本地 ONNX 模型。

```text
models/
└─ depth-anything-v2-small/
   ├─ LICENSE                  # Apache-2.0
   ├─ preprocessor_config.json # 共享预处理参数
   └─ q4f16/
      ├─ model_q4f16.onnx      # 默认模型（约 19 MB）
      └─ preprocessor_config.json
```

## 运行时

- 程序默认读取 `<exe目录>/models/depth-anything-v2-small/q4f16/model_q4f16.onnx`。
- `preprocessor_config.json` 提供输入尺寸与归一化参数；优先读取模型同目录，找不到时回退到上一级。
- `LICENSE` 用于许可证合规。
- 模型目录随 CMake `POST_BUILD` 一起复制到构建产物目录。
- 模型缺失或损坏时，UI 明确提示路径，不生成假深度。
- 不提供下载入口、不读取 `%LOCALAPPDATA%`、不访问网络。

## 兼容性

- 默认使用 **q4f16** 量化模型，体积小、加载快，适合作为随包默认值。
- q4、q4f16、bnb4 等量化导出通常保持 FLOAT32/FLOAT16 输入输出，可以由 ONNX Runtime 直接加载。
- int8 导出若包含 `ConvInteger` 等当前 ONNX Runtime CPU 未实现的算子，会加载失败；设置页会显示模型类型和 ONNX Runtime 错误，不会静默回退。
- 需要更高质量时，可在设置中选择自己下载的 FP32/FP16 或兼容量化模型。

## 来源

- `onnx-community/depth-anything-v2-small`
- Revision `4472b7362082ad9968fee890ca0f1e5aca36b93d`
- 许可证 Apache-2.0；权重再分发需保留 `LICENSE` 与本说明。
