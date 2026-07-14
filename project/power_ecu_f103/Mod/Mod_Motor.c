#include "Mod_Motor.h"

/* 全局电机状态实例：零初始化，目标值由 CAN RX 写入，实测值由 drv_motor 填充 */
Motor_Struct motor = {0};

void Mod_Motor_Init(void)
{
	drv_motor_init();
}
