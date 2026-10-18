# Third-Party Notices

ImRelighter 使用以下第三方组件。各组件保留其原始许可证；本文件只做汇总，不替代各目录中的 LICENSE 文本。

## 运行时代码

| 组件 | 版本 | 许可证 | 位置 |
|---|---|---|---|
| GLAD | 0.1.36（gl=4.5 core） | MIT | `extern/glad/` |
| GLFW | 3.5.1 | zlib/libpng | `extern/glfw/LICENSE.md` |
| Dear ImGui | 1.92.5 | MIT | `extern/imgui/LICENSE.txt` |
| stb | vendored snapshot | MIT / public domain | `extern/stb/LICENSE` |
| nlohmann/json | 3.11.3 | MIT | `extern/json/LICENSE.MIT` |
| DirectML | 1.15.4 x64 | Microsoft 许可 | `extern/directml/README.md` |
| ONNX Runtime | 1.20.1 x64 (DirectML build) | MIT | `extern/onnxruntime/` |
| libwebp | 1.5.x x64（含 libsharpyuv） | BSD-3 | `extern/libwebp/` |

## 模型

| 组件 | 许可证 | 位置 |
|---|---|---|
| Depth Anything V2 Small（ONNX） | Apache-2.0 | `models/depth-anything-v2-small/LICENSE` |
| 来源 | `onnx-community/depth-anything-v2-small`，revision `4472b7362082ad9968fee890ca0f1e5aca36b93d` | 模型目录 |

模型权重随项目/构建产物分发；不联网下载。模型许可证不改变本项目自身代码的许可证。

## 字体

| 组件 | 许可证 | 位置 |
|---|---|---|
| Source Han Sans CN Normal（思源黑体） | SIL Open Font License 1.1 | `assets/fonts/LICENSE-SourceHanSans.txt` |

## 参考与声明

- 产品交互参考 AiDeNier 的 Relight Desk 视频；本项目为独立实现，不包含原作者的代码或素材。
- 原参考项目的示例插画未进入本仓库。
- 本项目自身源代码采用 MIT License，Copyright 2026 PZA，见根目录 `LICENSE`。

## 维护规则

- 新增/升级依赖时同步更新本文件与 `extern/README.md`。
- 发布包必须包含本文件、各依赖 LICENSE、模型 LICENSE 与字体 OFL。
- 不把模型、字体、第三方代码的许可证混入本项目自身的许可证。
