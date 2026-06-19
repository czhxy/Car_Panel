/**
  ******************************************************************************
  * @file    boot_main.c
  * @brief   Bootloader 入口 — YMODEM OTA + 双分区 A/B 回滚
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "boot_config.h"
#include "flash_control.h"
#include "ota_params.h"
#include "ymodem.h"
#include "usart.h"
#include <stdio.h>

// ===== 全局 OTA 参数 =====
static ota_param_t g_ota_param;

static void delay_ms(uint32_t ms);  // 前置声明

// ===== 物理按键 (PA0, 按下为低电平) =====
#define BTN_GPIO_PORT   GPIOA
#define BTN_GPIO_PIN    GPIO_Pin_0
#define BTN_PRESSED     0   // 按下时引脚为低

static void btn_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef g;
    GPIO_StructInit(&g);
    g.GPIO_Mode  = GPIO_Mode_IN;
    g.GPIO_Pin   = BTN_GPIO_PIN;
    g.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(BTN_GPIO_PORT, &g);
}

static int btn_is_pressed(void)
{
    return (GPIO_ReadInputDataBit(BTN_GPIO_PORT, BTN_GPIO_PIN)
            == BTN_PRESSED);
}

// 等待按键按下，timeout_ms 超时返回 0，按下返回 1
static int btn_wait_press(uint32_t timeout_ms)
{
    while (timeout_ms--) {
        if (btn_is_pressed()) return 1;
        delay_ms(1);
    }
    return 0;
}

// ===== SysTick 轮询延时（Bootloader 中不使用中断） =====
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

// ===== 跳转到 App =====
typedef void (*app_entry_t)(void);

static void jump_to_app(uint32_t app_addr)
{
    uint32_t sp = *((volatile uint32_t *)app_addr);
    uint32_t pc = *((volatile uint32_t *)(app_addr + 4));

    printf("[BOOT] Jumping to App at 0x%08X...\r\n", (unsigned int)app_addr);
    printf("[BOOT]   SP = 0x%08X, PC = 0x%08X\r\n", (unsigned int)sp, (unsigned int)pc);

    if ((sp & 0x2FFE0000) != 0x20000000) {
        printf("[BOOT] ERROR: Invalid stack pointer! Abort jump.\r\n");
        return;
    }

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->VTOR = app_addr & 0xFFFFFF00;
    __set_MSP(sp);

    app_entry_t entry = (app_entry_t)pc;
    entry();
}

// ===== 检查 App 分区是否有效 =====
static int partition_is_valid(uint32_t addr)
{
    uint32_t sp = *((volatile uint32_t *)addr);
    return ((sp & 0x2FFE0000) == 0x20000000);
}

// ===== 获取分区地址 =====
static uint32_t get_active_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_A_ADDR : APP_B_ADDR;
}

static uint32_t get_inactive_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_B_ADDR : APP_A_ADDR;
}

static uint32_t get_inactive_size(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_B_SIZE : APP_A_SIZE;
}

// ===== 切换活跃分区 =====
static void swap_active_partition(void)
{
    g_ota_param.active_partition =
        (g_ota_param.active_partition == APP_A_ACTIVE)
        ? APP_B_ACTIVE : APP_A_ACTIVE;
}

// ===== 回滚到旧分区 =====
// 返回 0 成功，-1 失败（旧分区也无效）
static int perform_rollback(void)
{
    printf("[BOOT] ROLLBACK triggered!\r\n");

    swap_active_partition();
    uint32_t old_addr = get_active_addr();

    printf("[BOOT] Rolling back to 0x%08X\r\n", (unsigned int)old_addr);

    if (!partition_is_valid(old_addr)) {
        printf("[BOOT] FATAL: Old partition also invalid!\r\n");
        printf("[BOOT] Entering safe mode (wait for upgrade)...\r\n");
        return -1;
    }

    // 清除升级状态
    g_ota_param.ota_state = OTA_STATE_IDLE;
    g_ota_param.boot_count = 0;
    ota_params_save(&g_ota_param);

    printf("[BOOT] Rollback OK.\r\n");
    return 0;
}

// ===== 启动决策状态机 =====
// 返回 1 — 跳转到 App，0 — 留在 Bootloader 等待升级
static int boot_decision(void)
{
    switch (g_ota_param.ota_state) {

    case OTA_STATE_IDLE:
        // 正常启动
        g_ota_param.boot_count = 0;
        ota_params_save(&g_ota_param);

        if (partition_is_valid(get_active_addr())) {
            return 1;  // 跳转
        } else {
            printf("[BOOT] No valid app in active partition.\r\n");
            return 0;  // 留在 Bootloader
        }

    case OTA_STATE_COMPLETE:
        // 新固件刚写入，验证并切换
        {
            uint32_t new_addr = get_inactive_addr();

            g_ota_param.boot_count++;
            ota_params_save(&g_ota_param);

            printf("[BOOT] Boot attempt %u/%u\r\n",
                   g_ota_param.boot_count,
                   g_ota_param.max_boot_count);

            // 超过最大尝试次数 → 回滚
            if (g_ota_param.boot_count > g_ota_param.max_boot_count) {
                printf("[BOOT] Max boot attempts exceeded.\r\n");
                if (perform_rollback() == 0) {
                    return 1;
                }
                return 0;
            }

            // 校验新固件 CRC32
            uint32_t new_size = (new_addr == APP_A_ADDR)
                                ? g_ota_param.app_a_size
                                : g_ota_param.app_b_size;

            if (new_size == 0 || new_size == 0xFFFFFFFF) {
                printf("[BOOT] Invalid new firmware size (0x%08X).\r\n",
                       (unsigned int)new_size);
                perform_rollback();
                return 1;
            }

            uint32_t calc_crc = crc32_flash(new_addr, new_size);
            uint32_t saved_crc = (new_addr == APP_A_ADDR)
                                 ? g_ota_param.app_a_crc32
                                 : g_ota_param.app_b_crc32;

            printf("[BOOT] CRC32 verify: saved=0x%08X calc=0x%08X\r\n",
                   (unsigned int)saved_crc, (unsigned int)calc_crc);

            if (calc_crc == saved_crc) {
                // 校验通过，确认升级
                printf("[BOOT] New firmware verified, switching partition.\r\n");
                swap_active_partition();
                g_ota_param.ota_state = OTA_STATE_IDLE;
                g_ota_param.boot_count = 0;
                ota_params_save(&g_ota_param);
                return 1;
            } else {
                printf("[BOOT] CRC32 mismatch, rolling back.\r\n");
                perform_rollback();
                return 1;
            }
        }

    case OTA_STATE_FAILED:
        // 上次升级失败，回滚
        printf("[BOOT] Previous upgrade failed, rolling back.\r\n");
        perform_rollback();
        return 1;

    default:
        printf("[BOOT] Unknown OTA state %u, entering upgrade mode.\r\n",
               g_ota_param.ota_state);
        return 0;
    }
}

// ===== OTA 升级入口 =====
static void ota_ymodem_start(void)
{
    printf("[BOOT] Starting YMODEM OTA...\r\n");

    // 确定写入目标（非活跃分区）
    uint32_t target_addr = get_inactive_addr();
    uint32_t target_size = get_inactive_size();

    printf("[BOOT] Target: 0x%08X (%uKB)\r\n",
           (unsigned int)target_addr,
           (unsigned int)(target_size / 1024));

    ymodem_status_t status;
    int ret = ymodem_receive(target_addr, target_size, &status);

    if (ret == YMODEM_OK) {
        printf("[BOOT] YMODEM transfer OK, verifying...\r\n");

        // CRC32 校验
        uint32_t crc = crc32_flash(target_addr, status.total_received);
        printf("[BOOT] CRC32: 0x%08X\r\n", (unsigned int)crc);

        // 更新 OTA 参数
        g_ota_param.ota_state = OTA_STATE_COMPLETE;
        if (target_addr == APP_B_ADDR) {
            g_ota_param.app_b_version = 0x00010001;
            g_ota_param.app_b_size = status.total_received;
            g_ota_param.app_b_crc32 = crc;
        } else {
            g_ota_param.app_a_version = 0x00010001;
            g_ota_param.app_a_size = status.total_received;
            g_ota_param.app_a_crc32 = crc;
        }
        ota_params_save(&g_ota_param);

        printf("[BOOT] OTA params updated. Rebooting...\r\n");
        delay_ms(500);
        NVIC_SystemReset();
    } else {
        printf("[BOOT] YMODEM failed (code: %d).\r\n", ret);
    }
}

// ===== 串口接收辅助（基于 delay_ms 精确定时） =====
static int uart_getc_timeout(uint8_t *c, uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < timeout_ms; i++) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)) {
            *c = USART_ReceiveData(USART1);
            return 1;
        }
        delay_ms(1);
    }
    return 0;
}

// =====================================================================
//  main - Bootloader 入口
// =====================================================================
int main(void)
{
    UART_Init();
    btn_init();

    printf("\r\n================================\r\n");
    printf("Bootloader v1.0 (STM32F411)\r\n");
    printf("Flash: %dKB (0x%08X - 0x%08X)\r\n",
           (int)(FLASH_TOTAL_SIZE / 1024),
           (unsigned int)FLASH_BASE_ADDR,
           (unsigned int)(FLASH_BASE_ADDR + FLASH_TOTAL_SIZE - 1));
    printf("================================\r\n\r\n");

    // ===== 加载 / 初始化 OTA 参数 =====
    ota_params_load(&g_ota_param);

    if (g_ota_param.magic != OTA_MAGIC) {
        printf("[BOOT] OTA param magic mismatch (0x%08X), initializing...\r\n",
               (unsigned int)g_ota_param.magic);
        if (ota_params_init() != 0) {
            printf("[BOOT] OTA param init FAILED!\r\n");
            while (1);
        }
        ota_params_load(&g_ota_param);
        printf("[BOOT] OTA param initialized.\r\n");
    } else {
        printf("[BOOT] OTA param loaded OK.\r\n");
    }

    // 打印参数信息
    printf("[BOOT] Active partition: %s\r\n",
           g_ota_param.active_partition == APP_A_ACTIVE ? "App A" : "App B");
    printf("[BOOT] OTA state: %u\r\n", g_ota_param.ota_state);
    printf("[BOOT] App A ver: 0x%08X, size: %u\r\n",
           (unsigned int)g_ota_param.app_a_version,
           (unsigned int)g_ota_param.app_a_size);
    printf("[BOOT] App B ver: 0x%08X, size: %u\r\n",
           (unsigned int)g_ota_param.app_b_version,
           (unsigned int)g_ota_param.app_b_size);

    // ===== 阶段七：启动决策状态机 =====
    int should_jump = boot_decision();

    if (should_jump) {
        uint32_t addr = get_active_addr();
        if (partition_is_valid(addr)) {
            printf("[BOOT] App found at 0x%08X (%s).\r\n",
                   (unsigned int)addr,
                   (g_ota_param.active_partition == APP_A_ACTIVE)
                       ? "App A" : "App B");
            printf("[BOOT] OTA upgrade: press PA0 button within 2s...\r\n");

            // 等待 2 秒, 若按 PA0 键则进入 OTA 升级模式
            if (btn_wait_press(2000)) {
                printf("[BOOT] BTN pressed, entering OTA mode.\r\n");
            } else {
                // 超时无按键, 正常跳转 App
                printf("[BOOT] Timeout, jumping to App...\r\n");
                jump_to_app(addr);
            }
        }
    }

    // ===== 未能跳转或用户选择升级, 直接启动 OTA =====
    printf("[BOOT] Entering upgrade mode, starting YMODEM...\r\n");
    ota_ymodem_start();
    // ota_ymodem_start 成功后调用 NVIC_SystemReset, 不会返回
    // 如果失败则回到此循环
    while (1) {
        printf("[BOOT] OTA failed, retry in 3s...\r\n");
        delay_ms(3000);
        ota_ymodem_start();
    }
}
