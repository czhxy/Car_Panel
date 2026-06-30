# OTA GUI 上位机 — 实现计划 (OTA GUI Plan)

## 一、Context

当前项目使用命令行 `ymodem_send.py` 工具通过 YMODEM-1K 协议进行串口 OTA 升级，每次需要手动输入 COM 端口号，操作不便。需要开发一个 GUI 桌面工具，在保留现有升级能力的同时增加：端口管理、芯片信息显示、文件拖拽、升级提醒等功能，并打包为独立 exe。

## 二、技术选型

| 项 | 选择 | 理由 |
|----|------|------|
| GUI 框架 | tkinter + ttk + tkinterdnd2 + Pillow | Python 内置，Pillow 用于渐变按钮 |
| 串口 | pyserial | 与现有工具一致，成熟稳定 |
| YMODEM | 复用现有 ymodem_send.py 核心函数，包装为类 | 不重复实现，保持协议一致性 |
| 主题 | 独立 JSON 配色文件 (3 套) + Pillow 渐变 | 亮/暗/经典三套，热切换 |
| 打包 | pyinstaller --onefile | 生成单文件 exe |
| 线程 | threading + queue.Queue | 串口通信在后台线程，UI 不阻塞 |

## 三、文件结构

```
project/display_ecu_f429/tools/ota_gui/
├── main.py                  # GUI 主程序，所有 UI 逻辑
├── ota_core.py              # YMODEM + ChipInfo 核心逻辑（从 ymodem_send.py 改造）
├── theme_manager.py         # 主题管理器（三套主题加载 + 热切换）
├── gradient_utils.py        # Pillow 渐变图片生成（按钮玻璃高光）
├── themes/                  # 主题配色目录
│   ├── apple_dark.json      # Apple 深色主题 (iOS 风格)
│   ├── apple_light.json     # Apple 浅色主题 (iOS 风格)
│   └── classic.json         # 经典灰色主题
├── ui_config.json           # UI 布局参数配置（不含颜色）
├── preview.bat              # 预览脚本
├── build.bat                # 一键打包 exe 脚本
├── ota_gui.spec             # pyinstaller spec
└── icon.ico                 # 程序图标（可选）
```

```
docs/tool_plan/
├── OTA_GUI_Plan.md          # 本计划文档
├── OTA_GUI_Acceptance.md    # 验收标准 Checklist
└── OTA_GUI_UI_Guide.md      # UI 定制说明文档
```

### 脚本化运维流程

```
修改 UI → 编辑 ui_config.json → 双击 preview.bat 预览 → 满意后双击 build.bat 打包
```

| 脚本 | 用途 | 执行内容 |
|------|------|---------|
| `preview.bat` | 预览 UI | `python main.py`（直接运行，无需每次打包） |
| `build.bat` | 打包 exe | 安装依赖 + `pyinstaller --onefile main.py` → 输出到 `dist/` |

## 四、详细设计

### 4.1 类结构

```
ota_core.py:
├── class OTASender          # YMODEM 发送器
│   ├── __init__(log_cb, progress_cb, cancel_flag)
│   ├── ymodem_send(ser, filepath) -> bool
│   ├── _crc16(data)
│   ├── _wait_for_byte(ser, expected, timeout, label)
│   ├── _wait_for_c(ser)     # 同时收集 MCU 启动日志
│   ├── _send_packet(ser, seq, data)
│   └── _send_filename_packet(ser, seq, data)
├── class ChipInfo           # 芯片信息查询（Phase 2）
│   ├── parse_boot_log(text) -> dict  # 解析 bootloader 启动日志
│   └── query_by_command(ser) -> dict # 通过查询指令获取（需 MCU 配合）

main.py:
├── class OTAApp(tk.Tk)      # 主窗口（继承 TkinterDnD.Tk 支持拖拽）
│   ├── __init__()            # 初始化：加载布局配置 → 主题管理器 → 创建 UI → 应用主题
│   ├── _load_layout_config() # 读取 ui_config.json（仅布局参数，不含颜色）
│   ├── _setup_ui()          # 创建所有 UI 组件（空白，不设颜色）
│   ├── _setup_layout()      # 按顺序 pack 所有面板
│   ├── _apply_theme()       # 从 ThemeManager 读取颜色并设置到所有 widget
│   ├── _on_theme_changed()  # 主题切换回调
│   ├── _toggle_theme()      # 循环切换三部主题
│   ├── ...                  # OTA、文件、日志等（同上）
└── if __name__ == "__main__":
    └── app = OTAApp()

theme_manager.py:
├── class ThemeManager       # 主题管理器
│   ├── __init__(default_theme)
│   ├── colors               # property: 当前主题颜色字典
│   ├── current / current_name / current_icon
│   ├── get(key)             # 获取颜色值（自动解析 rgba → hex）
│   ├── set_theme(key)       # 切换到指定主题
│   ├── toggle_next()        # 循环切换
│   └── add_listener()       # 注册切换监听

gradient_utils.py:
├── create_button_image(w, h, top, bottom, radius)  # 生成渐变圆角按钮背景
├── create_glass_overlay(w, h, radius)              # 生成玻璃高光叠加层
└── create_panel_bg(w, h, bg_color, radius)         # 生成圆角面板背景

themes/apple_dark.json:     # Apple 深色（~30 色值）
themes/apple_light.json:     # Apple 浅色（~30 色值）
themes/classic.json:         # 经典灰色（~30 色值）

ui_config.json:
├── window.title/window.width/window.height/resizable
├── theme.default             # 默认主题（"apple_dark"）
├── chip_panel                # 芯片信息面板布局
├── serial_panel              # 串口面板布局
├── file_panel                # 文件面板布局
├── progress_bar              # 进度条布局
├── log_panel                 # 日志面板布局
└── button                    # 按钮尺寸参数
注：配色（colors）已剥离到 themes/*.json，layout 仅控制尺寸/位置
```

