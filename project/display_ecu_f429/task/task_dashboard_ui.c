/* task_dashboard_ui.c — 仪表盘 UI 构建 + 周期更新
 *
 * 将 Figma 设计的 240×320 汽车仪表盘用 LVGL v8.3 控件实现。
 * Dashboard_UI_Init() 一次性构建所有静态元素，
 * Dashboard_Update() 每 25ms 从 g_dash_state 读取数据并刷新动态元素。
 */

#include "task_dashboard_ui.h"
#include "mod_dashboard_data.h"
#include "mod_dashboard_fault.h"
#include "mod_comm_can.h"
#include "task_comm_can_protocol.h"
#include "dashboard_images.h"
#include "bsp_log.h"
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
static lv_obj_t *s_error_icon   = NULL;
static lv_obj_t *s_error_box    = NULL;
static lv_obj_t *s_error_label  = NULL;
static lv_obj_t *s_error_code   = NULL;
static lv_obj_t *s_error_title  = NULL;
static lv_meter_indicator_t *s_rpm_arc = NULL;
static lv_obj_t *s_rpm_label    = NULL;
static lv_meter_scale_t *s_rpm_scale = NULL;
static lv_obj_t *s_meter        = NULL;
static lv_obj_t *s_card_frames[DASH_CARD_COUNT] = {NULL};
static lv_obj_t *s_card_labels[DASH_CARD_COUNT] = {NULL};
static lv_obj_t *s_load_slider  = NULL;
static lv_obj_t *s_load_label   = NULL;
static lv_obj_t *s_warning_dots[6] = {NULL};

/* ---- 心跳超时检测 ---- */
#define HEARTBEAT_TIMEOUT_MS  1500
static TickType_t s_last_hb_tick = 0;

/* ---- 卡片颜色常量 ---- */
static const lv_color_t s_warning_colors[6] = {
    COLOR_ABS, COLOR_ESC, COLOR_ENGINE,
    COLOR_BATT_GREEN, COLOR_HIGHBEAM, COLOR_DOOR
};

/* ---- 辅助：设置错误框颜色 ---- */
static void set_error_box_style(bool is_fault, uint16_t error_code)
{
    lv_color_t bg, border, icon_color, text_color, code_color;
    const char *code_str;

    if (is_fault) {
        bg         = lv_color_hex(0x0A0202);  /* rgba(245,66,54,0.04) 近似 */
        border     = COLOR_ERROR_RED;
        icon_color = COLOR_ERROR_RED;
        text_color = COLOR_ERROR_RED;
        code_color = COLOR_WHITE_LOW;
        code_str   = "E002";
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

    lv_label_set_text(s_error_code, code_str);
}

/* ============================================================
 * 事件回调
 * ============================================================ */

/* Load Bar 值变化 → 更新 g_dash_state.rpm_target 并发送 CAN 帧 */
static void on_load_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t pct = lv_slider_get_value(slider);

    Dashboard_Data_Lock();
    g_dash_state.load_pct = (uint8_t)pct;
    g_dash_state.rpm_target = (uint16_t)((pct * 3U));  /* 0–100 → 0–300 RPM */
    uint16_t target = g_dash_state.rpm_target;
    Dashboard_Data_Unlock();

    /* 通过 CAN 发送目标转速 */
    uint8_t data[8];
    memset(data, 0, sizeof(data));
    int16_t speed_enc = (int16_t)target;
    data[0] = (uint8_t)(speed_enc & 0xFF);
    data[1] = (uint8_t)((speed_enc >> 8) & 0xFF);
    data[7] = 3;
    CanProto_SendFrame(CAN_PRIO_REALTIME, CAN_ADDR_MOTORBOARD,
                       CAN_FTYPE_NORMAL, MODE_ID_CTRL_LF, 0, data, 8);

    /* 更新滑块上方标签 */
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
    g_dash_state.selected_card =
        (g_dash_state.selected_card + DASH_CARD_COUNT - 1) % DASH_CARD_COUNT;
    Dashboard_Data_Unlock();
}

