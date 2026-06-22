/**
  ******************************************************************************
  * @file    flash_control.c
  * @brief   Flash 操作层 — 扇区擦除 + Word 写入 (STM32F429IGTx)
  *
  *          STM32F429IGTx Flash 1MB (Bank1 only), 12 扇区:
  *            Sector 0-3: 16KB   (0x08000000 - 0x0800FFFF)
  *            Sector 4:   64KB   (0x08010000 - 0x0801FFFF)
  *            Sector 5-11:128KB  (0x08020000 - 0x080FFFFF)
  *
  *          Flash 操作使用 VoltageRange_3 (x32, 2.7V-3.6V)
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

// ===== 根据地址获取扇区编号 (STM32F429IGTx, 1MB, 12扇区) =====
uint32_t flash_if_get_sector(uint32_t addr)
{
    if (addr < 0x08004000)  return FLASH_Sector_0;
    if (addr < 0x08008000)  return FLASH_Sector_1;
    if (addr < 0x0800C000)  return FLASH_Sector_2;
    if (addr < 0x08010000)  return FLASH_Sector_3;
    if (addr < 0x08020000)  return FLASH_Sector_4;
    if (addr < 0x08040000)  return FLASH_Sector_5;
    if (addr < 0x08060000)  return FLASH_Sector_6;
    if (addr < 0x08080000)  return FLASH_Sector_7;
    if (addr < 0x080A0000)  return FLASH_Sector_8;
    if (addr < 0x080C0000)  return FLASH_Sector_9;
    if (addr < 0x080E0000)  return FLASH_Sector_10;
    /* addr < 0x08100000 */  return FLASH_Sector_11;
}

// ===== 擦除多个扇区 =====
int flash_if_erase(uint32_t start_addr, uint32_t size)
{
    uint32_t end_addr = start_addr + size;
    uint32_t cur_addr = start_addr;

    while (cur_addr < end_addr) {
        uint32_t sector = flash_if_get_sector(cur_addr);

        // F42x/43x @ 3.3V VDD 使用 VoltageRange_3 (2.7V-3.6V)
        // PSIZE_WORD = x32 并行度，VoltageRange_4 要求 External Vpp（F4 无此引脚）
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
