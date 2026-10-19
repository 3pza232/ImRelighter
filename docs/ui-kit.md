# UI 组件套件

ImRelighter 的界面由 `src/ui/widgets.*` 中的可复用组件和 `src/ui/theme.*` 中的设计 token 组成。`src/ui/ui_*.cpp` 只负责把业务状态翻译成组件调用，不直接拼装基础输入控件。

## Dear ImGui 在这个项目中的作用

- **全部界面都由 Dear ImGui 绘制**：窗口、侧栏、右栏、设置对话框、滑块、下拉框、颜色选择、字体、输入路由、裁剪和绘制命令。
- GLFW 只负责创建窗口、接收键鼠/拖放事件；OpenGL 只负责最终绘制和图像渲染。
- 因此 ImGui 不是“可有可无的小工具”，而是 UI 层本身。移除它等于重写整个 UI：布局、输入焦点、文本编辑、滑块、颜色选择、DPI 缩放和字体渲染都要重新实现。
- 当前工具是动态参数面板 + 画布交互，Immediate Mode 很适合这种“每帧根据状态重建界面”的结构；没有必须换 Qt/Win32 的理由。
- 如果未来要在别的 C++ 项目复用，可以复用 `widgets`、`theme` 和本文档，但仍然需要 Dear ImGui 作为 UI 运行时。

## 组件分层

### 容器与生命周期

| 组件 | 用途 |
|---|---|
| `Panel` | RAII `BeginChild`，用于固定尺寸面板、滚动区域 |
| `Card` | 自动高度卡片，用于右栏分组 |
| `Box` | 固定尺寸带边框容器 |
| `Dialog` | 居中浮层、遮罩和关闭按钮 |
| `DisabledScope` | RAII `BeginDisabled` / `EndDisabled` |
| `FontScale` | 临时放大/缩小当前字体 |

### 表单与属性行

| 组件 | 用途 |
|---|---|
| `readOnlyField` | 只读值框；没有下拉箭头，适合单值/不可选状态 |
| `labelValueRow` | 标签 + 只读值框 |
| `formSliderFloat` | 标签 + 全宽浮点滑条 |
| `formSliderInt` | 标签 + 全宽整数滑条（内部用浮点拖动，滑钮宽度统一） |
| `formCheckbox` | 标签 + 复选框 |
| `formColor` | 标签 + 颜色选择器（`#rrggbb`） |
| `formCombo` | 标签 + 下拉框 |
| `vector3Control` | X/Y/Z 三轴编辑，带轴颜色和自动高度布局 |

### 操作与布局

| 组件 | 用途 |
|---|---|
| `centeredButton` | 显式高度时文字垂直居中 |
| `iconButton` | 紧凑图标按钮 |
| `closeButton` | 全高危险按钮/关闭按钮 |
| `segmentButton` | 分段选择（主题、语言、渲染方案） |
| `sectionTitle` | 小节标题 + 分隔线 |
| `formLabel` | 固定标签宽度，右对齐到输入列 |
| `centeredText` | 在当前内容区水平居中 |

## 使用约定

- 标签文本由调用方传入，通常来自 `app.tr("key")`；组件本身不做 i18n。
- 每个控件都应有稳定 ID。`form*` 会内部 `PushID(id)`，调用方不需要重复绑定。
- 输入控件默认使用 `ImGui::SetNextItemWidth(-1.0f)`，自动占满右栏剩余宽度。
- 颜色全部来自 `theme::*` token，组件不写死调色板。
- 高 DPI 由 `theme::setScale()` + `style.ScaleAllSizes()` 统一处理，组件使用 ImGui 的 `FrameHeight` / `FramePadding`，不手写固定像素。
- 只读、单值或不可选字段使用 `readOnlyField` / `labelValueRow`，不要用只有一项的 `Combo`。

## 示例

```cpp
// 普通属性行
ui::formSliderFloat("exposure", app.tr("surface.exposure").c_str(),
                    doc->settings.exposure, -5.0f, 5.0f, "%.2f",
                    ui::scaled(160.0f));

ui::formCheckbox("shadows", app.tr("shading.shadows").c_str(),
                 doc->settings.shadows);

ui::formColor("ambient", app.tr("shading.ambient_color").c_str(),
              doc->settings.ambientColor, ui::scaled(160.0f));

// 只读状态
ui::labelValueRow(app.tr("panel.image.backend").c_str(),
                  backendLabel(app, app.config.backend).c_str(),
                  ui::scaled(160.0f));

// 整组禁用
ui::DisabledScope disabled(!hasDocument);
if (ui::formCombo("inference_size", app.tr("inference.size").c_str(),
                  sizeIndex, sizeLabels, count, ui::scaled(160.0f))) {
    // ...
}
```

## 复用方式

1. 复制 `src/ui/widgets.h`、`src/ui/widgets.cpp`、`src/ui/theme.h`、`src/ui/theme.cpp`。
2. 提供一个 Dear ImGui 上下文和渲染后端（项目当前是 GLFW + OpenGL 3/4 backend）。
3. 启动时调用 `theme::setMode()`、`theme::setScale()`、`theme::apply()`。
4. 业务 UI 只调用组件；不要在页面文件里重复写 `SliderFloat` / `Combo` / `BeginDisabled`。

## 边界

- 组件依赖 Dear ImGui 和本项目的 `theme` token，不依赖 `App`、`core::Document` 或渲染器，因此可以直接复制到其他项目。
- `scaled()` 和高 DPI 启动逻辑属于应用层；复用方需要按自己的 DPI 策略调用 `ImGuiStyle::ScaleAllSizes()`。
- 复杂专用控件（画布视图标签、任务卡、灯光标记）仍留在 `ui_canvas.cpp` / `ui_sidebar.cpp`，因为它们依赖业务数据和交互语义。
