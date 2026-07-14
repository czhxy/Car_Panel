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

    /* PA12 -> CAN_TX：复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA11 -> CAN_RX：上拉输入
     * 原配置与 TX 一样设成 AF_PP(推挽输出)，输出驱动器会与收发器 RXD 输出冲突，
     * 把总线/接收脚拉死，CAN_RX 完全失效。RX 必须为输入模式。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* CAN 初始化：500kbps @ 36MHz APB1
     * F103 在 SYSCLK=72MHz 下 PCLK1=HCLK/2=36MHz（不是 F4 的 42MHz！）。
     * 波特率 = fcan / (Prescaler × (1+BS1+BS2)) = 36M / (6×(1+9+2)) = 500kbps
     * 采样点 = (1+BS1)/(1+BS1+BS2) = 10/12 ≈ 83.3%
     * 与显示域 F429(45MHz, Prescaler=9, BS1=7, BS2=2, 500kbps, SP=80%) 速率一致。
     * 原 BS1=11tq 在 36MHz 下实为 428.6kbps，与 F429 对不上，无法通信。 */
    CAN_StructInit(&CAN_InitStructure);
    CAN_InitStructure.CAN_Prescaler = 6;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_9tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStructure.CAN_ABOM = ENABLE;   /* 自动 Bus-Off 恢复：单节点无 ACK 也会触发 BusOff，靠硬件自恢复 */
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

    /* 使能 SCE 错误中断。
     * 注意：F1 的 CAN IER 中 EWG/EPV/BOF 没有独立的使能位(对应位是保留位)，
     *       上面那种 CAN_ITConfig(EWG/EPV/BOF) 实际写的是保留位，ERRIE 从未被置位，
     *       结果 SCE 中断永远不进、Bus-Off 无法恢复。错误中断由 ERRIE 统一使能，
     *       进入 ISR 后再按 ESR 区分是 Warning/Passive/BusOff。 */
    CAN_ITConfig(CAN1, CAN_IT_ERR, ENABLE);   /* 错误中断总使能 */
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	if (CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
	{
		CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);
		if(can_rx_cb != NULL)
		{
			can_rx_cb(RxMessage);   /* 回调形参为 CanRxMsg(按值)，直接传结构体副本 */
		}
	}
}

/* CAN SCE 错误中断：Bus-Off / Error Passive / Error Warning
 * ABOM 已使能，Bus-Off 由硬件在 128×11 个隐性位后自动恢复，无需软件手动退出。
 * 这里只读 ESR 记录错误状态(供故障保护)，并清 ERRIE 挂起位避免重复触发。
 * （旧实现里那段手动 INRQ/SLAK 翻转很脆弱，且因 ERRIE 没使能根本不会执行） */
void CAN1_SCE_IRQHandler(void)
{
    /* ESR: [2]BOFF [1]EPVF [0]EWGF [23:16]TEC [15:8]REC —— 需要时据此诊断 */
    (void)CAN1->ESR;
    /* EWGF/EPVF/BOFF 是只读状态位，由硬件按总线状态自动清；这里清 ERRIE 与 LEC */
    CAN_ClearITPendingBit(CAN1, CAN_IT_ERR);
}
