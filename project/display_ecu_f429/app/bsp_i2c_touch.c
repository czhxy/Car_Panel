#include "bsp_i2c_touch.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * 全局变量
 * ============================================================ */
_m_tp_dev tp_dev =
{
    .init = NULL,
    .scan = NULL,
    .sta  = 0,
};

/* ============================================================
 * I2C1 硬件初始化（SPL, 400kHz Fast Mode）
 * ============================================================ */
static void I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef  I2C_InitStructure;

    /* 时钟使能 */
    RCC_AHB1PeriphClockCmd(TOUCH_I2C_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(TOUCH_I2C_CLK, ENABLE);

    /* PB6=SCL, PB7=SDA: 开漏输出、复用功能 */
    GPIO_InitStructure.GPIO_Pin   = TOUCH_I2C_SCL_PIN | TOUCH_I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(TOUCH_I2C_PORT, &GPIO_InitStructure);

    GPIO_PinAFConfig(TOUCH_I2C_PORT, GPIO_PinSource6, TOUCH_I2C_AF);
    GPIO_PinAFConfig(TOUCH_I2C_PORT, GPIO_PinSource7, TOUCH_I2C_AF);

    /* I2C1 配置：400kHz Fast Mode */
    I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1         = 0x00;
    I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed          = 400000;
    I2C_Init(TOUCH_I2C, &I2C_InitStructure);

    I2C_Cmd(TOUCH_I2C, ENABLE);
}

/* ============================================================
 * CTP 控制引脚初始化 (PB8=INT, PB9=RST)
 * ============================================================ */
static void CTP_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* PB8: CTP_INT, 输入上拉 */
    GPIO_InitStructure.GPIO_Pin   = CTP_INT_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(CTP_INT_PORT, &GPIO_InitStructure);

    /* PB9: CTP_RST, 推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = CTP_RST_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(CTP_RST_PORT, &GPIO_InitStructure);
}

/* ============================================================
 * I2C 底层读写
 * ============================================================ */
static uint8_t I2C_WriteBytes(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    /* 起始条件 */
    I2C_GenerateSTART(TOUCH_I2C, ENABLE);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_MODE_SELECT));

    /* 发送从机地址（写） */
    I2C_Send7bitAddress(TOUCH_I2C, addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    /* 发送寄存器地址 */
    I2C_SendData(TOUCH_I2C, reg);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    /* 发送数据 */
    for (i = 0; i < len; i++)
    {
        I2C_SendData(TOUCH_I2C, buf[i]);
        while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    }

    /* 停止条件 */
    I2C_GenerateSTOP(TOUCH_I2C, ENABLE);

    return 0;
}

static void I2C_ReadBytes(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    /* 起始条件 */
    I2C_GenerateSTART(TOUCH_I2C, ENABLE);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_MODE_SELECT));

    /* 发送从机地址（写），用于设置寄存器地址 */
    I2C_Send7bitAddress(TOUCH_I2C, addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    /* 发送寄存器地址 */
    I2C_SendData(TOUCH_I2C, reg);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    /* 重复起始 + 读 */
    I2C_GenerateSTART(TOUCH_I2C, ENABLE);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(TOUCH_I2C, addr, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    for (i = 0; i < len; i++)
    {
        if (i == (len - 1))
        {
            /* 最后一个字节：关闭 ACK */
            I2C_AcknowledgeConfig(TOUCH_I2C, DISABLE);
        }
        while (!I2C_CheckEvent(TOUCH_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED));
        buf[i] = I2C_ReceiveData(TOUCH_I2C);
    }

    I2C_AcknowledgeConfig(TOUCH_I2C, ENABLE);

    /* 停止条件 */
    I2C_GenerateSTOP(TOUCH_I2C, ENABLE);
}

/* ============================================================
 * FT6336G 寄存器操作
 * ============================================================ */
uint8_t FT6336_WR_Reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    return I2C_WriteBytes(FT_CMD_WR, (uint8_t)(reg & 0xFF), buf, len);
}

void FT6336_RD_Reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    I2C_ReadBytes(FT_CMD_WR, (uint8_t)(reg & 0xFF), buf, len);
}

