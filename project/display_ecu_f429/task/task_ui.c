/* task_ui.c — 显示域 UI 任务
 *
 * 顶层与显示屏相关的任务统一放在本文件（合并自 task_lcd_demo + task_dashboard_ui）：
 *   - Task_UI()：LVGL 主循环任务（初始化 LVGL/显示/触摸驱动 + lv_timer_handler 渲染）
 *   - Dashboard_UI_Init()：一次性构建仪表盘所有静态元素
 *   - Dashboard_Update()：每 25ms 从共享状态（mod_ui）刷新动态元素
 *
 * 将 Figma 设计的 240×320 汽车仪表盘用 LVGL v8.3 控件实现。
 * 数据源为 mod_ui 的 DashboardState（g_dash_state）。
 *
 * CAN 指示灯：不再使用 Top Bar 背景图（其内嵌静态状态点无法受控，
 * 导致"绿色常亮不闪烁"）。改用 LVGL 原生控件——红/绿圆点(lv_obj)
 * + "CAN" 文本框(lv_label)，在线绿色闪烁 / 离线红色常亮。
 *
 * RPM 显示：显示动力域回传实测 rpm（CAN 状态帧 0x110 上报，×10 解码）。
 * Load Bar 拖动条只更新目标值 rpm_target（0–100 → 0–300 RPM，×3），
 * CAN 周期帧 CanProtocol_WheelCtlSend() 读取并以 ×10 编码发送。
 *
 * Pause 按钮：仪表盘与 Load Bar 之间的红色圆角矩形，一键暂停/恢复。
 * 暂停时 rpm_target 归 0、滑块回 0 并锁定，恢复时回到暂停前位置。
 */

#include "task_ui.h"
#include "mod_ui.h"
#include "dashboard_images.h"
#include "bsp_log.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 颜色常量 (RGB565)
 * ============================================================ */
#define COLOR_BG              lv_color_hex(0x05080D)
#define COLOR_ACCENT           lv_color_hex(0x00E5FF)
#define COLOR_ACCENT_DIM       lv_color_hex(0x00CCFF)
#define COLOR_ERROR_RED        lv_color_hex(0xF54236)
#define COLOR_OK_GREEN         lv_color_hex(0x33CC4D)
#define COLOR_DIVIDER          lv_color_hex(0x0A1A2A)   /* rgba(0,204,255,0.08) */
#define COLOR_CARD_BORDER_DIM  lv_color_hex(0x0A1A2A)
#define COLOR_CARD_BORDER_HL   lv_color_hex(0x00E5FF)
#define COLOR_SLIDER_BG        lv_color_hex(0x0A0F18)
#define COLOR_SLIDER_FILL      lv_color_hex(0x00CCFF)
#define COLOR_WHITE_LOW        lv_color_hex(0x2A2A2A)   /* 白色 18% */
#define COLOR_TEXT_DIM         lv_color_hex(0x607080)

/* ---- 警示指示灯颜色 ---- */
#define COLOR_ABS              lv_color_hex(0xFFEB3B)   /* 黄 */
#define COLOR_ESC              lv_color_hex(0xFF9900)   /* 橙 */
#define COLOR_ENGINE           lv_color_hex(0xF44336)   /* 红 */
#define COLOR_BATT_GREEN       lv_color_hex(0x4CAF50)   /* 绿 */
#define COLOR_HIGHBEAM         lv_color_hex(0x2196F3)   /* 蓝 */
#define COLOR_DOOR             lv_color_hex(0xE91E63)   /* 粉 */

/* ============================================================
 * 静态 UI 引用（供 Dashboard_Update 访问）
 * ============================================================ */
static lv_obj_t *s_can_led      = NULL;
static lv_obj_t *s_rpm_label    = NULL;
static lv_obj_t *s_card_frames[DASH_CARD_COUNT] = {NULL};
static lv_obj_t *s_card_labels[DASH_CARD_COUNT] = {NULL};
static lv_obj_t *s_load_slider  = NULL;
static lv_obj_t *s_load_label   = NULL;
static lv_obj_t *s_pause_btn    = NULL;
static int32_t   s_pre_pause_pct = 0;    /* 暂停前滑块位置(0-100)，恢复用 */
static lv_obj_t *s_warning_dots[6] = {NULL};

/* ---- 心跳超时检测 ---- */
#define HEARTBEAT_TIMEOUT_MS  1500