### 4.2 GUI 窗口布局

```
+--------------------------------------------------+
|  Car Panel OTA Tool v1.0                    [_][X] |
+--------------------------------------------------+
|  [芯片信息面板]                                    |
|  MCU: STM32F429  活跃分区: App A                  |
|  BOOT: 0x08000000  APP: 0x08020000  Ver: v1.0.0  |
|  [获取芯片信息]                                    |
+--------------------------------------------------+
|  串口设置                                         |
|  COM端口: [▼ COM3 ] [刷新]  波特率: [▼ 115200]    |
+--------------------------------------------------+
|  固件文件                                         |
|  [/path/to/firmware.bin          ] [浏览...]      |
|  (支持拖拽 .bin 文件到此区域)                      |
|  文件: firmware.bin | 大小: 256KB | 修改时间: ...  |
+--------------------------------------------------+
|  [开始升级] [取消升级]                             |
|  [████████████          ] 45%  (128/256 KB)       |
+--------------------------------------------------+
|  日志                                              |
|  +----------------------------------------------+|
|  || 10:23:15 打开串口 COM3 (115200)             ||
|  || 10:23:16 等待 MCU 发送 'C'...               ||
|  || 10:23:18 收到 'C', 开始传输                  ||
|  || ...                                        ||
|  +----------------------------------------------+|
|  [清空日志] [保存日志]                             |
+--------------------------------------------------+
```

### 4.3 YMODEM 核心改造

**不改动原有 `ymodem_send.py`**，新建 `ota_core.py`：

1. 从 `ymodem_send.py` 复制所有常量和核心函数（`crc16`, `send_packet`, `send_filename_packet`）
2. 将 `ymodem_send()` 改造为接受回调参数：
   - `log_callback(msg: str)` — 替代所有 `print()` → 同时写 GUI 日志
   - `progress_callback(total_sent: int, filesize: int)` — 进度更新
   - `cancel_flag` — `threading.Event` 对象，用于取消
3. `wait_for_c()` 收到的非 'C' 字节同时输出到日志（可收集 bootloader 启动信息）
4. 在各等待点检查 `cancel_flag.is_set()`，若取消则清理并返回 False

### 4.4 线程安全设计

```
主线程 (GUI)                     后台线程 (OTA)
─────────────────────────────────────────────────
_start_ota()
  ├─ 创建 cancel_flag (Event)
  ├─ 创建 log_queue (Queue)       _ota_thread()
  ├─ 创建 progress_queue (Queue)    ├─ 打开串口
  ├─ 启动 Thread(ota_thread)        ├─ DTR 复位
  └─ 开始轮询队列 (after 100ms)     ├─ ymodem_send(ser, file, log_cb, progress_cb)
                                    │    ├─ log_cb → log_queue.put()
                                    │    └─ progress_cb → progress_queue.put()
                                    └─ 关闭串口 (finally)
  _poll_queues()  ← after() 循环
    ├─ log_queue → _log() 更新 Text widget
    ├─ progress_queue → _update_progress()
    └─ 线程结束 → 弹窗提示成功/失败
```

### 4.5 拖拽文件实现

- 安装 `tkinterdnd2`：`pip install tkinterdnd2`
- 在文件路径 Label/Entry 上绑定 `<<Drop>>` 事件
- 支持 Windows 文件管理器拖入，自动过滤 `.bin` 扩展名

### 4.6 芯片信息获取（Phase 2）

**方式一：解析 Bootloader 启动日志**
- `wait_for_c()` 在等待 'C' 期间会回显 MCU 的其他输出
- 这些输出即为 bootloader 的 printf 日志（包含分区、版本等信息）
- 用正则表达式提取关键字段

**方式二：查询指令（需 MCU 固件修改）**
- PC 发送：`[0xAA, 0x55, 0x01, 0x00]` (4 字节查询指令)
- MCU 返回：结构化数据包（参照 `ota_param_t` 结构体）

### 4.7 UI 参数外部化（`ui_config.json`）

所有可调的 UI 参数集中在一个 JSON 文件中，修改 UI 只需改配置 → 运行 `preview.bat` 看效果 → 运行 `build.bat` 打包。

## 五、量化验收标准

见 `OTA_GUI_Acceptance.md`（28 项 checklist）。

## 六、依赖清单

```
pyserial==3.5
tkinterdnd2==0.4.2
pyinstaller==6.x
```

## 七、关键文件路径

| 文件 | 路径 |
|------|------|
| 原始 YMODEM 工具 | `project/display_ecu_f429/tools/ymodem_send.py` |
| 新 OTA Core | `project/display_ecu_f429/tools/ota_gui/ota_core.py` |
| 新 GUI 主程序 | `project/display_ecu_f429/tools/ota_gui/main.py` |
| 配置文件 | `project/display_ecu_f429/tools/ota_gui/ui_config.json` |
| 打包脚本 | `project/display_ecu_f429/tools/ota_gui/build.bat` |
| 预览脚本 | `project/display_ecu_f429/tools/ota_gui/preview.bat` |
