#ifndef CAN_PROTOCOL_H__
#define CAN_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * 29 位扩展帧 ID 协议定义（统一协议头，全工程唯一来源）
 *
 * ID [28:26] 3 bit  优先级        数值越小越高
 * ID [25:22] 4 bit  源地址
 * ID [21:18] 4 bit  目标地址       (0 = 广播)
 * ID [17:16] 2 bit  帧类型         0=普通 1=OTA 2=组合 3=预留
 * ID [15: 6] 10 bit mode_id       功能/命令号
 * ID [ 5: 0] 6 bit  功能字段
 * ================================================================ */

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

/* ---- 29 位 ID 构造宏 ---- */
#define CAN_ID_BUILD(prio, src, dst, ftype, mode, func) \
    ((((uint32_t)(prio)  & CAN_ID_MASK_PRIO)  << CAN_ID_OFFSET_PRIO) | \
     (((uint32_t)(src)   & CAN_ID_MASK_SRC)   << CAN_ID_OFFSET_SRC)  | \
     (((uint32_t)(dst)   & CAN_ID_MASK_DST)   << CAN_ID_OFFSET_DST)  | \
     (((uint32_t)(ftype) & CAN_ID_MASK_FTYPE) << CAN_ID_OFFSET_FTYPE)| \
     (((uint32_t)(mode)  & CAN_ID_MASK_MODE)  << CAN_ID_OFFSET_MODE) | \
     (((uint32_t)(func)  & CAN_ID_MASK_FUNC)  << CAN_ID_OFFSET_FUNC))

/* ---- 29 位 ID 解析宏 ---- */
#define CAN_ID_GET_PRIO(id)  (((id) >> CAN_ID_OFFSET_PRIO) & CAN_ID_MASK_PRIO)
#define CAN_ID_GET_SRC(id)   (((id) >> CAN_ID_OFFSET_SRC)  & CAN_ID_MASK_SRC)
#define CAN_ID_GET_DST(id)   (((id) >> CAN_ID_OFFSET_DST)  & CAN_ID_MASK_DST)
#define CAN_ID_GET_FTYPE(id) (((id) >> CAN_ID_OFFSET_FTYPE) & CAN_ID_MASK_FTYPE)
#define CAN_ID_GET_MODE(id)  (((id) >> CAN_ID_OFFSET_MODE) & CAN_ID_MASK_MODE)
#define CAN_ID_GET_FUNC(id)  (((id) >> CAN_ID_OFFSET_FUNC) & CAN_ID_MASK_FUNC)

/* ================================================================
 * 设备地址定义
 * ================================================================ */
#define CAN_ADDR_BROADCAST      0x0U    /* 广播 */
#define CAN_ADDR_MAINBOARD      0x1U    /* 主板 */
#define CAN_ADDR_MOTORBOARD     0x2U    /* 电机板 */
#define CAN_ADDR_MAX            0x0FU   /* 地址上限 */

#define CAN_SELF_ADDR   CAN_ADDR_MOTORBOARD

/* ================================================================
 * 优先级定义
 * ================================================================ */
#define CAN_PRIO_EMERGENCY      0U      /* 紧急（停止、全局刹车、故障上报） */
#define CAN_PRIO_REALTIME       1U      /* 实时运动控制 */
#define CAN_PRIO_QUERY_REPLY    2U      /* 查询应答 */
#define CAN_PRIO_ALERT          3U      /* 异常告警上报 */
#define CAN_PRIO_HEARTBEAT      4U      /* 心跳/同步 */
#define CAN_PRIO_CONFIG         5U      /* 参数配置/读取 */
#define CAN_PRIO_OTA            6U      /* OTA */
#define CAN_PRIO_MAX            7U      /* 优先级上限 */

/* ================================================================
 * 帧类型定义
 * ================================================================ */
#define CAN_FTYPE_NORMAL        0U      /* 普通帧 */
#define CAN_FTYPE_OTA           1U      /* OTA 数据帧 */
#define CAN_FTYPE_COMBINED      2U      /* 组合帧 */
#define CAN_FTYPE_RESERVED      3U      /* 预留 */