/* ---- 卡片颜色常量 ---- */
static const lv_color_t s_warning_colors[6] = {
    LV_COLOR_MAKE(0xFF, 0xEB, 0x3B),
    LV_COLOR_MAKE(0xFF, 0x99, 0x00),
    LV_COLOR_MAKE(0xF4, 0x43, 0x36),
    LV_COLOR_MAKE(0x4C, 0xAF, 0x50),
    LV_COLOR_MAKE(0x21, 0x96, 0xF3),
    LV_COLOR_MAKE(0xE9, 0x1E, 0x63)
};

#if 0 /* 暂时隐藏错误码区域，保留代码便于后续恢复 */
static lv_obj_t *s_error_icon   = NULL;
static lv_obj_t *s_error_box    = NULL;
static lv_obj_t *s_error_label  = NULL;
static lv_obj_t *s_error_code   = NULL;
static lv_obj_t *s_error_title  = NULL;

/* ---- 辅助：设置错误框颜色 ---- */
static void set_error_box_style(bool is_fault, uint16_t error_code)
{
    lv_color_t bg, border, icon_color, text_color, code_color;
    const char *code_str;
    const char *message = Dashboard_Fault_Lookup(error_code, (FaultLevel *)0);
    char code_buf[8];

    if (is_fault) {
        bg         = lv_color_hex(0x0A0202);  /* rgba(245,66,54,0.04) 近似 */
        border     = COLOR_ERROR_RED;
        icon_color = COLOR_ERROR_RED;
        text_color = COLOR_ERROR_RED;
        code_color = COLOR_WHITE_LOW;
        snprintf(code_buf, sizeof(code_buf), "E%03X", (unsigned int)error_code);
        code_str   = code_buf;
    } else {
        bg         = lv_color_hex(0x020A04);  /* rgba(51,204,77,0.04) 近似 */
        border     = COLOR_OK_GREEN;
        icon_color = COLOR_OK_GREEN;
        text_color = COLOR_OK_GREEN;
        code_color = COLOR_OK_GREEN;
        code_str   = "OK";
    }

    lv_obj_set_style_bg_color(s_error_box, bg, 0);
    lv_obj_set_style_border_color(s_error_box, border, 0);
    lv_obj_set_style_bg_color(s_error_icon, icon_color, 0);

    lv_obj_set_style_text_color(s_error_title, text_color, 0);
    lv_obj_set_style_text_color(s_error_label, text_color, 0);
    lv_obj_set_style_text_color(s_error_code, code_color, 0);

    lv_label_set_text(s_error_label, message);
    lv_label_set_text(s_error_code, code_str);
}
#endif

/* ============================================================
 * 事件回调
 * ============================================================ */

/* Load Bar 值变化 → 仅更新 g_dash_state.rpm_target（数据源），
 * CAN 周期帧 CanProtocol_WheelCtlSend() 从中读取并以 ×10 编码发送 */
static void on_load_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t pct = lv_slider_get_value(slider);

    Dashboard_Data_Lock();
    if (g_dash_state.paused) {   /* 暂停中滑块已锁定，忽略变化（防御） */
        Dashboard_Data_Unlock();
        return;
    }
    g_dash_state.load_pct = (uint8_t)pct;
    g_dash_state.rpm_target = (uint16_t)((pct * 3U));  /* 0–100 → 0–300 RPM */
    uint16_t target = g_dash_state.rpm_target;
    Dashboard_Data_Unlock();

    /* 更新滑块上方标签 */
    if (s_load_label != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u RPM", target);
        lv_label_set_text(s_load_label, buf);
    }
}

/* 一键暂停按钮 → 切换暂停状态
 *  进入暂停: 保存当前滑块位置 → rpm_target 归 0 → 滑块回 0 并锁定
 *  解除暂停: 滑块回到暂停前位置 → rpm_target 恢复原值
 *  下发由 CAN 周期帧读取 rpm_target 完成 */
