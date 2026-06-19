/**
  ******************************************************************************
  * @file    boot_decision.h
  * @brief   启动决策状态机 — 分区校验/回滚/切换
  ******************************************************************************
  */

#ifndef __BOOT_DECISION_H
#define __BOOT_DECISION_H

#include <stdint.h>

int      boot_decision(void);
int      perform_rollback(void);
void     swap_active_partition(void);
uint32_t get_active_addr(void);
uint32_t get_inactive_addr(void);
uint32_t get_inactive_size(void);

#endif /* __BOOT_DECISION_H */