/* ================================================================
 * Mode ID 定义
 * ================================================================ */
   /* 紧急帧 0x000~0x01F */
#define MODE_ID_ESTOP           0x000U  /* 全局急停 */
#define MODE_ID_BRAKE           0x001U  /* 全局刹车 */
#define MODE_ID_FAULT_ALARM     0x002U  /* 故障报警 */

/* 算法帧/指令 0x020~0x07F */
#define MODE_ID_CTRL_LF         0x020U  /* 左前轮转向+轮毂控制 */
#define MODE_ID_CTRL_RF         0x021U  /* 右前轮转向+轮毂控制 */
#define MODE_ID_CTRL_REAR       0x022U  /* 后轮轮毂控制 */
#define MODE_ID_CTRL_BLADE      0x023U  /* 刀盘控制 */
#define MODE_ID_CTRL_EDGE       0x024U  /* 边线控制 */
#define MODE_ID_INTERRUPT       0x025U  /* 中断帧 */
#define MODE_ID_ALERT_ACK       0x026U  /* 告警ACK */

/* 算法数据查询 0x080~0x08F */
#define MODE_ID_QUERY_FAST      0x080U  /* 高速广播查询 */
#define MODE_ID_QUERY_MID       0x081U  /* 中速查询 */
#define MODE_ID_QUERY_SLOW      0x082U  /* 慢速查询 */
#define MODE_ID_QUERY_LOG       0x083U  /* 日志查询 */

/* 主控板通信帧 0x100~0x10F */
#define MODE_ID_ALERT           0x101U  /* 异常告警 */
#define MODE_ID_FAST_DATA       0x102U  /* 高速应答数据帧 */
#define MODE_ID_MID_DATA        0x103U  /* 中速应答数据帧 */
#define MODE_ID_SLOW_DATA       0x104U  /* 慢速应答数据帧 */
#define MODE_ID_LOG_DATA        0x105U  /* 日志应答数据帧 */

/* 主控板状态上报 0x110~0x1FF */
#define MODE_ID_STATUS_MOTOR    0x110U  /* 电机状态 */
#define MODE_ID_STATUS_MAIN     0x111U  /* 主板状态 */

/* OTA 指令 0x300~0x31F */
#define MODE_ID_OTA_START       0x300U
#define MODE_ID_OTA_END         0x301U
#define MODE_ID_OTA_ACK         0x302U
#define MODE_ID_OTA_ENTER       0x303U
#define MODE_ID_OTA_EXIT        0x304U
#define MODE_ID_OTA_REBOOT      0x305U

/* 心跳帧 0x320~0x33F */
#define MODE_ID_HEARTBEAT       0x320U  /* 心跳帧 */

/* ================================================================
 * 预置 CAN ID
 * ================================================================ */
#define CAN_TX_ID         CAN_ID_BUILD(CAN_PRIO_ALERT,     CAN_SELF_ADDR, CAN_ADDR_BROADCAST, CAN_FTYPE_NORMAL, 0x001, 0x01)
#define CAN_HEARTBEAT_ID  CAN_ID_BUILD(CAN_PRIO_HEARTBEAT, CAN_SELF_ADDR, CAN_ADDR_BROADCAST, CAN_FTYPE_NORMAL, 0x000, 0x01)

/* ================================================================
 * CanProtocolId — 协议 ID 结构化编解码
 * ================================================================ */

/* 解码结构体 */
typedef struct {
    uint8_t  priority;      /* [28:26] 优先级 */
    uint8_t  src_addr;      /* [25:22] 源地址 */
    uint8_t  dst_addr;      /* [21:18] 目标地址 */
    uint8_t  frame_type;    /* [17:16] 帧类型 */
    uint16_t mode_id;       /* [15: 6] Mode ID */
    uint8_t  func_field;    /* [ 5: 0] 功能字段 */
} CanProtocolId;

