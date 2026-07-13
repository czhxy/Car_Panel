#ifndef __TASK_COMM_CAN_H
#define __TASK_COMM_CAN_H

#include "stm32f10x.h"                  // Device header
#include "Mod_Comm_Can.h"
void Task_Comm_Can_Init(void);
void Task_Comm_Rx_Can(void);
void Task_Comm_Tx_Can(void);
#endif
