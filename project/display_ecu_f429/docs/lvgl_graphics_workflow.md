# LVGL 图形设计与显示完整链路

## SDL 是否需要添加

**不需要。** SDL 是 LVGL 的 PC 端渲染后端，用于在 Windows/Linux/macOS 上模拟运行 LVGL（无需真实硬件即可调试 UI）。在 STM32F429 上，显示走的是 **纯软件渲染器（`draw/sw/`）+ ILI9341 SPI**，根本不涉及 SDL。

如果需要 PC 端模拟器（方便开发调试），那是另一套独立工程，不影响本次嵌入式工程。

---

## 仪表盘图形显示的执行链路

嵌入式 LVGL 下，图形的来源有三种方式：

### 方式一：LVGL 内置控件绘制（无外部资源）

```
┌─────────────┐    ┌──────────────┐    ┌──────────┐    ┌───────────┐
│ LVGL 控件    │───▶│ 纯软件渲染器  │───▶│ 帧缓冲    │───▶│ ILI9341   │
│ (arc/bar/   │    │ (draw/sw/)   │    │ (SRAM)    │    │ (SPI)     │
│  meter等)   │    │              │    │           │    │           │
└─────────────┘    └──────────────┘    └──────────┘    └───────────┘
```

**适用场景**：仪表盘上的刻度盘、进度条、指示灯、按钮等 UI 元素。直接用 LVGL widget API 创建，如：

```c
// 表盘示例
lv_obj_t * meter = lv_meter_create(lv_scr_act());
lv_meter_scale_t * scale = lv_meter_add_scale(meter);
lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_RED), -10);
lv_meter_set_scale_ticks(meter, scale, 21, 2, 10, lv_palette_main(LV_PALETTE_GREY));
```

### 方式二：外部图片 → 取模 → 嵌入代码

```
┌──────────┐    ┌──────────────┐    ┌───────────────┐    ┌──────────┐
│ 设计图片  │───▶│ 导出 PNG/BMP │───▶│ C 数组取模     │───▶│ 编译进   │
│ (PS/AI/  │    │ 240x320 RGB  │    │ (在线转换器)   │    │ Flash    │
│  Figma)  │    │              │    │               │    │          │
└──────────┘    └──────────────┘    └───────────────┘    └──────────┘
                                                            │
                    ┌───────────────────────────────────────┘
                    ▼
              ┌──────────┐    ┌──────────┐    ┌───────────┐
              │ lv_img   │───▶│ 渲染器    │───▶│ ILI9341   │
              │ 控件显示  │    │          │    │           │
              └──────────┘    └──────────┘    └───────────┘
```

**适用场景**：
- 仪表盘背景图（静态底图）
- Logo、图标、启动画面
- 复杂纹理（如木纹、碳纤维纹理）

**具体操作**：

1. 用 Photoshop / Illustrator / Figma / GIMP 设计图形，画布设为 240×320（或对应控件区域尺寸）

2. 导出为 PNG（推荐）或 BMP，RGB 色彩

3. 用 LVGL 官方在线转换器转为 C 数组：
   - 地址：`https://lvgl.io/tools/imageconverter`
   - 选择 `True color`（RGB565 对应 `CF_TRUE_COLOR`）
   - 输出格式选 `C array`

4. 将生成的 `.c` 文件放入工程（如 `assets/img_dashboard_bg.c`），并声明：
   ```c
   LV_IMG_DECLARE(dashboard_bg);
   ```

5. 在代码中显示：
   ```c
   lv_obj_t * img = lv_img_create(lv_scr_act());
   lv_img_set_src(img, &dashboard_bg);
   ```

> **注意**：240×320 全屏 RGB565 图片 = 240 × 320 × 2 = **150KB Flash**。F429 有 2MB Flash，经得起，但建议尽量用方式一画 UI。

### 方式三：LVGL Canvas 自绘（程序化像素绘制）

```
┌──────────┐    ┌──────────────┐    ┌───────────┐    ┌───────────┐
│ 程序代码  │───▶│ Canvas 缓冲  │───▶│ lv_img    │───▶│ ILI9341   │
│ (算法画   │    │ (SRAM)      │    │ 控件显示   │    │           │
│  点/线/弧) │    │             │    │           │    │           │
└──────────┘    └──────────────┘    └───────────┘    └───────────┘
```

