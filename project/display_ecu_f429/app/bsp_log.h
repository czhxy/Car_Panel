/**
  ******************************************************************************
  * @file    bsp_log.h
  * @brief   统一日志宏 — 通过 printf 输出到串口
  *
  *          使用示例:
  *            LOG_I("System init OK");
  *            LOG_E("Error code: %d", err);
  *            LOG_D("Debug value = 0x%08X", val);
  ******************************************************************************
  */

#ifndef __BSP_LOG_H
#define __BSP_LOG_H

#include <stdio.h>

#define LOG_E(fmt, ...)  printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...)  printf("[WARN]  " fmt "\r\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...)  printf("[INFO]  " fmt "\r\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...)  printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)

#endif /* __BSP_LOG_H */
