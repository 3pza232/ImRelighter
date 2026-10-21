# 07 · 构建、测试与发布

## 工具链基线

| 项 | 基线 |
|---|---|
| JDK | 17 |
| Gradle | 9.1.0 |
| Android Gradle Plugin | 9.0.1 |
| compileSdk / targetSdk | 36 |
| minSdk | 26 |
| NDK | 26.3.11579264 |
| CMake | 3.22.1（SDK 组件） |
| C++ | C++20 |
| ABI | `arm64-v8a` 默认；调试额外 `x86_64` |
| 图形 | OpenGL ES 3.1+ |

以上组合已在验证探测项目中实际构建并通过真机安装；正式工程沿用，不引入第二套构建体系。

## 工程结构

```text
android/
  README.md
  settings.gradle.kts
  build.gradle.kts
  gradle.properties
  gradle/wrapper/
  app/
    build.gradle.kts
    src/main/AndroidManifest.xml
    src/main/java/...          # 薄 Kotlin 宿主
    src/main/cpp/              # native core / app / ml / io / gpu / ui
    src/main/res/
    src/main/assets/           # 仅生成的模型/配置副本，不手工提交大文件
  third_party/
  docs/design/
  dist/                        # 忽略
```

模型不提交到 `android/app/src/main/assets`。Gradle 任务从仓库根 `models/depth-anything-v2-small/` 同步：

- `q4/model_q4.onnx` → assets；
- 顶层 `preprocessor_config.json` → assets；
- 打包前校验存在性/大小，缺失时构建失败。

这样可以避免同一份 27 MB 模型在仓库中出现两份。

## Gradle / CMake 集成

- `externalNativeBuild.cmake` 指向 `app/src/main/cpp/CMakeLists.txt`；
- `ndkVersion` 固定；`abiFilters` 由构建类型/属性控制；
- native 目标链接 `android`、`log`、`EGL`、`GLESv3`、`c++_shared` 和 ORT；
- ORT 通过 `third_party/onnxruntime-android-1.20.0/jni/${ANDROID_ABI}/libonnxruntime.so` 链接；
- 不引用桌面 `src/`、`CMakeLists.txt`、GLFW、DirectML、Windows 库；
- Debug 保留符号；Release 开启优化并保留 native 调试符号的独立副本。

推荐命令：

```powershell
cd android
.\gradlew.bat assembleDebug --offline --no-daemon
.\gradlew.bat assembleRelease --offline --no-daemon
```

离线构建依赖预置的 Gradle 缓存；CI 使用在线依赖缓存。

## 测试分层

### 1. 主机侧单元测试

- 纯算法：深度归一化、法线计算、参数校验、配置解析、i18n key；
- 不依赖 Android/GL/ORT；
- 可在 Windows/Linux CI 运行。

### 2. Android instrumented / native 测试

- JNI 边界、SAF 返回、生命周期、assets 读取；
- ORT session 创建、q4 推理形状/有限值；
- GLES 资源创建、FBO 完整性、shader 编译。

### 3. 设备/模拟器渲染测试

- x86_64 SwiftShader 模拟器：快速回归、UI 布局；
- 真机 arm64：最终视觉/性能验收；
- 固定输入 `mzm.jpg`（878×1200）和固定参数，保存 golden PNG；
- 比较深度/法线/灯光结果，允许明确的容差；
- 重点检查轮廓噪点、方向、颜色、阴影边界。

### 4. 性能测试

- 冷启动到可交互；
- q4 推理时间（目标 <4 s，V2243A 实测约 1.95 s）；
- 法线/阴影重算时间；
- 预览 FPS（目标 30）；
- 峰值内存、APK 大小、发热降频。

## 设备矩阵

| 设备 | 用途 |
|---|---|
| V2243A / arm64 / Adreno 740 | 主真机基线 |
| 至少一台中端 arm64 | 性能下限 |
| 一台 GLES 3.1 老设备 | 兼容性 |
| x86_64 API 34 SwiftShader | CI/快速回归 |

当前只完成 V2243A 验证；其他设备在正式开发阶段补齐。

## 版本与发布

- Android 版本独立于桌面版本，不沿用 `0.4.2` 作为 Android 版本；
- `versionCode` 单调递增，`versionName` 使用语义化版本；
- 签名 keystore 不提交仓库，通过环境变量/CI secret 注入；
- 发布产物：
  - `android/dist/ImRelighter-<version>.apk`（内测/侧载）
  - `android/dist/ImRelighter-<version>.aab`（商店）
- `dist/`、`local.properties`、`.cxx`、构建缓存全部忽略；
- 不修改桌面安装器，不把 Android 产物放进桌面 `dist/`；
- 当前阶段不打 Android tag，验证完成后再确定首个版本。

## 发布前检查

- `assembleRelease` 成功，APK 只含目标 ABI；
- 安装/覆盖安装/卸载重装正常；
- 模型与配置随 APK 打包，首次启动无需联网；
- SAF 打开/导出在 API 26–36 设备可用；
- 旋转/后台/锁屏/表面重建不崩溃；
- 无明文密钥、无调试日志泄漏；
- 许可证/第三方 notices 完整；
- 真机 golden 与性能报告归档到 `docs/design/appendices/`。
