#ifndef __MOD_COMM_CAN_H
#define __MOD_COMM_CAN_H

#include "queue.h"
#include "drv_can.h"
void Mod_Can_Init(void);
void Can_Tx_Event(void);
void Can_Rx_Event(void);
void Can_Tx_Process(void);
void Can_Rx_Process(void);
void Can_Rx_Cb(CanRxMsg * msg);
#endif 
