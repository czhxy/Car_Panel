/**
 * \file            lwrb.h
 * \brief           Lightweight ring buffer
 */

/*
 * Copyright (c) 2024 Tilen MAJERLE
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of LwRB - Lightweight ring buffer library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         v3.2.0
 */
#ifndef LWRB_HDR_H
#define LWRB_HDR_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \defgroup        LWRB Lightweight ring buffer manager
 * \brief           Lightweight ring buffer manager
 * \{
 */

/* 对于 Keil MDK ARM Compiler 5，不支持 C11 stdatomic.h，禁用原子操作。
 * 在单核 Cortex-M4 上（ISR 写 + Task 读的场景），32位对齐访问天然原子，
 * FreeRTOS 上下文切换提供足够的内存同步保证。 */
#ifndef LWRB_DISABLE_ATOMIC
#define LWRB_DISABLE_ATOMIC
#endif

/**
 * \brief           Size type for buffer variables
 */
#ifndef LWRB_SZ_T
#define LWRB_SZ_T uint32_t
#endif
typedef LWRB_SZ_T lwrb_sz_t;

/**
 * \brief           Event function prototype
 */
typedef void (*lwrb_evt_fn)(void*, uint8_t, lwrb_sz_t);

/* Forward declaration */
struct lwrb;

/**
 * \brief           Ring buffer instance structure
 */
typedef struct lwrb {
    uint8_t* buff;          /*!< Pointer to buffer data array */
    lwrb_sz_t size;         /*!< Size of buffer data array in bytes */
    lwrb_sz_t r_ptr;        /*!< Read pointer (next byte to read) */
    lwrb_sz_t w_ptr;        /*!< Write pointer (next byte to write) */
    void* arg;              /*!< Custom user argument for event callback */
    lwrb_evt_fn evt_fn;     /*!< Event callback function */
} lwrb_t;

/**
 * \brief           Buffer event types
 */
#define LWRB_EVT_READ               ((uint8_t)0)
#define LWRB_EVT_WRITE              ((uint8_t)1)
#define LWRB_EVT_RESET              ((uint8_t)2)

/**
 * \brief           Flags for write extended operation
 */
#define LWRB_FLAG_WRITE_ALL         ((uint16_t)(1 << 0))

/**
 * \brief           Flags for read extended operation
 */
#define LWRB_FLAG_READ_ALL          ((uint16_t)(1 << 0))

/* ------------------------------------------------------------------------- */
/*  Core buffer management functions                                         */
/* ------------------------------------------------------------------------- */

uint8_t     lwrb_init(lwrb_t* buff, void* buffdata, lwrb_sz_t size);
uint8_t     lwrb_is_ready(lwrb_t* buff);
void        lwrb_free(lwrb_t* buff);
void        lwrb_set_evt_fn(lwrb_t* buff, lwrb_evt_fn evt_fn);
void        lwrb_set_arg(lwrb_t* buff, void* arg);
void*       lwrb_get_arg(lwrb_t* buff);

/* ------------------------------------------------------------------------- */
/*  Read / Write operations                                                  */
/* ------------------------------------------------------------------------- */

lwrb_sz_t   lwrb_write(lwrb_t* buff, const void* data, lwrb_sz_t btw);
uint8_t     lwrb_write_ex(lwrb_t* buff, const void* data, lwrb_sz_t btw,
                          lwrb_sz_t* bwritten, uint16_t flags);
lwrb_sz_t   lwrb_read(lwrb_t* buff, void* data, lwrb_sz_t btr);
uint8_t     lwrb_read_ex(lwrb_t* buff, void* data, lwrb_sz_t btr,
                         lwrb_sz_t* bread, uint16_t flags);
lwrb_sz_t   lwrb_peek(const lwrb_t* buff, lwrb_sz_t skip_count,
                      void* data, lwrb_sz_t btp);

/* ------------------------------------------------------------------------- */
/*  Buffer status functions                                                  */
/* ------------------------------------------------------------------------- */

lwrb_sz_t   lwrb_get_free(const lwrb_t* buff);
lwrb_sz_t   lwrb_get_full(const lwrb_t* buff);
void        lwrb_reset(lwrb_t* buff);

/* ------------------------------------------------------------------------- */
/*  Linear block access (for zero-copy operations)                           */
/* ------------------------------------------------------------------------- */

void*       lwrb_get_linear_block_read_address(const lwrb_t* buff);
lwrb_sz_t   lwrb_get_linear_block_read_length(const lwrb_t* buff);
void*       lwrb_get_linear_block_write_address(const lwrb_t* buff);
lwrb_sz_t   lwrb_get_linear_block_write_length(const lwrb_t* buff);

/* ------------------------------------------------------------------------- */
/*  Pointer manipulation (for DMA / ISR use)                                 */
/* ------------------------------------------------------------------------- */

lwrb_sz_t   lwrb_skip(lwrb_t* buff, lwrb_sz_t len);
lwrb_sz_t   lwrb_advance(lwrb_t* buff, lwrb_sz_t len);

/* ------------------------------------------------------------------------- */
/*  Data search                                                              */
/* ------------------------------------------------------------------------- */

uint8_t     lwrb_find(const lwrb_t* buff, const void* bts, lwrb_sz_t len,
                      lwrb_sz_t start_offset, lwrb_sz_t* found_idx);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LWRB_HDR_H */
