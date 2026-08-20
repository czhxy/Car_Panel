#ifndef __BOOT_WDG_H
#define __BOOT_WDG_H

#include "stm32f4xx.h"

/* ===== IWDG 独立看门狗（寄存器直操作，不依赖 SPL iwdg 驱动文件） =====
 * 用途：真 AB 启动回滚闭环的"运行期存活检测"。
 *   bootloader 跳转 App 前调用 wdg_start() 启动；App 必须在超时窗口内
 *   开始周期喂狗（wdg_feed）。若 App 在窗口内崩溃/卡死未喂狗，IWDG 复位
 *   回 bootloader → boot_decision 的 boot_count 递增 → 达到 max 即回滚。
 *
 * 超时计算：LSI ≈ 32kHz；PR=6 → /256 = 125Hz；(RLR+1) / 125Hz ≈ 16.4s，
 * 覆盖 App 完整启动序列（UART/FreeRTOS/LCD/LVGL/任务创建，实测 < 2s）。
 *
 * 注意：
 *  - IWDG 一旦启动无法停止（除非复位），App 接管后必须持续喂狗；
 *  - App 被 bootloader 跳转时 IWDG 已启动；若直接烧录运行（不经 boot），
 *    未启动的 IWDG 喂狗写 KR=0xAAAA 无副作用，故 App 可无条件喂狗。
 */
#define WDG_PRESCALER_VAL       6U          /* IWDG PR 寄存器: 6 = /256 (LSI/256) */
#define WDG_RELOAD_VAL          2047U       /* (2047+1) / 125Hz ≈ 16.4s */

/* 启动 IWDG（bootloader 跳转 App 前调用） */
static inline void wdg_start(void)
{
    IWDG->KR = 0x5555;          /* 解锁写访问 */
    IWDG->PR = WDG_PRESCALER_VAL;
    IWDG->RLR = WDG_RELOAD_VAL;
    IWDG->KR = 0xAAAA;          /* 重载计数器（先于启动加载初值） */
    IWDG->KR = 0xCCCC;          /* 启动 */
}

/* 喂狗（App 周期调用） */
static inline void wdg_feed(void)
{
    IWDG->KR = 0xAAAA;          /* 重载，清零递减计数器 */
}

#endif /* __BOOT_WDG_H */
