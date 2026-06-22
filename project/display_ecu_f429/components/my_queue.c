#include "my_queue.h"
#include <string.h>

/* ============================================================
 * Queue_Init — 初始化环形队列
 * 要求 buffer_size / item_size >= 2（至少留一个空位区分空/满）
 * ============================================================ */
bool Queue_Init(QueueType *q, void *buffer, uint32_t buffer_size, uint32_t item_size)
{
    if (q == NULL || buffer == NULL || item_size == 0) {
        return false;
    }

    uint32_t count = buffer_size / item_size;
    if (count < 2) {
        return false; /* 至少需要 2 个元素才能区分空/满 */
    }

    q->front     = 0;
    q->rear      = 0;
    q->itemSize  = item_size;
    q->itemCount = count;
    q->pool      = (uint8_t *)buffer;

    return true;
}

/* ============================================================
 * Queue_Put — 入队
 * 满则返回 false，否则 memcpy 数据并入队
 * ============================================================ */
bool Queue_Put(QueueType *q, void *pdata)
{
    if (q == NULL || pdata == NULL || q->pool == NULL) {
        return false;
    }

    uint32_t next_rear = (q->rear + 1) % q->itemCount;
    if (next_rear == q->front) {
        return false; /* 队列满 */
    }

    memcpy(&q->pool[q->rear * q->itemSize], pdata, q->itemSize);
    q->rear = next_rear;
    return true;
}

/* ============================================================
 * Queue_Get — 出队并移动头指针
 * 空则返回 false
 * ============================================================ */
bool Queue_Get(QueueType *q, void *pdata)
{
    if (q == NULL || pdata == NULL || q->pool == NULL) {
        return false;
    }

    if (q->front == q->rear) {
        return false; /* 队列空 */
    }

    memcpy(pdata, &q->pool[q->front * q->itemSize], q->itemSize);
    q->front = (q->front + 1) % q->itemCount;
    return true;
}

/* ============================================================
 * Queue_Query — 只读队首元素，不移动指针
 * 空则返回 false
 * ============================================================ */
bool Queue_Query(QueueType *q, void *pdata)
{
    if (q == NULL || pdata == NULL || q->pool == NULL) {
        return false;
    }

    if (q->front == q->rear) {
        return false; /* 队列空 */
    }

    memcpy(pdata, &q->pool[q->front * q->itemSize], q->itemSize);
    return true;
}
