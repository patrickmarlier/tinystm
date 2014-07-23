/*
 * File:
 *   wrappers.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   STM wrapper functions for different data types.
 *
 * Copyright (c) 2007-2008.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _WRAPPERS_H_
#define _WRAPPERS_H_

#include <stdint.h>

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/* Usigned types of various sizes */
uint8_t stm_load8(volatile uint8_t *addr);
uint16_t stm_load16(volatile uint16_t *addr);
uint32_t stm_load32(volatile uint32_t *addr);
uint64_t stm_load64(volatile uint64_t *addr);

void stm_store8(volatile uint8_t *addr, uint8_t value);
void stm_store16(volatile uint16_t *addr, uint16_t value);void stm_store32(volatile uint32_t *addr, uint32_t value);
void stm_store64(volatile uint64_t *addr, uint64_t value);

/* Basic C data types */
char stm_load_char(volatile char *addr);
unsigned char stm_load_uchar(volatile unsigned char *addr);
short stm_load_short(volatile short *addr);
unsigned short stm_load_ushort(volatile unsigned short *addr);
int stm_load_int(volatile int *addr);
unsigned int stm_load_uint(volatile unsigned int *addr);
long stm_load_long(volatile long *addr);
unsigned long stm_load_ulong(volatile unsigned long *addr);
float stm_load_float(volatile float *addr);
double stm_load_double(volatile double *addr);

void stm_store_char(volatile char *addr, char value);
void stm_store_uchar(volatile unsigned char *addr, unsigned char value);
void stm_store_short(volatile short *addr, short value);
void stm_store_ushort(volatile unsigned short *addr, unsigned short value);
void stm_store_int(volatile int *addr, int value);
void stm_store_uint(volatile unsigned int *addr, unsigned int value);
void stm_store_long(volatile long *addr, long value);
void stm_store_ulong(volatile unsigned long *addr, unsigned long value);
void stm_store_float(volatile float *addr, float value);
void stm_store_double(volatile double *addr, double value);

#define stm_load_ptr(addr)              w2vp(stm_load((stm_word_t *)(void *)(addr)))
#define stm_store_ptr(addr, val)        stm_store((stm_word_t *)(void *)(addr), vp2w(val))

#ifdef __cplusplus
}
#endif

#endif /* _WRAPPERS_H_ */
