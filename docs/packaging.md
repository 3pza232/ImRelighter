# 发布打包

## 运行包内容

只包含 `build/mingw-release/bin/`：

```text
ImRelighter.exe
assets/                   字体、图标、i18n、shaders/
models/                   默认 ONNX + LICENSE
DirectML.dll
onnxruntime.dll
onnxruntime_providers_shared.dll
libwebp.dll
libsharpyuv.dll
LICENSE-ONNXRUNTIME.txt
LICENSE-DIRECTML.txt
LICENSE-DIRECTML-CODE.txt
LICENSE-LIBWEBP.txt
PATENTS-LIBWEBP.txt
THIRD_PARTY_NOTICES.md
```

测试程序统一输出到 `build/<preset>/tests/`，不进入 bin。

## 检查清单

- 干净 Windows 机器（未安装 VC++ 运行库）验证推理启动
- 无网络启动、推理、导出
- 模型缺失/损坏时的错误提示
- DirectML 不可用时 Auto 回退 CPU
- WebP 有 libwebp.dll 和 libsharpyuv.dll
- 所有第三方许可证随包
- `config.json` 不预置用户路径；发布前删除本地运行配置
- 版本号在 CMake、资源文件、About 中保持一致

## 许可

项目源码许可证尚未选定；在选定前不自动授予第三方再分发权利。第三方组件的许可见 `THIRD_PARTY_NOTICES.md` 与各 `extern` 目录。
