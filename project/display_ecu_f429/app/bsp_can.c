#include "bsp_can.h"
#include "bsp_log.h"

/* ============================================================
 * BSP_CAN_Init — CAN1 硬件初始化（GPIO、时钟、CAN、滤波器、NVIC）
 * ============================================================ */
void BSP_CAN_Init(void)
{		
    GPIO_InitTypeDef GPIO_InitStructure;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 时钟使能 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	
    /* PA12 -> CAN_TX, PA11 -> CAN_RX */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_CAN1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_CAN1);

    /* CAN 初始化：500kbps @ 45MHz APB1 (SYSCLK=180MHz, HCLK/4)
     * 45MHz / 9 / (1+7+2) = 500kHz */
    CAN_StructInit(&CAN_InitStructure);
    CAN_InitStructure.CAN_Prescaler = 9;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_7tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    if (CAN_Init(CAN1, &CAN_InitStructure) == 0) {
        LOG_E("[CAN] Init FAILED!\r\n");
    }

    /* 滤波器配置：接收所有消息 */
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInit(&CAN_FilterInitStructure);

    /* NVIC 配置 */
    NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 使能 FIFO0 消息挂起中断（滤波器全部分配给 FIFO0，FIFO1 不使用） */
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

    LOG_I("[CAN] Init OK  MSR=0x%04X ESR=0x%08X RF0R=0x%08X\r\n",
          (unsigned int)CAN1->MSR,
          (unsigned int)CAN1->ESR,
          (unsigned int)CAN1->RF0R);
}
