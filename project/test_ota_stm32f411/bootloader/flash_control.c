/**
  ******************************************************************************
  * @file    flash_control.c
  * @brief   Flash 操作层 — 扇区擦除 + Word 写入
  *
  * @note    STM32F411 只有一个 Flash Bank，擦写期间不能访问被操作的扇区。
  *          Bootloader 代码在 Sector 0-2，因此擦除 Sector 3-7 是安全的。
  *          VoltageRange_3 对应 Scale1 模式（2.7-3.6V），与 SystemInit() 匹配。
  ******************************************************************************
  */

#include "flash_control.h"
#include "stm32f4xx_flash.h"

// ===== 初始化（解锁 Flash） =====
int flash_if_init(void)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                    FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    return 0;
}

// ===== 锁定 Flash =====
void flash_if_lock(void)
{
    FLASH_Lock();
}

// ===== 根据地址获取扇区编号 =====
// STM32F4 标准外设库中 FLASH_Sector_n 的值是 n*8：
//   Sector 0  -> FLASH_Sector_0  (0x00): 0x0800 0000 - 0x0800 3FFF (16KB)
//   Sector 1  -> FLASH_Sector_1  (0x08): 0x0800 4000 - 0x0800 7FFF (16KB)
//   Sector 2  -> FLASH_Sector_2  (0x10): 0x0800 8000 - 0x0800 BFFF (16KB)
//   Sector 3  -> FLASH_Sector_3  (0x18): 0x0800 C000 - 0x0800 FFFF (16KB)
//   Sector 4  -> FLASH_Sector_4  (0x20): 0x0801 0000 - 0x0801 FFFF (64KB)
//   Sector 5  -> FLASH_Sector_5  (0x28): 0x0802 0000 - 0x0803 FFFF (128KB)
//   Sector 6  -> FLASH_Sector_6  (0x30): 0x0804 0000 - 0x0805 FFFF (128KB)
//   Sector 7  -> FLASH_Sector_7  (0x38): 0x0806 0000 - 0x0807 FFFF (128KB)
uint32_t flash_if_get_sector(uint32_t addr)
{
    if (addr < 0x08004000)  return FLASH_Sector_0;
    if (addr < 0x08008000)  return FLASH_Sector_1;
    if (addr < 0x0800C000)  return FLASH_Sector_2;
    if (addr < 0x08010000)  return FLASH_Sector_3;
    if (addr < 0x08020000)  return FLASH_Sector_4;
    if (addr < 0x08040000)  return FLASH_Sector_5;
    if (addr < 0x08060000)  return FLASH_Sector_6;
    /* addr < 0x08080000 */  return FLASH_Sector_7;
}

// ===== 擦除多个扇区 =====
// 从 start_addr 开始，按扇区逐个擦除，直到覆盖 size 范围
// 返回 0 成功，-1 失败
int flash_if_erase(uint32_t start_addr, uint32_t size)
{
    uint32_t end_addr = start_addr + size;
    uint32_t cur_addr = start_addr;

    while (cur_addr < end_addr) {
        uint32_t sector = flash_if_get_sector(cur_addr);

        if (FLASH_EraseSector(sector, VoltageRange_3) != FLASH_COMPLETE) {
            return -1;
        }

        // 根据扇区大小推进地址
        if (sector == FLASH_Sector_0 || sector == FLASH_Sector_1 ||
            sector == FLASH_Sector_2 || sector == FLASH_Sector_3) {
            cur_addr += 0x4000;   // 16KB
        } else if (sector == FLASH_Sector_4) {
            cur_addr += 0x10000;  // 64KB
        } else {
            cur_addr += 0x20000;  // 128KB
        }
    }

    return 0;
}

// ===== 写入一个 Word（4字节） =====
int flash_if_write_word(uint32_t addr, uint32_t data)
{
    if (FLASH_ProgramWord(addr, data) != FLASH_COMPLETE) {
        return -1;
    }
    // 校验写入
    if (*(volatile uint32_t *)addr != data) {
        return -1;
    }
    return 0;
}

// ===== 批量写入 =====
// 自动处理 4 字节对齐：非对齐部分用 FLASH_ProgramByte，
// 对齐部分用 FLASH_ProgramWord
int flash_if_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t remaining;
    uint32_t i;

    // 1. 写入开头的非对齐字节，直到 4 字节对齐
    while (len > 0 && (addr & 0x03)) {
        if (FLASH_ProgramByte(addr, *data) != FLASH_COMPLETE) {
            return -1;
        }
        addr++;
        data++;
        len--;
    }

    // 2. 按 Word 批量写入
    uint32_t word_count = len / 4;
    for (i = 0; i < word_count; i++) {
        uint32_t word = ((uint32_t)data[0] <<  0) |
                        ((uint32_t)data[1] <<  8) |
                        ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 24);
        if (flash_if_write_word(addr + i * 4, word) != 0) {
            return -1;
        }
        data += 4;
    }

    // 3. 写入末尾剩余字节（不足 4 字节）
    remaining = len % 4;
    addr += word_count * 4;
    for (i = 0; i < remaining; i++) {
        if (FLASH_ProgramByte(addr + i, data[i]) != FLASH_COMPLETE) {
            return -1;
        }
    }

    return 0;
}
