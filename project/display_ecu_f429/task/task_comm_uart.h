/* task_comm_uart.h — UART 发送/接收任务 API */

#ifndef __TASK_COMM_UART_H
#define __TASK_COMM_UART_H

void Task_UartTx(void *pvParameters);   /* UART 发送任务（统一消费 TX 队列并发送） */
void Task_UartRx(void *pvParameters);   /* UART 接收任务（字节队列 → 拼包 → 回调） */

#endif /* __TASK_COMM_UART_H */
