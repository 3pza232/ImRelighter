# Fonts

第一版只使用一套字体，中英文统一。

| 文件 | 字体 | 用途 | 许可证 |
|---|---|---|---|
| `SourceHanSansCN-Normal.otf` | 思源黑体 Normal（Source Han Sans CN） | 全部 UI：中文、英文、数字、状态栏 | SIL Open Font License 1.1，见 `LICENSE-SourceHanSans.txt` |

## 运行时加载建议

- 将 `SourceHanSansCN-Normal.otf` 作为 ImGui 默认字体加载，字号 15–16px；标题 18px，小字 13px。
- Source Han Sans 本身包含拉丁字形，不需要再合并第二套英文字体。
- 中文字形范围至少覆盖 `U+4E00–U+9FFF`、`U+3000–U+303F`、`U+FF00–U+FFEF`；ImGui 1.92 可使用动态字体按需加载。
- 数字列/状态栏也用同一字体；如后续需要等宽数字，可在字体加载时启用 tabular figures（若字体支持），不额外引入像素字体。
- 字体随应用 `assets/fonts/` 分发；打包时保留 OFL 许可证与版权声明。

## 注意

- 字体约 16 MB，会增大安装包；后续可用 `pyftsubset` 子集化，但必须保留 OFL 许可证与版权声明，且不得使用 Reserved Font Name 发布修改版。
- 不允许把系统字体（如微软雅黑）直接打包分发。
- 原 `ProggyClean.ttf` 已移除；项目不再使用像素字体。
