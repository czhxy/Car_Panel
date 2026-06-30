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

// 日志级别定义（数值越小越严重）
#define LOG_LVL_ASSERT  0
#define LOG_LVL_ERROR   1
#define LOG_LVL_WARNING 2
#define LOG_LVL_INFO    3
#define LOG_LVL_DBG     4

// 当前日志级别（可在此修改，或通过编译选项 -DLOG_LVL=... 传入）
#ifndef LOG_LVL
#define LOG_LVL LOG_LVL_DBG
#endif

// 条件编译各日志宏
#if LOG_LVL >= LOG_LVL_ASSERT
#define LOG_A(fmt, ...) printf("[ASSERT] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_A(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_ERROR
#define LOG_E(fmt, ...) printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_E(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_WARNING
#define LOG_W(fmt, ...) printf("[WARN]  " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_W(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_INFO
#define LOG_I(fmt, ...) printf("[INFO]  " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_DBG
#define LOG_D(fmt, ...) printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) ((void)0)
#endif

#endif /* __BSP_LOG_H */
