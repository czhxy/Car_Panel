#include "mod_comm_can.h"
#include "mod_dashboard_data.h"
#include "task_comm_can_protocol.h"
#include "bsp_can.h"
#include "bsp_key.h"
#include "bsp_log.h"
#include "semphr.h"
#include <string.h>
#include "task.h"

/* ============================================================
 * 弱符号兼容宏
 * MDK: __weak / GCC: __attribute__((weak))
 * ============================================================ */
#if defined(__GNUC__) && !defined(__CC_ARM)
  #define WEAK __attribute__((weak))
#else
  #define WEAK __weak
#endif

/* ---- 静态变量 ---- */
static QueueHandle_t CanTxQueue = NULL;
static QueueHandle_t CanRxQueue = NULL;

static struct {
    uint8_t tx_err_count;
    uint8_t rx_err_count;
} event_err_count;

/* ---- 统计变量（非静态，供外部只读访问） ---- */
const uint8_t *ModCan_TxErrCount  = &event_err_count.tx_err_count;
const uint8_t *ModCan_RxErrCount  = &event_err_count.rx_err_count;

/* ---- 诊断计数器：CAN RX 中断帧数（定义于此处，供外部诊断读取） ---- */
volatile uint32_t can_rx_isr_cnt = 0;

/* ============================================================
 * Mod_Can_Init — 创建 TX/RX FreeRTOS 队列
 * ============================================================ */
void Mod_Can_Init(void)
{
    CanTxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(ModCanFrame));
    CanRxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanRxMsg));
}

/* ============================================================
 * Mod_Can_TxEvent — 非阻塞入 TX 队列
 * ============================================================ */
bool Mod_Can_TxEvent(const ModCanFrame *frame)
{
    if (CanTxQueue == NULL || frame == NULL) { return false; }
    if (xQueueSend(CanTxQueue, frame, 0) == pdPASS) { return true; }
    event_err_count.tx_err_count++;
    return false;
}

/* ============================================================
 * Mod_Can_RxIRQHandler — FIFO0 中断读取，入接收队列
 * 在 CAN1_RX0_IRQHandler 中调用
 * ============================================================ */
