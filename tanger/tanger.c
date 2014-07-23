/*
 * File:
 *   tanger.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Tanger adapter for tinySTM.
 *
 * Copyright (c) 2007-2009.
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

#include <assert.h>
#include <string.h>

#include <pthread.h>

#include <tanger-stm-internal.h>

#include "stm.h"
#include "wrappers.h"
#include "mod_mem.h"

typedef struct stack_area {
  uintptr_t low;
  uintptr_t high;
  void *data;
  size_t size;
} stack_area_t;

#ifdef TLS
static __thread stack_area_t* stack;
#else /* ! TLS */
static pthread_key_t stack;
#endif /* ! TLS */

#ifdef TANGER_STATS
typedef struct stats {
  unsigned long nb_load1;
  unsigned long nb_load8;
  unsigned long nb_load16;
  unsigned long nb_load32;
  unsigned long nb_load64;
  unsigned long nb_load16aligned;
  unsigned long nb_load32aligned;
  unsigned long nb_load64aligned;
  unsigned long nb_loadregion;
  unsigned long nb_store1;
  unsigned long nb_store8;
  unsigned long nb_store16;
  unsigned long nb_store32;
  unsigned long nb_store64;
  unsigned long nb_store16aligned;
  unsigned long nb_store32aligned;
  unsigned long nb_store64aligned;
  unsigned long nb_storeregion;
  unsigned long nb_save_stack;
  unsigned long nb_restore_stack;
} stats_t;
# ifdef TLS
static __thread stats_t* stats;
# else /* ! TLS */
static pthread_key_t stats;
# endif /* ! TLS */
# define INC_STATS(x)                   get_stats()->x++;
#else /* ! TANGER_STATS */
# define INC_STATS(x)                   /* Nothing */
#endif /* ! TANGER_STATS */

/* ################################################################### *
 * COMPATIBILITY FUNCTIONS
 * ################################################################### */

#if defined(__APPLE__)
/* OS X */
# include <malloc/malloc.h>
inline size_t block_size(void *ptr)
{
  return malloc_size(ptr);
}
#elif defined(__linux__) || defined(__CYGWIN__)
/* Linux, WIN32 (CYGWIN) */
# include <malloc.h>
inline size_t block_size(void *ptr)
{
  return malloc_usable_size(ptr);
}
#else /* ! (defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)) */
# error "Target OS does not provide size of allocated blocks"
#endif /* ! (defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)) */

/* ################################################################### *
 * STATIC
 * ################################################################### */

static inline stack_area_t *get_stack()
{
#ifdef TLS
  return stack;
#else /* ! TLS */
  return (stack_area_t *)pthread_getspecific(stack);
#endif /* ! TLS */
}

#ifdef TANGER_STATS
static inline stats_t *get_stats()
{
#ifdef TLS
  return stats;
#else /* ! TLS */
  return (stats_t *)pthread_getspecific(stats);
#endif /* ! TLS */
}
#endif /* TANGER_STATS */

#ifndef NO_STACK_CHECK
/* Ensure function call creates a new stack frame */
static uintptr_t get_sp() __attribute__ ((noinline));
static uintptr_t get_sp()
{
# if defined(__GNUC__)
  /* Use current frame pointer (sufficient approximation) */
  return (uintptr_t)__builtin_frame_address(0);
# else /* ! defined(__GNUC__) */
  uintptr_t i;
  /* Use variable on stack (sufficient approximation) */
  return (uintptr_t)&i;
# endif /* ! defined(__GNUC__) */
}

static inline int on_stack(void *a)
{
  uintptr_t sp = get_sp();
  uintptr_t h = get_stack()->high;

  return (sp > h ? sp >= (uintptr_t)a && (uintptr_t)a >= h : sp <= (uintptr_t)a && (uintptr_t)a <= h);
}
#endif /* ! NO_STACK_CHECK */

/* ################################################################### *
 * TANGER FUNCTIONS
 * ################################################################### */

uint8_t tanger_stm_load1(tanger_stm_tx_t *tx, uint8_t *addr)
{
  INC_STATS(nb_load1);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load8((struct stm_tx *)tx, addr);
}

uint8_t tanger_stm_load8(tanger_stm_tx_t *tx, uint8_t *addr)
{
  INC_STATS(nb_load8);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load8((struct stm_tx *)tx, addr);
}

uint16_t tanger_stm_load16(tanger_stm_tx_t *tx, uint16_t *addr)
{
  INC_STATS(nb_load16);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load16((struct stm_tx *)tx, addr);
}

uint32_t tanger_stm_load32(tanger_stm_tx_t *tx, uint32_t *addr)
{
  INC_STATS(nb_load32);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load32((struct stm_tx *)tx, addr);
}

uint64_t tanger_stm_load64(tanger_stm_tx_t *tx, uint64_t *addr)
{
  INC_STATS(nb_load64);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load64((struct stm_tx *)tx, addr);
}

