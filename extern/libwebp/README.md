# libwebp

| 项目 | 值 |
|---|---|
| 文件 | `libwebp.dll`、`libsharpyuv.dll` |
| 架构 | x64（PE32+） |
| 用途 | WebP 静态图片解码（RGBA） |
| 许可 | WebP BSD-style（见 `COPYING` / `PATENTS`） |

`libsharpyuv.dll` 是 `libwebp.dll` 的运行时依赖，必须与主程序一起分发。
运行时通过 `LoadLibraryExW` 从 exe 同级目录加载，避免链接 MSVC import library。
