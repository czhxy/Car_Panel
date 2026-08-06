#include "mod_comm_can.h"
#include "mod_ui.h"
#include "bsp_can.h"
#include "bsp_log.h"
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
static QueueHandle_t CanTxQueue = NULL;   /* TX FIFO 队列（与 RX 对称，深度 64） */
static QueueHandle_t CanRxQueue = NULL;   /* RX FIFO 队列（ISR 推入，RX 任务消费） */

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
 * 队列满时返回 false 并累计 tx_err_count（有界队列的正常背压行为）
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
 * Mod_Can_RxDequeue — 从 RX 队列取一帧（供接收任务调用）
 * timeout: portMAX_DELAY 阻塞等待 / 0 立即返回
 * 返回: 是否成功取到一帧
 * ============================================================ */
bool Mod_Can_RxDequeue(CanRxMsg *msg, TickType_t timeout)
{
    if (CanRxQueue == NULL || msg == NULL) { return false; }
    return (xQueueReceive(CanRxQueue, msg, timeout) == pdPASS);
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
        if (rx_msg->DLC < 5U) {
            break;
        }

        /* 心跳帧: [status, uptime_L, uptime_H, err_L, err_H, 0, 0, 0]
         * 解析 error_code 并更新在线状态 */
        uint16_t error_code = ((uint16_t)rx_msg->Data[4] << 8) | rx_msg->Data[3];

        Dashboard_Data_Lock();
        g_dash_state.error_code   = error_code;
        g_dash_state.motor_online = true;
        g_dash_state.last_hb_tick = xTaskGetTickCount();
        Dashboard_Data_Unlock();

        LOG_D("[CAN] HB from motor: status=0x%02X error=0x%04X\r\n",
              rx_msg->Data[0], error_code);
        break;
    }

    case MODE_ID_STATUS_MOTOR:
    {
        if (rx_msg->DLC < 8U) {
            break;
        }

        /* 电机状态帧 (0x110): CanStatusMotor 真实线序（packed 小端）
         * [0..1]=speed(×10 rpm) [2..3]=current(mA) [4..5]=angle(°×10)
         * [6]=status [7]=temp；当前仅取 rpm，其余字段暂不关注 */
        CanStatusMotor st;
        memcpy(&st, rx_msg->Data, sizeof(st));

        int16_t  speed = st.motor_speed;              /* rpm×10 */
        uint16_t rpm   = (uint16_t)(speed > 0 ? speed / 10 : 0);

        Dashboard_Data_Lock();
        g_dash_state.rpm          = rpm;
        g_dash_state.motor_status = 0;   /* 暂不解析 */
        g_dash_state.odo_value    = 0;   /* 动力域暂无里程字段 */
        g_dash_state.motor_online = true;
        Dashboard_Data_Unlock();

        LOG_D("[CAN] Motor status: rpm=%u\r\n", rpm);
        break;
    }

    default:
        /* 未识别的帧，忽略 */
        break;
    }
}

/* ============================================================
 * ModCommCan_Tx — 统一消费 TX 队列，提交硬件发送
 * 用 xQueuePeek 而非"取出后回灌队首"：先看队首帧，发送成功才出队；
 * 邮箱满则不取出、帧留在队首下一轮再试。相比回灌方案：
 *   - 不存在 xQueueSendToFront 回灌失败导致的静默丢帧；
 *   - 不存在回灌队首造成的乱序插队（旧帧插到新帧前面）。
 * 队列空立即返回。
 * ============================================================ */
void ModCommCan_Tx(void)
{
    ModCanFrame frame;
    while (xQueuePeek(CanTxQueue, &frame, 0) == pdPASS) {
        CanTxMsg tx_msg;
        memset(&tx_msg, 0, sizeof(tx_msg));
        if (frame.ide == MOD_CAN_IDE_EXT) {
            tx_msg.ExtId = frame.id & 0x1FFFFFFFU;
            tx_msg.IDE   = CAN_ID_EXT;
        } else {
            tx_msg.StdId = frame.id & 0x7FFU;
            tx_msg.IDE   = CAN_ID_STD;
        }
        tx_msg.RTR = (frame.rtr == MOD_CAN_RTR_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        tx_msg.DLC = (frame.dlc > 8) ? 8 : frame.dlc;
        memcpy(tx_msg.Data, frame.data, tx_msg.DLC);

        if (CAN_Transmit(CAN1, &tx_msg) == CAN_TxStatus_NoMailBox) {
            break;   /* 邮箱满：帧留在队首，下一轮再试 */
        }
        xQueueReceive(CanTxQueue, &frame, 0);   /* 发送成功，出队 */
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
    ModCanFrame frame;
    CanHeartbeatData hb;

    memset(&frame, 0, sizeof(frame));
    memset(&hb, 0, sizeof(hb));

    /* 心跳帧与动力域统一为 mode_id=0x320 + CanHeartbeatData 载荷 */
    hb.status     = 0x00;   /* 显示域状态: 0=正常 */
    hb.uptime     = (uint16_t)(xTaskGetTickCount() / pdMS_TO_TICKS(1000U));
    hb.error_code = 0x00;   /* 显示域暂不上报故障 */

    frame.id  = CAN_ID_BUILD(CAN_PRIO_HEARTBEAT, CAN_SELF_ADDR, CAN_ADDR_BROADCAST,
                             CAN_FTYPE_NORMAL, MODE_ID_HEARTBEAT, 0x00U);
    frame.ide = MOD_CAN_IDE_EXT;
    frame.rtr = MOD_CAN_RTR_DATA;
    frame.dlc = (uint8_t)sizeof(hb);
    memcpy(frame.data, &hb, sizeof(hb));

    Mod_Can_TxEvent(&frame);
}
