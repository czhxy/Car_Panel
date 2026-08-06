#include "mod_can_protocol.h"
#include "mod_comm_can.h"
#include "mod_motor.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ============================================================
 * CanProto_SendFrame — 构造 ModCanFrame 并入 TX 队列
 * ============================================================ */
void CanProto_SendFrame(uint8_t prio, uint8_t dev_id, uint8_t ftype,
                        uint16_t mode_id, uint8_t func,
                        const uint8_t *data, uint8_t dlc)
{
    ModCanFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.id  = CAN_ID_BUILD(prio, CAN_SELF_ADDR, dev_id, ftype, mode_id, func);
    frame.ide = MOD_CAN_IDE_EXT;
    frame.rtr = MOD_CAN_RTR_DATA;
    frame.dlc = (dlc > 8) ? 8 : dlc;
    if (data != NULL && dlc > 0) {
        memcpy(frame.data, data, frame.dlc);
    }
    Mod_Can_TxEvent(&frame);
}

/* ============================================================
 * CanProtocol_WheelCtlSend — 电机控制帧周期推送（10ms 限频）
 * 读取电机转速/角度 → 编码 int16(*10) → CanProto_SendFrame
 * ============================================================ */
void CanProtocol_WheelCtlSend(void)
{
    static TickType_t last_send_tick = 0;
    TickType_t now = xTaskGetTickCount();

    if ((now - last_send_tick) < pdMS_TO_TICKS(10)) {
        return;
    }
    last_send_tick = now;

    float speed = Mod_Motor_Get_Speed();   /* LVGL 仪表盘 rpm_target */

    uint8_t data[8];
    memset(data, 0, sizeof(data));

    int16_t speed_enc = (int16_t)(speed * 10.0f);
    data[0] = (uint8_t)(speed_enc & 0xFF);
    data[1] = (uint8_t)((speed_enc >> 8) & 0xFF);

    /* 角度不再上报，直接填 0；data[4..7] 保持 0，线序干净 */
    data[2] = 0;
    data[3] = 0;

    CanProto_SendFrame(CAN_PRIO_REALTIME, CAN_ADDR_MOTORBOARD,
                       CAN_FTYPE_NORMAL, MODE_ID_CTRL_LF, 0, data, 8);
}
