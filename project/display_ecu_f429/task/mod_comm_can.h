#ifndef __MOD_COMM_CAN_H
#define __MOD_COMM_CAN_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

/* ============================================================
 * 29 位扩展帧 ID 协议定义
 *
 * [28:26] priority   3 bit  数值越小越高
 * [25:22] src        4 bit  源地址 (0~15)
 * [21:18] dst        4 bit  目标地址 (0=广播)
 * [17:16] ftype      2 bit  0=普通 1=OTA 2=组合 3=预留
 * [15: 6] mode_id   10 bit  功能/命令号
 * [ 5: 0] func       6 bit  功能字段
 * ============================================================ */

/* ---- 位偏移定义 ---- */
#define CAN_ID_OFFSET_PRIO    26
#define CAN_ID_OFFSET_SRC     22
#define CAN_ID_OFFSET_DST     18
#define CAN_ID_OFFSET_FTYPE   16
#define CAN_ID_OFFSET_MODE     6
#define CAN_ID_OFFSET_FUNC     0

#define CAN_ID_MASK_PRIO      0x7
#define CAN_ID_MASK_SRC       0xF
#define CAN_ID_MASK_DST       0xF
#define CAN_ID_MASK_FTYPE     0x3
#define CAN_ID_MASK_MODE      0x3FF
#define CAN_ID_MASK_FUNC      0x3F

/* ---- 构造宏 ---- */
#define CAN_ID_BUILD(prio, src, dst, ftype, mode, func) \
    ((((uint32_t)(prio)  & CAN_ID_MASK_PRIO)  << CAN_ID_OFFSET_PRIO) | \
     (((uint32_t)(src)   & CAN_ID_MASK_SRC)   << CAN_ID_OFFSET_SRC)  | \
     (((uint32_t)(dst)   & CAN_ID_MASK_DST)   << CAN_ID_OFFSET_DST)  | \
     (((uint32_t)(ftype) & CAN_ID_MASK_FTYPE) << CAN_ID_OFFSET_FTYPE)| \
     (((uint32_t)(mode)  & CAN_ID_MASK_MODE)  << CAN_ID_OFFSET_MODE) | \
     (((uint32_t)(func)  & CAN_ID_MASK_FUNC)  << CAN_ID_OFFSET_FUNC))

/* ---- 解析宏 ---- */
#define CAN_ID_GET_PRIO(id)  (((id) >> CAN_ID_OFFSET_PRIO) & CAN_ID_MASK_PRIO)
#define CAN_ID_GET_SRC(id)   (((id) >> CAN_ID_OFFSET_SRC)  & CAN_ID_MASK_SRC)
#define CAN_ID_GET_DST(id)   (((id) >> CAN_ID_OFFSET_DST)  & CAN_ID_MASK_DST)
#define CAN_ID_GET_FTYPE(id) (((id) >> CAN_ID_OFFSET_FTYPE) & CAN_ID_MASK_FTYPE)
#define CAN_ID_GET_MODE(id)  (((id) >> CAN_ID_OFFSET_MODE) & CAN_ID_MASK_MODE)
#define CAN_ID_GET_FUNC(id)  (((id) >> CAN_ID_OFFSET_FUNC) & CAN_ID_MASK_FUNC)

/* ---- 设备地址枚举 ---- */
typedef enum {
    CAN_DEVICE_ID_BROADCAST  	= 0x00,
    CAN_DEVICE_ID_MAINBOARD  	= 0x01,
    CAN_DEVICE_ID_MOTOR     	= 0x02,
    CAN_DEVICE_ID_MAX        	= 0x0F
} CAN_DeviceID;

#define CAN_SELF_ADDR   CAN_DEVICE_ID_MAINBOARD

/* ---- 优先级枚举 ---- */
typedef enum {
    CAN_PRIO_EMERGENCY   = 0,
    CAN_PRIO_REALTIME    = 1,
    CAN_PRIO_QUERY_REPLY = 2,
    CAN_PRIO_ALERT       = 3,
    CAN_PRIO_HEARTBEAT   = 4,
    CAN_PRIO_CONFIG      = 5,
    CAN_PRIO_OTA         = 6,
    CAN_PRIO_MAX         = 7
} CAN_Priority;

/* ---- 帧类型枚举 ---- */
typedef enum {
    CAN_FTYPE_NORMAL   = 0,
    CAN_FTYPE_OTA      = 1,
    CAN_FTYPE_COMBINED = 2,
    CAN_FTYPE_RESERVED = 3
} CAN_FrameType;

/* ---- 发送帧默认 ID ---- */
#define CAN_TX_ID  CAN_ID_BUILD(CAN_PRIO_ALERT, CAN_SELF_ADDR, CAN_DEVICE_ID_BROADCAST, CAN_FTYPE_NORMAL, 0x001, 0x01)

/* ---- 队列深度 ---- */
#define CAN_QUEUE_LENGTH 64

/* ---- API 声明 ---- */
void Mod_Can_Init(void);
bool Mod_Can_TxEvent(CanTxMsg tx_message);
void Mod_Can_RxIRQHandler(void);
void Mod_Can_TxTask(void *pvParameters);
void Mod_Can_RxTask(void *pvParameters);
void CAN_Test_Task(void *pvParameters);
void Mod_Can_TxTest(void);

/* 弱符号回调：应用层可重写 */
void ModCommCan_OnRxFrame(const CanRxMsg *rx_msg);
void ModCommCan_PrintRxFrame(const CanRxMsg *rx_msg);

#endif /* __MOD_COMM_CAN_H */
