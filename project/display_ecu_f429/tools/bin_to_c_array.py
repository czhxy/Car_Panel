#!/usr/bin/env python3
"""bin_to_c_array.py — 将 pic/*.bin 转换为 C 数组 + lv_img_dsc_t

   .bin 文件格式：前 4 字节为 header，之后为 RGB565 像素数据 (2 字节/像素)
   输出：pic/dashboard_images.h (仅 extern 声明) 和 pic/dashboard_images.c (数据定义)
"""

import os
import sys

# 图片尺寸（从 Figma 设计稿已知，单位：像素）
IMAGE_DEFS = {
    "Top Bar":       (208, 32),
    "arc-bg":        (99, 99),
    "arc-fill":      (99, 99),
    "can-dot_green": (6, 6),
    "can-dot_red":   (6, 6),
    "can-label":     (17, 6),
    "Turn Signals":  (208, 20),
    "mode":          (24, 12),   # 288 像素，24×12 = 288
}

HEADER_SIZE = 4  # 跳过前 4 字节 header


def var_name(filename):
    """将文件名转换为合法的 C 变量名"""
    return "img_" + filename.replace(" ", "_").lower().replace("-", "_")


def array_name(filename):
    return var_name(filename) + "_map"


def read_pixel_data(filepath):
    """读取 .bin 文件，跳过 header，返回像素字节数据"""
    with open(filepath, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        print(f"[WARN] {filepath}: 文件太小 ({len(data)} 字节)，跳过")
        return None

    return data[HEADER_SIZE:]


def format_c_array(name, data):
    """将字节数据格式化为 C const 数组字符串"""
    lines = [f"const uint8_t {name}[] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        comma = "," if i + 16 < len(data) else ""
        lines.append(f"    {hex_str}{comma}")
    lines.append("};")
    return "\n".join(lines)


def format_lv_img_dsc(var_n, arr_n, w, h):
    """生成 lv_img_dsc_t 定义（用于 .c 文件）"""
    return (
        f"const lv_img_dsc_t {var_n} = {{\n"
        f"    .header.cf = LV_IMG_CF_TRUE_COLOR,\n"
        f"    .header.always_zero = 0,\n"
        f"    .header.reserved = 0,\n"
        f"    .header.w = {w},\n"
        f"    .header.h = {h},\n"
        f"    .data_size = {w * h * 2},\n"
        f"    .data = {arr_n},\n"
        f"}};"
    )


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    pic_dir = os.path.join(project_dir, "pic")

    if not os.path.isdir(pic_dir):
        print(f"[ERROR] pic 目录不存在: {pic_dir}")
        sys.exit(1)

    bin_files = [f for f in os.listdir(pic_dir) if f.endswith(".bin")]
    if not bin_files:
        print("[ERROR] pic/ 目录下没有 .bin 文件")
        sys.exit(1)

    h_lines = []   # .h 文件内容
    c_lines = []   # .c 文件内容

    h_lines.append("/* dashboard_images.h — 自动生成，请勿手动编辑 */")
    h_lines.append("/* 来源: tools/bin_to_c_array.py */")
    h_lines.append("")
    h_lines.append("#ifndef __DASHBOARD_IMAGES_H")
    h_lines.append("#define __DASHBOARD_IMAGES_H")
    h_lines.append("")
    h_lines.append('#include "lvgl.h"')
    h_lines.append("")
    h_lines.append("/* ---- lv_img_dsc_t 外部声明 ---- */")
    h_lines.append("")

    c_lines.append("/* dashboard_images.c — 自动生成，请勿手动编辑 */")
    c_lines.append("/* 来源: tools/bin_to_c_array.py */")
    c_lines.append("")
    c_lines.append('#include "lvgl.h"')
    c_lines.append('#include "dashboard_images.h"')
    c_lines.append("")
    c_lines.append("/* ---- 原始像素数据 (RGB565) ---- */")
    c_lines.append("")

    any_output = False

    for bin_file in sorted(bin_files):
        stem = os.path.splitext(bin_file)[0]
        vname = var_name(stem)
        aname = array_name(stem)

        if stem not in IMAGE_DEFS:
            print(f"[SKIP] {bin_file}: 尺寸未定义，跳过")
            continue

        w, h = IMAGE_DEFS[stem]
        full_path = os.path.join(pic_dir, bin_file)

        pixel_data = read_pixel_data(full_path)
        if pixel_data is None:
            continue

        expected = w * h * 2
        actual = len(pixel_data)

        if actual < expected:
            pixel_data += b'\x00' * (expected - actual)
            print(f"[WARN] {bin_file}: 数据不足 ({actual}/{expected})，已补零")
        elif actual > expected:
            pixel_data = pixel_data[:expected]
            print(f"[WARN] {bin_file}: 数据超额 ({actual}/{expected})，已截断")

        print(f"[OK]  {bin_file}: {w}x{h}, {len(pixel_data)} 字节")

        # .c 文件：数组定义
        c_lines.append(format_c_array(aname, pixel_data))
        c_lines.append("")

        # .h 文件：extern 声明
        h_lines.append(f"extern const uint8_t {aname}[];")

        any_output = True

    if not any_output:
        print("[ERROR] 没有成功转换的图片")
        sys.exit(1)

    # .h 文件：lv_img_dsc_t extern 声明
    h_lines.append("")
    h_lines.append("/* ---- lv_img_dsc_t 外部声明 ---- */")
    h_lines.append("")
    for bin_file in sorted(bin_files):
        stem = os.path.splitext(bin_file)[0]
        if stem not in IMAGE_DEFS:
            continue
        vname = var_name(stem)
        h_lines.append(f"extern const lv_img_dsc_t {vname};")

    h_lines.append("")
    h_lines.append("#endif /* __DASHBOARD_IMAGES_H */")

    # .c 文件：lv_img_dsc_t 定义
    c_lines.append("/* ---- lv_img_dsc_t 描述符 ---- */")
    c_lines.append("")
    for bin_file in sorted(bin_files):
        stem = os.path.splitext(bin_file)[0]
        if stem not in IMAGE_DEFS:
            continue
        vname = var_name(stem)
        aname = array_name(stem)
        w, h = IMAGE_DEFS[stem]
        c_lines.append(format_lv_img_dsc(vname, aname, w, h))
        c_lines.append("")

    # 写入文件
    h_path = os.path.join(pic_dir, "dashboard_images.h")
    with open(h_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(h_lines).rstrip("\n") + "\n")
    print(f"\n[OK] 已生成 {h_path}")

    c_path = os.path.join(pic_dir, "dashboard_images.c")
    with open(c_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(c_lines).rstrip("\n") + "\n")
    print(f"[OK] 已生成 {c_path}")

    print("\n===== 转换完成 =====")


if __name__ == "__main__":
    main()
