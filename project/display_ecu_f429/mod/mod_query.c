/**
  ******************************************************************************
  * @file    mod_query.c
  * @brief   查询协议业务 — 通过弱符号回调接入 UART 通信框架（mod_comm_uart）
  *
  * 原 UART_Query_Task（轮询解析 + 透传 CAN 脚手架）已移除，查询链路拆分为
  * Task_UartTx / Task_UartRx 两个任务。本模块只负责协议业务：
  *   - type=0x01 chip info → 组织应答并经 Mod_Uart_SendPacket() 回发
  *   - 其他 type → 原样回发（环路验证 echo）
  *
  * M3/M4: 地址/分区不再本地重定义、也不再读 OTA 参数区；运行槽位由 SCB->VTOR
  *        自证（App A 链接于 0x08020000、App B 链接于 0x08080000，VTOR 即运行基址）。
  ******************************************************************************
  */
#include "mod_query.h"
#include "main.h"
#include "boot_config.h"   /* 地址常量单一来源（FLASH_BASE_ADDR / APP_A_ADDR / APP_B_ADDR） */
#include "bsp_log.h"
#include "mod_comm_uart.h"
#include <string.h>

#define CMD_CHIP_INFO   0x01
#define PKT_DATA_LEN    13

// ======================== Command 0x01: Chip info query ========================
static void handle_chip_info_query(void)
{
    uint32_t boot_addr  = FLASH_BASE_ADDR;
    uint32_t active_app = SCB->VTOR;                       /* 当前运行镜像基址 */
    uint8_t  partition  = (active_app == APP_A_ADDR) ? 1 : 2;  /* M4: A=1 / B=2，自证 */

    uint8_t data[PKT_DATA_LEN];
    data[0] = 0xF4;
    data[1] = partition;
    data[2] = (uint8_t)(boot_addr);
    data[3] = (uint8_t)(boot_addr >> 8);
    data[4] = (uint8_t)(boot_addr >> 16);
    data[5] = (uint8_t)(boot_addr >> 24);
    data[6] = (uint8_t)(active_app);
    data[7] = (uint8_t)(active_app >> 8);
    data[8] = (uint8_t)(active_app >> 16);
    data[9] = (uint8_t)(active_app >> 24);
    data[10] = (uint8_t)((int)APP_VERSION);
    data[11] = (uint8_t)((int)((APP_VERSION - (int)APP_VERSION) * 10 + 0.5));
    data[12] = 0;

    /* 组包 + 入 TX 队列由通信框架完成，本模块不直接写串口 */
    Mod_Uart_SendPacket(CMD_CHIP_INFO, data, PKT_DATA_LEN);
}

// ======================== 初始化（确保本模块被链接） ========================
void Mod_Query_Init(void)
{
    LOG_I("[QUERY] protocol handler ready\r\n");
}

// ======================== 强符号覆盖弱符号回调 ========================
// mod_comm_uart.c 中 ModCommUart_OnRxPacket 为弱符号，此处强符号自动覆盖，
// 与 ModCommCan_OnRxFrame 的覆盖方式一致。
void ModCommUart_OnRxPacket(uint8_t type, const uint8_t *data, uint8_t len)
{
    if (type == CMD_CHIP_INFO) {
        handle_chip_info_query();
    } else {
        /* 环路验证：其他 type 原样回发（框架重新计算 CRC） */
        Mod_Uart_SendPacket(type, data, len);
    }
}
