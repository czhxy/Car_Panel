#include "mod_motor.h"

/* TODO: 待接入真实电机编码器/传感器读取，替换占位返回。
 *   Mod_Motor_Get_Speed : 转速 rpm ;  Mod_Motor_Angle : 角度 度
 *   上层 CanProtocol_WheelCtlSend 会将值 *10 后按 int16 编码进控制帧。 */
float Mod_Motor_Get_Speed(void) { return 0.0f; }
float Mod_Motor_Angle(void)     { return 0.0f; }
