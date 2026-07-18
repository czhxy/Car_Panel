#include "Mod_Motor.h"

/* 全局电机状态实例：零初始化，目标值由 CAN RX 写入，实测值由 drv_motor 填充 */
Motor_Struct motor_left  = {0};
Motor_Struct motor_right = {0};

void Mod_Motor_Init(void)
{
	drv_motor_init(MOTOR_ID_LEFT);
	drv_motor_init(MOTOR_ID_RIGHT);
}

/** Mod_Motor_Update — 由任务层 5ms 周期调用，读取左右编码器并更新实测值 */
void Mod_Motor_Update(void)
{
	drv_motor_update(MOTOR_ID_LEFT);
	drv_motor_update(MOTOR_ID_RIGHT);
}
