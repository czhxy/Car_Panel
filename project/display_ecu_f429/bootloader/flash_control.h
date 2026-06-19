#ifndef __FLASH_CONTROL_H
#define __FLASH_CONTROL_H

#include <stdint.h>

// 初始化（解锁 Flash）
int  flash_if_init(void);

// 锁定 Flash
void flash_if_lock(void);

// 擦除多个扇区 (start_addr 到 start_addr + size)
int  flash_if_erase(uint32_t start_addr, uint32_t size);

// 按 32 位写入一个字
int  flash_if_write_word(uint32_t addr, uint32_t data);

// 批量写入（自动处理字节对齐）
int  flash_if_write(uint32_t addr, const uint8_t *data, uint32_t len);

// 根据地址获取 Flash 扇区编号（FLASH_Sector_n 常量）
uint32_t flash_if_get_sector(uint32_t addr);

#endif /* __FLASH_CONTROL_H */
