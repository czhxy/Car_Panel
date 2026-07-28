# LVGL 仪表盘 UI 实现计划

> 来源: Figma 设计稿 → dashboard_figma_spec.md → STM32F429 LVGL v8.3 移植
> 分支: `feature/lvgl`
> 生成时间: 2026-07-28 (实施前)

---

## 一、背景

将 Figma 设计的 240×320 汽车仪表盘移植到 STM32F429IGT6 的 LVGL v8.3 环境中，替换当前的 `lv_demo_widgets()`。

- 当前状态: LVGL 移植已完成，Widgets Demo 运行正常
- 目标: 显示与 Figma 一致的技术风格仪表盘
- 数据源: 动力域 ECU 通过 CAN 总线（500kbps）上报心跳帧（0x320）、电机状态帧（0x110）
- **Load Bar**: 是下发 RPM 的交互控件，0–100% 映射到 0–300 RPM 目标转速，通过 CAN 发送给动力域

---

## 二、文件清单

### 2.1 新建文件

| 文件 | 职责 |
|---|---|
| `tools/bin_to_c_array.py` | 一次性脚本: 将 pic/*.bin 转换为 C 数组 + lv_img_dsc_t |
| `pic/dashboard_images.h` | 所有图片的 lv_img_dsc_t 声明 |
| `pic/dashboard_images.c` | 所有图片的 const uint8_t[] C 数组定义 |
| `task/mod_dashboard_data.h` | 仪表盘共享状态结构体 + FreeRTOS 互斥锁 API |
| `task/mod_dashboard_data.c` | 仪表盘共享状态全局变量 + 锁实现 |
| `task/mod_dashboard_fault.h` | 故障码→消息映射表声明 |
| `task/mod_dashboard_fault.c` | 故障码→消息映射表实现（可扩展添加新故障码） |
| `task/task_dashboard_ui.h` | 仪表盘 UI 构建/更新 API |
| `task/task_dashboard_ui.c` | 仪表盘全部 UI 元素构建 + 周期更新 |

### 2.2 修改现有文件

| 文件 | 修改 |
|---|---|
| `task/task_lcd_demo.c` | 注释掉 `lv_demo_widgets()`，改为调用 `Dashboard_UI_Init(scr)` + `Dashboard_Update()` |
| `task/mod_comm_can.c` | 新增强符号 `ModCommCan_OnRxFrame()`，解析心跳帧 → 写入 g_dash_state |
| `protocol/CAN_Protocol.h` | 补充 UI 侧电机状态帧 ID 宏 |
| `third_lib/LVGL/lvgl/lv_conf.h` | 启用 `LV_FONT_MONTSERRAT_28` |
| `mdk/app.uvprojx` | 添加 dashboard_images.c, mod_dashboard_data.c, mod_dashboard_fault.c, task_dashboard_ui.c |

---

## 三、图片处理

### 3.1 当前状态

`pic/` 目录同时保留历史导出图片和本次新增图片。转换脚本只处理已在 `IMAGE_DEFS` 中定义的资源，其余历史 `.bin` 文件跳过；每个资源前 4 字节是 header，后面是 RGB565 像素数据（2 字节/像素）。

已验证的 header 原始字节:

| 文件 | Header 字节 (LE uint32) | 期望尺寸 | 验证: data_size |
|---|---|---|---|
| Top Bar.bin | `04 40 03 04` → `0x04034004` | 208×32 | 13316-4=13312, 208×32×2=13312 ✓ |
| Frame.bin | `04 E0 C1 0D` → `0x0DC1E004` | 120×110 | 26404-4=26400, 120×110×2=26400 ✓ |
| ODO.bin | `04 04 01 06` → `0x06010404` | 65×48 | 6244-4=6240, 65×48×2=6240 ✓ |
| Error Box.bin | `04 58 E2 02` → `0x02E25804` | 150×23 | 6904-4=6900, 150×23×2=6900 ✓ |
| mode.bin | `04 90 00 01` → `0x01009004` | ? | 580-4=576, 576/2=288px |

### 3.2 转换策略

由于 header 格式不确定（cf/尺寸位域布局未完全解码），采用以下步骤:

1. **编写 Python 脚本** `tools/bin_to_c_array.py`，读取 .bin 文件，跳过前 4 字节 header，输出为 C 数组
2. **手动指定**每张图片的宽高（从 Figma 设计稿已知尺寸），生成正确的 `lv_img_dsc_t`
3. mode.bin（档位 "D"）: Figma 档位框 38×28.5，但实际导出图片为 288 像素（可能是裁剪后的文本轮廓），尺寸由脚本尝试 16×18 / 18×16 / 12×24 等组合确定

### 3.3 图片使用决策

| 图片 | LVGL 实现 | 原因 |
|---|---|---|
| Top Bar.bin | `lv_img` 背景 | SPORT/档位已固化在图中，CAN 指示用 `lv_led` 叠放 |
| arc-bg.bin + arc-fill.bin | `lv_img` 叠加 | 替换 Frame.bin；RPM 数字由 LVGL 标签实时显示 |
| ODO/BATT/SOC.bin | **不使用** | 仅保留卡片和实时数值标签，图标创建代码置于 `#if 0` |
| can-dot_green.bin + can-dot_red.bin | `lv_img` 状态点 | 替换旧 CAN 状态点，在线闪烁、离线红色常亮 |
| can-label.bin | `lv_img` 文本 | 与状态点一起替换 Top Bar 中的 CAN 区域 |
| Turn Signals.bin | `lv_img` 背景 | 箭头背景，点击区用透明 `lv_obj` 覆盖 |
| mode.bin | `lv_img` | 档位 "D" 指示器 |
| **不使用** | | |
| Error Box.bin | **不使用** | 错误码区域按当前需求隐藏，原实现保留在 `#if 0` |
| load-bg/load-fill/load-label-*.bin | `lv_bar` + `lv_label` | 进度条用 LVGL 原生控件 |

---

## 四、数据架构

### 4.1 仪表盘共享状态

```c
// mod_dashboard_data.h

typedef enum {
    DASH_CARD_ODO = 0,
    DASH_CARD_BATT = 1,
    DASH_CARD_SOC = 2
} DashboardCard;

typedef struct {
    uint16_t rpm;               // 发动机转速 (RPM)，来自 CAN 电机状态帧
    uint8_t  motor_status;      // 状态位 (RUN/ENABLE/FAULT)
    uint16_t error_code;        // 当前故障码
    bool     motor_online;      // 动力域 CAN 通信是否在线 (心跳超时 1.5s = 离线)
    uint32_t odo_value;         // 累计圈数
    uint8_t  batt_level;        // 电池电量 % (暂静态 78)
    uint8_t  soc_level;         // SOC % (暂静态 85)
    uint8_t  load_pct;          // 负载 % (0–100, 映射到 0–300 RPM 目标)
    uint16_t rpm_target;        // 目标转速 (由 Load Bar 交互设定, 0–300 RPM)
    DashboardCard selected_card; // 当前选中卡片
} DashboardState;

extern DashboardState g_dash_state;
extern SemaphoreHandle_t g_dash_mutex;
```

### 4.2 CAN 数据流

**RX 方向（动力域 → 显示域）:**

```
动力域 CAN ──► CAN1_RX0_IRQ ──► CanRxQueue ──► Mod_Can_RxTask()
                                                      │
                                        ModCommCan_OnRxFrame() [强符号]
                                                      │
                                      ┌─ 心跳帧 (0x320): status, error_code
                                      │     → g_dash_state.error_code
                                      │     → g_dash_state.motor_online = true
                                      │
                                      └─ 电机状态帧 (0x110, 待动力域实现):
                                            → g_dash_state.rpm, odo_value
                                            → g_dash_state.motor_status
```

**TX 方向（显示域 → 动力域）:**

```
LCD_DEMO Task:
  │ Load Bar 交互 → g_dash_state.rpm_target 变化
  │ 调用 CanProtocol_WheelCtlSend(rpm_target) → CAN TX 队列
  │                                              │
  │                                     ModCommCan_Tx() 出队
  │                                              │
  │                                    CAN_Transmit() → 硬件邮箱
  │                                              │
  ▼                                     CAN 总线 → 动力域 ECU
```

**UI 刷新循环 (LCD_DEMO Task, 5ms):**

```
  │ 每 5 次循环 (25ms) 调用 Dashboard_Update()
  │ Dashboard_Data_Lock() → 读取快照 → Dashboard_Data_Unlock()
  │ 更新 RPM 数值、弧线、错误框颜色、卡片值等
  │ 检查 motor_online 超时 (1.5s)
```

### 4.3 线程安全

- CAN RX 任务（优先级 4）写入 `g_dash_state`
- LCD_DEMO 任务（优先级 3）读取 `g_dash_state`
- 使用 FreeRTOS 互斥锁保护，读/写前加锁，操作完释放
- 加锁时间极短（只拷贝结构体字段，不阻塞 LVGL 渲染）

---

## 五、UI 布局与实现

### 5.1 页面框架 (240×320)

```
scr_act (#05080d 深色底)
│
├── [y=7]  Top Bar    ← lv_img (img_top_bar, 208×32, 居中)
│   └── CAN 区域       ← 覆盖旧静态区域，叠加 CAN 状态点和文本图片
│
├── [y=45] Divider    ← lv_obj (208×1, rgba(0,204,255,0.08))
│
├── [y=49] Error Box  ← 已隐藏，原实现保留在 #if 0
│
├── [y=72] Gauge      ← 容器 (120×110, 居中)
│   ├── 圆弧背景/填充   ← lv_img (img_arc_bg) + lv_img (img_arc_fill)
│   ├── RPM 数值       ← lv_label ("6800", font_28)
│   └── RPM 单位       ← lv_label ("RPM")
│
├── [y=190] Bottom Cards  ← flex row 容器 (208×48, gap 6, 居中)
│   ├── ODO Card      ← 卡片/数值标签，图标已隐藏
│   ├── BATT Card     ← 卡片/数值标签，图标已隐藏
│   └── SOC Card      ← 卡片/数值标签，图标已隐藏
│
├── [y=240] Load Bar 刻度 ← lv_obj (208×12)
│   ├── 目标值标签       ← lv_label (y=253, 实时显示 RPM)
│   └── 滑块             ← lv_slider (y=270, 208×20, 0–100→0–300 RPM)
│
├── [y=292] Turn Signals  ← lv_img (img_turn_signals, 208×20)
│   ├── 左箭头点击区   ← 透明 lv_obj (104×20)
│   └── 右箭头点击区   ← 透明 lv_obj (104×20)
│
└── [y=312] Warning Dots   ← flex row 容器 (208×8)
    └── 6个圆点 (8×8), 颜色各异, 电池点亮
```

### 5.2 错误框颜色切换（当前隐藏）

| 状态 | 背景 | 边框 | 图标 | 文字 | 错误码 |
|---|---|---|---|---|---|
| 正常 | rgba(51,204,77,0.04) | rgba(51,204,77,0.12) | `#33CC4D` | `#33CC4D` 80% | `"OK"` `#33CC4D` |
| 故障 | rgba(245,66,54,0.04) | rgba(245,66,54,0.12) | `#F54236` | `#F54236` 80% | `"E002"` 白色 18% |

错误码区域当前按需求隐藏，原有颜色切换代码保留在 `#if 0` 中；CAN 状态点同步为在线绿色闪烁、离线红色常亮。

### 5.3 RPM 表盘

- 使用 `arc-bg.bin` + `arc-fill.bin` 两张 RGB565 图片叠加，替换原 `Frame.bin`
- RPM 数值: `Montserrat 28` 字体，居中叠放
- 更新: 每 25ms 从 `g_dash_state.rpm` 读取值并刷新 LVGL 标签；圆弧图片本身为静态资源

### 5.4 负载进度条（交互型 RPM 下发控件）

Load Bar **不是一个显示控件**，而是用于**设定目标 RPM** 的交互滑块:

- 使用 `lv_slider` 替代 `lv_bar`（支持触摸拖动）
- 范围: 0–100，每 1% 步进
- 映射: `rpm_target = (load_pct / 100.0) × 300`（100% = 300 RPM）
- 用户拖动滑块时，实时更新 `g_dash_state.rpm_target`
- CAN TX 路径: 滑块回调 → `g_dash_state.rpm_target` → `CanProto_SendFrame()` 发送电机控制帧（Mode ID 0x020）
- 视觉样式: 背景 `rgba(255,255,255,0.04)`，填充 `#00ccff`，圆角 2，高 4px
- 刻度标签: "0", "75", "150"（对应 RPM 映射值）

```c
// 示例代码
slider = lv_slider_create(scr);
lv_obj_set_size(slider, 208, 20);
lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 16, 270);
lv_slider_set_range(slider, 0, 100);
lv_slider_set_value(slider, 42, LV_ANIM_OFF);
lv_obj_add_event_cb(slider, on_load_change, LV_EVENT_VALUE_CHANGED, NULL);
```

### 5.5 转向灯按钮（卡片选择器）

点击行为:
- 左箭头 → 选中卡片左移: `selected = (selected - 1 + 3) % 3`
- 右箭头 → 选中卡片右移: `selected = (selected + 1) % 3`
- 选中卡片高亮（边框变亮为 `#00e5ff`），未选中保持暗色

### 5.6 警示指示灯

6 个 8×8 圆形，水平等距排列 (gap 26px)，初始状态: 电池(绿色)点亮（`opa=90%`），其余暗态（`opa=20%`）。

| 灯 | 颜色 | 说明 |
|---|---|---|
| ABS | `#ffeb3b` 黄 | 待 CAN 数据驱动 |
| ESC | `#ff9900` 橙 | 待 CAN 数据驱动 |
| 引擎 | `#f44336` 红 | 电机故障时点亮 |
| 电量 | `#4caf50` 绿 | 默认点亮 |
| 远光 | `#2196f3` 蓝 | 待 CAN 数据驱动 |
| 车门 | `#e91e63` 粉 | 待 CAN 数据驱动 |

---

## 六、故障码映射系统（可扩展）

### 6.1 设计

```c
// mod_dashboard_fault.h

typedef enum {
    FAULT_NONE  = 0,  // 无故障
    FAULT_INFO  = 1,  // 信息 (保持绿色)
    FAULT_WARN  = 2,  // 警告 (黄色)
    FAULT_ERROR = 3,  // 错误 (红色)
} FaultLevel;

typedef struct {
    uint16_t    code;
    const char *message;
    FaultLevel  level;
} FaultCodeEntry;

const char* Dashboard_Fault_Lookup(uint16_t error_code, FaultLevel *out_level);
```

### 6.2 当前映射表

| 故障码 | 消息 | 级别 |
|---|---|---|
| `0x0000` | "ALL SYSTEMS NORMAL" | NONE |
| `0x0001` | "CAN TIMEOUT" | ERROR |
| `0x0002` | "MOTOR STALL" | ERROR |
| `0x0004` | "ENCODER LOSS" | ERROR |

### 6.3 扩展方法

新增故障码只需在映射表中追加一行，无需修改 UI 逻辑:
```c
{ 0x0008, "OVERCURRENT",     FAULT_ERROR },
{ 0x0010, "OVERTEMP WARNING", FAULT_WARN },
```

---

## 七、实现步骤

### 阶段 1: 基础设施

| 步骤 | 任务 | 输出 |
|---|---|---|
| 1.1 | 编写 `tools/bin_to_c_array.py`，转换 .bin → `dashboard_images.h/c` | C 数组 + lv_img_dsc_t |
| 1.2 | 创建 `mod_dashboard_data.h/c` | DashboardState + 锁 API |
| 1.3 | 创建 `mod_dashboard_fault.h/c` | 故障码映射表 |
| 1.4 | 添加 4 个新 .c 到 Keil 工程 | 编译 0 error |
| 1.5 | 启用 `LV_FONT_MONTSERRAT_28` | 大字 RPM 显示 |

### 阶段 2: UI 构建

| 步骤 | 任务 | LCD 可见变化 |
|---|---|---|
| 2.1 | 创建 `task_dashboard_ui.c`，实现 `Dashboard_UI_Init(scr)` | 静态仪表盘全貌 |
| 2.2 | 修改 `task_lcd_demo.c`，注释 demo，调用 Dashboard_UI_Init | 仪表盘替换 demo |

### 阶段 3: 数据对接

| 步骤 | 任务 |
|---|---|
| 3.1 | 在 `mod_comm_can.c` 添加强符号 `ModCommCan_OnRxFrame()`，解析心跳帧 |
| 3.2 | 实现心跳超时检测 (1.5s 阈值) |
| 3.3 | 实现 `Dashboard_Update()`，25ms 周期从 g_dash_state 更新 UI |

### 阶段 4: 交互与动态

| 步骤 | 任务 |
|---|---|
| 4.1 | 实现错误框颜色切换 (红/绿) |
| 4.2 | 实现 CAN 指示灯闪烁 |
| 4.3 | 实现 RPM 弧线/数值动态更新 |
| 4.4 | 实现转向灯点击切换卡片 |

### 阶段 5: 验证与优化

| 步骤 | 验证方式 |
|---|---|
| 5.1 | 编译 0 error 0 warning |
| 5.2 | LCD 显示与 Figma 预览一致 |
| 5.3 | 串口日志验证 CAN 数据通路 |
| 5.4 | 修改 g_dash_state 测试值，验证错误框颜色切换 |
| 5.5 | FPS 监控 > 20，内存稳定不增长 |
| 5.6 | 运行 10 分钟无崩溃 |

---

## 八、已确认事项

| 问题 | 确认结果 |
|---|---|
| Load Bar 用途 | **交互型 RPM 下发控件**，0–100% → 0–300 RPM 目标，通过 CAN 发送电机控制帧 |
| Turn Signals 点击 | **仅 UI 卡片切换**，不发送 CAN 帧 |
| mode.bin (档位 "D") | Figma 档位框 38×28.5，实际导出像素尺寸由 Python 转换脚本尝试匹配（288 像素 = 16×18 或 18×16 等） |

## 九、关键技术约束

- **SRAM**: LVGL 48KB 池 + 38.4KB 双缓冲 ≈ 86KB，剩余 42KB 用于 FreeRTOS + 栈 + CAN 队列，安全余量约 10KB
- **图片存储**: 嵌入 .rodata (Flash)，约 60KB
- **字体**: Montserrat 28 ≈ +8KB Flash
- **任务栈**: 当前 4KB，LVGL 嵌套渲染可能较深，栈溢出时扩展到 8KB
- **DMA2D**: 已启用 GPU 加速，图片为 const 在 Flash，LVGL 逐字节读入 SRAM 缓冲再 DMA2D 绘制
