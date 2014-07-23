/*
 * File:
 *   tanger.c
 * Author(s):
 *   Pascal Felber <Pascal.Felber@unine.ch>
 * Description:
 *   Tanger adapter for tinySTM.
 *
 * Copyright (c) 2007.
 */

#include <malloc.h>
#include <string.h>

#include <tanger-stm-internal.h>

#include "tinySTM.h"
#include "wrappers.h"

typedef struct stack_area {
  void *data;
  size_t size;
} stack_area_t;

uint8_t tanger_stm_load8(tanger_stm_tx_t *tx, uint8_t *addr)
{
  return stm_load8((stm_tx_t *)tx, addr);
}

uint16_t tanger_stm_load16(tanger_stm_tx_t *tx, uint16_t *addr)
{
  return stm_load16((stm_tx_t *)tx, addr);
}

uint32_t tanger_stm_load32(tanger_stm_tx_t *tx, uint32_t *addr)
{
  return stm_load32((stm_tx_t *)tx, addr);
}

uint64_t tanger_stm_load64(tanger_stm_tx_t *tx, uint64_t *addr)
{
  return stm_load64((stm_tx_t *)tx, addr);
}

void tanger_stm_store8(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
  stm_store8((stm_tx_t *)tx, addr, value);
}

void tanger_stm_store16(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
  stm_store16((stm_tx_t *)tx, addr, value);
}

void tanger_stm_store32(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
  stm_store32((stm_tx_t *)tx, addr, value);
}

void tanger_stm_store64(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
  stm_store64((stm_tx_t *)tx, addr, value);
}

void tanger_stm_begin(tanger_stm_tx_t *tx)
{
  jmp_buf *env;

  env = stm_get_env((stm_tx_t *)tx);
  if (env == NULL) {
    fprintf(stderr, "Nested transactions are not supported");
    exit(1);
  }
  stm_start((stm_tx_t *)tx, env, NULL);
}

void tanger_stm_abort(tanger_stm_tx_t *tx)
{
  stm_abort((stm_tx_t *)tx);
}

void tanger_stm_commit(tanger_stm_tx_t *tx)
{
  stm_commit((stm_tx_t *)tx);
}

tanger_stm_tx_t *tanger_stm_get_tx()
{
  return (tanger_stm_tx_t *)stm_get_tx();
}

void *tanger_stm_get_jmpbuf(tanger_stm_tx_t *tx)
{
  jmp_buf *env;

  env = stm_get_env((stm_tx_t *)tx);
  if (env == NULL) {
    fprintf(stderr, "Nested transactions are not supported");
    exit(1);
  }
  return env;
}

void *tanger_stm_get_stack_area(tanger_stm_tx_t *tx, void *low, void *high)
{
  stack_area_t *area;

  area = (stack_area_t *)stm_get_specific((stm_tx_t *)tx);
  if (area == NULL) {
    if ((area = (stack_area_t *)malloc(sizeof(stack_area_t))) == NULL) {
      perror("malloc");
      exit(1);
    }
    area->data = NULL;
    area->size = 0;
    stm_set_specific((stm_tx_t *)tx, area);
  }
  if (area->size < high - low) {
    area->size = high - low;
    if ((area->data = (stack_area_t *)realloc(area->data, area->size)) == NULL) {
      perror("realloc");
      exit(1);
    }
  }
  return area->data;
}

void tanger_stm_init()
{
  stm_init(0);
}

void tanger_stm_shutdown()
{
  stm_exit(0);
}

void tanger_stm_thread_init()
{
  stm_new(NULL);
}

void tanger_stm_thread_shutdown()
{
  stm_tx_t *tx;
  stack_area_t *area;

  tx = stm_get_tx();
  if (tx != NULL) {
    area = (stack_area_t *)stm_get_specific((stm_tx_t *)tx);
    if (area != NULL) {
      free(area->data);
      free(area);
    }
    stm_delete(tx);
  }
}

void *tanger_stm_malloc(size_t size, tanger_stm_tx_t* tx)
{
  return stm_malloc(tx, size);
}

void tanger_stm_free(void *ptr, tanger_stm_tx_t* tx)
{
#ifdef NO_WRITE_ON_FREE
  stm_free(tx, ptr, 0);
#else
  stm_free(tx, ptr, malloc_usable_size(ptr));
#endif
}

void *tanger_stm_calloc(size_t nmemb, size_t size, tanger_stm_tx_t* tx)
{
  void *p = stm_malloc(tx, nmemb * size);
  memset(p, 0, nmemb * size);
  return p;
}

void *tanger_stm_realloc(void *ptr, size_t size, tanger_stm_tx_t* tx)
{
  fprintf(stderr, "Call to realloc() in transaction is not supported");
  exit(1);
  return NULL;
}
