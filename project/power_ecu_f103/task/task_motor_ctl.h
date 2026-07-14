#ifndef __TASK_MOTOR_CTL_H
#define __TASK_MOTOR_CTL_H

#include "stm32f10x.h"                  // Device header
#include "Mod_Motor.h"
#include "Mod_Comm_Can.h"
#include "CAN_Protocol.h"
#include <string.h>
void Task_Motor_Ctl_Init(void);
void Task_Motor_Ctl(void);
void Task_Can_Motor_Updata(void);
#endif
