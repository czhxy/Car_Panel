/**
  ******************************************************************************
  * @file    mod_comm_uart.c
  * @brief   UART 通信框架实现 — 参考 mod_comm_can.c
  *
  * 数据流：
  *   RX: USART1_IRQHandler → Mod_Uart_RxIRQHandler()（ISR 直入字节队列）
  *       → Task_UartRx 取字节 → Mod_Uart_RxByte 拼包状态机 → ModCommUart_OnRxPacket()
  *   TX: Mod_Uart_SendPacket() 组包入 TX 队列
  *       → Task_UartTx → Mod_Uart_TxSend 统一消费 → UART_SendArray()（临界区与 printf 互斥）
  ******************************************************************************
  */
#include "mod_comm_uart.h"
#include "usart.h"
#include "bsp_log.h"
#include "task.h"      /* taskENTER_CRITICAL / taskEXIT_CRITICAL */
#include <string.h>
#include <stdio.h>     /* vsnprintf（UART_Log 强符号覆盖用） */

/* ============================================================
 * 弱符号兼容宏
 * MDK: __weak / GCC: __attribute__((weak))
 * ============================================================ */
#if defined(__GNUC__) && !defined(__CC_ARM)
  #define WEAK __attribute__((weak))
#else
  #define WEAK __weak
#endif

/* ---- 发送包结构（UartTxQueue 元素）---- */
typedef struct {
    uint8_t buf[UART_PKT_MAX_LEN];   /* 完整帧：头 + type + len + data + crc */
    uint8_t len;                     /* 实际帧长度 */
} ModUartTxPacket;

/* ---- 静态变量 ---- */
static QueueHandle_t UartRxQueue = NULL;   /* 字节队列（ISR 推入，RX 任务消费） */
static QueueHandle_t UartTxQueue = NULL;   /* 发送包队列（业务推入，TX 任务消费） */
static QueueHandle_t UartLogQueue = NULL;  /* 日志字符串队列（LOG → TX 任务） */

/* ---- 诊断计数：ISR 累计收字节数（仿 can_rx_isr_cnt 模式） ---- */
volatile uint32_t uart_rx_isr_cnt = 0;

/* ---- 诊断计数：日志队列满丢弃计数（日志尽力而为） ---- */
volatile uint32_t uart_log_drop_cnt = 0;

/* ============================================================
 * crc16_calc — CRC16 (poly 0x1021, init 0x0000, MSB first)
 * 与 YMODEM / tools/ota_gui/ota_core.py 保持一致
 * ============================================================ */
