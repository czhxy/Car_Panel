#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
主题管理器 — 管理亮/暗/经典三套配色方案
支持热切换，切换时自动更新所有已注册的 widget 回调。
"""

import os
import sys
import json
from typing import Callable


def _get_themes_dir():
    """获取 themes 目录路径，兼容 PyInstaller 打包"""
    if getattr(sys, 'frozen', False):
        return os.path.join(sys._MEIPASS, 'themes')
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), 'themes')


THEMES_DIR = _get_themes_dir()

# 主题注册表
THEME_REGISTRY = {
    "apple_dark": {
        "name": "Apple 深色",
        "icon": "◉",
        "file": os.path.join(THEMES_DIR, "apple_dark.json"),
    },
    "apple_light": {
        "name": "Apple 浅色",
        "icon": "◎",
        "file": os.path.join(THEMES_DIR, "apple_light.json"),
    },
    "classic": {
        "name": "经典灰色",
        "icon": "○",
        "file": os.path.join(THEMES_DIR, "classic.json"),
    },
}


def _rgba_to_hex(rgba_str: str) -> str:
    """将 CSS rgba(r,g,b,a) 格式转为 #RRGGBB 近似值（忽略 alpha）"""
    import re
    m = re.match(r'rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)', rgba_str)
    if m:
        return f"#{int(m.group(1)):02X}{int(m.group(2)):02X}{int(m.group(3)):02X}"
    return rgba_str


def _resolve_color(theme_colors: dict, key: str, parent_bg: str = "#FFFFFF") -> str:
    """解析颜色值，rgba 转为纯色（Alpha 混合到 parent_bg 上）"""
    value = theme_colors.get(key, "#CCCCCC")
    if isinstance(value, str) and value.startswith("rgba"):
        import re
        m = re.match(r'rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([\d.]+)\)', value)
        if m:
            r, g, b, a = int(m.group(1)), int(m.group(2)), int(m.group(3)), float(m.group(4))
            if parent_bg.startswith("#") and len(parent_bg) == 7:
                br = int(parent_bg[1:3], 16)
                bg = int(parent_bg[3:5], 16)
                bb = int(parent_bg[5:7], 16)
                r = int(r * a + br * (1 - a))
                g = int(g * a + bg * (1 - a))
                b = int(b * a + bb * (1 - a))
            return f"#{r:02X}{g:02X}{b:02X}"
        return _rgba_to_hex(value)
    return value


class ThemeManager:
    """主题管理器 — 加载/切换配色方案，通知所有监听者"""

    def __init__(self, default_theme: str = "apple_dark"):
        self._current = default_theme
        self._colors = {}
        self._listeners: list[Callable[[dict], None]] = []
        self._load()
        self._initial_load_done = True

    @property
    def current(self) -> str:
        return self._current

    @property
    def current_name(self) -> str:
        return THEME_REGISTRY.get(self._current, {}).get("name", self._current)

    @property
    def current_icon(self) -> str:
        return THEME_REGISTRY.get(self._current, {}).get("icon", "○")

    @property
    def colors(self) -> dict:
        return self._colors

    def get(self, key: str, default: str = "#CCCCCC") -> str:
        """获取颜色值，自动解析 rgba"""
        return _resolve_color(self._colors, key, self._colors.get("bg", "#f5f5f5"))

    def _load(self):
        """从 JSON 文件加载主题"""
        info = THEME_REGISTRY.get(self._current)
        if not info:
            self._colors = {}
            return
        try:
            with open(info["file"], "r", encoding="utf-8") as f:
                self._colors = json.load(f)
        except Exception:
            self._colors = {}

    def add_listener(self, callback: Callable[[dict], None]):
        """注册主题变更监听器"""
        self._listeners.append(callback)

    def _notify(self):
        """通知所有监听者"""
        colors = dict(self._colors)
        for cb in self._listeners:
            try:
                cb(colors)
            except Exception:
                pass

    def set_theme(self, theme_key: str):
        """切换到指定主题"""
        if theme_key not in THEME_REGISTRY:
            return False
        self._current = theme_key
        self._load()
        self._notify()
        return True

    def toggle_next(self) -> str:
        """循环切换到下一个主题，返回新主题 key"""
        keys = list(THEME_REGISTRY.keys())
        idx = keys.index(self._current) if self._current in keys else 0
        self._current = keys[(idx + 1) % len(keys)]
        self._load()
        self._notify()
        return self._current
