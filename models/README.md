# Models

项目随包提供一个默认本地模型，不联网下载、不访问 Hugging Face。设置中也可以选择其他本地 ONNX 模型。

```text
models/
└─ depth-anything-v2-small/
   ├─ model.onnx       # 运行时唯一必需的权重文件（约 99 MB）
   ├─ preprocessor_config.json # 输入尺寸与归一化参数
   └─ LICENSE          # Apache-2.0
```

## 运行时

- 程序默认读取 `<exe目录>/models/depth-anything-v2-small/model.onnx`。
- `preprocessor_config.json` 提供预处理参数；`LICENSE` 用于许可证合规。
- 模型目录随 CMake `POST_BUILD` 一起复制到构建产物目录。
- 模型缺失或损坏时，UI 明确提示路径，不生成假深度。
- 不提供下载入口、不读取 `%LOCALAPPDATA%`、不访问网络。

## 来源

- `onnx-community/depth-anything-v2-small`
- Revision `4472b7362082ad9968fee890ca0f1e5aca36b93d`
- 许可证 Apache-2.0；权重再分发需保留 `LICENSE` 与本说明。
