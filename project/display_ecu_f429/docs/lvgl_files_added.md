# LVGL 添加文件清单与必要性分析

## 本次新增的 Keil 分组及文件（共 117 个文件，11 个分组）

### 1. `lvgl_core` (15 files) — **必须**

LVGL 核心模块，负责对象管理、事件分发、布局计算、刷新调度，是整个 LVGL 的骨架。

| 文件 | 用途 |
|---|---|
| `lv_disp.c` | 显示器管理 |
| `lv_event.c` | 事件系统 |
| `lv_group.c` | 对象编组 |
| `lv_indev.c` | 输入设备管理 |
| `lv_indev_scroll.c` | 输入设备滚动 |
| `lv_obj.c` | 基础对象 |
| `lv_obj_class.c` | 对象类系统 |
| `lv_obj_draw.c` | 对象绘制 |
| `lv_obj_pos.c` | 对象位置/尺寸 |
| `lv_obj_scroll.c` | 对象滚动 |
| `lv_obj_style.c` | 对象样式 |
| `lv_obj_style_gen.c` | 对象样式（自动生成） |
| `lv_obj_tree.c` | 对象树管理 |
| `lv_refr.c` | 刷新调度 |
| `lv_theme.c` | 主题管理 |

---

### 2. `lvgl_draw` (25 files) — **必须（仅 sw 部分）**

绘图引擎。`src/draw/sw/` 下的 13 个文件是纯软件渲染器（无硬件依赖），**必须编译**。`src/draw/` 根目录下的 12 个文件是绘图公共逻辑，也**必须**。

| 子目录 | 文件数 | 必要性 |
|---|---|---|
| `src/draw/` (根) | 12 | **必须** — 绘图公共接口与算法 |
| `src/draw/sw/` | 13 | **必须** — 纯软件渲染器，LVGL 唯一启用的渲染后端 |

**未添加的 GPU 后端**（arm2d、sdl、stm32_dma2d、swm341_dma2d）均已在 `lv_conf.h` 中禁用（`LV_USE_GPU_xxx=0`），不需要。

---

### 3. `lvgl_extra` (25 files) — **全部需要**

扩展功能，均已在 `lv_conf.h` 中启用（`LV_USE_xxx=1`）：

| 子模块 | 文件数 | 启用宏 |
|---|---|---|
| `lv_extra.c` | 1 | 入口注册 |
| `layouts/flex/` | 1 | `LV_USE_FLEX=1` |
| `layouts/grid/` | 1 | `LV_USE_GRID=1` |
| `themes/basic/` | 1 | `LV_USE_THEME_BASIC=1` |
| `themes/default/` | 1 | `LV_USE_THEME_DEFAULT=1` |
| `themes/mono/` | 1 | `LV_USE_THEME_MONO=1` |
| `widgets/` | 19 | 各控件均启用（animimg、calendar、chart、meter 等） |

--- 未添加的 extra 子模块（libs/、others/）均在 `lv_conf.h` 中禁用（`LV_USE_PNG=0`、`LV_USE_SNAPSHOT=0` 等），不需要。

---

### 4. `lvgl_font` (4 files) — **必须**

| 文件 | 用途 |
|---|---|
| `lv_font.c` | 字体基础 |
| `lv_font_fmt_txt.c` | 字体格式解析 |
| `lv_font_loader.c` | 字体加载器 |
| `lv_font_montserrat_14.c` | Montserrat 14 号（`LV_FONT_DEFAULT`） |

其余 Montserrat 字号（8、10、12、16-48）均已在 `lv_conf.h` 中设为 0，不需要编译。如需启用更多字号，需在 `lv_conf.h` 中设为 1 并将对应 `.c` 文件加入工程。

---

### 5. `lvgl_hal` (3 files) — **必须**

| 文件 | 用途 |
|---|---|
| `lv_hal_disp.c` | 显示硬件抽象 |
| `lv_hal_indev.c` | 输入设备硬件抽象 |
| `lv_hal_tick.c` | tick 时钟管理 |

---

### 6. `lvgl_misc` (22 files) — **必须**

杂项工具箱：动画、内存分配（TLSF）、定时器、样式、文本处理、printf、颜色、文件系统等。全部底层依赖，**必须编译**。

---

### 7. `lvgl_widgets` (15 files) — **全部需要**

基础控件。均已在 `lv_conf.h` 中启用（`LV_USE_BTN=1`、`LV_USE_LABEL=1` 等），**必须编译**。

---

### 8. `lvgl_porting` (2 files) — **必须（本项目实现）**

| 文件 | 用途 |
|---|---|
| `lv_port_disp.c` | 本项目实现的 ILI9341 SPI 显示刷新 |
| `lv_port_indev.c` | 本项目实现的 FT6336G 触摸读取 |

--- 未添加 `lv_port_fs.c`：文件系统接口，因所有 FS 选项均在 `lv_conf.h` 中禁用，不需要。

---

### 9. `lvgl_config` (2 files) — 仅用于工程浏览

| 文件 | FileType | 说明 |
|---|---|---|
| `lv_conf.h` | 5 (header) | LVGL 配置文件，**不编译**，仅方便在 Keil 中编辑 |
| `lvgl.h` | 5 (header) | LVGL 顶层头文件，**不编译**，仅方便在 Keil 中浏览 |

> 注意：这两个是头文件，FileType 已设为 5（Header），不会被 armcc 编译。它们通过 `#include` 和 include path 被引用。

---

### 10. `lvgl_demo_widgets` (4 files) — **当前需要（运行 Demo）**

| 文件 | 用途 |
|---|---|
| `lv_demo_widgets.c` | Widgets 演示主逻辑 |
| `img_clothes.c` | Demo 图片资源 |
| `img_demo_widgets_avatar.c` | Demo 图片资源 |
| `img_lvgl_logo.c` | LVGL logo 图片资源 |

> 后续仪表盘 UI 开发完成后，可移除此分组。

---

### 11. `lvgl_app` (0 files) — 预留

空分组，预留给未来仪表盘 UI 代码（如 `task_display.c`）的存放位置。当前无文件。

---

## 本次编译错误修复

### Error 1: `"../draw/nxp/vglite/lv_draw_vglite.h"` 找不到

**原因**：`lv_hal_disp.c` 中无条件 `#include` NXP VG Lite / PXP / Renesas GPU 头文件，而这些目录已被项目删除。

**修复**：在 `lv_hal_disp.c` 中将所有 GPU 后端头文件 include 包裹 `#if LV_USE_GPU_xxx` 条件编译：

```c
// 修复前（无条件的 include）
#include "../draw/nxp/vglite/lv_draw_vglite.h"  // 报错！

// 修复后
#if LV_USE_GPU_NXP_VG_LITE
#include "../draw/nxp/vglite/lv_draw_vglite.h"
#endif
```

同理处理了 SDL、STM32_DMA2D、SWM341_DMA2D、ARM2D、NXP_PXP、RA6M3_G2D 的 include。

### Error 2/3: `lv_conf.h` / `lvgl.h` 被当作源文件编译

**原因**：Keil 工程中这两个头文件的 FileType 被设为 1（C source），导致 armcc 尝试编译 `.h` 文件并报错。

**修复**：将 FileType 从 1 改为 5（Header），两个 Target 均已修正。
