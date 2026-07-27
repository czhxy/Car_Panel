/* task_dashboard_ui.h — 仪表盘 UI 构建/更新 API */

#ifndef __TASK_DASHBOARD_UI_H
#define __TASK_DASHBOARD_UI_H

#include "lvgl.h"

void Dashboard_UI_Init(lv_obj_t *scr);
void Dashboard_Update(void);

#endif /* __TASK_DASHBOARD_UI_H */