static void on_pause_click(lv_event_t *e)
{
    (void)e;
    int32_t pct;
    uint16_t target;

    Dashboard_Data_Lock();
    bool now_paused = !g_dash_state.paused;
    g_dash_state.paused = now_paused;
    if (now_paused) {
        s_pre_pause_pct = g_dash_state.load_pct;       /* 记住暂停前位置 */
        g_dash_state.load_pct = 0;
        g_dash_state.rpm_target = 0;
        pct    = 0;
        target = 0;
    } else {
        pct = s_pre_pause_pct;
        g_dash_state.load_pct = (uint8_t)pct;
        g_dash_state.rpm_target = (uint16_t)((uint16_t)pct * 3U);
        target = g_dash_state.rpm_target;
    }
    Dashboard_Data_Unlock();

    /* 同步滑块位置与锁定状态 */
    lv_slider_set_value(s_load_slider, pct, LV_ANIM_OFF);
    if (now_paused) {
        lv_obj_clear_flag(s_load_slider, LV_OBJ_FLAG_CLICKABLE);  /* 锁定滑块 */
    } else {
        lv_obj_add_flag(s_load_slider, LV_OBJ_FLAG_CLICKABLE);    /* 解除锁定 */
    }

    /* 更新 RPM 标签 */
    if (s_load_label != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u RPM", target);
        lv_label_set_text(s_load_label, buf);
    }
}

/* 左转向灯 → 选中上一个卡片 */
static void on_turn_left(lv_event_t *e)
{
    (void)e;
    Dashboard_Data_Lock();
    {
        unsigned int selected = (unsigned int)g_dash_state.selected_card;
        selected = (selected + (unsigned int)DASH_CARD_COUNT - 1U) %
                   (unsigned int)DASH_CARD_COUNT;
        g_dash_state.selected_card = (DashboardCard)selected;
    }
    Dashboard_Data_Unlock();
}

/* 右转向灯 → 选中下一个卡片 */
static void on_turn_right(lv_event_t *e)
{
    Dashboard_Data_Lock();
    {
        unsigned int selected = (unsigned int)g_dash_state.selected_card;
        selected = (selected + 1U) % (unsigned int)DASH_CARD_COUNT;
        g_dash_state.selected_card = (DashboardCard)selected;
    }
    Dashboard_Data_Unlock();
    (void)e;
}

/* ============================================================
 * 本地卡片标签 → 字符串
 * ============================================================ */
static void update_card_label(unsigned int idx, const DashboardState *snap)
{
    if (idx >= DASH_CARD_COUNT || s_card_labels[idx] == NULL) return;

    char buf[16];
    switch (idx) {
    case DASH_CARD_ODO:
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)snap->odo_value);
        break;
    case DASH_CARD_BATT:
        snprintf(buf, sizeof(buf), "%u%%", snap->batt_level);
        break;
    case DASH_CARD_SOC:
        snprintf(buf, sizeof(buf), "%u%%", snap->soc_level);
        break;
    default:
        buf[0] = '\0';
        break;
    }
    lv_label_set_text(s_card_labels[idx], buf);
}

/* 刷新卡片选中高亮 */
static void update_card_selection(DashboardCard selected)
{
    unsigned int i;
    for (i = 0; i < DASH_CARD_COUNT; i++) {
        if (s_card_frames[i] == NULL) continue;
        lv_color_t border = (i == (unsigned int)selected)
            ? COLOR_CARD_BORDER_HL : COLOR_CARD_BORDER_DIM;
        lv_obj_set_style_border_color(s_card_frames[i], border, 0);
        lv_obj_set_style_border_opa(s_card_frames[i],
            (i == (unsigned int)selected) ? LV_OPA_COVER : LV_OPA_20, 0);
    }
}

/* ============================================================
 * Dashboard_UI_Init — 一次性构建仪表盘
 * ============================================================ */
