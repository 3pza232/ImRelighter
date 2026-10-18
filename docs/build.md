# 构建

## 工具链

- Windows x64
- MinGW-w64 GCC 13.x 或更新版本
- CMake 3.26+（构建规则使用 `copy_directory_if_different`）
- Ninja
- OpenGL 4.5 驱动（运行与 GPU 测试需要）

`gcc`、`g++`、`ninja` 必须在 `PATH` 中。`CMakePresets.json` 不写死本机路径。

## 命令

```powershell
cmake --preset mingw-debug
cmake --build --preset mingw-debug

cmake --preset mingw-release
cmake --build --preset mingw-release
ctest --test-dir build/mingw-release --output-on-failure
```

也可以直接配置：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

项目对自身源码启用 `-Wall -Wextra`；第三方头以 `SYSTEM` 方式包含，避免上游警告淹没项目警告。

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

测试与基准程序输出到 `build/<preset>/tests/`，不进入运行包：

```text
imrelighter_tests.exe
imrelighter_gpu_tests.exe
imrelighter_inference_bench.exe
```

GPU 测试依赖 `assets/shaders/` 与 `tests/golden/` 的源目录路径，由 CMake 编译期宏传入。

## 运行时依赖

- ONNX Runtime 1.20.1 DirectML build（CPU + DML）
- DirectML 1.15.4
- libwebp + libsharpyuv
- ONNX Runtime 依赖 MSVC 2015–2022 运行库；干净机器需安装 VC++ Redistributable
- 默认模型位于 `bin/models/depth-anything-v2-small/model.onnx`

模型、DLL 与运行时资源由 CMake 的构建后步骤复制；不要手工只复制 exe。

## 配置与数据

- `config.json` 写在 exe 同级目录。
- 配置保存采用临时文件 + 原子替换；目录不可写时状态栏会显示错误。
- 程序不写注册表、不写 AppData、不联网。

## CI

`.github/workflows/windows.yml` 在 `windows-latest` 上通过 MSYS2 安装 MinGW-w64 GCC、CMake、Ninja 与 Python，执行：

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

无 OpenGL 4.5 的执行环境会把 GPU 测试标记为 SKIP，而不是失败。
