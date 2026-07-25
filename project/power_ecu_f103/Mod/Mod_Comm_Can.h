#ifndef __MOD_COMM_CAN_H
#define __MOD_COMM_CAN_H

#include "queue.h"
#include "drv_can.h"
#include <stdbool.h>
void Mod_Can_Init(void);
bool Can_Rx_Event(CanRxMsg RxMsg);
bool Can_Tx_Event(CanTxMsg TxMsg);
uint8_t Can_Tx_Process(void);
void Can_Rx_Process(void);
void Can_Rx_Cb(CanRxMsg msg);
#endif
