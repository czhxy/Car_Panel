/**
  ******************************************************************************
  * @file    boot_query.h
  * @brief   Bootloader 芯片信息查询 — 响应 PC 上位机 0xAA 0x55 0x01 0x00
  *
  * 与 app 侧 mod_query.c 保持同一响应格式（data 13 字节 + CRC16），
  * 使 OTA 上位机在 bootloader 阶段也能查询到活跃分区等信息。
  *   PC 发送: [0xAA, 0x55, 0x01, 0x00]
  *   Boot 应答: [0xAA, 0x55, 0x01, 0x0D, data(13B), crc16(2B)]
  ******************************************************************************
  */

#ifndef __BOOT_QUERY_H
#define __BOOT_QUERY_H

#include <stdint.h>

/* 轮询式处理已到达的查询字节（非阻塞，主循环每 1ms 调用一次） */
void Boot_Query_Poll(void);

/* 等待按键窗口内并行响应查询指令；按键按下返回 1，超时返回 0 */
int  Boot_Query_WaitPress(uint32_t timeout_ms);

#endif /* __BOOT_QUERY_H */
