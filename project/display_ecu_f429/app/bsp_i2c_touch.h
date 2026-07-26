#ifndef __BSP_I2C_TOUCH_H
#define __BSP_I2C_TOUCH_H

#include "stm32f4xx.h"

/* ============================================================
 * 引脚定义
 * ============================================================ */
/* I2C1 引脚 (PB6=SCL, PB7=SDA) */
#define TOUCH_I2C              I2C1
#define TOUCH_I2C_SCL_PIN      GPIO_Pin_6
#define TOUCH_I2C_SDA_PIN      GPIO_Pin_7
#define TOUCH_I2C_PORT         GPIOB
#define TOUCH_I2C_GPIO_CLK     RCC_AHB1Periph_GPIOB
#define TOUCH_I2C_CLK          RCC_APB1Periph_I2C1
#define TOUCH_I2C_AF           GPIO_AF_I2C1

/* CTP_INT 引脚 (PB8) */
#define CTP_INT_PIN            GPIO_Pin_8
#define CTP_INT_PORT           GPIOB

/* CTP_RST 引脚 (PB9) */
#define CTP_RST_PIN            GPIO_Pin_9
#define CTP_RST_PORT           GPIOB

/* I2C 地址 */
#define FT_CMD_WR              0x70
#define FT_CMD_RD              0x71

/* 最大触摸点数 */
#define CTP_MAX_TOUCH           2

/* ============================================================
 * FT6336 寄存器定义
 * ============================================================ */
#define FT_DEVIDE_MODE          0x00
#define FT_REG_NUM_FINGER       0x02

#define FT_TP1_REG              0x03
#define FT_TP2_REG              0x09

#define FT_ID_G_CIPHER_MID      0x9F
#define FT_ID_G_CIPHER_LOW      0xA0
#define FT_ID_G_LIB_VERSION     0xA1
#define FT_ID_G_CIPHER_HIGH     0xA3
#define FT_ID_G_MODE            0xA4
#define FT_ID_G_FOCALTECH_ID    0xA8
#define FT_ID_G_THGROUP         0x80
#define FT_ID_G_PERIODACTIVE    0x88

/* ============================================================
 * 触摸状态标志
 * ============================================================ */
#define TP_PRES_DOWN            0x80
#define TP_CATH_PRES            0x40

/* ============================================================
 * 触摸设备结构体
 * ============================================================ */
typedef struct
{
    uint8_t (*init)(void);
    uint8_t (*scan)(void);
    uint16_t x[CTP_MAX_TOUCH];
    uint16_t y[CTP_MAX_TOUCH];
    uint8_t  sta;
} _m_tp_dev;

extern _m_tp_dev tp_dev;

/* ============================================================
 * API 声明
 * ============================================================ */
void BSP_I2C_Touch_Init(void);
uint8_t FT6336_Init(void);
uint8_t FT6336_Scan(void);
uint8_t FT6336_WR_Reg(uint16_t reg, uint8_t *buf, uint8_t len);
void FT6336_RD_Reg(uint16_t reg, uint8_t *buf, uint8_t len);

#endif /* __BSP_I2C_TOUCH_H */
