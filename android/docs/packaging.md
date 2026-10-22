# 发布打包

## 发布形态

Android 版本目前以 APK 分发：

- Debug：`android/app/build/outputs/apk/debug/app-debug.apk`
- Release：`android/app/build/outputs/apk/release/app-release.apk`
- 发布归档：`android/dist/ImRelighterAndroid-release.apk`
- SHA-256：`android/dist/ImRelighterAndroid-release.apk.sha256`

Release APK 使用 `ImRelighter.jks` 签名（alias `imrelighter`），已通过
`apksigner verify`。Debug APK 只用于开发测试。

## APK 内容

| 内容 | 说明 |
|---|---|
| `classes.dex` | Kotlin Activity、JNI 声明、文件选择/导出 |
| `lib/arm64-v8a/libimrelighter.so` | arm64 真机原生库 |
| `lib/x86_64/libimrelighter.so` | x86_64 模拟器原生库 |
| ONNX Runtime | Android 1.20.0 原生库 |
| `assets/models/...` | q4 模型与预处理配置 |
| `assets/fonts/...` | Source Han Sans CJK 字体 |
| `assets/...` | 主题/图标等 |

应用不打包第三方桌面 DLL，不依赖桌面 `src/` 或 CMake 目标。

## 版本与签名

`app/build.gradle.kts`：

- `versionCode = 1`
- `versionName = "0.1.0"`
- `applicationId = "com.imrelighter.android"`

正式发布前应更新版本号。Release signingConfig 从 `android/keystore.properties`
读取 keystore 路径、alias 和密码；该文件与 `*.jks` 不提交仓库。当前签名方案为
APK Signature Scheme v2，最低 API 26 可直接安装。

## 安装

```powershell
adb install -r android/dist/ImRelighterAndroid-release.apk
```

设备要求：

- Android 8.0（API 26）及以上。
- OpenGL ES 3.1（Manifest 声明 required）。
- `arm64-v8a` 或 `x86_64` ABI。

## 发布检查

1. `assembleDebug`/`assembleRelease` 成功。
2. 确认 APK 含两个 ABI 和 q4 模型。
3. 在至少一台 arm64 真机完成导入、推理、五种视图、参数、导出、多任务回归。
4. 记录 APK 文件名、大小、SHA-256 和验证设备。
5. 把最终 APK 复制到 `android/dist/`，不要提交临时截图/日志。
6. 更新 `README.md`、本目录文档和版本说明。

## 资源与隐私

- 不申请网络权限。
- 图片、深度、模型都保存在本地。
- 自定义模型目录通过 SAF 读取并镜像到应用私有存储；卸载应用会删除镜像，但不会删除用户原目录。
- 导出写入 MediaStore `Pictures/ImRelighter`，卸载应用不会删除已导出图片。
