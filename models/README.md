# Models

第一版只使用一个本地模型，不联网下载、不访问 Hugging Face。

```text
models/
└─ depth-anything-v2-small/
   ├─ model.onnx       # 运行时唯一必需的权重文件（约 99 MB）
   ├─ manifest.json    # 来源、版本、SHA-256、预处理参数
   └─ LICENSE          # Apache-2.0
```

## 运行时

- 程序只读取 `<exe目录>/models/depth-anything-v2-small/model.onnx`。
- `manifest.json` 与 `LICENSE` 不参与推理，用于版本追踪与许可证合规。
- 模型目录随 CMake `POST_BUILD` 一起复制到构建产物目录。
- 模型缺失或损坏时，UI 明确提示路径，不生成假深度。
- 不提供下载入口、不读取 `%LOCALAPPDATA%`、不访问网络。

## 来源

- `onnx-community/depth-anything-v2-small`
- Revision `4472b7362082ad9968fee890ca0f1e5aca36b93d`
- 许可证 Apache-2.0；权重再分发需保留 `LICENSE` 与本说明。
