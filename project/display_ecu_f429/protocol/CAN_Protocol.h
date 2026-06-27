#ifndef CAN_PROTOCOL_H__
#define CAN_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


/* ================================================================

 * 设备地址定义
 * ================================================================ */
#define CAN_ADDR_BROADCAST      0x0U    /* 广播 */
#define CAN_ADDR_MAINBOARD      0x1U    /* 主板 */
#define CAN_ADDR_MOTORBOARD     0x2U    /* 电机板*/


/* ================================================================

 * 优先级定义
 * ================================================================ */
#define CAN_PRIO_EMERGENCY      0U      /* 紧急：急停、全局刹车、故障报警 */
#define CAN_PRIO_REALTIME       1U      /* 实时运动控制 */
#define CAN_PRIO_QUERY_REPLY    2U      /* 查询应答 */
#define CAN_PRIO_ALERT          3U      /* 主动告警上报 */
#define CAN_PRIO_HEARTBEAT      4U      /* 心跳/在线 */
#define CAN_PRIO_CONFIG         5U      /* 参数配置/调试 */
#define CAN_PRIO_OTA            6U      /* OTA */

/* ================================================================

 * 帧类型定义
 * ================================================================ */
#define CAN_FTYPE_NORMAL        0U      /* 普通帧 */
#define CAN_FTYPE_OTA           1U      /* OTA数据帧 */
#define CAN_FTYPE_COMBO         2U      /* 组合帧 */

/* ================================================================

 * Mode ID 定义
 * ================================================================ */
   /* 紧急帧 0x000~0x01F */
#define MODE_ID_ESTOP           0x000U  /* 全局急停 */
#define MODE_ID_BRAKE           0x001U  /* 全局刹车 */
#define MODE_ID_FAULT_ALARM     0x002U  /* 故障报警 */

/* 算法板指令 0x020~0x07F */
#define MODE_ID_CTRL_LF         0x020U  /* 左前驱动轮+舵轮控制 */
#define MODE_ID_CTRL_RF         0x021U  /* 右前驱动轮+舵轮控制 */
#define MODE_ID_CTRL_REAR       0x022U  /* 后驱动轮控制 */
#define MODE_ID_CTRL_BLADE      0x023U  /* 刀盘控制 */
#define MODE_ID_CTRL_EDGE       0x024U  /* 边切控制 */
#define MODE_ID_INTERRUPT       0x025U  /* 中断帧 */
#define MODE_ID_ALERT_ACK       0x026U  /* 告警ACK */

/* 算法板查询 0x080~0x08F */
#define MODE_ID_QUERY_FAST      0x080U  /* 快包广播查询 */
#define MODE_ID_QUERY_MID       0x081U  /* 中包查询 */
#define MODE_ID_QUERY_SLOW      0x082U  /* 慢包查询 */
#define MODE_ID_QUERY_LOG       0x083U  /* 日志查询 */

/* 控制板通用帧 0x100~0x10F */
#define MODE_ID_ALERT           0x101U  /* 异常告警 */
#define MODE_ID_FAST_DATA       0x102U  /* 快包应答数据帧 */
#define MODE_ID_MID_DATA        0x103U  /* 中包应答数据帧 */
#define MODE_ID_SLOW_DATA       0x104U  /* 慢包应答数据帧 */
#define MODE_ID_LOG_DATA        0x105U  /* 日志应答数据帧 */

/* 控制板状态上报 0x110~0x1FF */
#define MODE_ID_STATUS_MOTOR    0x110U  /* 电机状态 */
#define MODE_ID_STATUS_MAIN     0x111U  /* 主板 */

/* OTA指令 0x300~0x31F */
#define MODE_ID_OTA_START       0x300U
#define MODE_ID_OTA_END         0x301U
#define MODE_ID_OTA_ACK         0x302U
#define MODE_ID_OTA_ENTER       0x303U
#define MODE_ID_OTA_EXIT        0x304U
#define MODE_ID_OTA_REBOOT      0x305U

/* 共用帧 0x320~0x33F */
#define MODE_ID_HEARTBEAT       0x320U  /* 心跳帧 */

/* ================================================================

 * 29位 CAN ID 编解码
   *
 * ID [28:26] 3位 优先级
 * ID [25:22] 4位 源地址
 * ID [21:18] 4位 目标地址
 * ID [17:16] 2位 帧类型
 * ID [15: 6] 10位 mode_id
 * ID [ 5: 0] 6位 功能字段
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

/* 编码：结构体 → 29位ID */
static inline uint32_t CanProto_EncodeId(const CanProtocolId *id)
{
    return (((uint32_t)(id->priority  & 0x07U) << 26U) |
            ((uint32_t)(id->src_addr  & 0x0FU) << 22U) |
            ((uint32_t)(id->dst_addr  & 0x0FU) << 18U) |
            ((uint32_t)(id->frame_type & 0x03U) << 16U) |
            ((uint32_t)(id->mode_id   & 0x3FFU) << 6U) |
            ((uint32_t)(id->func_field & 0x3FU)));
}

/* 解码：29位ID → 结构体 */
static inline void CanProto_DecodeId(uint32_t ext_id, CanProtocolId *id)
{
    id->priority   = (uint8_t)((ext_id >> 26U) & 0x07U);
    id->src_addr   = (uint8_t)((ext_id >> 22U) & 0x0FU);
    id->dst_addr   = (uint8_t)((ext_id >> 18U) & 0x0FU);
    id->frame_type = (uint8_t)((ext_id >> 16U) & 0x03U);
    id->mode_id    = (uint16_t)((ext_id >> 6U) & 0x3FFU);
    id->func_field = (uint8_t)(ext_id & 0x3FU);
}

/* 快捷编码：普通帧 */
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

 * 控制指令数据结构（电机，mode_id=0x020）
 * [速度L, 速度H, 加速度L, 加速度H, 角度L, 角度H, 0, 0]
 * ================================================================ */
typedef struct {
	int16_t  	motor_speed;  	/* 电机转速 */
  int16_t 	motor_current;	/* 电机电流 */
	uint16_t reserved[2];
} CanCtrlMotor;

/* ================================================================

 * 心跳帧数据 (mode_id=0x320)
 * [status, uptime_L, uptime_H, err_L, err_H, 0, 0, 0]
 * ================================================================ */
typedef struct {
	uint8_t  status;
	uint16_t uptime;        /* 运行时间(秒) */
	uint16_t error_code;
	uint8_t  reserved[3];
} __attribute__((packed)) CanHeartbeatData;

#ifdef __cplusplus
}
#endif

#endif