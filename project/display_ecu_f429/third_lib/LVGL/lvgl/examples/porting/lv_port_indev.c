/**
 * @file lv_port_indev.c
 * LVGL 输入设备移植 — FT6336G 电容触摸屏
 * 仅启用触摸板, 其他设备 (鼠标/键盘/编码器/按钮) 已精简
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"
#include "bsp_i2c_touch.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    /* 触摸板 (FT6336G, I2C1 已在 task_entry 中初始化) */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* ============================================================
 * touchpad_read — LVGL 回调, 读取触摸坐标和按压状态
 * 数据来源于 FT6336G 驱动 (tp_dev 全局结构体)
 * 注意: 当前 LCD 为竖屏 (240×320), 触摸坐标无需旋转变换
 * ============================================================ */
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    (void)indev_drv;

    /* 调用 FT6336G 扫描获取最新触摸数据 */
    extern _m_tp_dev tp_dev;
    if (tp_dev.scan != NULL)
    {
        tp_dev.scan();
    }

    /* TP_PRES_DOWN 位 (0x80) 判断是否按下 */
    if (tp_dev.sta & TP_PRES_DOWN)
    {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = (lv_coord_t)tp_dev.x[0];
        data->point.y = (lv_coord_t)tp_dev.y[0];
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

#else
typedef int keep_pedantic_happy;
#endif
