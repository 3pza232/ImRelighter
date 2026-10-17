# 构建

## 工具链

- Windows x64
- MinGW-w64 GCC 13.x
- CMake 3.26+（`copy_directory_if_different` 需要 3.26）
- Ninja
- OpenGL 4.5 驱动

## 命令

```powershell
cmake --preset mingw-debug
cmake --build --preset mingw-debug

cmake --preset mingw-release
cmake --build --preset mingw-release
ctest --test-dir build/mingw-release --output-on-failure
```

## 产物目录

`build/mingw-release/bin/` 只包含运行应用所需内容：

```text
ImRelighter.exe
assets/                   字体、图标、i18n、shaders/
models/
DirectML.dll
onnxruntime.dll
onnxruntime_providers_shared.dll
libwebp.dll
libsharpyuv.dll
LICENSE-*.txt
THIRD_PARTY_NOTICES.md
```

测试程序输出到 `build/<preset>/tests/`，不进入运行包。

## 运行时依赖

- ONNX Runtime 1.20.1 DirectML build（CPU + DML）
- DirectML 1.15.4
- libwebp + libsharpyuv
- ONNX Runtime 依赖 MSVC 2015–2022 运行库；干净机器需安装 VC++ Redistributable
- 模型默认位于 `bin/models/depth-anything-v2-small/model.onnx`
