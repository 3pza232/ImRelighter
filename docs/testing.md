# 测试

## CTest

```powershell
ctest --test-dir build/mingw-release --output-on-failure
```

当前包含 3 个测试：

| 测试 | 内容 |
|---|---|
| `imrelighter_core` | 预处理尺寸/直通/rescale、后处理方向/NaN/平坦深度、16 位与 RGBA8 PNG、WebP、EXIF 8 方向与解析、FP16 边界与往返、config round-trip、图像头探测、模型元数据、i18n parity |
| `imrelighter_gpu` | 隐藏 GLFW 窗口下的 Lit/Normal/Depth/Lighting/Shadow 金图像，PSNR ≥ 28 dB；平坦深度、深度方向、法线 flipX、透明 alpha、深度平滑 |
| `imrelighter_i18n_keys` | 扫描源码，确认两个 locale 的每个键都被引用 |

GPU 测试在无 OpenGL 4.5 环境时返回 77，CTest 将其标记为 **SKIP**，不会误报通过。

## GPU 金图像

金图像位于 `tests/golden/*.rgba`，测试程序在 `build/<preset>/tests/`。

重新生成：

1. 在 CTest 工作目录（通常 build 根目录）创建 `update-golden.flag`
2. 运行 `imrelighter_gpu_tests`
3. 删除 flag，重新运行比较

只有视觉行为应当改变时才重新生成金图像；纯粹重构不应更新基线。

## 深度平滑回归

`testDepthSmoothing()` 使用低对比棋盘深度，比较 `smooth=0` 与 `smooth=1` 的渲染结果，防止 `uImageSize` 未设置导致平滑退化为无操作。

## FP16

覆盖 0 / -0、正规数、最大值、溢出到 Inf、Inf、NaN、最小次正规数与下溢，并对所有有限 half 做 `halfToFloat → floatToHalf` 往返测试。

模型类型在设置中显示为 fp32 / fp16 / q4 / q4f16 / bnb4 / int8；支持 FLOAT32、FLOAT16 输入/输出。q4 是默认模型；q4f16、bnb4 可手动选择；int8 若依赖 ConvInteger 等未实现算子会在加载时显示 ONNX Runtime 错误。

## 推理基准

```text
build/mingw-release/tests/imrelighter_inference_bench.exe
```

本机参考（512×512 合成图，仅表示相对量级）：

```text
CPU  479.6 ms（warmup 458.5 ms）
DML   70.5 ms（warmup 339.3 ms）
```

DirectML 首次运行包含初始化与图优化，warmup 明显高于稳定值。

## i18n

- `tests/core_tests.cpp` 检查 `zh-CN.json` 与 `en-US.json` 键集合一致、关键值非空。
- `tests/check_i18n_keys.py` 检查未被源码引用的键；新增 UI 文本必须同步两份词典。
- 该脚本只依赖 Python 标准库；未找到 Python 时 CMake 不注册该测试。

## 手工冒烟

发布前建议至少验证：

- 无图片启动、中文路径启动。
- 打开 PNG/JPEG/WebP、多选、切换、关闭。
- 生成深度、取消推理、切换任务后结果不串写。
- 7 个视图、缩放/平移/F 适应、从灯光标记拖动。
- 设置模态、语言/主题切换、模型选择/清除。
- 导出 5 种 PNG，检查尺寸与方向。
- 无 OpenGL 4.5 / 无 DirectML / 模型缺失时的提示与回退。
