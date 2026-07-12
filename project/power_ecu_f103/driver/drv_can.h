#ifndef __DRV_CAN_H
#define __DRV_CAN_H

#include "stm32f10x.h"                  // Device header
#include <stddef.h>

typedef void (*can_rx_callback)(CanRxMsg * msg);
typedef void (*can_tx_callback)(CanTxMsg * msg);
void drv_can_init(void);
void can_rx_cb_register(can_rx_callback cb);
#endif
