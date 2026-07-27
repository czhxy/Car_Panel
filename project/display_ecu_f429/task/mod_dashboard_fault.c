/* mod_dashboard_fault.c — 故障码映射表实现 */

#include "mod_dashboard_fault.h"

/* ---- 故障码映射表 ---- */
static const FaultCodeEntry s_fault_table[] = {
    /* code,     message,                  level */
    { 0x0000, "ALL SYSTEMS NORMAL",       FAULT_NONE },
    { 0x0001, "CAN TIMEOUT",              FAULT_ERROR },
    { 0x0002, "MOTOR STALL",              FAULT_ERROR },
    { 0x0004, "ENCODER LOSS",             FAULT_ERROR },
};

#define FAULT_TABLE_SIZE (sizeof(s_fault_table) / sizeof(s_fault_table[0]))

/* ---- 故障码查询 ---- */
const char* Dashboard_Fault_Lookup(uint16_t error_code, FaultLevel *out_level)
{
    unsigned int i;
    for (i = 0; i < FAULT_TABLE_SIZE; i++) {
        if (s_fault_table[i].code == error_code) {
            if (out_level != NULL) {
                *out_level = s_fault_table[i].level;
            }
            return s_fault_table[i].message;
        }
    }

    /* 未找到：默认 UNKNOWN ERROR */
    if (out_level != NULL) {
        *out_level = FAULT_ERROR;
    }
    return "UNKNOWN ERROR";
}
