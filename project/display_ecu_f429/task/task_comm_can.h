/* task_comm_can.h — CAN 发送/接收任务 API */

#ifndef __TASK_COMM_CAN_H
#define __TASK_COMM_CAN_H

void Task_CanTx(void *pvParameters);     /* CAN 发送任务（消费 TX 队列 + 周期推送电机控制帧） */
void Task_CanRx(void *pvParameters);     /* CAN 接收任务（取帧 → 回调处理） */
void Task_CanTest(void *pvParameters);   /* CAN 测试任务（KEY1 触发测试帧，预留） */

#endif /* __TASK_COMM_CAN_H */
