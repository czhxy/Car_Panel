/* mod_dashboard_data.c — 仪表盘共享状态实现 */

#include "mod_dashboard_data.h"
#include <string.h>

/* ---- 全局变量 ---- */
DashboardState g_dash_state;
SemaphoreHandle_t g_dash_mutex = NULL;

/* ---- 初始化 ---- */
void Dashboard_Data_Init(void)
{
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

    g_dash_mutex = xSemaphoreCreateMutex();
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
