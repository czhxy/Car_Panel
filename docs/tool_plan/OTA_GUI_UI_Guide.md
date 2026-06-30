# OTA GUI UI 定制说明文档

## 概述

OTA GUI 支持 **Apple 玻璃质感 UI**（参照 iOS 设计风格），布局参数和配色方案分离管理：
- **布局** → `ui_config.json`（窗口尺寸、控件位置、字体大小）
- **配色** → `themes/*.json`（三套独立配色文件，支持热切换）

## 修改流程

```
1. 修改布局 → 编辑 tools/ota_gui/ui_config.json
2. 修改配色 → 编辑 tools/ota_gui/themes/apple_dark.json 等
3. 双击 tools/ota_gui/preview.bat 预览效果
4. 满意后双击 tools/ota_gui/build.bat 打包
```

## 主题系统

OTA GUI 内置三套主题，运行时可热切换（底部 ◉/◎/○ 按钮）：

| 主题键 | 名称 | 图标 | 文件 |
|--------|------|------|------|
| `apple_dark` | Apple 深色 | ◉ | `themes/apple_dark.json` |
| `apple_light` | Apple 浅色 | ◎ | `themes/apple_light.json` |
| `classic` | 经典灰色 | ○ | `themes/classic.json` |

默认主题由 `ui_config.json` 的 `theme.default` 字段控制。

### Apple 玻璃质感实现

由于 tkinter 不支持原生 CSS 渐变/模糊，通过以下方式模拟：

1. **Pillow 渐变图片** — `开始升级` / `取消升级` 按钮使用 PIL 生成的圆角渐变背景（上亮下暗）
2. **iOS 系统色系** — 参考 Apple Human Interface Guidelines 的语义色
3. **卡式面板** — `LabelFrame` 使用浅色卡片背景 + 细边框模拟分层
4. **大圆角 (10px)** — 按钮使用 PIL 渲染圆角矩形
5. **玻璃高光** — 按钮顶部叠加白色半透明渐变模拟反射

Pillow 不可用时，按钮退化为纯色（`classic` 主题无需 Pillow）。

---

## ui_config.json 参数说明

### window — 窗口设置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `title` | string | `"Car Panel OTA Tool v1.0"` | 窗口标题栏文字 |
| `width` | int | `720` | 窗口宽度（像素） |
| `height` | int | `680` | 窗口高度（像素） |
| `resizable` | bool | `false` | 是否允许调整窗口大小 |

### theme — 主题设置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `default` | string | `"apple_dark"` | 默认主题，可选 `apple_dark` / `apple_light` / `classic` |

### chip_panel — 芯片信息面板

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `show` | bool | `true` | 是否显示芯片信息面板 |
| `font_size` | int | `10` | 字体大小 |

### serial_panel — 串口设置面板

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `label_width` | int | `80` | 标签宽度 |
| `combo_width` | int | `15` | 下拉框宽度 |
| `button_width` | int | `8` | 按钮宽度 |
| `font_size` | int | `10` | 字体大小 |
| `baudrate_options` | list | `[9600, ...]` | 波特率选项 |
| `default_baudrate` | int | `115200` | 默认波特率 |

### file_panel — 文件选择面板

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `entry_width` | int | `55` | 文件路径输入框宽度 |
| `button_width` | int | `10` | 浏览按钮宽度 |
| `font_size` | int | `10` | 字体大小 |
| `drop_hint_text` | string | `"拖拽 .bin 文件到此处..."` | 拖拽提示文字 |

### progress_bar — 进度条

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `length` | int | `500` | 进度条长度 |
| `font_size` | int | `9` | 百分比文字大小 |

### log_panel — 日志面板

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `width` | int | `100` | 文本框宽度（字符数） |
| `height` | int | `18` | 文本框高度（行数） |
| `max_lines` | int | `5000` | 最大行数（超出自动裁剪） |
| `font_size` | int | `9` | 日志字体大小 |
| `timestamp_format` | string | `"%H:%M:%S"` | 时间戳格式 |

### button — 按钮尺寸

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `primary_width` | int | `20` | 主按钮宽度 |
| `danger_width` | int | `16` | 取消按钮宽度 |
| `font_size` | int | `10` | 按钮字体大小 |
| `padding` | int | `2` | 按钮内边距 |

