#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
OTA GUI 主程序 — 基于 tkinter + tkinterdnd2
Apple 风格玻璃质感 UI，支持亮/暗/经典三套主题热切换。
所有 UI 布局参数从 ui_config.json 读取，配色从 themes/ 目录加载。
"""

import os
import sys
import json
import time
import threading
import queue
from datetime import datetime
from tkinter import messagebox, filedialog

# ======================== 兼容标准 tkinter 和 tkinterdnd2 ========================
try:
    from tkinterdnd2 import TkinterDnD
    _HAS_DND = True
except ImportError:
    TkinterDnD = None
    _HAS_DND = False

import tkinter as tk
from tkinter import ttk

import serial
import serial.tools.list_ports

from ota_core import OTASender, ChipInfo
from theme_manager import ThemeManager
from gradient_utils import create_button_image

# ======================== 获取资源绝对路径 ========================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def _get_config_path():
    """获取 ui_config.json 路径，兼容 PyInstaller 打包"""
    if getattr(sys, 'frozen', False):
        return os.path.join(sys._MEIPASS, 'ui_config.json')
    return os.path.join(BASE_DIR, 'ui_config.json')


# ======================== 主窗口 ========================

# 选择基类：有 tkinterdnd2 则继承 TkinterDnD.Tk，否则退化为 tk.Tk
if _HAS_DND:
    _BaseTk = TkinterDnD.Tk
else:
    _BaseTk = tk.Tk


class OTAApp(_BaseTk):
    """OTA 工具主窗口 — Apple 风格玻璃质感"""

    # 按钮渐变高度（pixel）
    BTN_HEIGHT = 34

    def __init__(self):
        super().__init__()

        # 1. 加载布局配置（仅布局参数，不含颜色）
        self._cfg = self._load_layout_config()

        # 2. 初始化主题管理器（默认主题从 ui_config.json 读取）
        default_theme = self._cfg.get("theme", {}).get("default", "apple_dark")
        self._theme = ThemeManager(default_theme=default_theme)
        self._theme.add_listener(lambda _: self._on_theme_changed())

        # 3. ttk 样式初始化（使用 'clam' 主题以支持颜色定制）
        self._style = ttk.Style()
        try:
            self._style.theme_use('clam')
        except Exception:
            pass

        # 4. 窗口配置
        self._apply_window_config()

        # 5. 状态变量
        self._setup_variables()

        # 6. 创建所有 UI 控件（此时暂不设置颜色）
        self._setup_ui()

        # 7. 首次应用主题（设置所有颜色 + 生成渐变图片）
        self._apply_theme()

        # 8. 布局（按顺序 pack）
        self._setup_layout()

        # 9. 枚举串口
        self._refresh_ports()

        # 10. 窗口关闭事件
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ======================== 布局配置加载 ========================

    def _load_layout_config(self) -> dict:
        """从 ui_config.json 加载布局参数（颜色从 theme 加载）"""
        default = {
            "window": {"title": "OTA Tool", "width": 720, "height": 680, "resizable": False},
            "chip_panel": {"show": True, "font_size": 10},
            "serial_panel": {"label_width": 80, "combo_width": 15, "button_width": 8,
                             "font_size": 10, "baudrate_options": [9600, 19200, 38400, 57600, 115200, 460800, 921600],
                             "default_baudrate": 115200},
            "file_panel": {"entry_width": 55, "button_width": 10, "font_size": 10,
                           "drop_hint_text": "拖拽 .bin 文件到此处或点击浏览选择"},
            "progress_bar": {"length": 500, "font_size": 9},
            "log_panel": {"width": 100, "height": 18, "font_size": 9, "max_lines": 5000,
                          "timestamp_format": "%H:%M:%S"},
            "button": {"primary_width": 20, "danger_width": 16, "font_size": 10, "padding": 2},
        }
        try:
            config_path = _get_config_path()
            if os.path.exists(config_path):
                with open(config_path, 'r', encoding='utf-8') as f:
                    user_cfg = json.load(f)
                for section, values in user_cfg.items():
                    if section in default and isinstance(default[section], dict):
                        default[section].update(values)
                    else:
                        default[section] = values
        except Exception as e:
            print(f"加载配置失败，使用默认值: {e}")
        return default

    def _apply_window_config(self):
        wc = self._cfg["window"]
        self.title(wc["title"])
        self.geometry(f"{wc['width']}x{wc['height']}")
        self.resizable(wc["resizable"], wc["resizable"])

    # ======================== 状态变量 ========================

    def _setup_variables(self):
        self.port_var = tk.StringVar()
        self.baud_var = tk.IntVar(value=self._cfg["serial_panel"]["default_baudrate"])
        self.file_path = tk.StringVar()
        self._busy = False           # 是否正在执行 OTA 或查询
        self.cancel_event = threading.Event()
        self.ota_thread = None
        self.chip_query_thread = None
        self.log_queue = queue.Queue()
        self.progress_queue = queue.Queue()

        # 存储 PIL 图片引用（防止被 GC 回收）
        self._btn_images: dict = {}

    # ======================== UI 控件创建 ========================

    def _setup_ui(self):
        """创建所有 UI 控件（不设置颜色，颜色由 _apply_theme 统一设置）"""
        # --- 芯片信息面板 ---
        self._setup_chip_panel()

        # --- 串口设置面板 ---
        self._setup_serial_panel()

        # --- 文件选择面板 ---
        self._setup_file_panel()

        # --- 按钮 + 进度条 ---
        self._setup_action_area()

        # --- 日志面板 ---
        self._setup_log_panel()

    def _setup_chip_panel(self):
        cp = self._cfg["chip_panel"]
        if not cp["show"]:
            self.chip_frame = None
            return

        self.chip_frame = tk.LabelFrame(self, text="芯片信息",
                                        font=("Microsoft YaHei UI", 10, "bold"))

        self.chip_vars = {
            "mcu": tk.StringVar(value="MCU: --"),
            "partition": tk.StringVar(value="活跃分区: --"),
            "boot_addr": tk.StringVar(value="BOOT: --"),
            "app_addr": tk.StringVar(value="APP: --"),
            "version": tk.StringVar(value="版本: --"),
            "query_time": tk.StringVar(value=""),
        }

        inner = tk.Frame(self.chip_frame)
        inner.pack(fill=tk.X, padx=12, pady=8)

        row1 = tk.Frame(inner)
        row1.pack(fill=tk.X, pady=2)
        tk.Label(row1, textvariable=self.chip_vars["mcu"],
                 font=("Consolas", cp["font_size"])).pack(side=tk.LEFT, padx=(0, 20))
        tk.Label(row1, textvariable=self.chip_vars["partition"],
                 font=("Consolas", cp["font_size"])).pack(side=tk.LEFT, padx=(0, 20))
        tk.Label(row1, textvariable=self.chip_vars["version"],
                 font=("Consolas", cp["font_size"])).pack(side=tk.LEFT)

        row2 = tk.Frame(inner)
        row2.pack(fill=tk.X, pady=2)
        tk.Label(row2, textvariable=self.chip_vars["boot_addr"],
                 font=("Consolas", cp["font_size"])).pack(side=tk.LEFT, padx=(0, 20))
        tk.Label(row2, textvariable=self.chip_vars["app_addr"],
                 font=("Consolas", cp["font_size"])).pack(side=tk.LEFT, padx=(0, 20))
        tk.Label(row2, textvariable=self.chip_vars["query_time"],
                 font=("Consolas", cp["font_size"] - 1),
                 foreground="gray").pack(side=tk.LEFT)

        self.chip_query_btn = tk.Button(self.chip_frame, text="获取芯片信息",
                                        command=self._query_chip_info,
                                        font=("Microsoft YaHei UI", 9),
                                        relief=tk.FLAT, bd=0, padx=12, pady=5)
        self.chip_query_btn.pack(side=tk.RIGHT, padx=12, pady=8)

    def _setup_serial_panel(self):
        sp = self._cfg["serial_panel"]

        self.serial_frame = tk.LabelFrame(self, text="串口设置",
                                          font=("Microsoft YaHei UI", 10, "bold"))

        # COM 端口行
        tk.Label(self.serial_frame, text="COM 端口:",
                 font=("Microsoft YaHei UI", sp["font_size"]),
                 width=sp["label_width"] // 10).pack(side=tk.LEFT, padx=(12, 4), pady=10)

        self.port_combo = ttk.Combobox(self.serial_frame, textvariable=self.port_var,
                                       width=sp["combo_width"], state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=2, pady=10)

        self.refresh_btn = tk.Button(self.serial_frame, text="刷新", command=self._refresh_ports,
                                     font=("Microsoft YaHei UI", sp["font_size"]),
                                     relief=tk.FLAT, bd=0, width=sp["button_width"], padx=8, pady=5)
        self.refresh_btn.pack(side=tk.LEFT, padx=(6, 24), pady=10)

        # 波特率行
        tk.Label(self.serial_frame, text="波特率:",
                 font=("Microsoft YaHei UI", sp["font_size"]),
                 width=sp["label_width"] // 10).pack(side=tk.LEFT, padx=(0, 4), pady=10)

        self.baud_combo = ttk.Combobox(self.serial_frame, textvariable=self.baud_var,
                                       state="readonly", values=sp["baudrate_options"],
                                       width=sp["combo_width"])
        self.baud_combo.set(str(sp["default_baudrate"]))
        self.baud_combo.pack(side=tk.LEFT, padx=2, pady=10)

    def _setup_file_panel(self):
        fp = self._cfg["file_panel"]

        self.file_frame = tk.LabelFrame(self, text="固件文件",
                                        font=("Microsoft YaHei UI", 10, "bold"))

        # 文件路径输入 + 浏览按钮
        row1 = tk.Frame(self.file_frame)
        row1.pack(fill=tk.X, padx=12, pady=(10, 4))

        self.file_entry = tk.Entry(row1, textvariable=self.file_path,
                                   font=("Consolas", fp["font_size"]),
                                   width=fp["entry_width"], relief=tk.FLAT)
        self.file_entry.pack(side=tk.LEFT, padx=(0, 6))

        self.browse_btn = tk.Button(row1, text="浏览...", command=self._browse_file,
                                    font=("Microsoft YaHei UI", fp["font_size"]),
                                    relief=tk.FLAT, bd=0,
                                    width=fp["button_width"], padx=8, pady=5)
        self.browse_btn.pack(side=tk.LEFT)

        # 文件信息 / 拖拽提示
        self.file_info_label = tk.Label(self.file_frame,
                                        text=fp["drop_hint_text"],
                                        font=("Microsoft YaHei UI", max(fp["font_size"] - 1, 8)))
        self.file_info_label.pack(fill=tk.X, padx=12, pady=(0, 10))

        # 拖拽支持
        if _HAS_DND:
            self.drop_target_register('*')
            self.file_entry.drop_target_register('*')
            self.file_info_label.drop_target_register('*')
            self.dnd_bind('<<Drop>>', self._on_drag_drop)
            self.file_entry.dnd_bind('<<Drop>>', self._on_drag_drop)
            self.file_info_label.dnd_bind('<<Drop>>', self._on_drag_drop)

        # 监听文件路径变化
        self.file_path.trace_add('write', self._on_file_path_changed)

    def _setup_action_area(self):
        btn_cfg = self._cfg["button"]
        pb_cfg = self._cfg["progress_bar"]

        # 按钮区域
        self.action_frame = tk.Frame(self)

        self.start_btn = tk.Button(self.action_frame, text="开始升级",
                                   command=self._start_ota,
                                   font=("Microsoft YaHei UI", btn_cfg["font_size"], "bold"),
                                   relief=tk.FLAT, bd=0, padx=28, pady=10,
                                   width=btn_cfg["primary_width"])
        self.start_btn.pack(side=tk.LEFT, padx=(0, 10))

        self.cancel_btn = tk.Button(self.action_frame, text="取消升级",
                                    command=self._cancel_ota,
                                    font=("Microsoft YaHei UI", btn_cfg["font_size"]),
                                    relief=tk.FLAT, bd=0, padx=24, pady=10,
                                    width=btn_cfg["danger_width"],
                                    state=tk.DISABLED)
        self.cancel_btn.pack(side=tk.LEFT)

        # 进度条区域
        self.progress_frame = tk.Frame(self)

        self.progress_bar = ttk.Progressbar(self.progress_frame,
                                            length=pb_cfg["length"], mode="determinate")
        self.progress_bar.pack(fill=tk.X, padx=(0, 8))

        self.progress_label = tk.Label(self.progress_frame, text="0% (0/0 KB)",
                                       font=("Consolas", pb_cfg["font_size"]), width=20)
        self.progress_label.pack()

    def _setup_log_panel(self):
        lp = self._cfg["log_panel"]

        self.log_frame = tk.LabelFrame(self, text="日志",
                                       font=("Microsoft YaHei UI", 10, "bold"))

        # 日志文本框 + 滚动条
        text_frame = tk.Frame(self.log_frame)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=12, pady=6)

        self.log_text = tk.Text(text_frame, font=("Consolas", lp["font_size"]),
                                width=lp["width"], height=lp["height"],
                                wrap=tk.WORD, state=tk.DISABLED, relief=tk.FLAT,
                                borderwidth=0, padx=6, pady=4)
        scrollbar = tk.Scrollbar(text_frame, orient=tk.VERTICAL,
                                 command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # 底部按钮栏
        btn_frame = tk.Frame(self.log_frame)
        btn_frame.pack(fill=tk.X, padx=12, pady=(0, 10))

        self.clear_log_btn = tk.Button(btn_frame, text="清空日志",
                                       command=self._clear_log,
                                       font=("Microsoft YaHei UI", 9),
                                       relief=tk.FLAT, bd=0, padx=10, pady=5)
        self.clear_log_btn.pack(side=tk.LEFT, padx=(0, 8))

        self.save_log_btn = tk.Button(btn_frame, text="保存日志",
                                      command=self._save_log,
                                      font=("Microsoft YaHei UI", 9),
                                      relief=tk.FLAT, bd=0, padx=10, pady=5)
        self.save_log_btn.pack(side=tk.LEFT)

        # 主题切换按钮（右对齐）
        self.theme_btn = tk.Button(btn_frame, command=self._toggle_theme,
                                   font=("Microsoft YaHei UI", 9),
                                   relief=tk.FLAT, bd=0, padx=10, pady=5)
        self.theme_btn.pack(side=tk.RIGHT)

    def _setup_layout(self):
        """按顺序 pack 所有面板"""
        f = {"padx": 12, "pady": (12, 6), "fill": tk.X}

        if self.chip_frame:
            self.chip_frame.pack(**f)
        self.serial_frame.pack(**f)
        self.file_frame.pack(**f)
        self.action_frame.pack(fill=tk.X, padx=16, pady=(12, 0))
        self.progress_frame.pack(fill=tk.X, padx=16, pady=6)
        self.log_frame.pack(fill=tk.BOTH, expand=True, padx=12, pady=(6, 12))

    # ======================== 主题系统 ========================

    def _apply_theme(self):
        """根据当前主题设置所有 widget 颜色和样式"""
        t = self._theme  # ThemeManager

        # --- 窗口全局背景 ---
        self.configure(bg=t.get("bg"))

        # --- ttk 样式 ---
        self._configure_ttk_styles()

        # --- 芯片信息面板 ---
        if self.chip_frame:
            self._style_frame(self.chip_frame, t.get("card_bg"), t.get("card_border"),
                              t.get("fg"))
            for child in self.chip_frame.winfo_children():
                self._style_recursive(child, t.get("card_bg"))
            # chip_vars 的 label 颜色
            fg = t.get("fg")
            for var in self.chip_vars.values():
                # 通过 trace 更新实际 label
                pass
            self._style_button(self.chip_query_btn, t.get("button_secondary_bg"),
                               t.get("button_secondary_fg"), t.get("button_secondary_hover"),
                               is_secondary=True)

        # --- 串口面板 ---
        self._style_frame(self.serial_frame, t.get("card_bg"), t.get("card_border"),
                          t.get("fg"))
        for child in self.serial_frame.winfo_children():
            if isinstance(child, tk.Button):
                continue  # separately styled
            self._style_recursive(child, t.get("card_bg"))
        self._style_button(self.refresh_btn, t.get("button_secondary_bg"),
                           t.get("button_secondary_fg"), t.get("button_secondary_hover"),
                           is_secondary=True)

        # --- 文件面板 ---
        self._style_frame(self.file_frame, t.get("card_bg"), t.get("card_border"),
                          t.get("fg"))
        for child in self.file_frame.winfo_children():
            self._style_recursive(child, t.get("card_bg"))
        self.file_entry.configure(bg=t.get("entry_bg"), fg=t.get("entry_fg"),
                                  insertbackground=t.get("fg"),
                                  readonlybackground=t.get("entry_bg"))
        # 文件信息 label 颜色（运行时可能被 _on_file_path_changed 覆盖）
        self.file_info_label.configure(bg=t.get("card_bg"),
                                       fg=t.get("fg_secondary"))
        self._style_button(self.browse_btn, t.get("button_secondary_bg"),
                           t.get("button_secondary_fg"), t.get("button_secondary_hover"),
                           is_secondary=True)

        # --- 操作区 ---
        self.action_frame.configure(bg=t.get("bg"))
        self._style_button(self.start_btn, t.get("button_primary_bg"),
                           t.get("button_primary_fg"), t.get("button_primary_hover"),
                           is_primary=True)
        self._style_button(self.cancel_btn, t.get("button_danger_bg"),
                           t.get("button_danger_fg"), t.get("button_danger_hover"),
                           is_danger=True)
        self.progress_frame.configure(bg=t.get("bg"))
        self.progress_label.configure(bg=t.get("bg"), fg=t.get("fg"))

        # --- 日志面板 ---
        self._style_frame(self.log_frame, t.get("card_bg"), t.get("card_border"),
                          t.get("fg"))
        # 日志文字
        self.log_text.configure(bg=t.get("log_bg"), fg=t.get("log_fg"),
                                insertbackground=t.get("log_fg"))
        # 日志按钮
        self._style_button(self.clear_log_btn, t.get("button_secondary_bg"),
                           t.get("button_secondary_fg"), t.get("button_secondary_hover"),
                           is_secondary=True)
        self._style_button(self.save_log_btn, t.get("button_secondary_bg"),
                           t.get("button_secondary_fg"), t.get("button_secondary_hover"),
                           is_secondary=True)

        # 主题切换按钮
        self._update_theme_btn()

        # --- 刷新渐变图片（按钮需要延迟到 window 实际大小确定后） ---
        self.after(50, self._refresh_button_gradients)

    def _on_theme_changed(self):
        """ThemeManager 通知主题已切换"""
        self._apply_theme()

    def _toggle_theme(self):
        """切换主题"""
        self._theme.toggle_next()

    def _update_theme_btn(self):
        """更新主题按钮文字"""
        icon = self._theme.current_icon
        name = self._theme.current_name
        self.theme_btn.configure(text=f"  {icon}  {name}  ")
        self._style_button(self.theme_btn, self._theme.get("button_secondary_bg"),
                           self._theme.get("button_secondary_fg"),
                           self._theme.get("button_secondary_hover"),
                           is_secondary=True)

    def _refresh_button_gradients(self, retry_count=0):
        """为按钮生成 PIL 渐变背景图片（支持重试，防止布局未完成时尺寸为 1）"""
        try:
            import gradient_utils as gu
            if not gu._HAS_PIL:
                return

            t = self._theme
            w1 = self.start_btn.winfo_width()
            h1 = self.start_btn.winfo_height()
            w2 = self.cancel_btn.winfo_width()
            h2 = self.cancel_btn.winfo_height()

            # 如果按钮尚未完成布局（宽/高 < 20），延迟重试
            if (w1 < 20 or h1 < 10 or w2 < 20 or h2 < 10) and retry_count < 8:
                self.after(100, lambda: self._refresh_button_gradients(retry_count + 1))
                return

            # 主要按钮
            if w1 > 20 and h1 > 10:
                img = gu.create_button_image(w1, h1,
                                              t.get("button_primary_bg", "#0A84FF"),
                                              t.get("button_primary_pressed", "#006ACC"))
                if img:
                    self._btn_images["start"] = img
                    self.start_btn.configure(image=img, compound=tk.CENTER)

            # 危险按钮
            if w2 > 20 and h2 > 10:
                img = gu.create_button_image(w2, h2,
                                              t.get("button_danger_bg", "#FF453A"),
                                              t.get("button_danger_hover", "#FF6961"))
                if img:
                    self._btn_images["cancel"] = img
                    self.cancel_btn.configure(image=img, compound=tk.CENTER)

        except Exception:
            pass

    def _configure_ttk_styles(self):
        """配置 ttk 样式（Combobox, Progressbar）"""
        t = self._theme

        # Combobox
        self._style.configure("TCombobox",
                              fieldbackground=t.get("combobox_bg"),
                              background=t.get("combobox_bg"),
                              foreground=t.get("combobox_fg"),
                              arrowcolor=t.get("combobox_arrow"),
                              selectbackground=t.get("accent"),
                              selectforeground=t.get("fg"))

        # Combobox 下拉列表
        self._style.map("TCombobox",
                        fieldbackground=[("readonly", t.get("combobox_bg"))],
                        foreground=[("readonly", t.get("combobox_fg"))])

        # Progressbar
        self._style.configure("TProgressbar",
                              background=t.get("accent"),
                              troughcolor=t.get("progress_trough"),
                              bordercolor=t.get("card_border"),
                              lightcolor=t.get("accent"),
                              darkcolor=t.get("progress_trough"))

        # LabelFrame
        self._style.configure("TLabelframe",
                              background=t.get("card_bg"))

    # ======================== 样式辅助方法 ========================

    def _style_frame(self, frame, bg: str, border: str, fg: str):
        """统一设置 Frame/LabelFrame 样式"""
        frame.configure(bg=bg)
        if isinstance(frame, tk.LabelFrame):
            frame.configure(fg=fg)

    def _style_button(self, btn: tk.Button, bg: str, fg: str,
                      hover_bg: str = None, is_primary: bool = False,
                      is_danger: bool = False, is_secondary: bool = False):
        """统一设置 Button 样式（包括 hover 效果）"""
        btn.configure(bg=bg, fg=fg, activebackground=hover_bg or bg,
                      activeforeground=fg, disabledforeground=self._theme.get("fg_tertiary"))

    def _style_recursive(self, widget, parent_bg: str):
        """递归设置 widget 子控件的背景色"""
        try:
            if isinstance(widget, (tk.Label, tk.Frame)):
                widget.configure(bg=parent_bg)
            if isinstance(widget, tk.Label):
                # 保留已有 fg，只设置 bg
                pass
        except Exception:
            pass
        for child in widget.winfo_children():
            self._style_recursive(child, parent_bg)

    # ======================== 串口操作 ========================

    def _refresh_ports(self):
        """枚举可用 COM 端口"""
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])
        self._log(f"检测到 {len(ports)} 个串口: {', '.join(ports) if ports else '无'}")

    # ======================== 文件操作 ========================

    def _browse_file(self):
        """打开文件选择对话框"""
        path = filedialog.askopenfilename(
            title="选择固件文件",
            filetypes=[("固件文件", "*.bin"), ("所有文件", "*.*")]
        )
        if path:
            self.file_path.set(path)

    def _on_drag_drop(self, event):
        """处理文件拖拽"""
        raw = event.data.strip()
        if raw.startswith('{') and raw.endswith('}'):
            raw = raw[1:-1]
        if raw.lower().endswith('.bin'):
            self.file_path.set(raw)
        else:
            messagebox.showwarning("文件类型错误", "仅支持 .bin 固件文件")

    def _on_file_path_changed(self, *_):
        """文件路径变化时更新文件信息"""
        path = self.file_path.get()
        if not path or not os.path.isfile(path):
            return
        try:
            stat = os.stat(path)
            size_kb = stat.st_size / 1024
            mtime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stat.st_mtime))
            fname = os.path.basename(path)

            info_text = f"文件: {fname} | 大小: {size_kb:.1f} KB ({stat.st_size} 字节) | 修改时间: {mtime}"

            age = time.time() - stat.st_mtime
            if age > 300:
                info_text += f"  *** 警告: 文件 {age / 60:.0f} 分钟前生成，可能是旧版本！"
                self.file_info_label.config(fg=self._theme.get("error"))
            else:
                self.file_info_label.config(fg=self._theme.get("fg"))

            self.file_info_label.config(text=info_text)
        except Exception as e:
            self.file_info_label.config(text=f"读取文件信息失败: {e}")

    # ======================== OTA 操作 ========================

    def _start_ota(self):
        """点击开始升级按钮"""
        port = self.port_var.get()
        if not port:
            messagebox.showerror("参数错误", "请选择 COM 端口")
            return

        filepath = self.file_path.get()
        if not filepath:
            messagebox.showerror("参数错误", "请选择固件文件 (.bin)")
            return

        if not os.path.isfile(filepath):
            messagebox.showerror("文件不存在", f"文件不存在:\n{filepath}")
            return

        fname = os.path.basename(filepath)
        fsize = os.path.getsize(filepath)
        answer = messagebox.askyesno(
            "确认升级",
            f"即将对 {port} 上的设备进行固件升级:\n\n"
            f"  文件: {fname}\n"
            f"  大小: {fsize} 字节 ({fsize / 1024:.1f} KB)\n\n"
            f"请确保设备已进入 OTA 模式。\n\n"
            f"确认开始升级？"
        )
        if not answer:
            return

        self._set_busy_state(True)
        self.progress_bar['value'] = 0
        self.progress_label.config(text="准备中...")

        self.cancel_event.clear()
        self.log_queue = queue.Queue()
        self.progress_queue = queue.Queue()

        self.ota_thread = threading.Thread(
            target=self._ota_thread,
            args=(port, filepath),
            daemon=True
        )
        self.ota_thread.start()
        self._poll_queues()

    def _ota_thread(self, port: str, filepath: str):
        """后台 OTA 线程"""
        ser = None
        try:
            baudrate = self.baud_var.get()
            self._log_thread(f"打开串口 {port} (波特率 {baudrate})...")

            ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.5
            )

            self._log_thread("触发 DTR 复位...")
            ser.dtr = False
            time.sleep(0.1)
            ser.dtr = True
            time.sleep(0.3)
            ser.reset_input_buffer()

            sender = OTASender(
                log_cb=self._log_thread,
                progress_cb=self._progress_thread,
                cancel_flag=self.cancel_event
            )

            success = sender.ymodem_send(ser, filepath)

            boot_log = sender.get_boot_log_text()
            if boot_log.strip():
                chip_info = ChipInfo.parse_boot_log(boot_log)
                self._log_thread("=== 芯片信息 (从启动日志解析) ===")
                for k, v in chip_info.items():
                    self._log_thread(f"  {k}: {v}")
                self._queue_chip_info(chip_info)

            if success:
                self._log_thread("=" * 50)
                self._log_thread("  成功: 固件已发送!")
                self._log_thread("=" * 50)
                self._queue_result(True, "升级成功！\n\n设备将自动启动新固件。")
            else:
                self._log_thread("=" * 50)
                self._log_thread("  失败: 传输中断!")
                self._log_thread("=" * 50)
                self._queue_result(False,
                                   "升级失败，请检查:\n\n1. 设备是否正常连接\n2. OTA 模式是否正确进入\n3. 固件文件是否匹配")

        except serial.SerialException as e:
            self._log_thread(f"串口错误: {e}")
            self._queue_result(False,
                               f"串口错误:\n{e}\n\n请检查:\n1. 端口是否被其他程序占用\n2. 设备是否已连接")
        except Exception as e:
            self._log_thread(f"异常: {e}")
            self._queue_result(False, f"未知错误:\n{e}")
        finally:
            if ser and ser.is_open:
                ser.close()
                self._log_thread("串口已关闭")
            self._queue_done()

    def _set_busy_state(self, busy: bool):
        """切换 UI 忙碌/空闲状态（OTA 升级或芯片查询共用）"""
        self._busy = busy
        state = tk.DISABLED if busy else tk.NORMAL

        self.port_combo.config(state="disabled" if busy else "readonly")
        self.baud_combo.config(state="disabled" if busy else "readonly")
        self.refresh_btn.config(state=state)
        self.browse_btn.config(state=state)
        self.file_entry.config(state=state)
        self.start_btn.config(state=state)
        self.cancel_btn.config(state=tk.NORMAL if busy else tk.DISABLED)
        if hasattr(self, 'chip_query_btn'):
            self.chip_query_btn.config(state=state)

    # ======================== 线程通信 ========================

    def _log_thread(self, msg: str):
        self.log_queue.put(msg)

    def _progress_thread(self, total_sent: int, filesize: int):
        self.progress_queue.put((total_sent, filesize))

    def _queue_result(self, success: bool, msg: str):
        self.progress_queue.put(("RESULT", success, msg))

    def _queue_chip_info(self, info: dict):
        self.progress_queue.put(("CHIP_INFO", info))

    def _queue_done(self):
        self.progress_queue.put(("DONE",))

    def _poll_queues(self):
        """主线程轮询队列（每 100ms）"""
        try:
            while True:
                try:
                    msg = self.log_queue.get_nowait()
                    self._log(msg)
                except queue.Empty:
                    break

            while True:
                try:
                    item = self.progress_queue.get_nowait()
                    if item[0] == "DONE":
                        self._set_busy_state(False)
                    elif item[0] == "RESULT":
                        _, success, msg = item
                        self._set_busy_state(False)
                        if success:
                            messagebox.showinfo("升级结果", msg)
                        else:
                            messagebox.showerror("升级结果", msg)
                    elif item[0] == "CHIP_INFO":
                        _, info = item
                        self._update_chip_display(info)
                    else:
                        total_sent, filesize = item
                        self._update_progress_gui(total_sent, filesize)
                except queue.Empty:
                    break
        except Exception as e:
            self._log(f"[错误] 队列轮询异常: {e}")

        # 如果 OTA 或芯片查询线程仍在运行，继续轮询
        ota_alive = self.ota_thread and self.ota_thread.is_alive()
        query_alive = self.chip_query_thread and self.chip_query_thread.is_alive()
        if ota_alive or query_alive:
            self.after(100, self._poll_queues)
        else:
            if self._busy:
                self._set_busy_state(False)

    # ======================== GUI 更新方法 (主线程) ========================

    def _log(self, msg: str):
        """主线程：追加日志（线程安全）"""
        ts = datetime.now().strftime(self._cfg["log_panel"]["timestamp_format"])
        line = f"[{ts}] {msg}\n"
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, line)
        max_lines = self._cfg["log_panel"]["max_lines"]
        line_count = int(self.log_text.index('end-1c').split('.')[0])
        if line_count > max_lines:
            self.log_text.delete('1.0', f'{line_count - max_lines}.0')
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def _update_progress_gui(self, total_sent: int, filesize: int):
        """主线程：更新进度条"""
        if filesize > 0:
            pct = total_sent * 100 // filesize
            self.progress_bar['value'] = pct
            self.progress_label.config(
                text=f"{pct}% ({total_sent / 1024:.0f}/{filesize / 1024:.0f} KB)"
            )
        self.update_idletasks()

    def _update_chip_display(self, info: dict):
        """主线程：更新芯片信息面板"""
        if not self.chip_frame:
            return
        mapping = {
            "mcu": "mcu",
            "active_partition": "partition",
            "boot_addr": "boot_addr",
            "app_addr": "app_addr",
            "version": "version",
        }
        for key, var_key in mapping.items():
            if key in info and info[key] != "未知":
                var = self.chip_vars.get(var_key)
                if var:
                    label_map = {
                        "mcu": f"MCU: {info[key]}",
                        "active_partition": f"活跃分区: {info[key]}",
                        "boot_addr": f"BOOT: {info[key]}",
                        "app_addr": f"APP: {info[key]}",
                        "version": f"版本: {info[key]}",
                    }
                    var.set(label_map.get(key, info[key]))

        # 更新时间戳
        now_str = datetime.now().strftime("%H:%M:%S")
        tm = self.chip_vars.get("query_time")
        if tm:
            tm.set(f"查询时间: {now_str}")

    def _cancel_ota(self):
        """取消当前操作（OTA 升级或芯片查询）"""
        if self._busy:
            self.cancel_event.set()
            self._log("*** 用户请求取消 ***")
            self.cancel_btn.config(state=tk.DISABLED)

    def _clear_log(self):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.delete('1.0', tk.END)
        self.log_text.config(state=tk.DISABLED)

    def _save_log(self):
        default_name = f"ota_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        path = filedialog.asksaveasfilename(
            title="保存日志",
            defaultextension=".txt",
            initialfile=default_name,
            filetypes=[("文本文件", "*.txt"), ("所有文件", "*.*")]
        )
        if path:
            try:
                content = self.log_text.get('1.0', tk.END)
                with open(path, 'w', encoding='utf-8') as f:
                    f.write(content)
                self._log(f"日志已保存到: {path}")
            except Exception as e:
                messagebox.showerror("保存失败", f"保存日志失败:\n{e}")

    def _on_close(self):
        if self._busy:
            if messagebox.askyesno("确认退出", "OTA 正在进行中，确定要退出吗？"):
                self.cancel_event.set()
                time.sleep(0.3)
                self.destroy()
        else:
            self.destroy()

    # ======================== 芯片信息查询 ========================

    def _query_chip_info(self):
        """通过串口查询指令获取芯片信息 (MCU task_query.c 响应)"""
        port = self.port_var.get()
        if not port:
            messagebox.showerror("参数错误", "请选择 COM 端口")
            return

        if self._busy:
            messagebox.showinfo("提示", "有操作正在进行中，请稍后再试")
            return

        self._set_busy_state(True)
        self.cancel_event.clear()
        self._log("发送芯片信息查询指令...")
        self.chip_query_btn.config(state=tk.DISABLED, text="查询中...")

        def _query():
            ser = None
            try:
                ser = serial.Serial(
                    port=port,
                    baudrate=self.baud_var.get(),
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=0.5
                )
                ser.reset_input_buffer()

                info = ChipInfo.query_by_command(ser, log_cb=self._log_thread,
                                                  timeout_ms=2000,
                                                  cancel_flag=self.cancel_event)
                self._log_thread("=== 芯片信息 ===")
                for k, v in info.items():
                    self._log_thread(f"  {k}: {v}")
                self._queue_chip_info(info)

            except serial.SerialException as e:
                self._log_thread(f"串口错误: {e}")
                self.after(0, lambda: messagebox.showerror(
                    "查询失败", f"串口错误:\n{e}\n\n请检查:\n1. 端口是否被占用\n2. 设备是否已连接\n3. MCU 固件是否包含查询任务"))
            except Exception as e:
                self._log_thread(f"查询异常: {e}")
            finally:
                if ser and ser.is_open:
                    ser.close()
                    self._log_thread("串口已关闭")
                self.after(0, lambda: self._set_busy_state(False))
                self.after(0, lambda: self.chip_query_btn.config(
                    state=tk.NORMAL, text="获取芯片信息"))

        self.chip_query_thread = threading.Thread(target=_query, daemon=True)
        self.chip_query_thread.start()
        self._poll_queues()


# ======================== 程序入口 ========================
if __name__ == "__main__":
    app = OTAApp()
    app.mainloop()