/* 右转向灯 → 选中下一个卡片 */
static void on_turn_right(lv_event_t *e)
{
    Dashboard_Data_Lock();
    g_dash_state.selected_card =
        (g_dash_state.selected_card + 1) % DASH_CARD_COUNT;
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
     * 1. Top Bar (208×32, 居中, y=7)
     * ======================================================== */
    lv_obj_t *top_bar = lv_img_create(scr);
    lv_img_set_src(top_bar, &img_top_bar);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 7);

    /* CAN 指示灯 (小圆点, 叠在 CAN 位置 (85,13) 绝对坐标) */
    s_can_led = lv_obj_create(scr);
    lv_obj_set_size(s_can_led, 8, 8);
    lv_obj_set_pos(s_can_led, 85, 13);
    lv_obj_set_style_bg_color(s_can_led, COLOR_ERROR_RED, 0);  /* 初始离线=红色 */
    lv_obj_set_style_bg_opa(s_can_led, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_can_led, 0, 0);
    lv_obj_set_style_radius(s_can_led, LV_RADIUS_CIRCLE, 0);

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
    s_error_box = lv_obj_create(scr);
    lv_obj_set_size(s_error_box, 150, 23);
    lv_obj_align(s_error_box, LV_ALIGN_TOP_MID, 0, 49);
    lv_obj_set_style_bg_color(s_error_box, lv_color_hex(0x020A04), 0);
    lv_obj_set_style_bg_opa(s_error_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_error_box, COLOR_OK_GREEN, 0);
    lv_obj_set_style_border_width(s_error_box, 1, 0);
    lv_obj_set_style_border_opa(s_error_box, LV_OPA_12, 0);
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

    /* ========================================================
     * 4. Gauge 表盘 (120×110, 居中, y=72)
     * ======================================================== */
    lv_obj_t *gauge_cont = lv_obj_create(scr);
    lv_obj_set_size(gauge_cont, 120, 110);
    lv_obj_align(gauge_cont, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_opa(gauge_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge_cont, 0, 0);
    lv_obj_set_style_pad_all(gauge_cont, 0, 0);

    /* Frame 背景图片 */
    lv_obj_t *frame_img = lv_img_create(gauge_cont);
    lv_img_set_src(frame_img, &img_frame);
    lv_obj_align(frame_img, LV_ALIGN_CENTER, 0, 0);

    /* RPM 弧形指示 (lv_meter) */
    s_meter = lv_meter_create(gauge_cont);
    lv_obj_set_size(s_meter, 110, 110);
    lv_obj_align(s_meter, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_meter, 0, 0);

    s_rpm_scale = lv_meter_add_scale(s_meter);
    lv_meter_set_scale_ticks(s_meter, s_rpm_scale, 0, 0, 0, lv_color_black());
    lv_meter_set_scale_range(s_meter, s_rpm_scale, 0, 12000, 270, 135);

    s_rpm_arc = lv_meter_add_arc(s_meter, s_rpm_scale, 3, COLOR_ACCENT, 0);
    lv_meter_set_indicator_start_value(s_meter, s_rpm_arc, 0);
    lv_meter_set_indicator_end_value(s_meter, s_rpm_arc, 6800);

    /* RPM 数值 ("6800", Montserrat 28) */
    s_rpm_label = lv_label_create(gauge_cont);
    lv_obj_set_style_text_color(s_rpm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_rpm_label, &lv_font_montserrat_28, 0);
    lv_label_set_text(s_rpm_label, "6800");
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

    /* ODO Card */
    static const lv_img_dsc_t *card_imgs[DASH_CARD_COUNT] = {
        &img_odo, &img_battery, &img_soc
    };

    unsigned int i;
    for (i = 0; i < DASH_CARD_COUNT; i++) {
        s_card_frames[i] = lv_obj_create(cards_cont);
        lv_obj_set_size(s_card_frames[i], 65, 48);
        lv_obj_set_style_bg_opa(s_card_frames[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(s_card_frames[i],
            (i == 0) ? COLOR_CARD_BORDER_HL : COLOR_CARD_BORDER_DIM, 0);
        lv_obj_set_style_border_width(s_card_frames[i], 1, 0);
        lv_obj_set_style_border_opa(s_card_frames[i],
            (i == 0) ? LV_OPA_COVER : LV_OPA_20, 0);
        lv_obj_set_style_radius(s_card_frames[i], 2, 0);
        lv_obj_set_style_pad_all(s_card_frames[i], 0, 0);

        /* 卡片背景图 */
        lv_obj_t *img = lv_img_create(s_card_frames[i]);
        lv_img_set_src(img, card_imgs[i]);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

        /* 卡片数值标签 */
        s_card_labels[i] = lv_label_create(s_card_frames[i]);
        lv_obj_set_style_text_color(s_card_labels[i], lv_color_white(), 0);
        lv_obj_align(s_card_labels[i], LV_ALIGN_CENTER, 0, 0);

        update_card_label(i, &g_dash_state);
    }

    /* ========================================================
     * 6. Load Bar (lv_slider, 交互型 RPM 下发, 208×28, y=244)
     * ======================================================== */
    /* 刻度标签容器 */
    lv_obj_t *tick_cont = lv_obj_create(scr);
    lv_obj_set_size(tick_cont, 208, 14);
    lv_obj_align(tick_cont, LV_ALIGN_TOP_MID, 0, 244);
    lv_obj_set_style_bg_opa(tick_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tick_cont, 0, 0);
    lv_obj_set_style_pad_all(tick_cont, 0, 0);

    lv_obj_t *tick0 = lv_label_create(tick_cont);
    lv_obj_set_style_text_color(tick0, COLOR_TEXT_DIM, 0);
    lv_label_set_text(tick0, "0");
    lv_obj_align(tick0, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *tick75 = lv_label_create(tick_cont);
    lv_obj_set_style_text_color(tick75, COLOR_TEXT_DIM, 0);
    lv_label_set_text(tick75, "75");
    lv_obj_align(tick75, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *tick150 = lv_label_create(tick_cont);
    lv_obj_set_style_text_color(tick150, COLOR_TEXT_DIM, 0);
    lv_label_set_text(tick150, "150");
    lv_obj_align(tick150, LV_ALIGN_RIGHT_MID, 0, 0);

    /* RPM 目标值标签 */
    s_load_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_load_label, COLOR_ACCENT, 0);
    lv_label_set_text(s_load_label, "126 RPM");
    lv_obj_align(s_load_label, LV_ALIGN_TOP_MID, 0, 258);

    /* Slider 主体 (y=272) */
    s_load_slider = lv_slider_create(scr);
    lv_obj_set_size(s_load_slider, 208, 28);
    lv_obj_align(s_load_slider, LV_ALIGN_TOP_MID, 0, 272);
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
     * 7. Turn Signals (208×20, 居中, y=305)
     * ======================================================== */
    lv_obj_t *turn_bg = lv_img_create(scr);
    lv_img_set_src(turn_bg, &img_turn_signals);
    lv_obj_align(turn_bg, LV_ALIGN_TOP_MID, 0, 305);

    /* 左箭头点击区 */
    lv_obj_t *left_btn = lv_obj_create(scr);
    lv_obj_set_size(left_btn, 104, 20);
    lv_obj_set_pos(left_btn, 16, 305);
    lv_obj_set_style_bg_opa(left_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_btn, 0, 0);
    lv_obj_add_event_cb(left_btn, on_turn_left, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);

    /* 右箭头点击区 */
    lv_obj_t *right_btn = lv_obj_create(scr);
    lv_obj_set_size(right_btn, 104, 20);
    lv_obj_set_pos(right_btn, 120, 305);
    lv_obj_set_style_bg_opa(right_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_btn, 0, 0);
    lv_obj_add_event_cb(right_btn, on_turn_right, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(right_btn, LV_OBJ_FLAG_SCROLLABLE);

    /* ========================================================
     * 8. Warning Dots (208×16, flex row, 居中, y=325)
     * ========================================================
     *    6 个 8×8 圆点, gap 约 26px */
    lv_obj_t *dots_cont = lv_obj_create(scr);
    lv_obj_set_size(dots_cont, 208, 16);
    lv_obj_align(dots_cont, LV_ALIGN_TOP_MID, 0, 325);
    lv_obj_set_style_bg_opa(dots_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots_cont, 0, 0);
    lv_obj_set_style_pad_all(dots_cont, 0, 0);
    lv_obj_set_flex_flow(dots_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (i = 0; i < 6; i++) {
        s_warning_dots[i] = lv_obj_create(dots_cont);
        lv_obj_set_size(s_warning_dots[i], 8, 8);
        lv_obj_set_style_bg_color(s_warning_dots[i], s_warning_colors[i], 0);
        /* 电池(索引3)点亮 (90%不透明度)，其余暗 (20%) */
        lv_obj_set_style_bg_opa(s_warning_dots[i],
            (i == 3) ? LV_OPA_90 : LV_OPA_20, 0);
        lv_obj_set_style_border_width(s_warning_dots[i], 0, 0);
        lv_obj_set_style_radius(s_warning_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(s_warning_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 初始化心跳时间戳 */
    s_last_hb_tick = xTaskGetTickCount();

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
    bool was_online = snap.motor_online;
    if (snap.motor_online &&
        ((now - s_last_hb_tick) > pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS))) {
        snap.motor_online = false;
        Dashboard_Data_Lock();
        g_dash_state.motor_online = false;
        Dashboard_Data_Unlock();
    }

    /* ---- CAN 指示灯 ---- */
    if (s_can_led != NULL) {
        bool online = snap.motor_online;
        lv_color_t led_color = online ? COLOR_OK_GREEN : COLOR_ERROR_RED;

        /* 在线: 闪烁 (500ms 周期), 离线: 常亮 */
        if (online) {
            uint32_t half_period = 250;
            bool led_on = ((now % pdMS_TO_TICKS(500)) < pdMS_TO_TICKS(250));
            lv_obj_set_style_bg_color(s_can_led, led_on ? led_color : lv_color_hex(0x0A1A1A), 0);
        } else {
            lv_obj_set_style_bg_color(s_can_led, led_color, 0);
        }
    }

    /* ---- 错误框 ---- */
    bool is_fault = (snap.error_code != 0);
    set_error_box_style(is_fault, snap.error_code);

    /* ---- RPM 弧线 ---- */
    if (s_rpm_arc != NULL) {
        uint16_t rpm = snap.rpm;
        if (rpm > 12000) rpm = 12000;
        lv_meter_set_indicator_end_value(s_meter, s_rpm_arc, (int32_t)rpm);
    }

    /* ---- RPM 数值 ---- */
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

    /* ---- 离线时，如果之前在线则更新指示灯 ---- */
    if (was_online && !snap.motor_online) {
        if (s_can_led != NULL) {
            lv_obj_set_style_bg_color(s_can_led, COLOR_ERROR_RED, 0);
        }
    }
}
