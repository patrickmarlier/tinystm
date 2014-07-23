/*
 * File:
 *   types.c
 * Author(s):
 *   Pascal Felber <Pascal.Felber@unine.ch>
 * Description:
 *   Regression test for various data types.
 *
 * Copyright (c) 2007.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "tinySTM.h"
#include "wrappers.h"

union {
  uint8_t u8[256];
  uint16_t u16[128];
  uint32_t u32[64];
  uint64_t u64[32];
  int8_t s8[256];
  int16_t s16[128];
  int32_t s32[64];
  int64_t s64[32];
  float f[64];
  double d[32];
} tab, tab_ro;

typedef union {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int8_t s8;
  int16_t s16;
  int32_t s32;
  int64_t s64;
  float f;
  double d;
} val_t;

enum {
  TYPE_UINT8,
  TYPE_UINT16,
  TYPE_UINT32,
  TYPE_UINT64,
  TYPE_CHAR,
  TYPE_UCHAR,
  TYPE_SHORT,
  TYPE_USHORT,
  TYPE_INT,
  TYPE_UINT,
  TYPE_LONG,
  TYPE_ULONG,
  TYPE_FLOAT,
  TYPE_DOUBLE
};

#define NB_THREADS                      4
#define DURATION                        5000

volatile int verbose;
volatile int stop;

void compare(stm_tx_t *tx, int idx, val_t val, int type)
{
  int i;
  val_t v;

  switch(type) {
   case TYPE_UINT8:
     for (i = 0; i < 256 / sizeof(uint8_t); i++) {
       v.u8 = stm_load8(tx, &tab.u8[i]);
       assert(i == idx ? v.u8 == val.u8 : v.u8 == tab_ro.u8[i]);
     }
     break;
   case TYPE_UINT16:
     for (i = 0; i < 256 / sizeof(uint16_t); i++) {
       v.u16 = stm_load16(tx, &tab.u16[i]);
       assert(i == idx ? v.u16 == val.u16 : v.u16 == tab_ro.u16[i]);
     }
     break;
   case TYPE_UINT32:
     for (i = 0; i < 256 / sizeof(uint32_t); i++) {
       v.u32 = stm_load32(tx, &tab.u32[i]);
       assert(i == idx ? v.u32 == val.u32 : v.u32 == tab_ro.u32[i]);
     }
     break;
   case TYPE_UINT64:
     for (i = 0; i < 256 / sizeof(uint64_t); i++) {
       v.u64 = stm_load64(tx, &tab.u64[i]);
       assert(i == idx ? v.u64 == val.u64 : v.u64 == tab_ro.u64[i]);
     }
     break;
   case TYPE_CHAR:
     for (i = 0; i < 256 / sizeof(unsigned char); i++) {
       v.s8 = (int8_t)stm_load_char(tx, (char *)&tab.s8[i]);
       assert(i == idx ? v.s8 == val.s8 : v.s8 == tab_ro.s8[i]);
     }
     break;
   case TYPE_UCHAR:
     for (i = 0; i < 256 / sizeof(char); i++) {
       v.u8 = (uint8_t)stm_load_uchar(tx, (unsigned char *)&tab.u8[i]);
       assert(i == idx ? v.u8 == val.u8 : v.u8 == tab_ro.u8[i]);
     }
     break;
   case TYPE_SHORT:
     for (i = 0; i < 256 / sizeof(short); i++) {
       v.s16 = (int16_t)stm_load_short(tx, (short *)&tab.s16[i]);
       assert(i == idx ? v.s16 == val.s16 : v.s16 == tab_ro.s16[i]);
     }
     break;
   case TYPE_USHORT:
     for (i = 0; i < 256 / sizeof(unsigned short); i++) {
       v.u16 = (uint16_t)stm_load_ushort(tx, (unsigned short *)&tab.u16[i]);
       assert(i == idx ? v.u16 == val.u16 : v.u16 == tab_ro.u16[i]);
     }
     break;
   case TYPE_INT:
     for (i = 0; i < 256 / sizeof(int); i++) {
       v.s32 = (int32_t)stm_load_int(tx, (int *)&tab.s32[i]);
       assert(i == idx ? v.s32 == val.s32 : v.s32 == tab_ro.s32[i]);
     }
     break;
   case TYPE_UINT:
     for (i = 0; i < 256 / sizeof(unsigned int); i++) {
       v.u32 = (uint32_t)stm_load_uint(tx, (unsigned int *)&tab.u32[i]);
       assert(i == idx ? v.u32 == val.u32 : v.u32 == tab_ro.u32[i]);
     }
     break;
   case TYPE_LONG:
     for (i = 0; i < 256 / sizeof(long); i++) {
       if (sizeof(long) == 4) {
         v.s32 = (int32_t)stm_load_long(tx, (long *)&tab.s32[i]);
         assert(i == idx ? v.s32 == val.s32 : v.s32 == tab_ro.s32[i]);
       } else {
         v.s64 = (int64_t)stm_load_long(tx, (long *)&tab.s64[i]);
         assert(i == idx ? v.s64 == val.s64 : v.s64 == tab_ro.s64[i]);
       }
     }
     break;
   case TYPE_ULONG:
     for (i = 0; i < 256 / sizeof(unsigned long); i++) {
       if (sizeof(long) == 4) {
         v.u32 = (uint32_t)stm_load_ulong(tx, (unsigned long *)&tab.u32[i]);
         assert(i == idx ? v.u32 == val.u32 : v.u32 == tab_ro.u32[i]);
       } else {
         v.u64 = (uint64_t)stm_load_ulong(tx, (unsigned long *)&tab.u64[i]);
         assert(i == idx ? v.u64 == val.u64 : v.u64 == tab_ro.u64[i]);
       }
     }
     break;
   case TYPE_FLOAT:
     for (i = 0; i < 256 / sizeof(float); i++) {
       v.f = stm_load_float(tx, &tab.f[i]);
       assert(i == idx ? (isnan(v.f) && isnan(val.f)) || v.f == val.f : (isnan(v.f) && isnan(tab_ro.f[i])) || v.f == tab_ro.f[i]);
     }
     break;
   case TYPE_DOUBLE:
     for (i = 0; i < 256 / sizeof(double); i++) {
       v.d = stm_load_double(tx, &tab.d[i]);
       assert(i == idx ? (isnan(v.d) && isnan(val.d)) || v.d == val.d : (isnan(v.d) && isnan(tab_ro.d[i])) || v.d == tab_ro.d[i]);
     }
     break;
  }
}

void test_loads(stm_tx_t *tx)
{
  int i;
  val_t val;
  sigjmp_buf *e;

  e = stm_get_env(tx);
  if (e != NULL)
    sigsetjmp(*e, 0);
  stm_start(tx, e, NULL);

  if (verbose)
    printf("- Testing uint8_t\n");
  for (i = 0; i < 256 / sizeof(uint8_t); i++) {
    val.u8 = stm_load8(tx, &tab.u8[i]);
    assert(val.u8 == tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing uint16_t\n");
  for (i = 0; i < 256 / sizeof(uint16_t); i++) {
    val.u16 = stm_load16(tx, &tab.u16[i]);
    assert(val.u16 == tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing uint32_t\n");
  for (i = 0; i < 256 / sizeof(uint32_t); i++) {
    val.u32 = stm_load32(tx, &tab.u32[i]);
    assert(val.u32 == tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing uint64_t\n");
  for (i = 0; i < 256 / sizeof(uint64_t); i++) {
    val.u64 = stm_load64(tx, &tab.u64[i]);
    assert(val.u64 == tab_ro.u64[i]);
  }
  if (verbose)
    printf("- Testing char\n");
  for (i = 0; i < 256 / sizeof(char); i++) {
    val.s8 = (int8_t)stm_load_char(tx, (volatile char *)&tab.s8[i]);
    assert(val.s8 == tab_ro.s8[i]);
  }
  if (verbose)
    printf("- Testing unsigned char\n");
  for (i = 0; i < 256 / sizeof(unsigned char); i++) {
    val.u8 = (uint8_t)stm_load_uchar(tx, (volatile unsigned char *)&tab.u8[i]);
    assert(val.u8 == tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing short\n");
  for (i = 0; i < 256 / sizeof(short); i++) {
    val.s16 = (int16_t)stm_load_short(tx, (volatile short *)&tab.s16[i]);
    assert(val.s16 == tab_ro.s16[i]);
  }
  if (verbose)
    printf("- Testing unsigned short\n");
  for (i = 0; i < 256 / sizeof(unsigned short); i++) {
    val.u16 = (uint16_t)stm_load_ushort(tx, (volatile unsigned short *)&tab.u16[i]);
    assert(val.u16 == tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing int\n");
  for (i = 0; i < 256 / sizeof(int); i++) {
    val.s32 = (int32_t)stm_load_int(tx, (volatile int *)&tab.s32[i]);
    assert(val.s32 == tab_ro.s32[i]);
  }
  if (verbose)
    printf("- Testing unsigned int\n");
  for (i = 0; i < 256 / sizeof(unsigned int); i++) {
    val.u32 = (uint32_t)stm_load_uint(tx, (volatile unsigned int *)&tab.u32[i]);
    assert(val.u32 == tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing long\n");
  for (i = 0; i < 256 / sizeof(long); i++) {
    if (sizeof(long) == 4) {
      val.s32 = (int32_t)stm_load_long(tx, (volatile long *)&tab.s32[i]);
      assert(val.s32 == tab_ro.s32[i]);
    } else {
      val.s64 = (int64_t)stm_load_long(tx, (volatile long *)&tab.s64[i]);
      assert(val.s64 == tab_ro.s64[i]);
    }
  }
  if (verbose)
    printf("- Testing unsigned long\n");
  for (i = 0; i < 256 / sizeof(unsigned long); i++) {
    if (sizeof(long) == 4) {
      val.u32 = (uint32_t)stm_load_ulong(tx, (volatile unsigned long *)&tab.u32[i]);
      assert(val.u32 == tab_ro.u32[i]);
    } else {
      val.u64 = (uint64_t)stm_load_ulong(tx, (volatile unsigned long *)&tab.u64[i]);
      assert(val.u64 == tab_ro.u64[i]);
    }
  }
  if (verbose)
    printf("- Testing float\n");
  for (i = 0; i < 256 / sizeof(float); i++) {
    val.f = stm_load_float(tx, &tab.f[i]);
    assert((isnan(val.f) && isnan(tab_ro.f[i])) || val.f == tab_ro.f[i]);
  }
  if (verbose)
    printf("- Testing double\n");
  for (i = 0; i < 256 / sizeof(double); i++) {
    val.d = stm_load_double(tx, &tab.d[i]);
    assert((isnan(val.d) && isnan(tab_ro.d[i])) || val.d == tab_ro.d[i]);
  }

  stm_commit(tx);
}

void test_stores(stm_tx_t *tx)
{
  int i;
  val_t val;
  sigjmp_buf *e;

  e = stm_get_env(tx);
  if (e != NULL)
    sigsetjmp(*e, 0);
  stm_start(tx, e, NULL);

  if (verbose)
    printf("- Testing uint8_t\n");
  for (i = 0; i < 256; i++) {
    val.u8 = ~tab_ro.u8[i];
    stm_store8(tx, &tab.u8[i], val.u8);
    compare(tx, i, val, TYPE_UINT8);
    stm_store8(tx, &tab.u8[i], tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing uint16_t\n");
  for (i = 0; i < 256 / sizeof(uint16_t); i++) {
    val.u16 = ~tab_ro.u16[i];
    stm_store16(tx, &tab.u16[i], val.u16);
    compare(tx, i, val, TYPE_UINT16);
    stm_store16(tx, &tab.u16[i], tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing uint32_t\n");
  for (i = 0; i < 256 / sizeof(uint32_t); i++) {
    val.u32 = ~tab_ro.u32[i];
    stm_store32(tx, &tab.u32[i], val.u32);
    compare(tx, i, val, TYPE_UINT32);
    stm_store32(tx, &tab.u32[i], tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing uint64_t\n");
  for (i = 0; i < 256 / sizeof(uint64_t); i++) {
    val.u64 = ~tab_ro.u64[i];
    stm_store64(tx, &tab.u64[i], val.u64);
    compare(tx, i, val, TYPE_UINT64);
    stm_store64(tx, &tab.u64[i], tab_ro.u64[i]);
  }
  if (verbose)
    printf("- Testing char\n");
  for (i = 0; i < 256 / sizeof(char); i++) {
    val.s8 = ~tab_ro.s8[i];
    stm_store_char(tx, (volatile char *)&tab.s8[i], (char)val.s8);
    compare(tx, i, val, TYPE_CHAR);
    stm_store_char(tx, (volatile char *)&tab.s8[i], (char)tab_ro.s8[i]);
  }
  if (verbose)
    printf("- Testing unsigned char\n");
  for (i = 0; i < 256 / sizeof(unsigned char); i++) {
    val.u8 = ~tab_ro.u8[i];
    stm_store_uchar(tx, (volatile unsigned char *)&tab.u8[i], (unsigned char)val.u8);
    compare(tx, i, val, TYPE_UCHAR);
    stm_store_uchar(tx, (volatile unsigned char *)&tab.u8[i], (unsigned char)tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing short\n");
  for (i = 0; i < 256 / sizeof(short); i++) {
    val.s16 = ~tab_ro.s16[i];
    stm_store_short(tx, (volatile short *)&tab.s16[i], (short)val.s16);
    compare(tx, i, val, TYPE_SHORT);
    stm_store_short(tx, (volatile short *)&tab.s16[i], (short)tab_ro.s16[i]);
  }
  if (verbose)
    printf("- Testing unsigned short\n");
  for (i = 0; i < 256 / sizeof(unsigned short); i++) {
    val.u16 = ~tab_ro.u16[i];
    stm_store_ushort(tx, (volatile unsigned short *)&tab.u16[i], (unsigned short)val.u16);
    compare(tx, i, val, TYPE_USHORT);
    stm_store_ushort(tx, (volatile unsigned short *)&tab.u16[i], (unsigned short)tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing int\n");
  for (i = 0; i < 256 / sizeof(int); i++) {
    val.s32 = ~tab_ro.s32[i];
    stm_store_int(tx, (volatile int *)&tab.s32[i], (int)val.s32);
    compare(tx, i, val, TYPE_INT);
    stm_store_int(tx, (volatile int *)&tab.s32[i], (int)tab_ro.s32[i]);
  }
  if (verbose)
    printf("- Testing unsigned int\n");
  for (i = 0; i < 256 / sizeof(unsigned int); i++) {
    val.u32 = ~tab_ro.u32[i];
    stm_store_uint(tx, (volatile unsigned int *)&tab.u32[i], (unsigned int)val.u32);
    compare(tx, i, val, TYPE_UINT);
    stm_store_uint(tx, (volatile unsigned int *)&tab.u32[i], (unsigned int)tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing long\n");
  for (i = 0; i < 256 / sizeof(long); i++) {
    if (sizeof(long) == 4) {
      val.s32 = ~tab_ro.s32[i];
      stm_store_long(tx, (volatile long *)&tab.s32[i], (long)val.s32);
      compare(tx, i, val, TYPE_LONG);
      stm_store_long(tx, (volatile long *)&tab.s32[i], (long)tab_ro.s32[i]);
    } else {
      val.s64 = ~tab_ro.s64[i];
      stm_store_long(tx, (volatile long *)&tab.s64[i], (long)val.s64);
      compare(tx, i, val, TYPE_LONG);
      stm_store_long(tx, (volatile long *)&tab.s64[i], (long)tab_ro.s64[i]);
    }
  }
  if (verbose)
    printf("- Testing unsigned long\n");
  for (i = 0; i < 256 / sizeof(unsigned long); i++) {
    if (sizeof(long) == 4) {
      val.u32 = ~tab_ro.u32[i];
      stm_store_ulong(tx, (volatile unsigned long *)&tab.u32[i], (unsigned long)val.u32);
      compare(tx, i, val, TYPE_ULONG);
      stm_store_ulong(tx, (volatile unsigned long *)&tab.u32[i], (unsigned long)tab_ro.u32[i]);
    } else {
      val.s64 = ~tab_ro.s64[i];
      stm_store_long(tx, (volatile long *)&tab.s64[i], (long)val.s64);
      compare(tx, i, val, TYPE_LONG);
      stm_store_long(tx, (volatile long *)&tab.s64[i], (long)tab_ro.s64[i]);
    }
  }
  if (verbose)
    printf("- Testing float\n");
  for (i = 0; i < 256 / sizeof(float); i++) {
    val.u32 = ~tab_ro.u32[i];
    stm_store_float(tx, &tab.f[i], val.f);
    compare(tx, i, val, TYPE_FLOAT);
    stm_store_float(tx, &tab.f[i], tab_ro.f[i]);
  }
  if (verbose)
    printf("- Testing double\n");
  for (i = 0; i < 256 / sizeof(double); i++) {
    val.u64 = ~tab_ro.u64[i];
    stm_store_double(tx, &tab.d[i], val.d);
    compare(tx, i, val, TYPE_DOUBLE);
    stm_store_double(tx, &tab.d[i], tab_ro.d[i]);
  }

  stm_commit(tx);
}

void *test(void *v)
{
  stm_tx_t *tx;
  unsigned int seed;
  int nested, store;
  sigjmp_buf *e;

  seed = (unsigned int)time(0);
  tx = stm_new(NULL);
  while (stop == 0) {
    nested = (rand_r(&seed) % 3 == 0);
    store = (rand_r(&seed) % 3 == 0);
    if (nested) {
      e = stm_get_env(tx);
      sigsetjmp(*e, 0);
      stm_start(tx, e, NULL);
    }
    if (store)
      test_stores(tx);
    else
      test_loads(tx);
    if (nested) {
      stm_commit(tx);
    }
  }
  stm_delete(tx);

  return NULL;
}

int main(int argc, char **argv)
{
  int i;
  stm_tx_t *tx;
  pthread_t *threads;
  pthread_attr_t attr;
  struct timespec timeout;

  for (i = 0; i < 256; i++)
    tab_ro.u8[i] = tab.u8[i] = i;

  /* Init STM */
  printf("Initializing STM\n");
  stm_init(0);

  printf("int/long/ptr/word size: %d/%d/%d/%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(stm_word_t));

  verbose = 1;
  stop = 0;

  tx = stm_new(NULL);

  printf("TESTING LOADS...\n");
  test_loads(tx);
  printf("PASSED\n");

  printf("TESTING STORES...\n");
  test_stores(tx);
  printf("PASSED\n");

  stm_delete(tx);

  printf("TESTING CONCURRENT LOADS AND STORES...\n");
  verbose = 0;
  timeout.tv_sec = DURATION / 1000;
  timeout.tv_nsec = (DURATION % 1000) * 1000000;
  if ((threads = (pthread_t *)malloc(NB_THREADS * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < NB_THREADS; i++) {
    if (pthread_create(&threads[i], &attr, test, NULL) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);
  nanosleep(&timeout, NULL);
  printf("STOPPING...\n");
  stop = 1;
  for (i = 0; i < NB_THREADS; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }
  printf("PASSED\n");

  /* Cleanup STM */
  stm_exit(0);

  return 0;
}