void Mod_Can_RxIRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CanRxMsg rx_msg;

    while (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET) {
        CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
        can_rx_isr_cnt++;   /* 累计 RX 中断帧数，供诊断 */

        if (CanRxQueue != NULL) {
            xQueueSendFromISR(CanRxQueue, &rx_msg, &xHigherPriorityTaskWoken);
        }
        
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================
 * ModCommCan_PrintRxFrame — 解析并打印接收帧详情
 * ============================================================ */
void ModCommCan_PrintRxFrame(const CanRxMsg *rx_msg)
{
    uint8_t i;

    if (rx_msg->IDE == CAN_ID_EXT) {
        uint32_t id = rx_msg->ExtId;
        LOG_I("[RX] ExtID=0x%08X src=%u dst=%u type=%u mode=0x%03X func=%u DLC=%u data:",
                   id,
                   CAN_ID_GET_SRC(id),
                   CAN_ID_GET_DST(id),
                   CAN_ID_GET_FTYPE(id),
                   CAN_ID_GET_MODE(id),
                   CAN_ID_GET_FUNC(id),
                   rx_msg->DLC);
    } else {
        LOG_I("[RX] StdID=0x%03X DLC=%u data:", rx_msg->StdId, rx_msg->DLC);
    }

    for (i = 0; i < rx_msg->DLC; i++) {
        LOG_I(" %02X", rx_msg->Data[i]);
    }	
}

/* ============================================================
 * ModCommCan_OnRxFrame — 强符号接收回调（替换弱符号默认实现）
 *
 * 解析动力域 ECU 上报帧：
 * - 心跳帧 (mode_id=0x320): 更新 motor_online、error_code、status
 * - 电机状态帧 (mode_id=0x110): 更新 rpm、odo_value、motor_status
 * ============================================================ */
void ModCommCan_OnRxFrame(const CanRxMsg *rx_msg)
{
    if (rx_msg->IDE != CAN_ID_EXT) {
        return;
    }

    uint32_t ext_id = rx_msg->ExtId;
    uint8_t  src    = CAN_ID_GET_SRC(ext_id);
    uint16_t mode   = CAN_ID_GET_MODE(ext_id);

    /* 仅处理来自动力域 (src=0x02) 的帧 */
    if (src != CAN_ADDR_MOTORBOARD) {
        return;
    }

    switch (mode) {
    case MODE_ID_HEARTBEAT:
    {
        /* 心跳帧: [status, uptime_L, uptime_H, err_L, err_H, 0, 0, 0]
         * 解析 error_code 并更新在线状态 */
        uint16_t error_code = ((uint16_t)rx_msg->Data[4] << 8) | rx_msg->Data[3];

        Dashboard_Data_Lock();
        g_dash_state.error_code   = error_code;
        g_dash_state.motor_online = true;
        Dashboard_Data_Unlock();

        LOG_D("[CAN] HB from motor: status=0x%02X error=0x%04X\r\n",
              rx_msg->Data[0], error_code);
        break;
    }

    case MODE_ID_STATUS_MOTOR:
    {
        /* 电机状态帧 (0x110): [rpm_L, rpm_H, status, odo_0, odo_1, odo_2, odo_3, 0]
         * 待动力域正式实现后确认格式 */
        uint16_t rpm     = ((uint16_t)rx_msg->Data[1] << 8) | rx_msg->Data[0];
        uint8_t  status  = rx_msg->Data[2];
        uint32_t odo     = ((uint32_t)rx_msg->Data[6] << 24) |
                           ((uint32_t)rx_msg->Data[5] << 16) |
                           ((uint32_t)rx_msg->Data[4] << 8)  |
                            rx_msg->Data[3];

        Dashboard_Data_Lock();
        g_dash_state.rpm          = rpm;
        g_dash_state.motor_status = status;
        g_dash_state.odo_value    = odo;
        g_dash_state.motor_online = true;
        Dashboard_Data_Unlock();

        LOG_D("[CAN] Motor status: rpm=%u status=0x%02X odo=%lu\r\n",
              rpm, status, (unsigned long)odo);
        break;
    }

    default:
        /* 未识别的帧，忽略 */
        break;
    }
}

/* ============================================================
 * ModCommCan_Tx — 统一消费 TX 队列，提交硬件发送
 * 非阻塞取尽当前队列，每帧 ModCanFrame → CanTxMsg → CAN_Transmit；
 * 邮箱满则回灌队首并 break（本轮结束、下一轮再试），队列空立即返回。
 * ============================================================ */
void ModCommCan_Tx(void)
{
    ModCanFrame frame;
    while (xQueueReceive(CanTxQueue, &frame, 0) == pdPASS) {
        CanTxMsg tx_msg;
        memset(&tx_msg, 0, sizeof(tx_msg));
        if (frame.ide == MOD_CAN_IDE_EXT) 
				{ 
					tx_msg.ExtId = frame.id & 0x1FFFFFFFU; tx_msg.IDE = CAN_ID_EXT; 
				}
        else                               
				{ 
					tx_msg.StdId = frame.id & 0x7FFU;       
					tx_msg.IDE = CAN_ID_STD; 
				}
        tx_msg.RTR = (frame.rtr == MOD_CAN_RTR_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        tx_msg.DLC = (frame.dlc > 8) ? 8 : frame.dlc;
        memcpy(tx_msg.Data, frame.data, tx_msg.DLC);
        if (CAN_Transmit(CAN1, &tx_msg) == CAN_TxStatus_NoMailBox) {
            xQueueSendToFront(CanTxQueue, &frame, 0);   /* 回灌，下一轮再试 */
            break;
        }
    }
}

/* ============================================================
 * Mod_Can_TxTask — 发送任务
 * ① 数据发送 → ② 数据推送 → ③ 让出 CPU
 * ============================================================ */
void Mod_Can_TxTask(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        ModCommCan_Tx();               /* ① 数据发送：统一消费 TX 队列 */
        // CanProtocol_HeartbeatCheck();   /* 预留 */
        CanProtocol_WheelCtlSend();    /* ② 数据推送：电机控制帧（10ms 限频）*/
        CanProtocol_WheelDebugQuery(); /* ② 数据推送：电机调试查询（占位）*/
        vTaskDelay(pdMS_TO_TICKS(1));  /* ③ 让出 CPU */
    }
}

/* ============================================================
 * Mod_Can_RxTask — 接收任务
 * 从队列取帧 → 调 ModCommCan_OnRxFrame → 批量处理
 * ============================================================ */
void Mod_Can_RxTask(void *pvParameters)
{
    CanRxMsg rx_msg;

    (void)pvParameters;

    while (1) {
        if (xQueueReceive(CanRxQueue, &rx_msg, portMAX_DELAY) == pdPASS) {
            ModCommCan_OnRxFrame(&rx_msg);

            /* 本轮继续处理队列中剩余的消息 */
            while (xQueueReceive(CanRxQueue, &rx_msg, 0) == pdPASS) {
                ModCommCan_OnRxFrame(&rx_msg);
            }
        }
				vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ============================================================
 * Mod_Can_TxTest — 构造并发送一帧测试报文
 * 8 字节递增数据，使用 CAN_TX_ID 扩展帧
 * ============================================================ */
void Mod_Can_TxTest(void)
{
    static uint8_t seq = 0;
    ModCanFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.id  = CAN_TX_ID;
    frame.ide = MOD_CAN_IDE_EXT;
    frame.rtr = MOD_CAN_RTR_DATA;
    frame.dlc = 8;

    frame.data[0] = seq++;
    frame.data[1] = seq++;
    frame.data[2] = seq++;
    frame.data[3] = seq++;
    frame.data[4] = seq++;
    frame.data[5] = seq++;
    frame.data[6] = seq++;
    frame.data[7] = seq++;

    uint32_t id = frame.id;
    LOG_I("[TX] ExtID=0x%08X src=%u dst=%u type=%u mode=0x%03X func=%u DLC=%u data:",
               id,
               CAN_ID_GET_SRC(id),
               CAN_ID_GET_DST(id),
               CAN_ID_GET_FTYPE(id),
               CAN_ID_GET_MODE(id),
               CAN_ID_GET_FUNC(id),
               frame.dlc);
    uint8_t i;
    for (i = 0; i < frame.dlc; i++) {
        LOG_I(" %02X", frame.data[i]);
    }
    Mod_Can_TxEvent(&frame);
}
void Can_Heartbeat(void)
{
    static uint32_t sHeartbeatCnt = 0;
    ModCanFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.id  = CAN_HEARTBEAT_ID;
    frame.ide = MOD_CAN_IDE_EXT;
    frame.rtr = MOD_CAN_RTR_DATA;
    frame.dlc = 8;

    sHeartbeatCnt++;
    frame.data[0] = (uint8_t)(sHeartbeatCnt);
    frame.data[1] = (uint8_t)(sHeartbeatCnt >> 8);
    frame.data[2] = (uint8_t)(sHeartbeatCnt >> 16);
    frame.data[3] = (uint8_t)(sHeartbeatCnt >> 24);
    frame.data[4] = 0x00;   /* 设备状态: 0=正常 */
    frame.data[5] = 0x00;   /* 预留 */
    frame.data[6] = 0x00;   /* 预留 */
    frame.data[7] = 0x00;   /* 预留 */

    Mod_Can_TxEvent(&frame);
}
/* ============================================================
 * CAN_Test_Task — 测试任务
 * 按下 KEY1 后发送一帧测试报文
 * ============================================================ */
void CAN_Test_Task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        if(xSemaphoreTake(xKey1Sem, pdMS_TO_TICKS(100)) == pdTRUE)
				{
					
					Mod_Can_TxTest();
				}
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
