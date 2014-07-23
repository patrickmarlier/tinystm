/*
 * File:
 *   wrappers.c
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

#include "stm.h"

#define COMPILE_TIME_ASSERT(pred)       switch (0) { case 0: case pred: ; }

typedef union convert_64 {
  uint64_t u64;
  uint32_t u32[2];
  uint16_t u16[4];
  uint8_t u8[8];
  int64_t s64;
  double d;
} convert_64_t;

typedef union convert_32 {
  uint32_t u32;
  uint16_t u16[2];
  uint8_t u8[4];
  int32_t s32;
  float f;
} convert_32_t;

typedef union convert_16 {
  uint16_t u16;
  int16_t s16;
} convert_16_t;

typedef union convert_8 {
  uint8_t u8;
  int8_t s8;
} convert_8_t;

static void sanity_checks()
{
  COMPILE_TIME_ASSERT(sizeof(convert_64_t) == 8);
  COMPILE_TIME_ASSERT(sizeof(convert_32_t) == 4);
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == 4 || sizeof(stm_word_t) == 8);
  COMPILE_TIME_ASSERT(sizeof(char) == 1);
  COMPILE_TIME_ASSERT(sizeof(short) == 2);
  COMPILE_TIME_ASSERT(sizeof(int) == 4);
  COMPILE_TIME_ASSERT(sizeof(long) == 4 || sizeof(long) == 8);
  COMPILE_TIME_ASSERT(sizeof(float) == 4);
  COMPILE_TIME_ASSERT(sizeof(double) == 8);
}

/* ################################################################### *
 * LOADS
 * ################################################################### */

uint8_t stm_load8(volatile uint8_t *addr)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val;
    val.u32 = (uint32_t)stm_load((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03));
    return val.u8[(uintptr_t)addr & 0x03];
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)stm_load((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u8[(uintptr_t)addr & 0x07];
  }
}

uint16_t stm_load16(volatile uint16_t *addr)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val;
    val.u32 = (uint32_t)stm_load((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03));
    return val.u16[((uintptr_t)addr & 0x03) >> 1];
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)stm_load((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u16[((uintptr_t)addr & 0x07) >> 1];
  }
}

