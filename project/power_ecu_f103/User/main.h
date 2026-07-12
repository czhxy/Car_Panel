#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "task_comm_can.h"
#include "task_motor_ctl.h"
#include "task_uart.h"
#include "sysclock.h"
extern TaskPeriodFlag_t * tpf;
