#ifndef __MY_QUEUE_H
#define __MY_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * QueueType — 轻量级环形队列结构体
 * ============================================================ */
typedef struct QueueTypeStruct {
    uint32_t front;     /* 头指针（出队位置） */
    uint32_t rear;      /* 尾指针（入队位置） */
    uint32_t itemSize;  /* 单个元素字节数 */
    uint32_t itemCount; /* 最大元素个数 */
    uint8_t *pool;      /* 用户提供的缓冲区指针 */
} QueueType;

/* ---- API ---- */
bool Queue_Init(QueueType *q, void *buffer, uint32_t buffer_size, uint32_t item_size);
bool Queue_Put(QueueType *q, void *pdata);
bool Queue_Get(QueueType *q, void *pdata);
bool Queue_Query(QueueType *q, void *pdata);

#endif /* __MY_QUEUE_H */