uint32_t stm_load32(volatile uint32_t *addr)
{
  if (sizeof(stm_word_t) == 4) {
    return (uint32_t)stm_load((volatile stm_word_t *)addr);
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)stm_load((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u32[((uintptr_t)addr & 0x07) >> 2];
  }
}

uint64_t stm_load64(volatile uint64_t *addr)
{
  if (sizeof(stm_word_t) == 4) {
    convert_64_t val;
    val.u32[0] = (uint32_t)stm_load((volatile stm_word_t *)addr);
    val.u32[1] = (uint32_t)stm_load((volatile stm_word_t *)addr + 1);
    return val.u64;
  } else {
    return (uint64_t)stm_load((volatile stm_word_t *)addr);
  }
}

char stm_load_char(volatile char *addr)
{
  convert_8_t val;
  val.u8 = stm_load8((volatile uint8_t *)addr);
  return val.s8;
}

unsigned char stm_load_uchar(volatile unsigned char *addr)
{
  return (unsigned char)stm_load8((volatile uint8_t *)addr);
}

short stm_load_short(volatile short *addr)
{
  convert_16_t val;
  val.u16 = stm_load16((volatile uint16_t *)addr);
  return val.s16;
}

unsigned short stm_load_ushort(volatile unsigned short *addr)
{
  return (unsigned short)stm_load16((volatile uint16_t *)addr);
}

int stm_load_int(volatile int *addr)
{
  convert_32_t val;
  val.u32 = stm_load32((volatile uint32_t *)addr);
  return val.s32;
}

unsigned int stm_load_uint(volatile unsigned int *addr)
{
  return (unsigned int)stm_load32((volatile uint32_t *)addr);
}

long stm_load_long(volatile long *addr)
{
  if (sizeof(long) == 4) {
    convert_32_t val;
    val.u32 = stm_load32((volatile uint32_t *)addr);
    return val.s32;
  } else {
    convert_64_t val;
    val.u64 = stm_load64((volatile uint64_t *)addr);
    return val.s64;
  }
}

unsigned long stm_load_ulong(volatile unsigned long *addr)
{
  if (sizeof(long) == 4) {
    return (unsigned long)stm_load32((volatile uint32_t *)addr);
  } else {
    return (unsigned long)stm_load64((volatile uint64_t *)addr);
  }
}

float stm_load_float(volatile float *addr)
{
  convert_32_t val;
  val.u32 = stm_load32((volatile uint32_t *)addr);
  return val.f;
}

double stm_load_double(volatile double *addr)
{
  convert_64_t val;
  val.u64 = stm_load64((volatile uint64_t *)addr);
  return val.d;
}

/* ################################################################### *
 * STORES
 * ################################################################### */

void stm_store8(volatile uint8_t *addr, uint8_t value)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val, mask;
    val.u8[(uintptr_t)addr & 0x03] = value;
    mask.u32 = 0;
    mask.u8[(uintptr_t)addr & 0x03] = ~(uint8_t)0;
    stm_store2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03), (stm_word_t)val.u32, (stm_word_t)mask.u32);
  } else {
    convert_64_t val, mask;
    val.u8[(uintptr_t)addr & 0x07] = value;
    mask.u64 = 0;
    mask.u8[(uintptr_t)addr & 0x07] = ~(uint8_t)0;
    stm_store2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

void stm_store16(volatile uint16_t *addr, uint16_t value)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val, mask;
    val.u16[((uintptr_t)addr & 0x03) >> 1] = value;
    mask.u32 = 0;
    mask.u16[((uintptr_t)addr & 0x03) >> 1] = ~(uint16_t)0;
    stm_store2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03), (stm_word_t)val.u32, (stm_word_t)mask.u32);
  } else {
    convert_64_t val, mask;
    val.u16[((uintptr_t)addr & 0x07) >> 1] = value;
    mask.u64 = 0;
    mask.u16[((uintptr_t)addr & 0x07) >> 1] = ~(uint16_t)0;
    stm_store2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

void stm_store32(volatile uint32_t *addr, uint32_t value)
{
  if (sizeof(stm_word_t) == 4) {
    stm_store((volatile stm_word_t *)addr, (stm_word_t)value);
  } else {
    convert_64_t val, mask;
    val.u32[((uintptr_t)addr & 0x07) >> 2] = value;
    mask.u64 = 0;
    mask.u32[((uintptr_t)addr & 0x07) >> 2] = ~(uint32_t)0;
    stm_store2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

void stm_store64(volatile uint64_t *addr, uint64_t value)
{
  if (sizeof(stm_word_t) == 4) {
    convert_64_t val;
    val.u64 = value;
    stm_store((volatile stm_word_t *)addr, (stm_word_t)val.u32[0]);
    stm_store((volatile stm_word_t *)addr + 1, (stm_word_t)val.u32[1]);
  } else {
    return stm_store((volatile stm_word_t *)addr, (stm_word_t)value);
  }
}

void stm_store_char(volatile char *addr, char value)
{
  convert_8_t val;
  val.s8 = value;
  stm_store8((volatile uint8_t *)addr, val.u8);
}

void stm_store_uchar(volatile unsigned char *addr, unsigned char value)
{
  stm_store8((volatile uint8_t *)addr, (uint8_t)value);
}

void stm_store_short(volatile short *addr, short value)
{
  convert_16_t val;
  val.s16 = value;
  stm_store16((volatile uint16_t *)addr, val.u16);
}

void stm_store_ushort(volatile unsigned short *addr, unsigned short value)
{
  stm_store16((volatile uint16_t *)addr, (uint16_t)value);
}

void stm_store_int(volatile int *addr, int value)
{
  convert_32_t val;
  val.s32 = value;
  stm_store32((volatile uint32_t *)addr, val.u32);
}

void stm_store_uint(volatile unsigned int *addr, unsigned int value)
{
  stm_store32((volatile uint32_t *)addr, (uint32_t)value);
}

void stm_store_long(volatile long *addr, long value)
{
  if (sizeof(long) == 4) {
    convert_32_t val;
    val.s32 = value;
    stm_store32((volatile uint32_t *)addr, val.u32);
  } else {
    convert_64_t val;
    val.s64 = value;
    stm_store64((volatile uint64_t *)addr, val.u64);
  }
}

void stm_store_ulong(volatile unsigned long *addr, unsigned long value)
{
  if (sizeof(long) == 4) {
    stm_store32((volatile uint32_t *)addr, (uint32_t)value);
  } else {
    stm_store64((volatile uint64_t *)addr, (uint64_t)value);
  }
}

void stm_store_float(volatile float *addr, float value)
{
  convert_32_t val;
  val.f = value;
  stm_store32((volatile uint32_t *)addr, val.u32);
}

void stm_store_double(volatile double *addr, double value)
{
  convert_64_t val;
  val.d = value;
  stm_store64((volatile uint64_t *)addr, val.u64);
}