static uint16_t crc16_calc(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0;
    uint8_t  i, j;
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ============================================================
 * Mod_Uart_Init — 创建 RX/TX 队列
 * 必须在使能 RXNE 中断前调用（Task_Entry_All 中、创建任务前）
 * ============================================================ */
void Mod_Uart_Init(void)
{
    UartRxQueue  = xQueueCreate(UART_RX_QUEUE_LENGTH, sizeof(uint8_t));
    UartTxQueue  = xQueueCreate(UART_TX_QUEUE_LENGTH, sizeof(ModUartTxPacket));
    UartLogQueue = xQueueCreate(UART_LOG_QUEUE_LENGTH, UART_LOG_BUF_SIZE);

    if (UartRxQueue == NULL || UartTxQueue == NULL || UartLogQueue == NULL) {
        LOG_E("[UART] Queue create FAILED!\r\n");
    } else {
        LOG_I("[UART] Queues created rx=%u tx=%u log=%u\r\n",
              (unsigned)UART_RX_QUEUE_LENGTH, (unsigned)UART_TX_QUEUE_LENGTH,
              (unsigned)UART_LOG_QUEUE_LENGTH);
    }
}

/* ============================================================
 * Mod_Uart_RxIRQHandler — RXNE 中断取字节，入字节队列
 * 在 USART1_IRQHandler 中调用（stm32f4xx_it.c）
 * ============================================================ */
void Mod_Uart_RxIRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t    ch;

    /* 取尽 RXNE，避免中断风暴 */
    while (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        ch = (uint8_t)USART_ReceiveData(USART1);   /* 读 DR 自动清 RXNE */
        uart_rx_isr_cnt++;                         /* 诊断：累计收字节数 */

        if (UartRxQueue != NULL) {
            xQueueSendFromISR(UartRxQueue, &ch, &xHigherPriorityTaskWoken);
        }
    }

    /* ORE 溢出：读 SR + DR 清除，避免阻塞后续接收 */
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        (void)USART1->SR;
        (void)USART1->DR;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================
 * Mod_Uart_SendPacket — 组包并非阻塞入 TX 队列
 * 帧格式: [AA][55][type][len][data...][crc16_hi][crc16_lo]
 * ============================================================ */
bool Mod_Uart_SendPacket(uint8_t type, const uint8_t *data, uint8_t len)
{
    ModUartTxPacket pkt;
    uint8_t idx = 0;

    if (len > UART_PKT_MAX_DATA) {
        return false;
    }

    pkt.buf[idx++] = UART_PKT_HEADER1;
    pkt.buf[idx++] = UART_PKT_HEADER2;
    pkt.buf[idx++] = type;
    pkt.buf[idx++] = len;
    if (data != NULL && len > 0) {
        memcpy(&pkt.buf[idx], data, len);
        idx += len;
    }

    uint16_t crc = crc16_calc(data, len);
    pkt.buf[idx++] = (uint8_t)(crc >> 8);
    pkt.buf[idx++] = (uint8_t)(crc & 0xFF);
    pkt.len = idx;

    if (UartTxQueue == NULL) {
        return false;
    }
    if (xQueueSend(UartTxQueue, &pkt, 0) == pdPASS) {
        return true;
    }
    return false;
}

/* ============================================================
 * Mod_Uart_TxSend — 从 TX 队列取一个包并发送（发送任务调用）
 * timeout: 队列空时等待时间（portMAX_DELAY 阻塞 / 0 立即返回）
 * 返回: 是否成功发送了一个包
 * 与 printf(fputc 轮询写 USART1) 互斥，发送时用临界区保护
 * ============================================================ */
bool Mod_Uart_TxSend(TickType_t timeout)
{
    ModUartTxPacket pkt;

    if (UartTxQueue == NULL) { return false; }
    if (xQueueReceive(UartTxQueue, &pkt, timeout) != pdPASS) { return false; }

    taskENTER_CRITICAL();
    UART_SendArray(pkt.buf, pkt.len);
    taskEXIT_CRITICAL();
    return true;
}

/* ============================================================
 * UART_Log — 强符号覆盖 driver/usart.c 的弱符号 UART_Log
 * vsnprintf 格式化 → 非阻塞入日志队列（xQueueSendFromISR 任务/ISR 通用）
 *  - 队列满 → 丢弃新日志并累计 uart_log_drop_cnt（日志尽力而为，不阻塞调用方）
 *  - 队列未建（极端早期）→ 兜底直接 UART_SendString，保持现状行为
 * 发送统一由 Task_UartTx 经 Mod_Uart_LogSend 消费，日志字节流不再由
 * 各调用上下文直接写 USART1（解决 ISR 内 printf 阻塞 / 多任务交错问题）
 * ============================================================ */
void UART_Log(const char *format, ...)
{
    char buf[UART_LOG_BUF_SIZE];
    va_list arg;

    va_start(arg, format);
    vsnprintf(buf, sizeof(buf), format, arg);
    va_end(arg);

    /* 调度器未启动（main 初始化/banner 阶段）时直接发送：
     * 若此时走 xQueueSendFromISR 会触发 vPortValidateInterruptPriority()，
     * 而 ulMaxPRIGROUPValue 要到 xPortStartScheduler() 才初始化（初始为 0），
     * 断言 (AIRCR&0x700) <= 0 必失败 → configASSERT 死循环、无任何输出。
     * 调度器运行后（含 ISR 上下文）才入队，由 Task_UartTx 统一消费。 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        UART_SendString(buf);
        return;
    }

    if (UartLogQueue != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendFromISR(UartLogQueue, buf, &xHigherPriorityTaskWoken) != pdPASS) {
            uart_log_drop_cnt++;
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        UART_SendString(buf);
    }
}

/* ============================================================
 * Mod_Uart_LogSend — 从日志队列取一条并发送（发送任务调用）
 * timeout: 队列空时等待时间（portMAX_DELAY 阻塞 / 0 立即返回）
 * 返回: 是否成功发送了一条日志
 * 发送与协议包共用 taskENTER_CRITICAL 临界区，串行不交错
 * ============================================================ */
bool Mod_Uart_LogSend(TickType_t timeout)
{
    static char buf[UART_LOG_BUF_SIZE];

    if (UartLogQueue == NULL) { return false; }
    if (xQueueReceive(UartLogQueue, buf, timeout) != pdPASS) { return false; }

    taskENTER_CRITICAL();
    UART_SendArray((const uint8_t *)buf, (uint16_t)strlen(buf));
    taskEXIT_CRITICAL();
    return true;
}

/* ============================================================
 * 拼包状态机（RX 任务内部）
 *   WAIT_H1 → WAIT_H2 → TYPE → LEN → DATA → CRC_HI → CRC_LO → 回调
 *   len==0 兼容 PC 旧查询指令 [AA 55 type 00]（无 CRC），收到 len 立即回调
 * ============================================================ */
typedef enum {
    RX_ST_WAIT_H1,
    RX_ST_WAIT_H2,
    RX_ST_TYPE,
    RX_ST_LEN,
    RX_ST_DATA,
    RX_ST_CRC_HI,
    RX_ST_CRC_LO
} UartRxState;

static UartRxState uart_rx_state = RX_ST_WAIT_H1;
static uint8_t     uart_rx_type  = 0;
static uint8_t     uart_rx_len   = 0;       /* 期望 data 长度 */
static uint8_t     uart_rx_data[UART_PKT_MAX_DATA];
static uint8_t     uart_rx_cnt   = 0;       /* 已收 data 字节数 */
static uint8_t     uart_rx_crc_hi = 0;
static uint8_t     uart_rx_crc_lo = 0;

/* ---- 单字节拼包处理 ---- */
static void uart_rx_process_byte(uint8_t ch)
{
    switch (uart_rx_state) {
    case RX_ST_WAIT_H1:
        if (ch == UART_PKT_HEADER1) {
            uart_rx_state = RX_ST_WAIT_H2;
        }
        break;

    case RX_ST_WAIT_H2:
        uart_rx_state = (ch == UART_PKT_HEADER2) ? RX_ST_TYPE : RX_ST_WAIT_H1;
        break;

    case RX_ST_TYPE:
        uart_rx_type = ch;
        uart_rx_state = RX_ST_LEN;
        break;

    case RX_ST_LEN:
        uart_rx_len = ch;
        if (uart_rx_len == 0) {
            /* 兼容 PC 查询指令 [AA 55 type 00]（无 CRC） */
            ModCommUart_OnRxPacket(uart_rx_type, NULL, 0);
            uart_rx_state = RX_ST_WAIT_H1;
        } else if (uart_rx_len > UART_PKT_MAX_DATA) {
            /* 非法长度，重新同步 */
            uart_rx_state = RX_ST_WAIT_H1;
        } else {
            uart_rx_cnt = 0;
            uart_rx_state = RX_ST_DATA;
        }
        break;

    case RX_ST_DATA:
        uart_rx_data[uart_rx_cnt++] = ch;
        if (uart_rx_cnt >= uart_rx_len) {
            uart_rx_state = RX_ST_CRC_HI;
        }
        break;

    case RX_ST_CRC_HI:
        uart_rx_crc_hi = ch;
        uart_rx_state = RX_ST_CRC_LO;
        break;

    case RX_ST_CRC_LO:
    {
        uart_rx_crc_lo = ch;
        uint16_t crc_recv = ((uint16_t)uart_rx_crc_hi << 8) | uart_rx_crc_lo;
        uint16_t crc_calc = crc16_calc(uart_rx_data, uart_rx_len);
        if (crc_recv == crc_calc) {
            ModCommUart_OnRxPacket(uart_rx_type, uart_rx_data, uart_rx_len);
        } else {
            LOG_D("[UART] RX crc mismatch type=0x%02X\r\n", uart_rx_type);
        }
        uart_rx_state = RX_ST_WAIT_H1;
        break;
    }

    default:
        uart_rx_state = RX_ST_WAIT_H1;
        break;
    }
}

/* ============================================================
 * Mod_Uart_RxDequeue — 从字节队列取一字节（供接收任务调用）
 * timeout: portMAX_DELAY 阻塞等待 / 0 立即返回
 * 返回: 是否成功取到一字节
 * ============================================================ */
bool Mod_Uart_RxDequeue(uint8_t *ch, TickType_t timeout)
{
    if (UartRxQueue == NULL || ch == NULL) { return false; }
    return (xQueueReceive(UartRxQueue, ch, timeout) == pdPASS);
}

/* ============================================================
 * Mod_Uart_RxByte — 喂一个字节给拼包状态机（协议编解码在 mod 层）
 * ============================================================ */
void Mod_Uart_RxByte(uint8_t ch)
{
    uart_rx_process_byte(ch);
}

/* ============================================================
 * ModCommUart_OnRxPacket — 弱符号默认实现（空）
 * 业务层定义同名强符号覆盖，参考 ModCommCan_OnRxFrame
 * ============================================================ */
WEAK void ModCommUart_OnRxPacket(uint8_t type, const uint8_t *data, uint8_t len)
{
    (void)type;
    (void)data;
    (void)len;
}
