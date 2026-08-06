/**
 * @file lv_port_disp.c
 * LVGL 显示驱动移植 — ILI9341 SPI 接口
 * 分辨率: 240×320 (竖屏)
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "lvgl.h"
#include "stm32f4xx.h"
#include "bsp_spi_lcd.h"

/*********************
 *      DEFINES
 *********************/
/* 屏幕尺寸: ILI9341 2.8 英寸, 竖屏 240×320 */
#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    320

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /* 硬件已在 task_entry 中通过 BSP_SPI_LCD_Init() 完成初始化 */
    disp_init();

    /* 双缓冲: 各 40 行，静态分配在主 SRAM，可被 DMA2D 访问 */
    static lv_disp_draw_buf_t draw_buf_dsc;
    static lv_color_t buf_1[MY_DISP_HOR_RES * 40];
    static lv_color_t buf_2[MY_DISP_HOR_RES * 40];
    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, MY_DISP_HOR_RES * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res  = MY_DISP_HOR_RES;
    disp_drv.ver_res  = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* 显示硬件已经在 task_entry 中初始化, 此处仅作占位 */
static void disp_init(void)
{
    /* BSP_SPI_LCD_Init() 已在 task_entry 中调用 */
}

volatile bool disp_flush_enabled = true;

void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/* ============================================================
 * disp_flush — LVGL 绘图缓冲区刷新到 ILI9341
 * 使用批量 SPI 写入 (CS/RS 一次拉起, 连续写像素数据)
 * ============================================================ */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if (!disp_flush_enabled)
    {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    uint32_t total = (uint32_t)(w * h);

    /* 设置写入窗口 (自动调用 WriteRAM_Prepare) */
    LCD_SetWindows(area->x1, area->y1, area->x2, area->y2);

    /* CS 拉低, RS 拉高 (数据模式), 批量写入 */
    LCD_CS_CLR;
    LCD_RS_SET;

    for (uint32_t i = 0; i < total; i++)
    {
        uint16_t c = color_p[i].full;

        /* 高字节 */
        while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(LCD_SPI, c >> 8);
        while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        (void)SPI_I2S_ReceiveData(LCD_SPI);

        /* 低字节 */
        while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(LCD_SPI, (uint8_t)c);
        while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        (void)SPI_I2S_ReceiveData(LCD_SPI);
    }

    LCD_CS_SET;

    /* 通知 LVGL 刷新完成 */
    lv_disp_flush_ready(disp_drv);
}

#else
typedef int keep_pedantic_happy;
#endif
