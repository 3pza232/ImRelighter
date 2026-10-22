# 构建

## 环境

| 项 | 要求 |
|---|---|
| 宿主 | Windows 10/11 x64 |
| JDK | 17 |
| Gradle | 9.1.0（仓库自带 wrapper） |
| Android Gradle Plugin | 9.0.1 |
| Android SDK | compileSdk / targetSdk 36 |
| 最低系统 | Android 8.0（API 26） |
| NDK | 26.3.11579264 |
| CMake | 3.22.1 |
| C++ | C++20，`c++_static` |
| 图形 | OpenGL ES 3.1 |

Gradle 用户目录、SDK 和 NDK 路径按本机环境配置；仓库中的 wrapper 固定 Gradle 版本，不写死本机路径。

## 构建命令

在 `android/` 目录执行：

```powershell
$env:GRADLE_USER_HOME='D:\Android\.gradle'
.\gradlew.bat assembleDebug --offline --no-daemon
```

带完整日志（排查原生编译/依赖问题）：

```powershell
.\gradlew.bat assembleDebug --no-daemon --stacktrace
```

Release（已配置签名）：

```powershell
.\gradlew.bat assembleRelease
```

离线且本地未缓存 lint 依赖时：

```powershell
.\gradlew.bat assembleRelease --offline --no-daemon `
  -x lintVitalAnalyzeRelease -x lintVitalRelease
```

Release 签名从根目录 `keystore.properties` 读取（该文件与 `*.jks` 均被 git 忽略）。当前配置：

- keystore：`android/ImRelighter.jks`
- alias：`imrelighter`
- 密码：构建机本地 `keystore.properties` 保存，不提交仓库

## 原生构建

`app/src/main/cpp/CMakeLists.txt` 由 Gradle 的 `externalNativeBuild.cmake` 调用，ABI 固定为：

- `arm64-v8a`
- `x86_64`

CMake 生成 `libimrelighter.so`，链接 ONNX Runtime Android 1.20.0 预编译库，并使用静态 C++ 运行库。原生代码启用 C++20。

## 资源生成

构建前任务 `syncModels` 会把桌面仓库的：

- `models/depth-anything-v2-small/q4/model_q4.onnx`
- `models/depth-anything-v2-small/preprocessor_config.json`

复制到 `app/build/generated/assets/models/...`，并清理旧的 q4f16 资源。Android 只打包 q4 作为内置模型。

## 产物

```text
android/app/build/outputs/apk/debug/app-debug.apk
android/app/build/outputs/apk/release/app-release.apk
android/dist/ImRelighterAndroid-release.apk
```

APK 包含：

- Kotlin/JNI 桥和 `libimrelighter.so`（arm64-v8a、x86_64）。
- ONNX Runtime Android 1.20.0 原生库。
- q4 模型和预处理配置。
- 主题、字体、图标等资源。

发布前将 APK 复制到 `android/dist/`，文件名和 SHA-256 记录在发布说明中。

## 常见问题

### `SDK processing... SDK XML version 4`

构建工具版本不一致时会打印该警告；不影响构建。使用与 Android Studio 匹配的 command-line tools 可消除。

### 原生库加载失败

确认 APK 的 ABI 与设备匹配。模拟器通常是 `x86_64`，真机通常是 `arm64-v8a`。`System.loadLibrary("imrelighter")` 失败时 Activity 会显示启动错误。

### 16 KB 页大小

原生库使用 16 KB 页对齐；Android 15+ 的 16 KB 页设备可加载。
