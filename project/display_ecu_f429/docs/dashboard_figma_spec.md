# 汽车仪表盘 Figma 设计规格书

> 来源: Figma 文件 `8x7TEOMb7NLWzO7MwcGZSJ` / node `1:2`
> 画布: 240×320 px
> 主题: 深色科技风 (#05080d 底, #00e5ff 主色)

---

## 层级树 (Top-Down)

```
Tech Dashboard (240×320, #05080d)
│
├── bg-glow                     (装饰) 背景光晕, 200×200, (20,60), #008cff 4% + blur40
├── scan-0 ~ scan-318           (装饰) 扫描线 106 条, 240×1, 每 3px, rgba(0,0,0,0.01)
│
├── 1:116 Top Bar               (16,7), 208×32, 水平居中
│   ├── 1:118 SPORT             文字, Inter Medium 10px, rgba(0,229,255,0.6), tracking 0.8px
│   ├── 1:119 CAN Indicator     水平 flex, gap 4px
│   │   ├── 1:120 can-dot       绿点 6×6, rgba(77,179,77)
│   │   └── 1:121 CAN           文字, Inter Regular 8px, rgba(255,255,255,0.25)
│   └── 1:122 Gear Box          38×28.5, bg rgba(0,229,255,0.05), border rgba(0,229,255,0.15), radius 5
│       └── 1:123 D             文字, Inter Bold 19px, #00e5ff
│
├── 1:117 divider               (16,45), 208×1, rgba(0,204,255,0.08)
│
├── 1:178 Error Box             top=49, 150×23, 居中, bg rgba(245,66,54,0.04), border rgba(245,66,54,0.12)
│   ├── 1:179 error-icon        红方块 7×7, #f54236, 圆角 1.4
│   ├── 1:180 error-text        纵向 flex (flex:1)
│   │   ├── 1:181 MOTOR STATUS  文字, Inter Medium 5px, rgba(245,66,54,0.5)
│   │   └── 1:182 LEFT MOTOR:.. 文字, Inter Bold 7px, rgba(245,66,54,0.8)
│   └── 1:183 E002              文字, Inter Regular 6px, rgba(255,255,255,0.18)
│
├── 1:124 Gauge Frame           top=72, 120×110, 居中
│   ├── 1:125 arc-bg            弧形底, 98×98, rgba(0,204,255,0.06), stroke 6
│   ├── 1:126 arc-fill          弧形填充, 98×98, #00e5ff, stroke 6 (270度弧)
│   ├── 1:127 6800              RPM 数值, Inter Regular 30px, #00e5ff, (23.6,36.9)
│   └── 1:128 RPM               RPM 单位, Inter Medium 6.3px, rgba(255,255,255,0.3), tracking 1.3px
│
├── 1:168 Bottom Cards          top=190, 208×48, 居中, 水平 flex, gap 6
│   ├── 1:169 ODO Card          65×48, bg rgba(0,229,255,0.03), border rgba(0,229,255,0.06), radius 5
│   │   ├── 1:170 ODO           Label, Inter Regular 7px, rgba(255,255,255,0.2), (7,5)
│   │   └── 1:171 12,458        Value, Inter Bold 16px, #e0e0e0, (4,19)
│   ├── 1:172 BATTERY Card     65×48, 同上样式
│   │   ├── 1:173 BATT          Label, Inter Regular 7px, rgba(255,255,255,0.2), (7,5)
│   │   └── 1:174 78%           Value, Inter Bold 16px, #00e5ff, (15,19)
│   └── 1:175 SOC Card          65×48, 同上样式
│       ├── 1:176 SOC           Label, Inter Regular 7px, rgba(255,255,255,0.2), (7,5)
│       └── 1:177 85%           Value, Inter Bold 16px, #00e5ff, (14,19)
│
├── 1:138 Load Bar              (16,244), 208×28
│   ├── 1:139 load-bg           底条, 208×4, rgba(255,255,255,0.04), rounded 2
│   ├── 1:140 load-fill         填充, 90×4, #00ccff, rounded 2 (代表 42% = 90/208)
│   ├── 1:141 0                 刻度, Inter Regular 8px, rgba(255,255,255,0.2), (0,8)
│   ├── 1:142 25                刻度, (55,8)
│   ├── 1:143 50                刻度, (108,8)
│   └── 1:144 LOAD %            刻度, (178,8)
│
├── 1:145 Turn Signals          top=277, 208×20, 居中
│   ├── (left arrow)           12×14 Vector, rgba(0,229,255,0.12)
│   └── (right arrow)          12×14 Vector, rgba(0,229,255,0.15)
│
└── 1:148 Warning Dots          top=300, 208×16, 居中
    包含 6 个 8×8 圆形, 间距 34px:
    ├── dot-abs    (12,4) rgba(255,235,59,0.08)
    ├── dot-esc    (46,4) rgba(255,153,0,0.08)
    ├── dot-engine (80,4) rgba(244,67,54,0.08)
    ├── dot-batt   (114,4) rgba(76,175,80,0.9)  ← 激活
    ├── dot-beam   (148,4) rgba(33,150,243,0.08)
    └── dot-door   (182,4) rgba(233,30,99,0.08)
```

---

## 完整坐标速查表

| y 范围 | 元素 | 位置 | 尺寸 | 对齐 |
|---|---|---|---|---|
| 0-6 | 扫描线(装饰) | (0,0) | 240×1×106 | - |
| 7-39 | Top Bar | (0,7) | 208×32 | 水平居中 (`-translate-x-1/2 left-1/2`) |
| 45 | divider | (16,45) | 208×1 | left 16 |
| 49-72 | Error Box | (0,49) | 150×23 | 水平居中 |
| 72-182 | Gauge (RPM) | (0,72) | 120×110 | 水平居中 |
| 190-238 | Bottom Cards | (0,190) | 208×48 | 水平居中 |
| 244-272 | Load Bar | (16,244) | 208×28 | left 16 |
| 277-297 | Turn Signals | (0,277) | 208×20 | 水平居中 |
| 300-316 | Warning Dots | (0,300) | 208×16 | 水平居中 |

> "水平居中" = 使用 `lv_obj_center(obj)` 或 `lv_obj_set_align(obj, LV_ALIGN_TOP_MID)` 实现

---

## 颜色表 (LVGL lv_color_hex)

| 用途 | Figma 颜色 | LVGL 宏 |
|---|---|---|
| 主背景 | `#05080d` | `lv_color_hex(0x05080d)` |
| 主色 (cyan) | `#00e5ff` | `lv_color_hex(0x00e5ff)` |
| 主色 60% | `rgba(0,229,255,0.6)` | `lv_color_hex(0x00e5ff)` + opacity |
| 分隔线 | `rgba(0,204,255,0.08)` | `lv_color_hex(0x00ccff)` + opacity |
| 错误红 | `#f54236` | `lv_color_hex(0xf54236)` |
| 错误红底 | `rgba(245,66,54,0.04)` | 同上 + opacity |
| 卡片底 | `rgba(0,229,255,0.03)` | `lv_color_hex(0x00e5ff)` + opacity |
| 卡片边框 | `rgba(0,229,255,0.06)` | 同上 |
| 进度填充 | `#00ccff` | `lv_color_hex(0x00ccff)` |
| 文字白 | `#e0e0e0` | `lv_color_hex(0xe0e0e0)` |
| 文字暗白 | `rgba(255,255,255,0.2)` | `lv_color_hex(0xffffff)` + opacity |
| 指示灯-ABS | `#ffeb3b` | `lv_color_hex(0xffeb3b)` |
| 指示灯-ESC | `#ff9900` | `lv_color_hex(0xff9900)` |
| 指示灯-引擎 | `#f44336` | `lv_color_hex(0xf44336)` |
| 指示灯-电量 | `#4caf50` | `lv_color_hex(0x4caf50)` |
| 指示灯-远光 | `#2196f3` | `lv_color_hex(0x2196f3)` |
| 指示灯-车门 | `#e91e63` | `lv_color_hex(0xe91e63)` |

---

## 字体

| 元素 | Figma 设置 | LVGL 实现 |
|---|---|---|
| SPORT | Inter Medium 10px | Montserrat 14 + 缩放 |
| CAN 标签 | Inter Regular 8px | Montserrat 14 + 缩放 |
| 档位 D | Inter Bold 19px | 可启用更大字号或自定义 |
| RPM 数值 (6800) | Inter Regular 30px | 启用 Montserrat 28 字号 |
| RPM 单位 | Inter Medium 6.3px | 默认字体 + 缩放 |
| 卡片 Label | Inter Regular 7px | 默认字体 + 缩放 |
| 卡片 Value | Inter Bold 16px | Montserrat 14 |
| 错误标题 | Inter Medium 5px | 默认字体 + 缩放 |
| 错误信息 | Inter Bold 7px | 默认字体 + 缩放 |
| 刻度数字 | Inter Regular 8px | 默认字体 + 缩放 |

---

## 层级关系 (LVGL parent-child)

```
scr_act (lv_scr_act())
├── bg_gauge (lv_meter)
│   ├── scale
│   └── needle
├── lbl_rpm_val (lv_label)      "6800"
├── lbl_rpm_unit (lv_label)     "RPM"
├── cont_topbar (lv_obj)        208×32, flex row
│   ├── lbl_mode (lv_label)     "SPORT"
│   ├── cont_can (lv_obj)       flex row, gap 4
│   │   ├── led_can (lv_led)
│   │   └── lbl_can (lv_label)  "CAN"
│   └── cont_gear (lv_obj)      38×28
│       └── lbl_gear (lv_label) "D"
├── divider (lv_obj)           208×1 line
├── cont_error (lv_obj)         150×23, flex row
│   ├── icon_err (lv_obj)      7×7 red rect
│   ├── lbl_err_title (lv_label) "MOTOR STATUS"
│   ├── lbl_err_msg (lv_label)  "LEFT MOTOR: OVERCURRENT"
│   └── lbl_err_code (lv_label) "E002"
├── cont_bottom (lv_obj)        208×48, flex row
│   ├── card_odo (lv_obj)
│   │   ├── lbl (lv_label)     "ODO"
│   │   └── val (lv_label)     "12,458"
│   ├── card_batt (lv_obj)
│   │   ├── lbl (lv_label)     "BATT"
│   │   └── val (lv_label)     "78%"
│   └── card_soc (lv_obj)
│       ├── lbl (lv_label)     "SOC"
│       └── val (lv_label)     "85%"
├── bar_load (lv_bar)
├── cont_turn (lv_obj)          208×20
│   ├── arrow_l (lv_obj)
│   └── arrow_r (lv_obj)
├── cont_dots (lv_obj)          208×16, flex row
│   ├── dot_abs (lv_obj)       8×8 round
│   ├── dot_esc (lv_obj)
│   ├── dot_engine (lv_obj)
│   ├── dot_batt (lv_obj)      active
│   ├── dot_beam (lv_obj)
│   └── dot_door (lv_obj)
```

---

## LVGL 控件映射参考

| Figma 元素 | LVGL 控件 | 关键 API |
|---|---|---|
| 弧形表盘 + 指针 | `lv_meter` | `lv_meter_create`, `lv_meter_add_scale`, `lv_meter_add_needle_line` |
| RPM 数值文字 | `lv_label` | `lv_label_set_text_fmt` |
| 档位指示器 | `lv_obj` + `lv_label` | 圆角矩形容器 + 居中文字 |
| 错误信息框 | `lv_obj` + `lv_label` | 圆角矩形 + flex 布局 |
| 底部分块卡片 | `lv_obj` × 3 | 固定宽 65, flex row |
| 负载进度条 | `lv_bar` | `lv_bar_set_value` |
| 转向灯箭头 | `lv_obj` | 矢量或纯色三角形, 闪烁用 `lv_anim` |
| 指示灯圆点 | `lv_obj` × 6 | 8×8 圆形, 颜色切换 |
| 扫描线/分隔线 | `lv_obj` | 1px 高矩形 |
