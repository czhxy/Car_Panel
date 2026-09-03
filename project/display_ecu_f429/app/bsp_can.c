#include "bsp_can.h"
#include "bsp_log.h"
#include "can_protocol.h"   /* CAN_ADDR_MOTORBOARD、协议位定义（全工程唯一来源） */

/* ============================================================
 * CAN 接收过滤器：接收动力域 (src=0x02) 与 广播/全局源 (src=0x00) 的扩展数据帧
 *
 * 背景：显示域是双 ECU 系统的主板(CAN_SELF_ADDR=0x01)，对端是动力域(0x02)。
 * 需求：除动力域上报外，还要接收 src=0x00 的广播/全局报文。因此过滤器放行
 * src ∈ {0x02, 0x00}，其余帧（自己发的 src=0x01、标准帧、远程帧等）在硬件层
 * 丢弃，不触发中断、不挤占 RX 队列。
 *
 * 位映射（ST 32-bit 掩码过滤器，扩展帧）：
 *   寄存器 32 位 = (EXID[28:0] << 3) | (IDE<<2) | (RTR<<1)
 *     - bit[3..31] : 29 位扩展帧 ID（EXID 左移 3，对齐到 bit3 起）
 *     - bit[2]     : IDE (1=扩展帧, 0=标准帧)
 *     - bit[1]     : RTR (0=数据帧, 1=远程帧)
 *     - bit[0]     : 保留
 *   源地址 src 字段位于 EXID[25:22] → 映射到寄存器 bit[28:25]。
 *
 * 掩码 bit = 1 表示"必须与 ID 值相符"，= 0 表示"不关心"：
 *   - src 段 (reg bit28:25) 置全 1 → 强制 src == 本 bank 配置的 src 值
 *   - IDE(bit2) / RTR(bit1) 置 1   → 强制"扩展数据帧"
 *   - 其余位（prio/dst/ftype/mode/func）掩码为 0，不关心，交由软件层精过滤
 *
 * 两个 bank 为"或"关系，任一命中即进 FIFO0：
 *   bank0 = 动力域 src=0x02；bank1 = 广播/全局源 src=0x00。
 * 注：src={0x02,0x00} 在 4 位里非连续 (0x02=0010, 0x00=0000)，单一掩码表达不
 *     了精确集合，故用两个 bank；将来接入第三节点再追加 bank 即可。
 * ============================================================ */
/* ---- 协议位 ↔ 过滤器寄存器位 换算（复用 can_protocol.h 的偏移/掩码宏）----
 * STM32 扩展帧过滤器：寄存器 = (EXID << 3) | (IDE<<2) | (RTR<<1)
 *   即协议 ID 第 n bit 落在过滤器寄存器第 (n+3) bit（整体左移 3）。
 *   所以协议的 src 段 (CAN_ID_OFFSET_SRC=22, 宽 4) 在寄存器里位于 bit[28:25]。
 * 用 CAN_ID_OFFSET_SRC / CAN_ID_MASK_SRC 推算，避免 25 / 0x0F 这类魔法数，
 * 保证与协议单一来源、协议位偏移变更时过滤器自动跟随。
 * IDE/RTR 是 CAN 硬件帧属性（非协议 ID 字段），用硬件位常量表达。
 * ---- */
#define BSP_CAN_REG_SHIFT         3U    /* EXID → 过滤器寄存器 左移 3（占 bit3..31） */
#define BSP_CAN_IDE_BIT           (1U << 2U)   /* 过滤器寄存器 bit2 = IDE (1=扩展帧, 0=标准帧) */
#define BSP_CAN_RTR_BIT           (1U << 1U)   /* 过滤器寄存器 bit1 = RTR (0=数据帧, 1=远程帧) */

/* 过滤器 ID：把协议 src 段按"左移 3"搬到寄存器 bit[28:25]，并置 IDE=1/RTR=0 */
#define BSP_CAN_FILTER_ID(src) \
    (((uint32_t)((src) & CAN_ID_MASK_SRC) << (CAN_ID_OFFSET_SRC + BSP_CAN_REG_SHIFT)) | BSP_CAN_IDE_BIT)
/* 过滤器 Mask：src 段置全 1(强制匹配) + IDE/RTR(强制扩展数据帧) */
#define BSP_CAN_FILTER_MASK \
    (((uint32_t)CAN_ID_MASK_SRC << (CAN_ID_OFFSET_SRC + BSP_CAN_REG_SHIFT)) | (BSP_CAN_IDE_BIT | BSP_CAN_RTR_BIT))

/* 配置一个 32-bit 掩码过滤器 bank，仅放行"源地址 = src"的扩展数据帧 */
static void BSP_CAN_FilterConfig(uint8_t bank, uint32_t src)
{
    CAN_FilterInitTypeDef filter_cfg;

    filter_cfg.CAN_FilterNumber          = bank;
    filter_cfg.CAN_FilterMode            = CAN_FilterMode_IdMask;
    filter_cfg.CAN_FilterScale           = CAN_FilterScale_32bit;
    filter_cfg.CAN_FilterFIFOAssignment  = CAN_Filter_FIFO0;
    filter_cfg.CAN_FilterActivation      = ENABLE;
    filter_cfg.CAN_FilterIdHigh          = (uint16_t)(BSP_CAN_FILTER_ID(src) >> 16U);
    filter_cfg.CAN_FilterIdLow           = (uint16_t)(BSP_CAN_FILTER_ID(src) & 0xFFFFU);
    filter_cfg.CAN_FilterMaskIdHigh      = (uint16_t)(BSP_CAN_FILTER_MASK >> 16U);
    filter_cfg.CAN_FilterMaskIdLow       = (uint16_t)(BSP_CAN_FILTER_MASK & 0xFFFFU);

    CAN_FilterInit(&filter_cfg);
}

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
		 //当前采样点（1+bs1)/(1+bs1+bs2)=80%   这里的1为时间同步段的长度1TS
    CAN_StructInit(&CAN_InitStructure);
    CAN_InitStructure.CAN_Prescaler = 9;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_7tq;//包含 PTS（传播时间段）+ 相位缓冲段1
    CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStructure.CAN_ABOM = ENABLE;   /* M5: 总线异常 Bus-Off 后自动恢复，无需软件重入 */
    if (CAN_Init(CAN1, &CAN_InitStructure) == 0) {
        LOG_E("[CAN] Init FAILED!\r\n");
    }

    /* 接收动力域(0x02) 与 广播/全局源(0x00) 的扩展数据帧，其余帧硬件丢弃。
     * CAN_FilterInit 内部会清 FMR.FINIT，可连续配置多个 bank（bank 间为"或"）。 */
    BSP_CAN_FilterConfig(0U, CAN_ADDR_MOTORBOARD);   /* bank0: 动力域 src=0x02 */
    BSP_CAN_FilterConfig(1U, CAN_ADDR_BROADCAST);    /* bank1: 广播/全局 src=0x00 */

    /* NVIC 配置：仅使能 FIFO0 接收中断。
     * SCE 错误中断已移除：无对端时 EWG/EPV/BOF 状态转换会触发中断风暴，
     * 饿死低优先级任务（UI/心跳）。总线恢复靠硬件 ABOM，发送故障兜底在
     * ModCommCan_Tx 的 NoMailBox 软件状态机，无需中断参与。 */
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
