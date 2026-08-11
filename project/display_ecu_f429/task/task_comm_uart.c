/* task_comm_uart.c — UART 发送/接收任务
 *
 * 任务函数原位于 mod/mod_comm_uart.c，按「任务放 task 层、机制放 mod 层」拆分：
 *   - Task_UartTx : 发送任务（统一消费 TX 队列并发送，与 printf 临界区互斥）
 *   - Task_UartRx : 接收任务（字节队列 → 拼包状态机 → ModCommUart_OnRxPacket 回调）
 *
 * 拼包状态机（协议编解码）留在 mod_comm_uart，任务通过 Mod_Uart_RxByte 喂字节。
 */

#include "task_comm_uart.h"
#include "mod_comm_uart.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * Task_UartTx — UART 发送任务
 * 统一消费「协议包队列 + 日志队列」并发送（各自内部临界区与残留 printf 互斥）
 * 协议包优先，双队列都空时让出 CPU（与 CAN_TX 任务风格一致）
 * ============================================================ */
void Task_UartTx(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        /* 协议包优先（查询应答等需及时） */
        if (Mod_Uart_TxSend(0)) {
            continue;
        }
        /* 无协议包则取日志发送 */
        if (Mod_Uart_LogSend(0)) {
            continue;
        }
        /* 双队列都空，让出 CPU */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* ============================================================
 * Task_UartRx — UART 接收任务
 * 从字节队列取字节 → 喂拼包状态机（编解码在 mod 层）
 * ============================================================ */
void Task_UartRx(void *pvParameters)
{
    uint8_t ch;

    (void)pvParameters;

    while (1) {
        if (Mod_Uart_RxDequeue(&ch, portMAX_DELAY)) {
            Mod_Uart_RxByte(ch);

            /* 本轮继续消费队列中剩余的字节 */
            while (Mod_Uart_RxDequeue(&ch, 0)) {
                Mod_Uart_RxByte(ch);
            }
        }
    }
}
