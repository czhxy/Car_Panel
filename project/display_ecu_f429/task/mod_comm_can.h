#ifndef __MOD_COMM_CAN_H
#define __MOD_COMM_CAN_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "CAN_Protocol.h"

/* ============================================================
 * ModCanFrame —— 平台无关 CAN 帧（应用层统一使用、与芯片无关）
 * ============================================================ */
typedef enum {
    MOD_CAN_IDE_STD = 0,
    MOD_CAN_IDE_EXT = 1
} ModCan_IDE;

typedef enum {
    MOD_CAN_RTR_DATA   = 0,
    MOD_CAN_RTR_REMOTE = 1
} ModCan_RTR;

typedef struct {
    uint32_t     id;        /* ExtId[28:0] 或 StdId[10:0] */
    ModCan_IDE   ide;       /* 扩展帧 / 标准帧 */
    ModCan_RTR   rtr;       /* 数据帧 / 远程帧 */
    uint8_t      dlc;       /* 数据长度 0–8 */
    uint8_t      data[8];   /* 数据负载 */
} ModCanFrame;

/* ---- 队列深度 ---- */
#define CAN_QUEUE_LENGTH 64

/* ---- 诊断计数器：CAN RX 帧数，在 ISR 中递增 ---- */
extern volatile uint32_t can_rx_isr_cnt;

/* ---- 统计变量（非静态，供外部只读访问） ---- */
extern const uint8_t *ModCan_TxErrCount;
extern const uint8_t *ModCan_RxErrCount;

/* ---- API 声明 ---- */
void Mod_Can_Init(void);
bool Mod_Can_TxEvent(const ModCanFrame *frame);   /* 应用层统一发送入口（平台无关帧）*/
void Mod_Can_RxIRQHandler(void);
void Mod_Can_TxTask(void *pvParameters);
void Mod_Can_RxTask(void *pvParameters);
void CAN_Test_Task(void *pvParameters);
void Mod_Can_TxTest(void);
void Can_Heartbeat(void);
void ModCommCan_Tx(void);                         /* 统一消费 TX 队列，提交硬件发送 */
/* 弱符号回调：应用层可重写 */
void ModCommCan_OnRxFrame(const CanRxMsg *rx_msg);
void ModCommCan_PrintRxFrame(const CanRxMsg *rx_msg);

#endif /* __MOD_COMM_CAN_H */