uint16_t tanger_stm_load16aligned(tanger_stm_tx_t *tx, uint16_t *addr)
{
  INC_STATS(nb_load16aligned);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load16((struct stm_tx *)tx, addr);
}

uint32_t tanger_stm_load32aligned(tanger_stm_tx_t *tx, uint32_t *addr)
{
  INC_STATS(nb_load32aligned);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  if (sizeof(stm_word_t) == 4)
    return (uint32_t)stm_load((struct stm_tx *)tx, (volatile stm_word_t *)addr);
  return stm_load32((struct stm_tx *)tx, addr);
}

uint64_t tanger_stm_load64aligned(tanger_stm_tx_t *tx, uint64_t *addr)
{
  INC_STATS(nb_load64aligned);
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  if (sizeof(stm_word_t) == 8)
    return (uint64_t)stm_load((struct stm_tx *)tx, (volatile stm_word_t *)addr);
  return stm_load64((struct stm_tx *)tx, addr);
}

void tanger_stm_loadregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest)
{
  INC_STATS(nb_loadregion);
#ifndef NO_STACK_CHECK
  if (on_stack(src))
    memcpy(dest, src, bytes);
#endif /* ! NO_STACK_CHECK */
  stm_load_bytes((struct stm_tx *)tx, src, dest, bytes);
}

void* tanger_stm_loadregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

