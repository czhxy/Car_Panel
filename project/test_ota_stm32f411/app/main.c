/**
  ******************************************************************************
  * @file    main.c
  * @brief   App 入口 — OTA 升级确认 + VTOR 自检
  *
  *          Bootloader 在跳转前已设置 SCB->VTOR 为正确的分区地址。
  *          SystemInit() 在 App 模式下不再覆盖 VTOR。
  *          启动后检查 OTA 参数区，确认当前运行状态。
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

// ===== OTA 参数常量（与 boot_config.h 保持一致） =====
#define OTA_PARAM_ADDR      0x0800C000U
#define OTA_MAGIC           0x4F544152
#define OTA_STATE_IDLE      0
#define OTA_STATE_COMPLETE  3
#define APP_A_ACTIVE        0
#define APP_B_ACTIVE        1

// ===== SysTick 延时 =====
static void delay_ms(uint32_t ms)
{
    SysTick->LOAD = 100000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    for (uint32_t i = 0; i < ms; i++) {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);
    }
    SysTick->CTRL = 0;
}

// ===== 检查 OTA 参数（只读） =====
static void app_check_ota_params(void)
{
    volatile uint32_t *p = (volatile uint32_t *)OTA_PARAM_ADDR;
    uint32_t magic = p[0];

    if (magic != OTA_MAGIC) {
        printf("[APP] OTA params not initialized (magic=0x%08X).\r\n",
               (unsigned int)magic);
        return;
    }

    // 读取参数（ota_param_t 的字段偏移）
    volatile uint8_t *bp = (volatile uint8_t *)p;
    uint8_t active   = bp[8];   // active_partition
    uint8_t ota_state = bp[9];  // ota_state
    uint8_t boot_cnt  = bp[10]; // boot_count
    uint32_t ver_a    = p[3];   // app_a_version
    uint32_t size_a   = p[4];   // app_a_size
    uint32_t ver_b    = p[6];   // app_b_version
    uint32_t size_b   = p[7];   // app_b_size

    printf("[APP] OTA params found:\r\n");
    printf("[APP]   Active: %s, State: %u, Boot count: %u\r\n",
           active == APP_A_ACTIVE ? "App A" : "App B",
           ota_state, boot_cnt);
    printf("[APP]   App A: ver=0x%08X, size=%u\r\n",
           (unsigned int)ver_a, (unsigned int)size_a);
    printf("[APP]   App B: ver=0x%08X, size=%u\r\n",
           (unsigned int)ver_b, (unsigned int)size_b);

    if (ota_state == OTA_STATE_COMPLETE) {
        printf("[APP] OTA upgrade confirmed! System running OK.\r\n");
        /*
         * 注意：App 不直接写 Flash（ota_params_save 需要 flash_if 函数）。
         * Bootloader 在下次启动时，boot_decision() 的 IDLE 分支会清除状态。
         * 只要 App 不崩溃，boot_count 不会增加，下次正常启动。
         */
    }
}

// =====================================================================
//  main - App 入口
// =====================================================================
int main(void)
{
    UART_Init();
    printf("\r\n================================\r\n");
    printf("App v2.0 (OTA)\r\n");
    printf("================================\r\n\r\n");

    // ===== VTOR 自检 =====
    printf("[APP] SCB->VTOR = 0x%08X\r\n", (unsigned int)SCB->VTOR);

    // 校验 VTOR 是否在有效 Flash 范围
    uint32_t vtor = SCB->VTOR;
    if (vtor < 0x08000000 || vtor > 0x08080000) {
        printf("[APP] ERROR: Invalid VTOR! Halting.\r\n");
        while (1);
    }

    // ===== 检查 OTA 参数 =====
    app_check_ota_params();

    // ===== 主循环 =====
    printf("\r\n[APP] Running...\r\n\r\n");

    uint32_t counter = 0;
    while (1) {
        printf("[APP] cz Heartbeat #%u\r\n", (unsigned int)++counter);
        delay_ms(2000);
    }
}
