# DirectML

| 项目 | 值 |
|---|---|
| 文件 | `DirectML.dll` |
| 版本 | 1.15.4 |
| 架构 | x64（PE32+） |
| 来源 | Microsoft DirectML 运行时 |
| 用途 | ONNX Runtime DirectML Execution Provider（可选） |

## 说明

- 这是可再分发的 DirectML 运行时 DLL，随应用输出目录一起复制。
- 已与 ONNX Runtime 1.20.1 DirectML 包验证：CPU/DML session 均可创建。
- CPU 推理不依赖此 DLL；没有它时设置面板中的 DirectML 选项应置灰。
- 再分发时遵守 Microsoft DirectML 的许可条款；具体条款以 Microsoft 发布包为准。
