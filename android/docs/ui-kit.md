# UI 组件与触摸约定

Android UI 完全由 Dear ImGui 绘制，但业务屏幕不直接拼装基础控件。可复用组件在 `app/src/main/cpp/ui/widgets.h/.cpp`，主题 token 在 `ui/theme.*`。

## 为什么使用 Dear ImGui

- 窗口、卡片、滑块、复选框、颜色选择器、下拉框、字体、裁剪和输入焦点都由 ImGui 负责。
- Kotlin `SurfaceView` 只负责 Surface/系统栏；OpenGL 负责最终绘制和图像 Pass。
- 对实时参数面板 + 画布交互，Immediate Mode 的“每帧根据状态重建 UI”模型简单可靠。
- ImGui 不是可替换的小组件，而是 UI 层本身；移除它等于重写整个界面层。

## 组件

### 容器

| 组件 | 用途 |
|---|---|
| `Panel` | RAII `BeginChild`，固定尺寸/滚动区域 |
| `Card` | 自动高度卡片，参数分组 |
| `DisabledScope` | RAII `BeginDisabled` / `EndDisabled` |
| `FontScale` | 临时字体缩放 |

### 表单

| 组件 | 用途 |
|---|---|
| `readOnlyField` | 只读值框 |
| `labelValueRow` | 标签 + 只读值 |
| `formSliderFloat` | 标签 + 浮点滑块 |
| `formSliderInt` | 标签 + 整数滑块 |
| `formCheckbox` | 标签 + 复选框 |
| `formColor` | 标签 + 颜色块/行内取色器 |
| `formCombo` | 标签 + 下拉框 |

### 操作

- `centeredButton`、`primaryButton`、`iconButton`、`closeButton`。
- `sectionTitle`、`formLabel`、`centeredText`、`mutedText`。
- `segmentButton`：视图/材质/光照模型等分段选择。
- `clearTouchTargets`、`registerTouchTarget`、`touchHitsTarget`：每帧可点击区域注册。
- `cancelActiveControl`：滚动接管时清除 ImGui active item。

## 触摸契约

1. 从已注册的直接控件（滑块圆点、画布）开始时，立即把按下事件交给 ImGui。
2. 从其他位置开始视为潜在滚动手势；不发送 mouse-down，滚过按钮不会误触。
3. 移动超过 slop 后调用 `cancelActiveControl()`，滑块失去 hover/active，只有滚动容器移动。
4. 点击在抬起时生效，不在按下时生效。
5. 滚动容器拥有手势直到抬起，即使手指跨入画布；只有离开源面板时取消。
6. 设置弹窗打开时，下层面板/画布的触摸被禁用；点击遮罩关闭设置。

### 滑块规则

- 只有从圆点附近开始拖动才改变值，点轨道不跳变，避免手机误触。
- 圆点命中区略大于绘制圆点。
- 拖动参数时 `interaction_active_` 为真：高度平滑使用快速核；阴影保持全步数、采样数减半实时刷新，松手后恢复完整采样。

## 滚动

- 任务抽屉、下方面板和设置使用自定义惯性滚动。
- 滚动条为 6dp 半透明圆角，可拖动。
- 下方面板垂直滚动；任务抽屉内部列表独立滚动。
- 惯性衰减在释放速度过低/触边时停止。

## 主题与语言

- `ui::palette()` 是唯一颜色来源，不能硬编码深色主题颜色。
- `ui::setThemeMode` 切换深色/浅色；`ui::tr` 提供中英文。
- 主题和语言由应用层持久化到 `theme.txt` / `language.txt`。
- 视图标签、设置、状态栏、模型描述和导出菜单应保持中英文覆盖。

## 当前边界

屏幕组合仍在 `AndroidApp::draw_*` 中；组件层可复用，但设置对话框、下方面板等还没有完全拆成独立类。新增界面应使用组件套件并遵守上面的触摸契约，不要直接添加裸 ImGui 控件。
