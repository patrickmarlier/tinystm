/*
 * File:
 *   tanger.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Tanger adapter for tinySTM.
 *
 * Copyright (c) 2007-2010.
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

/* This file is designed to work with DTMC (Tanger/LLVM).
 * DTMC is not 100% compatible with Intel ABI yet thus this file 
 * permits to propose a workaround.
 * */

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <bits/wordsize.h>

/* A transaction descriptor/handle/... */
//typedef void tanger_stm_tx_t;

#ifndef TANGER_LOADSTORE_ATTR
# define TANGER_LOADSTORE_ATTR __attribute__((nothrow,always_inline))
#endif /* TANGER_LOADSTORE_ATTR */

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
  stack_area_t *stack = get_stack();
  assert(stack);

  uintptr_t sp = get_sp();
  /* FIXME add define to know if stack increase or decrease (arch dependant) */
  return sp <= (uintptr_t)a && (uintptr_t)a <= stack->high;
}
#endif /* ! NO_STACK_CHECK */

/* ################################################################### *
 * TANGER FUNCTIONS
 * ################################################################### */

#ifdef EXPLICIT_TX_PARAMETER
# define TX_PARAM (struct stm_tx *)tx,
#else
# define TX_PARAM
#endif

TANGER_LOADSTORE_ATTR
uint8_t tanger_stm_load1(tanger_stm_tx_t *tx, uint8_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load_u8(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint8_t tanger_stm_load8(tanger_stm_tx_t *tx, uint8_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load_u8(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint16_t tanger_stm_load16(tanger_stm_tx_t *tx, uint16_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load_u16(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint32_t tanger_stm_load32(tanger_stm_tx_t *tx, uint32_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load_u32(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint64_t tanger_stm_load64(tanger_stm_tx_t *tx, uint64_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load_u64(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint16_t tanger_stm_load16aligned(tanger_stm_tx_t *tx, uint16_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
  return stm_load_u16(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint32_t tanger_stm_load32aligned(tanger_stm_tx_t *tx, uint32_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
#if __WORDSIZE == 32
  return (uint32_t)stm_load(TX_PARAM (volatile stm_word_t *)addr);
#else
  return stm_load_u32(TX_PARAM addr);
#endif
}

TANGER_LOADSTORE_ATTR
uint64_t tanger_stm_load64aligned(tanger_stm_tx_t *tx, uint64_t *addr)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* ! NO_STACK_CHECK */
#if __WORDSIZE == 64
  return (uint64_t)stm_load(TX_PARAM (volatile stm_word_t *)addr);
#else 
  return stm_load_u64(TX_PARAM addr);
#endif 
}

TANGER_LOADSTORE_ATTR
void tanger_stm_loadregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest)
{
#ifndef NO_STACK_CHECK
  if (on_stack(src))
    memcpy(dest, src, bytes);
#endif /* ! NO_STACK_CHECK */
  stm_load_bytes(TX_PARAM src, dest, bytes);
}

TANGER_LOADSTORE_ATTR
void* tanger_stm_loadregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

TANGER_LOADSTORE_ATTR
void tanger_stm_loadregionpost(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store1(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_u8(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store8(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_u8(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store16(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_u16(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store32(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_u32(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store64(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_u64(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store16aligned(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_u16(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store32aligned(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
#if __WORDSIZE == 32
  stm_store(TX_PARAM (volatile stm_word_t *)addr, (stm_word_t)value);
#else
  stm_store_u32(TX_PARAM addr, value);
#endif
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store64aligned(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
#ifndef NO_STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* ! NO_STACK_CHECK */
#if __WORD_SIZE == 64
  stm_store(TX_PARAM (volatile stm_word_t *)addr, (stm_word_t)value);
#else
  stm_store_u64(TX_PARAM addr, value);
#endif
}

TANGER_LOADSTORE_ATTR
void tanger_stm_storeregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest)
{
#ifndef NO_STACK_CHECK
  if (on_stack(dest)) {
    memcpy(dest, src, bytes);
    return;
  }
#endif /* ! NO_STACK_CHECK */
  stm_store_bytes(TX_PARAM src, dest, bytes);
}

TANGER_LOADSTORE_ATTR
void* tanger_stm_storeregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

TANGER_LOADSTORE_ATTR
void* tanger_stm_updateregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

/* TODO check if used? */
tanger_stm_tx_t *tanger_stm_get_tx()
{
  return (tanger_stm_tx_t *)stm_current_tx();
}

void tanger_stm_save_restore_stack(void* low_addr, void* high_addr) __attribute__ ((noinline));
void tanger_stm_save_restore_stack(void* low_addr, void* high_addr)
{
  /**/
  /* TODO manage nesting (as flat) */
  /* if (nestingLevel != 0) return;*/

  if(stack == NULL) {
    printf("[%p] tanger_stm_save_restore_stack stack is NULL!\n", (void*) pthread_self());
    assert(0);
    return;
  }
  /* Allocate bigger buffer if necessary. */
  uintptr_t diff = (uintptr_t)high_addr - (uintptr_t)low_addr;
  if (stack->size < diff) {
    printf("[%p] low =%p high=%p area=%p area->size=%u size=%u\n", (void*)pthread_self(), low_addr, high_addr, stack, (unsigned int)stack->size, (unsigned int)diff);
    /* FIXME need to reallocate memory */
    //area->size = diff;
    //area->data = Allocator<void>::realloc(area->data, area->size);
  }
  stack->high = (uintptr_t)high_addr;
  stack->low = (uintptr_t)low_addr;
  // Do NOT copy the stack yet. We cannot make LLVM not put code between
  // the calls to this function and to _ITM_beginTransaction. If this
  // code would modify the stack, we will loose the modifications
  // when we restart the transaction. We instead save the stack from
  // within _ITM_beginTransaction, which is never inlined.
}

void tanger_stm_stack_restorehack()
{
  /* TODO manage nesting as flat */
  /* if (nestingLevel != 1) return; */

  /* Restore the stack. */
  uintptr_t diff = (uintptr_t)stack->high - (uintptr_t)stack->low;
  memcpy((void *)stack->low, stack->data, diff);
}

void tanger_stm_stack_savehack()
{
  /* TODO manage nesting as flat */
  /* if (nesting != 1) return; */

  /* Save the stack. This function is called from within
   * _ITM_beginTransaction (never inlined), so the caller's stack frame
   * won't be modified between save and restore points.
   */
  uintptr_t diff = (uintptr_t)stack->high - (uintptr_t)stack->low;
  memcpy(stack->data, (void *)stack->low, diff);
}

void tanger_stm_threadstack_init()
{
  stack_area_t *area;
  /* TODO check that the stack is not already allocated */
  if ((area = (stack_area_t *)malloc(sizeof(stack_area_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  area->low = area->high = 0;
  area->size = 4096;
  area->data = malloc(area->size);

#ifdef TLS
  stack = area;
#else /* ! TLS */
  pthread_setspecific(stack, area);
#endif /* ! TLS */
}

void tanger_stm_threadstack_fini()
{
  stack_area_t *area = get_stack();

  free(area->data);
  free(area);
}

void tanger_stm_stack_init()
{
#ifndef TLS
  if (pthread_key_create(&stack, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
#endif /* ! TLS */
}

void tanger_stm_stack_fini()
{
#ifndef TLS
  pthread_key_delete(stack);
#endif /* ! TLS */
}

void tanger_stm_init()
{
  _ITM_initializeProcess();
}

void tanger_stm_shutdown()
{
  _ITM_finalizeProcess();
}

void tanger_stm_thread_init()
{
  _ITM_initializeThread();
}

void tanger_stm_thread_shutdown()
{
  _ITM_finalizeThread();
}

/* TODO check if ok */
//void *tanger_stm_malloc(size_t size, tanger_stm_tx_t* tx)
void *tanger_stm_malloc(size_t size)
{
  return _ITM_malloc(size);
}

void tanger_stm_free(void *ptr)
{
  _ITM_free(ptr);
}

void *tanger_stm_calloc(size_t nmemb, size_t size)
{
  void *p = _ITM_malloc(nmemb * size);
  memset(p, 0, nmemb * size);
  return p;
}

void *tanger_stm_realloc(void *ptr, size_t size)
{
  /* TODO to ITM_imize */
  void *p;
#ifdef EXPLICIT_TX_PARAMETER
  struct stm_tx * tx = stm_current_tx();
#endif /* EXPLICIT_TX_PARAMETER */
  if (ptr == NULL) {
    /* Equivalent to malloc */
    return tanger_stm_malloc(size);
  }
  if (size == 0) {
    /* Equivalent to free */
    tanger_stm_free(ptr);
    return NULL;
  }
  /* Allocate new region */
  p = tanger_stm_malloc(size);
  /* Copy old content to new region */
  stm_load_bytes(TX_PARAM ptr, p, size);
  /* Free old region */
  tanger_stm_free(ptr);

  return p;
}

