#ifndef __MOD_MOTOR_H
#define __MOD_MOTOR_H

#include "stm32f10x.h"                  // Device header
#include "drv_motor.h"
#include "pid.h"
/* 电机运行状态位（status 字段按位组合） */
#define MOTOR_STATUS_RUN     0x01u   /* 运行中 */
#define MOTOR_STATUS_ENABLE  0x02u   /* 已使能 */
#define MOTOR_STATUS_FAULT   0x80u   /* 故障 */

/* 电机数据中枢：RX(控制帧)写目标值，drv_motor 以后填实测值，TX(状态/心跳)读 */
typedef struct motor_struct_type{
	/* 来自显示域控制帧的目标值（RX 解析写入，×10 编码省去浮点） */
	int16_t  target_speed_enc;    /* 目标转速 rpm×10 */
	int16_t  target_angle_enc;    /* 目标角度 °×10 */
	/* 实测值（drv_motor/编码器以后填充，当前为 0） */
	int16_t  cur_speed_enc;  /* 实测转速 rpm×10 */
	int16_t  cur_current;    /* 实测电流 mA */
	int16_t  cur_angle_enc;  /* 编码器角度 °×10 */
	uint8_t  temperature;         /* 温度 ℃ */
	uint8_t  status;              /* 运行状态位，见 MOTOR_STATUS_* */
	uint16_t error_code;          /* 故障码 */
	uint32_t rx_ctrl_count;       /* 收到的控制帧计数（调试 + 后续超时判断） */
	uint32_t last_ctrl_ms;        /* 最近一次控制帧的时间戳 ms（供以后超时停机） */
	uint8_t  flag;                /* 通用标志位 */
}Motor_Struct;

extern Motor_Struct motor_left;   /* 左电机状态（Mod_Motor.c 定义） */
extern Motor_Struct motor_right;  /* 右电机状态（Mod_Motor.c 定义） */
#define motor motor_left          /* 向后兼容：旧代码中的 motor 即左电机 */

void Mod_Motor_Init(void);
void Mod_Motor_Update(void);       /* 周期调用 drv_motor_update，更新左右实测值 */
void Mod_Motor_Process(void);
#endif
