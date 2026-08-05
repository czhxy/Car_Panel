/* mod_dashboard_data.h — 仪表盘共享状态 + 线程安全锁 API
 *
 * DashboardState 是所有 UI 元素更新和 CAN 数据写入的单一数据源。
 * CAN RX 任务（优先级 4）写入，LCD_DEMO 任务（优先级 3）读取，
 * 通过 FreeRTOS 互斥锁保护。
 */

#ifndef __MOD_DASHBOARD_DATA_H
#define __MOD_DASHBOARD_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"

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

#endif /* __MOD_DASHBOARD_DATA_H */
