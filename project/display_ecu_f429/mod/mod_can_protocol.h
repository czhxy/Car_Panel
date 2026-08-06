#ifndef __MOD_CAN_PROTOCOL_H
#define __MOD_CAN_PROTOCOL_H

#include <stdint.h>

void CanProto_SendFrame(uint8_t prio, uint8_t dev_id, uint8_t ftype,
                        uint16_t mode_id, uint8_t func,
                        const uint8_t *data, uint8_t dlc);
void CanProtocol_WheelCtlSend(void);

#endif /* __MOD_CAN_PROTOCOL_H */