---

## 主题 JSON 参数说明 (themes/*.json)

### 全局色

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `bg` | 窗口背景色 | `#1C1C1E` |
| `fg` | 主文字色 | `#FFFFFF` |
| `fg_secondary` | 次要文字色 | `rgba(235,235,245,0.6)` |
| `fg_tertiary` | 禁用文字色 | `rgba(235,235,245,0.3)` |

### 卡片面板

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `card_bg` | 卡片背景 | `#2C2C2E` |
| `card_border` | 卡片边框 | `rgba(255,255,255,0.08)` |

### 语义色

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `success` | 成功 | `#30D158` |
| `error` | 错误/警告 | `#FF453A` |
| `warning` | 警示 | `#FF9F0A` |
| `info` | 信息/浏览按钮 | `#64D2FF` |
| `accent` | 强调色/进度条 | `#0A84FF` |

### 按钮色

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `button_primary_bg` | 主按钮背景 | `#0A84FF` |
| `button_primary_fg` | 主按钮文字 | `#FFFFFF` |
| `button_primary_hover` | 主按钮悬停 | `#409CFF` |
| `button_primary_pressed` | 主按钮按下（渐变底部） | `#006ACC` |
| `button_danger_bg` | 危险按钮背景 | `#FF453A` |
| `button_danger_fg` | 危险按钮文字 | `#FFFFFF` |
| `button_danger_hover` | 危险按钮悬停 | `#FF6961` |
| `button_secondary_bg` | 次要按钮背景 | `rgba(255,255,255,0.1)` |
| `button_secondary_fg` | 次要按钮文字 | `#FFFFFF` |

### 进度条

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `progress_trough` | 进度条底色 | `#3A3A3C` |
| `progress_fill` | 进度条填充色 | `#0A84FF` |

### 日志区

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `log_bg` | 日志背景 | `#000000` |
| `log_fg` | 日志文字 | `#D4D4D4` |

### 输入控件

| 参数 | 说明 | Apple 深色默认 |
|------|------|----------------|
| `entry_bg` | 输入框背景 | `#3A3A3C` |
| `entry_fg` | 输入框文字 | `#FFFFFF` |
| `entry_border` | 输入框边框 | `rgba(255,255,255,0.12)` |
| `combobox_bg` | 下拉框背景 | `#3A3A3C` |
| `combobox_fg` | 下拉框文字 | `#FFFFFF` |

---

## 颜色格式

支持两种格式：

1. **十六进制**：`"#RRGGBB"` → 纯色
2. **RGBA 函数**：`"rgba(R, G, B, A)"` → 半透明（自动混合到背景色）

> 注意：`classic.json` 使用纯十六进制（无 rgba），因为经典主题无需半透明效果。

---

## 定制示例

### 示例 1：默认浅色主题

修改 `ui_config.json`：
```json
"theme": {
    "default": "apple_light"
}
```

### 示例 2：自定义按钮色（Apple 深色主题）

编辑 `themes/apple_dark.json`：
```json
"button_primary_bg": "#BF5AF2",
"button_primary_hover": "#D27BFF",
"button_primary_pressed": "#9B3FB5",
"accent": "#BF5AF2",
"progress_fill": "#BF5AF2"
```
把蓝色系换成紫色系。

### 示例 3：调整日志区风格

编辑 `themes/apple_light.json`：
```json
"log_bg": "#FFFFFF",
"log_fg": "#1D1D1F"
```
浅色主题下日志区也变白，更像原生 macOS 终端。

### 示例 4：隐藏芯片面板

修改 `ui_config.json`：
```json
"chip_panel": {
    "show": false
}
```

---

## 故障排查

| 问题 | 解决 |
|------|------|
| 主题不生效 | 检查 JSON 格式（逗号、引号），程序会用默认值继续 |
| Pillow 未安装 | 按钮退化为纯色，Apple 主题仍可用 |
| 打包后主题丢失 | 检查 `build.bat` 中 `--add-data "themes;themes"` |
| RGBA 颜色不生效 | 使用 `rgba(R, G, B, A)` 格式，A 为 0~1 小数 |
