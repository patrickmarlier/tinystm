/*
 * File:
 *   mod_local.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module for local memory accesses.
 *
 * Copyright (c) 2008.
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

#ifndef _MOD_LOCAL_H_
#define _MOD_LOCAL_H_

#include "stm.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

void stm_store_local(stm_word_t *addr, stm_word_t value);

void stm_store_local_char(char *addr, char value);
void stm_store_local_uchar(unsigned char *addr, unsigned char value);
void stm_store_local_short(short *addr, short value);
void stm_store_local_ushort(unsigned short *addr, unsigned short value);
void stm_store_local_int(int *addr, int value);
void stm_store_local_uint(unsigned int *addr, unsigned int value);
void stm_store_local_long(long *addr, long value);
void stm_store_local_ulong(unsigned long *addr, unsigned long value);
void stm_store_local_float(float *addr, float value);
void stm_store_local_double(double *addr, double value);

void mod_local_init();

#define stm_store_local_ptr(addr, val)  stm_store_local((stm_word_t *)(void *)(addr), vp2w(val))

#ifdef __cplusplus
}
#endif

#endif /* _MOD_LOCAL_H_ */
