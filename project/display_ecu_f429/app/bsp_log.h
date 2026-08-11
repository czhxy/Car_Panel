/**
  ******************************************************************************
  * @file    bsp_log.h
  * @brief   统一日志宏 — 通过 UART_Log 输出到串口
  *          默认（弱符号）实现 = printf；App 由 mod_comm_uart 强符号覆盖为
  *          「入 UART 日志队列 → Task_UartTx 统一发送」，ISR/任务上下文均可调用。
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
#include "usart.h"   /* UART_Log：App 由 mod_comm_uart 强符号覆盖为队列发送 */

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
#define LOG_A(fmt, ...) UART_Log("[ASSERT] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_A(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_ERROR
#define LOG_E(fmt, ...) UART_Log("[ERROR] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_E(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_WARNING
#define LOG_W(fmt, ...) UART_Log("[WARN]  " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_W(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_INFO
#define LOG_I(fmt, ...) UART_Log("[INFO]  " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) ((void)0)
#endif

#if LOG_LVL >= LOG_LVL_DBG
#define LOG_D(fmt, ...) UART_Log("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) ((void)0)
#endif

#endif /* __BSP_LOG_H */