/* ============================================================
 * FT6336G 芯片初始化 + ID 验证
 * ============================================================ */
uint8_t FT6336_Init(void)
{
    uint8_t temp[2];

    /* 硬件复位 */
    GPIO_ResetBits(CTP_RST_PORT, CTP_RST_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    GPIO_SetBits(CTP_RST_PORT, CTP_RST_PIN);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 验证 VENDOR ID (0xA8=0x11) */
    FT6336_RD_Reg(FT_ID_G_FOCALTECH_ID, &temp[0], 1);
    if (temp[0] != 0x11)
    {
        LOG_E("[TOUCH] FT6336G ID check FAIL: VENDOR=0x%02X\r\n", temp[0]);
        return 1;
    }

    /* 验证芯片型号 (0x9F=0x26, 0xA0=0x00/01/02) */
    FT6336_RD_Reg(FT_ID_G_CIPHER_MID, &temp[0], 2);
    if (temp[0] != 0x26)
    {
        LOG_E("[TOUCH] FT6336G MID check FAIL: 0x%02X\r\n", temp[0]);
        return 1;
    }
    if ((temp[1] != 0x00) && (temp[1] != 0x01) && (temp[1] != 0x02))
    {
        LOG_E("[TOUCH] FT6336G Low check FAIL: 0x%02X\r\n", temp[1]);
        return 1;
    }

    /* 验证加密 ID (0xA3=0x64) */
    FT6336_RD_Reg(FT_ID_G_CIPHER_HIGH, &temp[0], 1);
    if (temp[0] != 0x64)
    {
        LOG_E("[TOUCH] FT6336G HIGH check FAIL: 0x%02X\r\n", temp[0]);
        return 1;
    }

    LOG_I("[TOUCH] FT6336G ID verified OK\r\n");
    return 0;
}

/* ============================================================
 * FT6336G 触摸扫描
 * ============================================================ */
static const uint16_t FT6336_TPX_TBL[2] = {FT_TP1_REG, FT_TP2_REG};

uint8_t FT6336_Scan(void)
{
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint8_t temp;
    uint8_t mode;
    static uint8_t t = 0;

    t++;
    if ((t % 10) == 0 || t < 10)
    {
        FT6336_RD_Reg(FT_REG_NUM_FINGER, &mode, 1);
        if (mode && (mode < 3))
        {
            temp = 0xFF << mode;
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            for (i = 0; i < CTP_MAX_TOUCH; i++)
            {
                FT6336_RD_Reg(FT6336_TPX_TBL[i], buf, 4);
                if (tp_dev.sta & (1 << i))
                {
                    tp_dev.x[i] = ((uint16_t)(buf[0] & 0x0F) << 8) + buf[1];
                    tp_dev.y[i] = ((uint16_t)(buf[2] & 0x0F) << 8) + buf[3];
                }
            }
            res = 1;
            if (tp_dev.x[0] == 0 && tp_dev.y[0] == 0) mode = 0;
            t = 0;
        }
    }

    if (mode == 0)
    {
        if (tp_dev.sta & TP_PRES_DOWN)
        {
            tp_dev.sta &= ~(1 << 7);
        }
        else
        {
            tp_dev.x[0] = 0xFFFF;
            tp_dev.y[0] = 0xFFFF;
            tp_dev.sta &= 0xE0;
        }
    }

    if (t > 240) t = 10;
    return res;
}

/* ============================================================
 * BSP_I2C_Touch_Init — I2C1 + FT6336G 全链路初始化
 * 调用时机：Task_Entry_All 中，BSP_SPI_LCD_Init 之后
 * ============================================================ */
void BSP_I2C_Touch_Init(void)
{
    LOG_I("[TOUCH] Initializing I2C1 + FT6336G...\r\n");

    I2C1_Init();
    CTP_GPIO_Init();

    if (FT6336_Init() == 0)
    {
        tp_dev.init = FT6336_Init;
        tp_dev.scan = FT6336_Scan;
        LOG_I("[TOUCH] FT6336G initialized OK\r\n");
    }
    else
    {
        LOG_W("[TOUCH] FT6336G init FAILED\r\n");
    }
}