void Dashboard_UI_Init(lv_obj_t *scr)
{
    /* ---- 0. 初始化数据 ---- */
    Dashboard_Data_Init();

    /* ---- 屏幕背景 ---- */
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ========================================================
     * 1. CAN 指示灯 — 不再使用 Top Bar 背景图
     *    原因：Top Bar 图片内嵌的静态状态点无法受控（此前"绿色常亮
     *    不闪烁"的根因）。改为 LVGL 原生控件：
     *      - 红/绿圆点: lv_obj（6×6 圆形），在线绿色闪烁 / 离线红色常亮
     *      - "CAN" 文本: lv_label 文本框
     *    全部由 Dashboard_Update 驱动，不依赖任何图片像素。
     * ======================================================== */
    s_can_led = Mod_UI_Box(scr, 89, 13, 6, 6, COLOR_ERROR_RED, LV_RADIUS_CIRCLE);
    /* 原 Top Bar 内置状态点位置 */

    lv_obj_t *can_label = Mod_UI_Label(scr, 0, 0, "CAN", lv_color_white(), NULL);
    lv_obj_align_to(can_label, s_can_led, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* ========================================================
     * 2. Divider 分割线 (208×1, y=45)
     * ======================================================== */
    lv_obj_t *divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 208, 1);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_bg_color(divider, COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_20, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);

    /* ========================================================
     * 3. Error Box (150×23, 居中, y=49)
     * ======================================================== */
#if 0 /* 暂时隐藏错误码区域 */
    s_error_box = lv_obj_create(scr);
    lv_obj_set_size(s_error_box, 150, 23);
    lv_obj_align(s_error_box, LV_ALIGN_TOP_MID, 0, 49);
    lv_obj_set_style_bg_color(s_error_box, lv_color_hex(0x020A04), 0);
    lv_obj_set_style_bg_opa(s_error_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_error_box, COLOR_OK_GREEN, 0);
    lv_obj_set_style_border_width(s_error_box, 1, 0);
    lv_obj_set_style_border_opa(s_error_box, LV_OPA_10, 0);
    lv_obj_set_style_radius(s_error_box, 2, 0);
    lv_obj_set_style_pad_all(s_error_box, 4, 0);

    /* 错误图标 (7×7 圆角方块) */
    s_error_icon = lv_obj_create(s_error_box);
    lv_obj_set_size(s_error_icon, 7, 7);
    lv_obj_align(s_error_icon, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_error_icon, COLOR_OK_GREEN, 0);
    lv_obj_set_style_bg_opa(s_error_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_error_icon, 0, 0);
    lv_obj_set_style_radius(s_error_icon, 1, 0);

    /* 标题 ("STATUS") */
    s_error_title = lv_label_create(s_error_box);
    lv_obj_set_style_text_color(s_error_title, COLOR_OK_GREEN, 0);
    lv_obj_set_style_text_opa(s_error_title, LV_OPA_80, 0);
    lv_label_set_text(s_error_title, "STATUS");
    lv_obj_align_to(s_error_title, s_error_icon, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* 消息 ("ALL SYSTEMS NORMAL") */
    s_error_label = lv_label_create(s_error_box);
    lv_obj_set_style_text_color(s_error_label, COLOR_OK_GREEN, 0);
    lv_obj_set_style_text_opa(s_error_label, LV_OPA_80, 0);
    lv_label_set_text(s_error_label, "ALL SYSTEMS NORMAL");
    lv_obj_align_to(s_error_label, s_error_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    /* 错误码 ("OK") */
    s_error_code = lv_label_create(s_error_box);
    lv_obj_set_style_text_color(s_error_code, COLOR_OK_GREEN, 0);
    lv_label_set_text(s_error_code, "OK");
    lv_obj_align(s_error_code, LV_ALIGN_RIGHT_MID, -4, 0);
#endif

    /* ========================================================
     * 4. Gauge 表盘 (120×110, 居中, y=72)
     * ======================================================== */
    lv_obj_t *gauge_cont = lv_obj_create(scr);
    lv_obj_set_size(gauge_cont, 120, 110);
    lv_obj_align(gauge_cont, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_opa(gauge_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge_cont, 0, 0);
    lv_obj_set_style_pad_all(gauge_cont, 0, 0);

    /* 使用不带数字的圆弧底图和填充图，数字由 LVGL 实时显示 */
    lv_obj_t *arc_bg_img = lv_img_create(gauge_cont);
    lv_img_set_src(arc_bg_img, &img_arc_bg);
    lv_obj_align(arc_bg_img, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *arc_fill_img = lv_img_create(gauge_cont);
    lv_img_set_src(arc_fill_img, &img_arc_fill);
    lv_obj_align(arc_fill_img, LV_ALIGN_CENTER, 0, 0);

    /* RPM 数值 ("6800", Montserrat 28) */
    s_rpm_label = Mod_UI_Label(gauge_cont, 0, -4, "0", lv_color_white(), &lv_font_montserrat_28);
    lv_obj_align(s_rpm_label, LV_ALIGN_CENTER, 0, -4);

    /* RPM 单位 ("RPM") */
    lv_obj_t *unit_label = lv_label_create(gauge_cont);
    lv_obj_set_style_text_color(unit_label, COLOR_TEXT_DIM, 0);
    lv_label_set_text(unit_label, "RPM");
    lv_obj_align_to(unit_label, s_rpm_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    /* ========================================================
     * 5. Bottom Cards 容器 (208×48, flex row, 居中, y=190)
     * ======================================================== */
    lv_obj_t *cards_cont = lv_obj_create(scr);
    lv_obj_set_size(cards_cont, 208, 48);
    lv_obj_align(cards_cont, LV_ALIGN_TOP_MID, 0, 190);
    lv_obj_set_style_bg_opa(cards_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cards_cont, 0, 0);
    lv_obj_set_style_pad_all(cards_cont, 0, 0);
    lv_obj_set_flex_flow(cards_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cards_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ODO/BATT/SOC 图标按当前需求隐藏，不创建图片对象。 */

//		unsigned int i;
//		for (i = 0; i < DASH_CARD_COUNT; i++) {
//				s_card_frames[i] = lv_obj_create(cards_cont);
//				lv_obj_set_size(s_card_frames[i], 65, 48);
//				lv_obj_set_style_bg_opa(s_card_frames[i], LV_OPA_TRANSP, 0);
//				lv_obj_set_style_border_color(s_card_frames[i],
//						(i == 0) ? COLOR_CARD_BORDER_HL : COLOR_CARD_BORDER_DIM, 0);
//				lv_obj_set_style_border_width(s_card_frames[i], 1, 0);
//				lv_obj_set_style_border_opa(s_card_frames[i],
//						(i == 0) ? LV_OPA_COVER : LV_OPA_20, 0);
//				lv_obj_set_style_radius(s_card_frames[i], 2, 0);
//				lv_obj_set_style_pad_all(s_card_frames[i], 0, 0);

//				/* 卡片数值标签 */
//				s_card_labels[i] = lv_label_create(s_card_frames[i]);
//				lv_obj_set_style_text_color(s_card_labels[i], lv_color_white(), 0);
//				lv_obj_align(s_card_labels[i], LV_ALIGN_CENTER, 0, 0);

//				update_card_label(i, &g_dash_state);
//		}

    /* ========================================================
     * 6. Load Bar (刻度 y=240, 滑块 208×20, y=270)
     * ======================================================== */
    /* 刻度标签容器（0/50/100 已按用户要求注释掉，保留代码便于恢复） */
//    lv_obj_t *tick_cont = lv_obj_create(scr);
//    lv_obj_set_size(tick_cont, 208, 12);
//    lv_obj_align(tick_cont, LV_ALIGN_TOP_MID, 0, 240);
//    lv_obj_set_style_bg_opa(tick_cont, LV_OPA_TRANSP, 0);
//    lv_obj_set_style_border_width(tick_cont, 0, 0);
//    lv_obj_set_style_pad_all(tick_cont, 0, 0);
//
//    lv_obj_t *tick0 = lv_label_create(tick_cont);
//    lv_obj_set_style_text_color(tick0, COLOR_TEXT_DIM, 0);
//    lv_label_set_text(tick0, "0");
//    lv_obj_align(tick0, LV_ALIGN_LEFT_MID, 0, 0);
//
//    lv_obj_t *tick75 = lv_label_create(tick_cont);
//    lv_obj_set_style_text_color(tick75, COLOR_TEXT_DIM, 0);
//    lv_label_set_text(tick75, "50");
//    lv_obj_align(tick75, LV_ALIGN_CENTER, 0, 0);
//
//    lv_obj_t *tick150 = lv_label_create(tick_cont);
//    lv_obj_set_style_text_color(tick150, COLOR_TEXT_DIM, 0);
//    lv_label_set_text(tick150, "100");
//    lv_obj_align(tick150, LV_ALIGN_RIGHT_MID, 0, 0);

    /* RPM 目标值标签 */
    s_load_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_load_label, COLOR_ACCENT, 0);
    lv_label_set_text(s_load_label, "126 RPM");
    lv_obj_align(s_load_label, LV_ALIGN_TOP_MID, 0, 253);

    /* Slider 主体 (y=270) */
    s_load_slider = lv_slider_create(scr);
    lv_obj_set_size(s_load_slider, 208, 20);
    lv_obj_align(s_load_slider, LV_ALIGN_TOP_MID, 0, 270);
    lv_slider_set_range(s_load_slider, 0, 100);

    /* Slider 主轨道样式 */
    lv_obj_set_style_bg_color(s_load_slider, COLOR_SLIDER_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_load_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_load_slider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_load_slider, 2, LV_PART_MAIN);

    /* Slider 指示器（填充）样式 */
    lv_obj_set_style_bg_color(s_load_slider, COLOR_SLIDER_FILL, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_load_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_load_slider, 2, LV_PART_INDICATOR);

    /* Slider 旋钮（隐藏） */
    lv_obj_set_style_bg_opa(s_load_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_load_slider, 0, LV_PART_KNOB);

    lv_slider_set_value(s_load_slider, g_dash_state.load_pct, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_load_slider, on_load_change, LV_EVENT_VALUE_CHANGED, NULL);

    /* ========================================================
     * 6b. Pause 按钮 — 一键暂停（红色圆角矩形）
     *     位于仪表盘与 Load Bar 之间 (y=200, 居中)。
     *     按下: 速度归 0 + 滑块锁定; 再按: 恢复暂停前值。
     * ======================================================== */
    s_pause_btn = lv_btn_create(scr);
    lv_obj_set_size(s_pause_btn, 80, 30);
    lv_obj_align(s_pause_btn, LV_ALIGN_TOP_MID, 0, 200);
    lv_obj_set_style_bg_color(s_pause_btn, COLOR_ERROR_RED, 0);
    lv_obj_set_style_bg_opa(s_pause_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_pause_btn, 8, 0);
    lv_obj_set_style_border_width(s_pause_btn, 0, 0);

    lv_obj_t *pause_label = lv_label_create(s_pause_btn);
    lv_label_set_text(pause_label, "PAUSE");
    lv_obj_set_style_text_color(pause_label, lv_color_white(), 0);
    lv_obj_center(pause_label);

    lv_obj_add_event_cb(s_pause_btn, on_pause_click, LV_EVENT_CLICKED, NULL);

    /* ========================================================
     * 7. Turn Signals (208×20, 居中, y=292)
     * ======================================================== */
//    lv_obj_t *turn_bg = lv_img_create(scr);
//    lv_img_set_src(turn_bg, &img_turn_signals);
//    lv_obj_align(turn_bg, LV_ALIGN_TOP_MID, 0, 292);

//    /* 左箭头点击区 */
//    lv_obj_t *left_btn = lv_obj_create(scr);
//    lv_obj_set_size(left_btn, 104, 20);
//    lv_obj_set_pos(left_btn, 16, 292);
//    lv_obj_set_style_bg_opa(left_btn, LV_OPA_TRANSP, 0);
//    lv_obj_set_style_border_width(left_btn, 0, 0);
//    lv_obj_add_event_cb(left_btn, on_turn_left, LV_EVENT_CLICKED, NULL);
//    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);

//    /* 右箭头点击区 */
//    lv_obj_t *right_btn = lv_obj_create(scr);
//    lv_obj_set_size(right_btn, 104, 20);
//    lv_obj_set_pos(right_btn, 120, 292);
//    lv_obj_set_style_bg_opa(right_btn, LV_OPA_TRANSP, 0);
//    lv_obj_set_style_border_width(right_btn, 0, 0);
//    lv_obj_add_event_cb(right_btn, on_turn_right, LV_EVENT_CLICKED, NULL);
//    lv_obj_clear_flag(right_btn, LV_OBJ_FLAG_SCROLLABLE);

    /* ========================================================
     * 8. Warning Dots (208×8, flex row, 居中, y=312)
     * ========================================================
     *    6 个 8×8 圆点, gap 约 26px */
    lv_obj_t *dots_cont = lv_obj_create(scr);
    lv_obj_set_size(dots_cont, 208, 8);
    lv_obj_align(dots_cont, LV_ALIGN_TOP_MID, 0, 312);
    lv_obj_set_style_bg_opa(dots_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots_cont, 0, 0);
    lv_obj_set_style_pad_all(dots_cont, 0, 0);
    lv_obj_set_flex_flow(dots_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

//    for (i = 0; i < 6; i++) {
//        s_warning_dots[i] = lv_obj_create(dots_cont);
//        lv_obj_set_size(s_warning_dots[i], 8, 8);
//        lv_obj_set_style_bg_color(s_warning_dots[i], s_warning_colors[i], 0);
//        /* 电池(索引3)点亮 (90%不透明度)，其余暗 (20%) */
//        lv_obj_set_style_bg_opa(s_warning_dots[i],
//            (i == 3) ? LV_OPA_90 : LV_OPA_20, 0);
//        lv_obj_set_style_border_width(s_warning_dots[i], 0, 0);
//        lv_obj_set_style_radius(s_warning_dots[i], LV_RADIUS_CIRCLE, 0);
//        lv_obj_clear_flag(s_warning_dots[i], LV_OBJ_FLAG_SCROLLABLE);
//    }

    /* 初始化心跳时间戳 */
    LOG_I("[DASH] UI initialized\r\n");
}

/* ============================================================
 * Dashboard_Update — 25ms 周期从共享状态刷新动态 UI
 * ============================================================ */
void Dashboard_Update(void)
{
    DashboardState snap = Dashboard_Data_GetSnapshot();

    /* ---- 心跳超时检测 ---- */
    TickType_t now = xTaskGetTickCount();
    if (snap.motor_online &&
        ((now - snap.last_hb_tick) > pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS))) {
        snap.motor_online = false;
        Dashboard_Data_Lock();
        g_dash_state.motor_online = false;
        Dashboard_Data_Unlock();
    }

    /* ---- CAN 指示灯（LVGL 原生圆点：始终闪烁，颜色区分状态） ----
     * 在线: 绿色 500ms 闪烁 / 离线: 红色 500ms 闪烁 */
    if (s_can_led != NULL) {
        bool led_on = ((now % pdMS_TO_TICKS(500)) < pdMS_TO_TICKS(250));
        lv_obj_set_style_bg_color(s_can_led,
            snap.motor_online ? COLOR_OK_GREEN : COLOR_ERROR_RED, 0);
        lv_obj_set_style_bg_opa(s_can_led, led_on ? LV_OPA_COVER : LV_OPA_20, 0);
    }

    /* ---- RPM 数值 ----
     * 显示动力域回传实测 rpm（CAN 状态帧 0x110 上报，×10 解码后为 RPM）。
     * 未收到状态帧前为初始占位值；拖动条只改目标值 rpm_target 不影响此显示。 */
    if (s_rpm_label != NULL) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", snap.rpm);
        lv_label_set_text(s_rpm_label, buf);
    }

    /* ---- 卡片值 ---- */
    for (unsigned int i = 0; i < DASH_CARD_COUNT; i++) {
        update_card_label(i, &snap);
    }

    /* ---- 卡片选中高亮 ---- */
    update_card_selection(snap.selected_card);

    /* ---- 警示灯: 引擎故障 (索引2) ---- */
    if (s_warning_dots[2] != NULL) {
        lv_opa_t opa = (snap.error_code != 0) ? LV_OPA_90 : LV_OPA_20;
        lv_obj_set_style_bg_opa(s_warning_dots[2], opa, 0);
    }

    /* ---- 警示灯: 电池 (索引3) 始终点亮 ---- */
}

/* ============================================================
 * Task_UI — LVGL 仪表盘 UI 任务（原 Task_LCD_Demo）
 * 初始化 LVGL 图形库并运行仪表盘界面
 * 栈: 1024 字 = 4KB (LVGL 渲染开销)
 * 优先级: 3
 * ============================================================ */
void Task_UI(void *pvParameters)
{
    (void)pvParameters;

    LOG_I("[LVGL] Initializing LVGL v%d.%d.%d...\r\n",
          lv_version_major(), lv_version_minor(), lv_version_patch());

    /* LVGL 核心初始化 */
    lv_init();

    /* 显示驱动 (ILI9341 SPI) */
    lv_port_disp_init();
    LOG_I("[LVGL] Display driver initialized\r\n");

    /* 触摸输入 (FT6336G I2C) */
    lv_port_indev_init();
    LOG_I("[LVGL] Input device initialized\r\n");

    /* 构建仪表盘 UI (替换原 lv_demo_widgets) */
    Dashboard_UI_Init(lv_scr_act());
    LOG_I("[LVGL] Dashboard UI started\r\n");

    /* 主循环: 每 5ms 更新 LVGL tick + 处理渲染
     * 每 25ms 调用 Dashboard_Update 刷新动态元素 */
    TickType_t last_tick = xTaskGetTickCount();
    uint32_t loop_cnt = 0;
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        lv_tick_inc((uint32_t)(now - last_tick));
        last_tick = now;

        lv_timer_handler();

        /* 每 5 次循环 (25ms) 更新仪表盘数据 */
        if ((loop_cnt % 5) == 0) {
            Dashboard_Update();
        }
        loop_cnt++;

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
