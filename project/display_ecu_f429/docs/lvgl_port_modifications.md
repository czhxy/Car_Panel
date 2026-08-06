# LVGL 移植与修改说明（显示域 ECU）

> 版本：LVGL **v8.3.11** ｜ 屏幕：ILI9341 **240×320**（竖屏）｜ 颜色：RGB565（`LV_COLOR_DEPTH 16`）
> 工程位置：`third_lib/LVGL/lvgl/`（含官方 demos/examples，工程只链接使用其中部分）

本文档回答两个问题：
1. **渲染频率与数据更新频率为什么不同**（各是多少、由什么决定）；
2. 从官方源码到本工程，对 LVGL 做了**哪些修改、各有什么作用**。

---

## 一、渲染循环 vs 数据更新：两个独立周期

### 1.1 Task_UI 主循环（`task/task_ui.c`）

```c
while (1) {
    lv_tick_inc(now - last_tick);   // ① 注入系统 tick
    lv_timer_handler();             // ② 驱动 LVGL 内部定时器（含显示刷新）
    if ((loop_cnt % 5) == 0) {      // ③ 每 5 次循环 = 25ms
        Dashboard_Update();         //    从共享状态刷新仪表盘动态元素
    }
    loop_cnt++;
    vTaskDelay(5ms);                // ④ 让出 CPU
}
```

主循环以 **5ms** 为周期轮询。注意 ①②③ 的周期并不相同：

| 环节 | 周期 | 由谁决定 | 作用 |
|---|---|---|---|
| tick 注入 `lv_tick_inc()` | 5ms（随主循环） | Task_UI 主循环 | LVGL 内部时间基准（动画/定时器/超时） |
| `lv_timer_handler()` 轮询 | 5ms（随主循环） | Task_UI 主循环 | 驱动 LVGL 全部内部定时器 |
| **LVGL 显示刷新 timer** | **30ms** | `lv_conf.h` `LV_DISP_DEF_REFR_PERIOD` | 有失效区域时执行重绘 |
| **数据更新 `Dashboard_Update()`** | **25ms** | `loop_cnt % 5 == 0` | 从 `g_dash_state` 刷新 RPM、CAN 灯等动态控件 |

### 1.2 为什么"数据更新"和"渲染"频率不同

这是**生产者-消费者解耦**的设计：

- **数据更新（25ms）**：`Dashboard_Update()` 只把共享状态里的新值写进 LVGL 控件（改文本、改颜色、改样式）。它**不直接画屏幕**。
- **渲染（≤30ms）**：改控件 → LVGL 内部把对应区域标记为"失效（invalidate）"→ 显示刷新 timer（30ms 周期）到点时，把所有失效区域**合并重绘**，交给 `disp_flush()` 写到 LCD。

两者**互不同步、各自独立**：
- 25ms 的数据更新周期决定"状态刷新多快"（比如 RPM 数字多久变一次）；
- 30ms 的刷新周期决定"画面重绘多快"（状态变了以后多久上屏）；
- 因为刷新是按失效区域增量做的，如果 25ms 内连续改了同一区域，30ms 到点时**只重绘一次**，不会重复绘制。

> 结论：简历/描述中的"LVGL 25ms/帧"表述不准确。准确说法是：
> **仪表盘动态数据每 25ms 刷新一次；LVGL 渲染循环每 5ms 轮询、画面重绘周期由 `LV_DISP_DEF_REFR_PERIOD`（30ms）决定，且按失效区域增量绘制。**

### 1.3 本工程相关关键配置（`lv_conf.h`）

| 配置 | 值 | 说明 |
|---|---|---|
| `LV_COLOR_DEPTH` | 16 | RGB565，与 ILI9341 像素格式一致 |
| `LV_MEM_SIZE` | 48KB | LVGL 内部堆（`lv_malloc`），独立于 FreeRTOS 的 64KB CCM 堆 |
| `LV_TICK_CUSTOM` | 0 | 不用自定义 tick，由 `lv_tick_inc()` 手动注入（Task_UI 主循环） |
| `LV_DISP_DEF_REFR_PERIOD` | 30 | 显示刷新 timer 周期（ms） |
| `LV_INDEV_DEF_READ_PERIOD` | 30 | 输入设备读取周期（ms） |
| `LV_FONT_DEFAULT` | montserrat_14 | 默认字体 |
| `LV_FONT_MONTSERRAT_28` | 1 | **本工程打开**，用于 RPM 大数字 |
| `LV_USE_GPU_STM32_DMA2D` | 1 | **本工程打开**，启用 DMA2D 加速 |

---

## 二、`lv_timer_handler()` 内部做了什么

`lv_timer_handler()`（`lvgl/src/misc/lv_timer.c:67`）是 LVGL 的**定时器泵**，每次调用执行一次"tick 批处理"。它本身**不直接画屏幕**，画屏发生在它触发的显示刷新 timer 回调里。逐步拆解：

1. **重入保护**：`already_running` 静态标志。若上一次调用还没返回（例如在某个 timer 回调里又调了一次），直接返回 1，避免并发执行破坏定时器链表。
2. **全局开关检查**：`lv_timer_run == false`（被 `lv_timer_enable(false)` 关过）则立即返回。
3. **tick 失速告警**：记录 `handler_start = lv_tick_get()`；若返回 0，累计 100 次后打 `LOG_WARN("It seems lv_tick_inc() is not called.")`——用来发现"忘了喂 tick"的移植错误。
4. **遍历所有 timer**：遍历 LVGL 全局定时器链表 `_lv_timer_ll`，对每个 timer 调 `lv_timer_exec()`：
   - 到期（`lv_timer_time_remaining` 已到 0）→ 执行它的回调；
   - 单次 timer（`repeat_count==1`）执行完被删除；
   - 若某回调创建/删除了 timer，则**从头重新遍历**，保证链表一致性。