/* 编码：结构体 → 29 位 ID */
static inline uint32_t CanProto_EncodeId(const CanProtocolId *id)
{
    return (((uint32_t)(id->priority   & 0x07U) << 26U) |
            ((uint32_t)(id->src_addr   & 0x0FU) << 22U) |
            ((uint32_t)(id->dst_addr   & 0x0FU) << 18U) |
            ((uint32_t)(id->frame_type & 0x03U) << 16U) |
            ((uint32_t)(id->mode_id    & 0x3FFU) << 6U) |
            ((uint32_t)(id->func_field & 0x3FU)));
}

/* 解码：29 位 ID → 结构体 */
static inline void CanProto_DecodeId(uint32_t ext_id, CanProtocolId *id)
{
    id->priority   = (uint8_t)((ext_id >> 26U) & 0x07U);
    id->src_addr   = (uint8_t)((ext_id >> 22U) & 0x0FU);
    id->dst_addr   = (uint8_t)((ext_id >> 18U) & 0x0FU);
    id->frame_type = (uint8_t)((ext_id >> 16U) & 0x03U);
    id->mode_id    = (uint16_t)((ext_id >> 6U) & 0x3FFU);
    id->func_field = (uint8_t)(ext_id & 0x3FU);
}

/* 快捷构造：普通帧 ID */
static inline uint32_t CanProto_MakeId(uint8_t prio, uint8_t src, uint8_t dst,
                                        uint16_t mode_id)
{
    CanProtocolId id;
    id.priority   = prio;
    id.src_addr   = src;
    id.dst_addr   = dst;
    id.frame_type = CAN_FTYPE_NORMAL;
    id.mode_id    = mode_id;
    id.func_field = 0;
    return CanProto_EncodeId(&id);
}

/* ================================================================
 * 电机控制帧数据载荷结构（mode_id=0x020，显示域→动力域）
 * ⚠ 注意：此结构体仅供参考，与显示域实际线序【不一致】，收发请按裸字节处理！
 *   显示域 task_comm_can_protocol.c 实际发送线序(小端)：
 *     data[0..1] = speed_enc(rpm×10)  data[2..3] = angle_enc(°×10)
 *     data[4..6] = 0                  data[7]    = 0x03(魔数)
 *   即 byte2-3 在线上是 angle，不是本结构体里的 motor_current，且【无电流字段】。
 * ================================================================ */
typedef struct {
    int16_t   motor_speed;       /* 电机转速 */
    int16_t   motor_current;     /* 电机电流 */
    uint16_t  reserved[2];
} CanCtrlMotor;

/* ================================================================
 * 心跳帧数据载荷结构 (mode_id=0x320)
 * [status, uptime_L, uptime_H, err_L, err_H, 0, 0, 0]
 * ================================================================ */
typedef struct {
    uint8_t   status;
    uint16_t  uptime;            /* 运行时间(秒) */
    uint16_t  error_code;
    uint8_t   reserved[3];
} __attribute__((packed)) CanHeartbeatData;

/* ================================================================
 * 电机状态帧数据载荷结构（mode_id=0x110，动力域→显示域，20ms）
 * 线序(小端, 与控制帧速度/角度同样用 ×10 编码)：
 *   [speedL H(rpm×10), currentL H(mA), angleL H(°×10), status, temp]
 * 注：显示域目前无此帧的解析实现，本载荷由动力域定义；两端 CAN_Protocol.h 须保持一致。
 * ================================================================ */
typedef struct {
    int16_t   motor_speed;       /* [0-1] 实测转速 ×10 rpm */
    int16_t   motor_current;     /* [2-3] 实测电流 mA */
    int16_t   encoder_angle;     /* [4-5] 编码器角度 ×10 ° */
    uint8_t   status;            /* [6] 运行状态位 */
    uint8_t   temperature;       /* [7] 温度 ℃ */
} __attribute__((packed)) CanStatusMotor;

#ifdef __cplusplus
}
#endif

#endif /* CAN_PROTOCOL_H__ */