void tanger_stm_loadregionpost(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

void tanger_stm_store1(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
  INC_STATS(nb_store1);
  stm_store8((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store8(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
  INC_STATS(nb_store8);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store8((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store16(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
  INC_STATS(nb_store16);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store16((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store32(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
  INC_STATS(nb_store32);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store32((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store64(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
  INC_STATS(nb_store64);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store64((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store16aligned(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
  INC_STATS(nb_store16aligned);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store16((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store32aligned(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
  INC_STATS(nb_store32aligned);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  if (sizeof(stm_word_t) == 4)
    stm_store((struct stm_tx *)tx, (volatile stm_word_t *)addr, (stm_word_t)value);
  stm_store32((struct stm_tx *)tx, addr, value);
}

void tanger_stm_store64aligned(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
  INC_STATS(nb_store64aligned);
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  if (sizeof(stm_word_t) == 8)
    stm_store((struct stm_tx *)tx, (volatile stm_word_t *)addr, (stm_word_t)value);
  stm_store64((struct stm_tx *)tx, addr, value);
}

void tanger_stm_storeregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest)
{
  INC_STATS(nb_storeregion);
#ifndef NO_STACK_CHECK
  if (on_stack(dest)) {
    memcpy(dest, src, bytes);
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_bytes((struct stm_tx *)tx, src, dest, bytes);
}

void* tanger_stm_storeregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

void* tanger_stm_updateregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

void tanger_stm_begin(tanger_stm_tx_t *tx)
{
  sigjmp_buf *env;

  env = stm_get_env((struct stm_tx *)tx);
  stm_start((struct stm_tx *)tx, env, NULL);
}

void tanger_stm_commit(tanger_stm_tx_t *tx)
{
  stm_commit((struct stm_tx *)tx);
  get_stack()->low = 0;
}

tanger_stm_tx_t *tanger_stm_get_tx()
{
  return (tanger_stm_tx_t *)stm_current_tx();
}

void *tanger_stm_get_jmpbuf(tanger_stm_tx_t *tx)
{
  sigjmp_buf *env;

  env = stm_get_env((struct stm_tx *)tx);
  if (env == NULL) {
    fprintf(stderr, "Nested transactions are not supported");
    exit(1);
  }
  return env;
}

void tanger_stm_save_restore_stack(void* low_addr, void* high_addr) __attribute__ ((noinline));
void tanger_stm_save_restore_stack(void* low_addr, void* high_addr)
{
  uintptr_t size;
  stack_area_t *area = get_stack();

  if (area->low) {
    /* Restore stack area */
    INC_STATS(nb_restore_stack);
    size = area->high - area->low;
    memcpy((void *)area->low, area->data, size);
  } else {
    /* Save stack area */
    INC_STATS(nb_save_stack);
    area->high = (uintptr_t)high_addr;
    area->low = (uintptr_t)low_addr;
    size = area->high - area->low;
    if (area->size < size) {
      /* Allocate twice the necessary size, and at least 1KB */
      area->size = (size < 1024/2 ? 1024 : size * 2);
      area->data = realloc(area->data, area->size);
    }
    memcpy(area->data, (void *)area->low, size);
  }
}

void tanger_stm_init()
{
#ifndef TLS
  if (pthread_key_create(&stack, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
# ifdef TANGER_STATS
  if (pthread_key_create(&stats, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
# endif /* TANGER_STATS */
#endif /* ! TLS */
  stm_init();
  mod_mem_init();
}

void tanger_stm_shutdown()
{
  stm_exit();
#ifndef TLS
  pthread_key_delete(stack);
# ifdef TANGER_STATS
  pthread_key_delete(stats);
# endif /* TANGER_STATS */
#endif /* ! TLS */
}

void tanger_stm_thread_init()
{
  stack_area_t *area;
#ifdef TANGER_STATS
  stats_t *s;
#endif /* TANGER_STATS */

  if ((area = (stack_area_t *)malloc(sizeof(stack_area_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  area->low = area->high = 0;
  area->data = NULL;
  area->size = 0;
#ifdef TLS
  stack = area;
#else /* ! TLS */
  pthread_setspecific(stack, area);
#endif /* ! TLS */
#ifdef TANGER_STATS
  if ((s = (stats_t *)malloc(sizeof(stats_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  s->nb_load1 = 0;
  s->nb_load8 = 0;
  s->nb_load16 = 0;
  s->nb_load32 = 0;
  s->nb_load64 = 0;
  s->nb_load16aligned = 0;
  s->nb_load32aligned = 0;
  s->nb_load64aligned = 0;
  s->nb_loadregion = 0;
  s->nb_store1 = 0;
  s->nb_store8 = 0;
  s->nb_store16 = 0;
  s->nb_store32 = 0;
  s->nb_store64 = 0;
  s->nb_store16aligned = 0;
  s->nb_store32aligned = 0;
  s->nb_store64aligned = 0;
  s->nb_storeregion = 0;
  s->nb_save_stack = 0;
  s->nb_restore_stack = 0;
# ifdef TLS
  stats = s;
# else /* ! TLS */
  pthread_setspecific(stats, s);
# endif /* ! TLS */
#endif /* TANGER_STATS */
  stm_init_thread();
}

void tanger_stm_thread_shutdown()
{
  stack_area_t *area = get_stack();
#ifdef TANGER_STATS
  stats_t *s = get_stats();
#endif /* TANGER_STATS */

  stm_exit_thread(stm_current_tx());
  free(area->data);
  free(area);
#ifdef TANGER_STATS
  /* Use a single printf call (thread-safe) */
  printf("Thread stats\n"
         "  #load1             : %lu\n"
         "  #load8             : %lu\n"
         "  #load16            : %lu\n"
         "  #load32            : %lu\n"
         "  #load64            : %lu\n"
         "  #load16 (aligned)  : %lu\n"
         "  #load32 (aligned)  : %lu\n"
         "  #load64 (aligned)  : %lu\n"
         "  #load (region)     : %lu\n"
         "  #store1            : %lu\n"
         "  #store8            : %lu\n"
         "  #store16           : %lu\n"
         "  #store32           : %lu\n"
         "  #store64           : %lu\n"
         "  #store16 (aligned) : %lu\n"
         "  #store32 (aligned) : %lu\n"
         "  #store64 (aligned) : %lu\n"
         "  #store (region)    : %lu\n"
         "  #save stack        : %lu\n"
         "  #restore stack     : %lu\n",
         s->nb_load1, s->nb_load8, s->nb_load16, s->nb_load32, s->nb_load64,
         s->nb_load16aligned, s->nb_load32aligned, s->nb_load64aligned, s->nb_loadregion,
         s->nb_store1, s->nb_store8, s->nb_store16, s->nb_store32, s->nb_store64,
         s->nb_store16aligned, s->nb_store32aligned, s->nb_store64aligned, s->nb_storeregion,
         s->nb_save_stack, s->nb_restore_stack);
  free(s);
#endif /* TANGER_STATS */
}

void *tanger_stm_malloc(size_t size, tanger_stm_tx_t* tx)
{
  return stm_malloc((struct stm_tx *)tx, size);
}

void tanger_stm_free(void *ptr, tanger_stm_tx_t* tx)
{
#ifdef NO_WRITE_ON_FREE
  stm_free(tx, ptr, 0);
#else
  stm_free(tx, ptr, block_size(ptr));
#endif
}

void *tanger_stm_calloc(size_t nmemb, size_t size, tanger_stm_tx_t* tx)
{
  void *p = tanger_stm_malloc(nmemb * size, tx);
  memset(p, 0, nmemb * size);
  return p;
}

void *tanger_stm_realloc(void *ptr, size_t size, tanger_stm_tx_t* tx)
{
  void *p;

  if (ptr == NULL) {
    /* Equivalent to malloc */
    return tanger_stm_malloc(size, tx);
  }
  if (size == 0) {
    /* Equivalent to free */
    tanger_stm_free(ptr, tx);
    return NULL;
  }
  /* Allocate new region */
  p = tanger_stm_malloc(size, tx);
  /* Copy old content to new region */
  stm_load_bytes((struct stm_tx *)tx, ptr, p, size);
  /* Free old region */
  tanger_stm_free(ptr, tx);

  return p;
}
