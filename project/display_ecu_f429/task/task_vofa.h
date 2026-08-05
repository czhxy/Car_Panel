#ifndef __TASK_VOFA_H
#define __TASK_VOFA_H

/* ---- VOFA+ 调试输出总开关（临时，测试完成后置 0 或整段删除） ---- */
#define VOFA_DEBUG   1

/* VOFA+ firewater 波形输出任务（USART6，每 100ms 发送 RPM） */
void Vofa_Task(void *pvParameters);

#endif /* __TASK_VOFA_H */
