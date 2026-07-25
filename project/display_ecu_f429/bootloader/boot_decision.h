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
uint32_t get_active_addr(void);

#endif /* __BOOT_DECISION_H */
