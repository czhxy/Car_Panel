/* mod_ui.c — 仪表盘 UI 相关 mod 聚合实现
 *
 * 三段内容（与 mod_ui.h 对应）：
 *   1. 仪表盘共享状态（原 mod_dashboard_data.c）
 *   2. 故障码映射（原 mod_dashboard_fault.c）
 *   3. LVGL 通用封装
 */

#include "mod_ui.h"
#include <string.h>
#include <stddef.h>
#include "task.h"

/* ============================================================
 * 1. 仪表盘共享状态（原 mod_dashboard_data.c）
 * ============================================================ */

/* ---- 全局变量 ---- */
DashboardState g_dash_state;
SemaphoreHandle_t g_dash_mutex = NULL;
static bool s_dashboard_data_initialized = false;

/* ---- 初始化 ---- */
void Dashboard_Data_Init(void)
{
    if (s_dashboard_data_initialized) {
        return;
    }

    memset(&g_dash_state, 0, sizeof(g_dash_state));

    /* 设置默认值 */
    g_dash_state.batt_level     = 78;
    g_dash_state.soc_level      = 85;
    g_dash_state.load_pct       = 42;
    g_dash_state.rpm_target     = 126;   /* 42% of 300 */
    g_dash_state.selected_card  = DASH_CARD_ODO;
    g_dash_state.rpm            = 6800;
    g_dash_state.error_code     = 0;
    g_dash_state.motor_online   = false;
    g_dash_state.last_hb_tick   = xTaskGetTickCount();

    g_dash_mutex = xSemaphoreCreateMutex();
    s_dashboard_data_initialized = true;
}

/* ---- 加锁 ---- */
void Dashboard_Data_Lock(void)
{
    if (g_dash_mutex != NULL) {
        xSemaphoreTake(g_dash_mutex, portMAX_DELAY);
    }
}

/* ---- 解锁 ---- */
void Dashboard_Data_Unlock(void)
{
    if (g_dash_mutex != NULL) {
        xSemaphoreGive(g_dash_mutex);
    }
}

/* ---- 快照读取 ---- */
DashboardState Dashboard_Data_GetSnapshot(void)
{
    DashboardState snap;
    Dashboard_Data_Lock();
    memcpy(&snap, &g_dash_state, sizeof(DashboardState));
    Dashboard_Data_Unlock();
    return snap;
}

/* ============================================================
 * 2. 故障码映射（原 mod_dashboard_fault.c）
 * ============================================================ */

/* ---- 故障码映射表 ---- */
static const FaultCodeEntry s_fault_table[] = {
    /* code,     message,                  level */
    { 0x0000, "ALL SYSTEMS NORMAL",       FAULT_NONE },
    { 0x0001, "CAN TIMEOUT",              FAULT_ERROR },
    { 0x0002, "MOTOR STALL",              FAULT_ERROR },
    { 0x0004, "ENCODER LOSS",             FAULT_ERROR },
};

#define FAULT_TABLE_SIZE (sizeof(s_fault_table) / sizeof(s_fault_table[0]))

/* ---- 故障码查询 ---- */
const char* Dashboard_Fault_Lookup(uint16_t error_code, FaultLevel *out_level)
{
    unsigned int i;
    for (i = 0; i < FAULT_TABLE_SIZE; i++) {
        if (s_fault_table[i].code == error_code) {
            if (out_level != NULL) {
                *out_level = s_fault_table[i].level;
            }
            return s_fault_table[i].message;
        }
    }

    /* 未找到：默认 UNKNOWN ERROR */
    if (out_level != NULL) {
        *out_level = FAULT_ERROR;
    }
    return "UNKNOWN ERROR";
}

/* ============================================================
 * 3. LVGL 通用封装
 * ============================================================ */

/* ---- 标准容器/圆点 ---- */
lv_obj_t *Mod_UI_Box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, lv_color_t bg, lv_coord_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

/* ---- 文本标签 ---- */
lv_obj_t *Mod_UI_Label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                       const char *text, lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, x, y);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    if (font != NULL) {
        lv_obj_set_style_text_font(lbl, font, 0);
    }
    return lbl;
}
