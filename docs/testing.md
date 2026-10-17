# 测试

## CTest

```powershell
ctest --test-dir build/mingw-release --output-on-failure
```

当前包含：

- `imrelighter_core`：预处理尺寸、后处理方向/NaN/平坦深度、16 位 PNG、WebP、i18n 键一致性
- `imrelighter_gpu`：隐藏 GLFW 窗口下的 Lit/Normal/Depth/Lighting/Shadow 金图像，PSNR ≥ 28 dB；平坦深度、深度方向、法线 flipX、透明 alpha

## GPU 金图像

金图像位于 `tests/golden/*.rgba`，测试程序在 `build/<preset>/tests/`。

重新生成：

1. 在 CTest 工作目录（通常 build 根目录）创建 `update-golden.flag`
2. 运行 `imrelighter_gpu_tests`
3. 删除 flag，重新运行比较

无 OpenGL 4.5 环境时 GPU 测试会跳过。

## FP16

选择模型文件后，设置中的模型信息会显示模型类型（fp32 / fp16）。当前支持 FLOAT32、FLOAT16；FP16 输入/输出会自动转换，INT8/Q4 不支持。

## DirectML 基准

```text
build/mingw-release/tests/imrelighter_inference_bench.exe
```

本机参考（512×512 合成图，仅表示相对量级）：

```text
CPU  529.8 ms
DML   96.5 ms（warmup 1313.6 ms）
```

## i18n

`tests/core_tests.cpp` 检查 `zh-CN.json` 与 `en-US.json` 键集合一致、关键值非空；新增 UI 文本必须同步两份词典。
