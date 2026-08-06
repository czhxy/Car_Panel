/* mod_ui.h — 仪表盘 UI 相关 mod 聚合模块
 *
 * 三段内容（原分散在三个文件，现收敛到 mod_ui 一个模块）：
 *   1. 仪表盘共享状态（原 mod_dashboard_data.h）
 *      DashboardState 是所有 UI 元素更新和 CAN 数据写入的单一数据源。
 *      CAN RX 任务（优先级 4）写入，UI 任务（优先级 3）读取，
 *      通过 FreeRTOS 互斥锁保护。
 *   2. 故障码映射（原 mod_dashboard_fault.h）
 *      故障码→消息映射表，新增故障码只需在映射表末尾追加一行。
 *   3. LVGL 通用封装
 *      Mod_UI_Box / Mod_UI_Label：常用控件创建封装，供 task 层复用。
 */

#ifndef __MOD_UI_H
#define __MOD_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "lvgl.h"

/* ============================================================
 * 1. 仪表盘共享状态（原 mod_dashboard_data.h）
 * ============================================================ */

/* ---- 卡片枚举 ---- */
typedef enum {
    DASH_CARD_ODO  = 0,
    DASH_CARD_BATT = 1,
    DASH_CARD_SOC  = 2,
    DASH_CARD_COUNT
} DashboardCard;

/* ---- 仪表盘共享状态 ---- */
typedef struct {
    uint16_t rpm;               /* 发动机转速 (RPM)，来自 CAN 电机状态帧 */
    uint8_t  motor_status;      /* 状态位: bit0=RUN, bit1=ENABLE, bit2=FAULT */
    uint16_t error_code;        /* 当前故障码 (0=无故障) */
    bool     motor_online;      /* 动力域 CAN 通信是否在线 */
    uint32_t odo_value;         /* 累计圈数 */
    uint8_t  batt_level;        /* 电池电量 % */
    uint8_t  soc_level;         /* SOC % */
    uint8_t  load_pct;          /* 负载 % (0–100, 映射到 0–300 RPM 目标) */
    uint16_t rpm_target;        /* 目标转速 (由 Load Bar 交互设定) */
    bool     paused;            /* 一键暂停标志: true=速度锁定为 0 */
    DashboardCard selected_card;/* 当前选中卡片 */
    uint32_t last_hb_tick;      /* 最后收到心跳的 tick (用于超时检测) */
} DashboardState;

/* ---- 全局变量 ---- */
extern DashboardState g_dash_state;
extern SemaphoreHandle_t g_dash_mutex;

/* ---- 线程安全 API ---- */
void Dashboard_Data_Init(void);
void Dashboard_Data_Lock(void);
void Dashboard_Data_Unlock(void);

/* 便捷快照读取（加锁→拷贝→解锁→返回拷贝，避免长锁） */
DashboardState Dashboard_Data_GetSnapshot(void);

/* ============================================================
 * 2. 故障码映射（原 mod_dashboard_fault.h）
 * ============================================================ */

/* ---- 故障级别 ---- */
typedef enum {
    FAULT_NONE  = 0,  /* 无故障 (保持绿色) */
    FAULT_INFO  = 1,  /* 信息 */
    FAULT_WARN  = 2,  /* 警告 (黄色) */
    FAULT_ERROR = 3,  /* 错误 (红色) */
} FaultLevel;

/* ---- 故障码条目 ---- */
typedef struct {
    uint16_t    code;
    const char *message;
    FaultLevel  level;
} FaultCodeEntry;

/* ---- API ---- */
const char* Dashboard_Fault_Lookup(uint16_t error_code, FaultLevel *out_level);

/* ============================================================
 * 3. LVGL 通用封装
 * ============================================================ */

/* 创建标准容器/圆点：设置位置/大小/背景色/圆角，去边框与滚动 */
lv_obj_t *Mod_UI_Box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, lv_color_t bg, lv_coord_t radius);

/* 创建文本标签：设置文字/颜色/可选字体/位置 */
lv_obj_t *Mod_UI_Label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                       const char *text, lv_color_t color, const lv_font_t *font);

#endif /* __MOD_UI_H */
