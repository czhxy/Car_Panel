/* task_vofa.c — VOFA+ firewater 波形输出任务
 *
 * 每 100ms 从共享状态读取 RPM（g_dash_state.rpm_target，与表盘显示一致），
 * 通过 USART6 以 firewater ASCII 格式发送：单通道 `%u\r\n`，换行结束一帧。
 * USART6 独立于 USART1（printf 日志 + 查询协议），波形数据不会被日志污染。
 *
 * VOFA+ 设置：协议选 firewater / 帧格式 ASCII；波特率 115200。
 * 临时调试代码：测试完成后删除本文件 + usart6.c/h + task_entry 引用。
 */

#include "task_vofa.h"
#include "mod_dashboard_data.h"
#include "usart6.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define VOFA_PERIOD_MS   100

void Vofa_Task(void *pvParameters)
{
    char buf[16];
    (void)pvParameters;

    UART6_Init();   /* PC6/PC7 = USART6 TX/RX，115200 */

    for (;;) {
        /* 与表盘显示同源：读取滑块设定的 rpm_target */
        DashboardState snap = Dashboard_Data_GetSnapshot();

        int n = snprintf(buf, sizeof(buf), "%u\r\n", snap.rpm_target);
        if (n > 0) {
            UART6_SendString(buf);
        }

        vTaskDelay(pdMS_TO_TICKS(VOFA_PERIOD_MS));
    }
}