5. **计算下次间隔**：扫一遍所有未暂停 timer，取最小剩余时间 `time_till_next`（即"最紧迫的下一个 timer 还有多久"）。
6. **空闲率统计**：累计 `busy_time`，每 `IDLE_MEAS_PERIOD` 算一次 `idle_last`（LVGL 用它判断系统空闲，供 API 查询）。
7. **返回 `time_till_next`**：告诉调用方"最晚什么时候再调一次"。本工程主循环固定每 5ms 调一次，忽略返回值（5ms < 绝大多数 timer 周期，不会饿死任何 timer）。

**显示刷新 timer 是关键成员**：它由 `lv_port_disp_init()` 注册的显示驱动自动创建，周期 30ms。每次到点：
- 若无失效区域 → 跳过（零开销）；
- 若有 → `lv_refr_now()` 合并并重绘所有失效区域，把像素写进双缓冲，随后调 `disp_flush()` 送到 ILI9341。

---

## 三、对官方 LVGL 做的修改及作用

### 3.1 `lv_conf.h` — 配置开关

| 修改 | 作用 |
|---|---|
| `LV_USE_GPU_STM32_DMA2D 0 → 1` | 启用 STM32 DMA2D 硬件加速（颜色填充/混合），降低 CPU 占用 |
| `LV_GPU_DMA2D_CMSIS_INCLUDE` 填 `"stm32f4xx.h"` | 让 DMA2D 驱动能找到 CMSIS 设备头（官方留空需自己填） |
| `LV_FONT_MONTSERRAT_28 0 → 1` | 打开 28 号字体内置，供 RPM 大数字标签使用 |

### 3.2 `examples/porting/lv_port_disp.c` — 显示驱动移植

| 修改 | 作用 |
|---|---|
| 双缓冲 `240×10 → 240×40` 行 | 各 19.2KB、共 **38.4KB**（静态分配在主 SRAM，DMA2D 可访问）；更大的缓冲让 LVGL 一次重绘更多行，减少刷新切块次数 |
| `disp_flush()` 批量 SPI 写 | CS/RS 一次拉起，逐像素 `SPI_SendData` 写（高字节/低字节），结束后 `lv_disp_flush_ready()` 通知 LVGL 继续 |
| 新增 `disp_flush_enabled` + `disp_enable_update()/disp_disable_update()` | 可整屏"冻结/解冻"刷新（移植自 benchmark demo），当前应用层未调用，预留作画面暂停钩子 |
| `disp_init()` 仅占位 | LCD 硬件初始化统一在 `BSP_SPI_LCD_Init()`（task_entry 中）完成，避免重复初始化 |

### 3.3 `examples/porting/lv_port_indev.c` — 触摸输入移植

| 修改 | 作用 |
|---|---|
| 仅保留触摸板设备 | 删除鼠标/键盘/编码器/按钮等无关设备的移植骨架 |
| `touchpad_read()` 读 FT6336G | 每次轮询调用 `tp_dev.scan()`，`TP_PRES_DOWN` 位判按下，上报首个触点坐标；**240×320 竖屏无需旋转坐标** |

### 3.4 `src/draw/stm32_dma2d/lv_gpu_stm32_dma2d.c` — DMA2D GPU 驱动适配

| 修改 | 作用 |
|---|---|
| 追加 `DMA2D_CR_MODE_Pos` 等 5 个 CMSIS 位域 `_Pos` 宏 | 兼容本工程用的旧版 STM32F4 CMSIS 位域定义，保证 DMA2D 寄存器位操作可编译 |
| 时钟使能宏追加 `STM32F429_439xx` | Keil 设备定义用的是 `STM32F429_439xx`（而非官方写的 `STM32F4`），追加后 DMA2D 时钟才真正打开 |
| `lv_area_get_offset` 初始化改 `.x=/.y=` | 把 C99 指定初始化写成 armcc V5 兼容写法 |
| `lv_draw_stm32_dma2d_img()` 改为直接返回 `LV_RES_INV` | **禁用图片的 DMA2D 路径**：官方实现要求源/目标 32 字节对齐、且无 D-Cache 清理支持，在 F429 上强行走 DMA2D 图片易花屏/失败，改为**回退软件绘制**（颜色填充/混合仍走 DMA2D） |

### 3.5 `task/task_ui.c` — 使用层（LVGL 生命周期）

| 环节 | 作用 |
|---|---|
| `lv_init()` → `lv_port_disp_init()` → `lv_port_indev_init()` | 按顺序完成 LVGL 核心、显示驱动、输入驱动初始化 |
| `Dashboard_UI_Init(lv_scr_act())` | 一次性构建仪表盘静态元素（背景、CAN 灯、表盘图、RPM 标签、Load Bar、Pause 按钮等） |
| 主循环 | 5ms tick + `lv_timer_handler()` + 25ms `Dashboard_Update()`（见第一节） |

---

## 四、一句话总结

| 修改点 | 核心作用 |
|---|---|
| 双缓冲 38.4KB（静态 SRAM） | 支持 DMA2D、减少刷屏切块 |
| DMA2D 使能 + 图片路径禁用 | 填色/混合硬件加速；规避对齐/缓存问题导致的图片花屏 |
| 手动 `lv_tick_inc()`（`LV_TICK_CUSTOM=0`） | 由 FreeRTOS 任务喂 tick，不依赖裸机 SysTick |
| 刷新周期 30ms / 数据更新 25ms | 渲染与数据解耦，增量重绘省带宽 |
| FT6336G 单触点直读 | 触摸坐标即屏幕坐标，无需旋转 |
