# ImRelighter 正式文档

本目录是当前实现的使用与维护文档。

| 文档 | 内容 |
|---|---|
| [overview.md](overview.md) | 产品目标、范围、模块与数据流 |
| [build.md](build.md) | 工具链、构建、产物与运行时依赖 |
| [usage.md](usage.md) | 界面、快捷键、模型选择、WebP 与导出 |
| [models.md](models.md) | ONNX 模型兼容性、硬编码假设、可用模型 |
| [testing.md](testing.md) | 单元测试、GPU 金图像、DirectML 基准 |
| [packaging.md](packaging.md) | 干净的发布包、许可证、发布检查 |

## 当前状态

- 渲染管线、CPU/DirectML 深度推理、导出与测试已实现。
- 正在完善发布包、干净机器验证和文档。
- 程序完全本地运行，不联网、不上传图片、不自动下载模型。