**适用场景**：
- 实时波形/曲线（如 CAN 信号监测）
- 指针动画
- 自定义图表

```c
// 创建 canvas
static lv_color_t cbuf[240 * 100];  // 100 行缓冲区
lv_obj_t * canvas = lv_canvas_create(lv_scr_act());
lv_canvas_set_buffer(canvas, cbuf, 240, 100, LV_IMG_CF_TRUE_COLOR);

// 自绘内容
lv_draw_line_dsc_t line_dsc;
lv_draw_line_dsc_init(&line_dsc);
lv_canvas_draw_line(canvas, points, 5, &line_dsc);
```

---

## 实际仪表盘开发推荐链路

对于本次汽车仪表盘项目：

```
第 1 步：仪表盘背景
  Figma/PS 设计 → PNG → 在线取模 → 放入 flash → lv_img 显示

第 2 步：动态 UI 元素（速度表盘、转速、指示灯）
  直接用 lv_meter / lv_arc / lv_bar / lv_led 控件

第 3 步：数据驱动
  CAN RX 任务 → 解析电机数据 → 更新 LVGL 控件值
  例如：lv_meter_set_indicator_value(meter, indic, speed);

第 4 步：渲染循环
  Task_LCD_Demo 中每 5ms 调用 lv_timer_handler()
  LVGL 自动计算脏区域，调用 disp_flush() 把变化部分写入 ILI9341
```

### 软件推荐

| 用途 | 软件 | 说明 |
|---|---|---|
| UI 原型/布局 | Figma（免费） | Web 端，无需安装，导出 PNG |
| 像素级位图 | Photoshop / GIMP | 导出 BMP/PNG |
| 矢量图标 | Illustrator / Inkscape | 导出为位图或 SVG→PNG |
| 取模工具 | [lvgl.io/tools/imageconverter](https://lvgl.io/tools/imageconverter) | 在线，支持格式转换 + C 数组输出 |
| 取模工具（离线） | SquareLine Studio 内置 | 有导出功能 |
| 字体取模 | [lvgl.io/tools/fontconverter](https://lvgl.io/tools/fontconverter) | TTF 字体转 LVGL 点阵字库 |

### 完整数据流

```
┌──────────────────────────────────────────────────────────────────┐
│                       PC 端（设计阶段）                           │
│                                                                  │
│  Figma/PS 设计 UI  →  导出 PNG  →  LVGL 在线取模  →  .c 数组    │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ▼ .c 文件加入 Keil 工程
┌──────────────────────────────────────────────────────────────────┐
│                      STM32F429 端（运行时）                       │
│                                                                  │
│  CAN RX ───▶ 解析电机数据 ───▶ 更新 LVGL 控件值                   │
│                                    │                             │
│  lv_img_create()  ←──  背景图.c   │   lv_meter_set_value()       │
│                                    │   lv_arc_set_value()        │
│                                    │   lv_label_set_text()       │
│                                    ▼                             │
│                           lv_timer_handler()                     │
│                                    │                             │
│                                    ▼                             │
│                          lv_refr.c 脏区域计算                     │
│                                    │                             │
│                                    ▼                             │
│                    lv_draw_sw 纯软件渲染到帧缓冲                   │
│                                    │                             │
│                                    ▼                             │
│                    disp_flush() → SPI → ILI9341 屏幕              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 注意事项

1. **Flash 消耗**：全屏 RGB565 背景图 = 150KB，仪表盘留 384KB 给 App 分区，可以放 2-3 张大图。建议背景图用 JPEG 压缩或减少色深
2. **SRAM 消耗**：当前配置 48KB LVGL 内存池 + 9.6KB 显示双缓冲 ≈ 60KB，占主 SRAM 的 47%，够用但注意后续不要开太多 Canvas
3. **渲染性能**：240×320 @ 180MHz，纯软件渲染可达 30+ FPS，仪表盘足够
4. **设计规范**：由于仅启用了 Montserrat 14 字体，设计的文字标注请以 14px 为准（可后续增加字号）
