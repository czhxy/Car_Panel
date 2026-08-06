/* task_ui.h — 显示域 UI 任务 API */

#ifndef __TASK_UI_H
#define __TASK_UI_H

#include "lvgl.h"

void Task_UI(void *pvParameters);
void Dashboard_UI_Init(lv_obj_t *scr);
void Dashboard_Update(void);

#endif /* __TASK_UI_H */
