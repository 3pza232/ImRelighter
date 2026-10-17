# Icons

当前为临时占位图标，深色底 + 暖色光点，后续可替换：

- `ImRelight.png`：256×256，窗口图标源文件。
- `ImRelight.ico`：多尺寸（16/32/48/64/128/256），exe 资源与任务栏图标。

替换时保持文件名不变即可；CMake `POST_BUILD` 会把整个 `assets/` 复制到产物目录。
