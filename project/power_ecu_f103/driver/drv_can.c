#include "drv_can.h"

can_rx_callback can_rx_cb = NULL;
CanRxMsg RxMessage;

void can_rx_cb_register(can_rx_callback cb)
{
	can_rx_cb = cb;
}

void drv_can_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 时钟使能 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    /* PA12 -> CAN_TX, PA11 -> CAN_RX */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* CAN 初始化：500k @ 42MHz APB1 */
    CAN_StructInit(&CAN_InitStructure);
    CAN_InitStructure.CAN_Prescaler = 6;      // 42M / 6 /(11+2+1) = 500k
    CAN_InitStructure.CAN_BS1 = CAN_BS1_11tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    CAN_Init(CAN1,&CAN_InitStructure);

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

    /* NVIC 配置：CAN RX0 中断 */
    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* NVIC 配置：CAN SCE（状态变化/错误）中断 */
    NVIC_InitStructure.NVIC_IRQChannel = CAN1_SCE_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 使能 FIFO0 消息挂起中断 */
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

    /* 使能 SCE 错误状态变化中断 */
    CAN_ITConfig(CAN1, CAN_IT_EWG, ENABLE);   /* 错误警告 */
    CAN_ITConfig(CAN1, CAN_IT_EPV, ENABLE);   /* 错误被动 */
    CAN_ITConfig(CAN1, CAN_IT_BOF, ENABLE);   /* Bus-Off */
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	if (CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
	{
		CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);
		if(can_rx_cb != NULL)
		{
			can_rx_cb(&RxMessage);
		}
	}
}

/* CAN SCE 错误中断：处理 Bus-Off 自动恢复 */
void CAN1_SCE_IRQHandler(void)
{
    if (CAN_GetITStatus(CAN1, CAN_IT_BOF) != RESET)
    {
        CAN_ClearITPendingBit(CAN1, CAN_IT_BOF);
        /* Bus-Off：尝试恢复 */
        CAN1->MCR &= ~CAN_MCR_ABOM;  /* 已手动恢复，关闭自动离线管理 */
        if ((CAN1->MSR & CAN_MSR_SLAK) != 0)
        {
            CAN1->MCR &= ~CAN_MCR_SLEEP;
        }
        CAN1->MCR |= CAN_MCR_INRQ;
        while ((CAN1->MSR & CAN_MSR_INAK) == 0);
        CAN1->MCR &= ~CAN_MCR_INRQ;
        while ((CAN1->MSR & CAN_MSR_INAK) != 0);
    }
    else if (CAN_GetITStatus(CAN1, CAN_IT_EPV) != RESET)
    {
        CAN_ClearITPendingBit(CAN1, CAN_IT_EPV);
        /* Error Passive：记录但不动作，硬件自动恢复 */
    }
    else if (CAN_GetITStatus(CAN1, CAN_IT_EWG) != RESET)
    {
        CAN_ClearITPendingBit(CAN1, CAN_IT_EWG);
        /* Error Warning：记录但不动作，硬件自动恢复 */
    }
}
