# 发布打包

ImRelighter 提供两种发布形态：

- **Setup 安装程序**（推荐）：`dist/ImRelighter-0.4.0-setup.exe`
- **便携运行目录**：直接压缩 `build/mingw-release/bin/`

两种形态使用同一份运行文件，区别只在安装、快捷方式与卸载行为。

## 安装程序

### 行为

- 语言：安装器启动时可选简体中文或 English。
- 许可：显示根目录 `LICENSE`（MIT）。
- 目录：按用户安装到 `%LOCALAPPDATA%\Programs\ImRelighter`，无需管理员权限；用户也可以在向导中改到其他可写目录。
- 快捷方式：创建开始菜单项；桌面快捷方式为可选项。
- 卸载：开始菜单与安装文件会被删除；应用生成的 `config.json` 与 `models` 目录保留，避免删除用户下载的模型。
- 升级：安装器使用固定的 `AppId`，复用上次选择的安装目录与任务选择。已有模型文件不覆盖，缺失的默认模型或安装包内新增模型会补装；`config.json` 不进入安装包，升级不会替换用户配置。
- 配置兼容：应用读取缺失字段时使用默认值，保存时保留本版本不认识的 JSON 字段；新增/删除/改名字段不会让旧配置整体失效。
- 运行库：使用 `-FetchVCRedist` 构建时附带 Microsoft Visual C++ 2015–2022 x64 运行库；系统缺少运行库时安装器会调用它。未附带时，安装结束会提示下载地址。

安装到 `Program Files` 等只读目录时，应用仍可运行，但保存设置会失败并显示错误；安装器默认目录避开了这个问题。

### 构建安装器

要求：

- 已按 [build.md](build.md) 完成 Release 构建
- Inno Setup 6.5 或更新版本
- 可选：联网下载 VC++ 运行库

```powershell
# 只编译安装器（要求 build/mingw-release/bin 已存在）
powershell -ExecutionPolicy Bypass -File scripts/build-installer.ps1 -SkipBuild

# 重新构建应用，并下载、附带 VC++ 运行库
powershell -ExecutionPolicy Bypass -File scripts/build-installer.ps1 -FetchVCRedist
```

脚本会：

1. 从 `CMakeLists.txt` 读取版本号；
2. 调用 `cmake --build --preset mingw-release`（除非 `-SkipBuild`）；
3. 用 `installer/ImRelighter.iss` 编译安装器；
4. 输出 `dist/ImRelighter-<version>-setup.exe` 与 `.sha256`。

`installer/redist/` 已被 `.gitignore` 忽略，下载的运行库不会进入版本库。

## 运行包内容

`build/mingw-release/bin/`：

```text
ImRelighter.exe
assets/                   字体、图标、i18n、shaders/
models/                   默认 ONNX + LICENSE
DirectML.dll
onnxruntime.dll
onnxruntime_providers_shared.dll
libwebp.dll
libsharpyuv.dll
LICENSE
LICENSE-ONNXRUNTIME.txt
LICENSE-DIRECTML.txt
LICENSE-DIRECTML-CODE.txt
LICENSE-LIBWEBP.txt
PATENTS-LIBWEBP.txt
THIRD_PARTY_NOTICES.md
```

测试程序、基准程序、`update-golden.flag` 与本地 `config.json` 不进入发布包。安装器按文件类型与目录显式收集运行文件，因此不会带入 `config.json`。

## 运行时要求

- Windows 10 1809+ / Windows 11，x64
- OpenGL 4.5 驱动
- ONNX Runtime 依赖 Microsoft Visual C++ 2015–2022 x64 运行库
- DirectML 可选；缺失时 Auto 回退 CPU
- 无网络连接也可安装与运行

## 发布检查清单

### 构建与自动化

- [ ] `cmake --build --preset mingw-release` 零告警
- [ ] `ctest --test-dir build/mingw-release --output-on-failure` 3/3 通过
- [ ] `scripts/build-installer.ps1 -FetchVCRedist` 成功并生成 `.sha256`

### 安装器

- [ ] 简体中文与 English 向导均可完成安装
- [ ] 默认目录为 `%LOCALAPPDATA%\Programs\ImRelighter`
- [ ] 非管理员账户可安装、运行、卸载
- [ ] 中文与空格路径可安装、运行
- [ ] 开始菜单快捷方式可用；桌面快捷方式仅在勾选时创建
- [ ] 覆盖安装复用上次安装目录，保留 `config.json` 与用户模型，缺失的默认模型会补回
- [ ] 卸载后用户模型与 `config.json` 仍在
- [ ] 卸载删除程序与快捷方式，保留 `config.json`
- [ ] 干净 Windows 虚拟机（无 VC++ 运行库）可完成安装并启动
- [ ] 安装包 SHA-256 与版本号写入发布说明

### 功能

- [ ] 无网络启动、打开图片、推理、渲染、导出
- [ ] DirectML 可用时使用 GPU；不可用时 Auto 回退 CPU
- [ ] 模型缺失或损坏时显示明确错误
- [ ] WebP 依赖 `libwebp.dll` 与 `libsharpyuv.dll` 完整
- [ ] 所有第三方许可证随包
- [ ] 版本号在 CMake、资源文件、安装器与关于页中一致

## 签名

当前安装器未签名。正式分发前建议使用代码签名证书签署 `ImRelighter.exe` 与安装器，减少 SmartScreen 警告；签名不属于构建必需步骤。

## 配置目录策略

应用把 `config.json` 写在 exe 同级目录。安装器默认安装到用户可写目录，因此不需要迁移配置。

若以后改为安装到 `Program Files`，需要把配置迁移到 `%APPDATA%\ImRelighter`，并同步修改设置加载、错误提示与卸载策略。

## 许可

项目源代码采用 **MIT License**，见根目录 `LICENSE`（Copyright 2026 PZA）。

模型、字体与第三方组件保留各自许可证：

- Depth Anything V2 Small：Apache-2.0
- 思源黑体：SIL OFL 1.1
- ONNX Runtime、GLAD、ImGui、stb、nlohmann/json：MIT 等
- GLFW：zlib/libpng
- libwebp：BSD-3
- DirectML：Microsoft 许可
- Microsoft Visual C++ 运行库：Microsoft 可再分发条款

完整汇总见 `THIRD_PARTY_NOTICES.md` 与各 `extern` 目录的 LICENSE。
