/* mod_dashboard_fault.h — 故障码→消息映射表 (可扩展)
 *
 * 新增故障码只需在映射表末尾追加一行，无需修改 UI 逻辑。
 */

#ifndef __MOD_DASHBOARD_FAULT_H
#define __MOD_DASHBOARD_FAULT_H

#include <stdint.h>

/* ---- 故障级别 ---- */
typedef enum {
    FAULT_NONE  = 0,  /* 无故障 (保持绿色) */
    FAULT_INFO  = 1,  /* 信息 */
    FAULT_WARN  = 2,  /* 警告 (黄色) */
    FAULT_ERROR = 3,  /* 错误 (红色) */
} FaultLevel;

/* ---- 故障码条目 ---- */
typedef struct {
    uint16_t    code;
    const char *message;
    FaultLevel  level;
} FaultCodeEntry;

/* ---- API ---- */
const char* Dashboard_Fault_Lookup(uint16_t error_code, FaultLevel *out_level);

#endif /* __MOD_DASHBOARD_FAULT_H */
