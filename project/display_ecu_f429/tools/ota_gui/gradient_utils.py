#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
渐变工具 — 用 Pillow 生成玻璃质感渐变图片
用于 tkinter 按钮背景（模拟 Apple 风格上亮下暗的玻璃高光效果）。
"""

import math

try:
    from PIL import Image, ImageDraw, ImageTk
    _HAS_PIL = True
except ImportError:
    _HAS_PIL = False


def hex_to_rgb(hex_color: str) -> tuple:
    """#RRGGBB → (R, G, B)"""
    c = hex_color.lstrip("#")
    return tuple(int(c[i:i + 2], 16) for i in (0, 2, 4))


def _lerp(a: int, b: int, t: float) -> int:
    return int(a + (b - a) * t)


def create_gradient_image(width: int, height: int,
                          color_top: str = "#0A84FF",
                          color_bottom: str = "#006ACC",
                          radius: int = 10) -> Image.Image:
    """
    创建圆角渐变矩形图片。

    Args:
        width: 图片宽度
        height: 图片高度
        color_top: 顶部颜色 (#RRGGBB)
        color_bottom: 底部颜色 (#RRGGBB)
        radius: 圆角半径

    Returns:
        PIL Image (RGBA)
    """
    if not _HAS_PIL:
        return None

    top = hex_to_rgb(color_top)
    bottom = hex_to_rgb(color_bottom)

    # 创建带透明度的图片
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 绘制垂直渐变
    for y in range(height):
        t = y / max(height - 1, 1)
        r = _lerp(top[0], bottom[0], t)
        g = _lerp(top[1], bottom[1], t)
        b = _lerp(top[2], bottom[2], t)
        draw.rectangle([0, y, width, y + 1], fill=(r, g, b, 255))

    # 圆角 mask
    mask = Image.new("L", (width, height), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle([0, 0, width, height], radius=radius, fill=255)

    # 应用 mask
    img.putalpha(mask)

    return img


def create_glass_overlay(width: int, height: int, radius: int = 10) -> Image.Image:
    """
    创建玻璃高光叠加层 — 顶部亮条模拟玻璃反射。

    Args:
        width: 宽度
        height: 高度
        radius: 圆角半径

    Returns:
        PIL Image (RGBA) 半透明高光
    """
    if not _HAS_PIL:
        return None

    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 顶部 45% 区域绘制白色高光渐变
    highlight_height = int(height * 0.45)
    for y in range(highlight_height):
        alpha = int(40 * (1 - y / highlight_height))  # 从 40 衰减到 0
        draw.rectangle([0, y, width, y + 1], fill=(255, 255, 255, alpha))

    # 底部 15% 区域绘制暗色阴影
    shadow_height = int(height * 0.15)
    for y in range(height - shadow_height, height):
        alpha = int(20 * ((y - (height - shadow_height)) / shadow_height))
        draw.rectangle([0, y, width, y + 1], fill=(0, 0, 0, alpha))

    # 圆角 mask
    mask = Image.new("L", (width, height), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle([0, 0, width, height], radius=radius, fill=255)
    img.putalpha(Image.composite(img.getchannel("A"), Image.new("L", (width, height), 0), mask))

    return img


def create_button_image(width: int, height: int,
                        color_top: str = "#0A84FF",
                        color_bottom: str = "#006ACC",
                        radius: int = 10) -> 'ImageTk.PhotoImage | None':
    """
    创建带渐变 + 玻璃高光的按钮背景图片（可直接用于 tkinter 按钮）。

    Returns:
        ImageTk.PhotoImage 或 None (Pillow 不可用时)
    """
    if not _HAS_PIL:
        return None

    base = create_gradient_image(width, height, color_top, color_bottom, radius)
    glass = create_glass_overlay(width, height, radius)

    if base is None or glass is None:
        return None

    # 合并渐变底 + 玻璃高光
    result = Image.alpha_composite(base, glass)

    return ImageTk.PhotoImage(result)


def create_panel_bg(width: int, height: int, bg_color: str = "#2C2C2E",
                    radius: int = 14) -> 'ImageTk.PhotoImage | None':
    """
    创建 Apple 风格圆角面板背景图片。

    Returns:
        ImageTk.PhotoImage 或 None
    """
    if not _HAS_PIL:
        return None

    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    rgb = hex_to_rgb(bg_color)

    # 圆角矩形
    draw.rounded_rectangle([0, 0, width, height], radius=radius, fill=(*rgb, 255))

    return ImageTk.PhotoImage(img)
