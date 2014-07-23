/*
 * File:
 *   memory.c
 * Author(s):
 *   Pascal Felber <Pascal.Felber@unine.ch>
 * Description:
 *   STM memory functions.
 *
 * Copyright (c) 2007.
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

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef struct mem_block {              /* Block of allocated memory */
  void *addr;                           /* Address of memory */
  struct mem_block *next;               /* Next block */
} mem_block_t;

struct mem_info {                       /* Memory descriptor */
  mem_block_t *allocated;               /* Memory allocated by this transation (freed upon abort) */
  mem_block_t *freed;                   /* Memory freed by this transation (freed upon commit) */
  stm_tx_t *tx;                         /* Transaction owning memory descriptor */
};

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Allocate memory descriptor.
 */
mem_info_t *mem_new(stm_tx_t *tx)
{
  mem_info_t *mi;

  if ((mi = (mem_info_t *)malloc(sizeof(mem_info_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  mi->allocated = mi->freed = NULL;
  mi->tx = tx;

  return mi;
}

/*
 * Delete memory descriptor.
 */
void mem_delete(mem_info_t *mi)
{
  free(mi);
}

/*
 * Allocate memory within a transaction.
 */
void *mem_alloc(mem_info_t *mi, size_t size)
{
  /* Memory will be freed upon abort */
  mem_block_t *mb;

  /* Round up size */
  if (sizeof(stm_word_t) == 4) {
    size = (size + 3) & ~(size_t)0x03;
  } else {
    size = (size + 7) & ~(size_t)0x07;
  }

  if ((mb = (mem_block_t *)malloc(sizeof(mem_block_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((mb->addr = malloc(size)) == NULL) {
    perror("malloc");
    exit(1);
  }
  mb->next = mi->allocated;
  mi->allocated = mb;

  return mb->addr;
}

/*
 * Free memory within a transaction.
 */
void mem_free(mem_info_t *mi, void *addr, size_t size)
{
  /* Memory disposal is delayed until commit */
  mem_block_t *mb;
  stm_word_t *a;

  if (size > 0) {
    /* Overwrite to prevent inconsistent reads (we could check if block
     * was allocated in this transaction => no need to overwrite) */
    if (sizeof(stm_word_t) == 4) {
      size = (size + 3) >> 2;
    } else {
      size = (size + 7) >> 3;
    }
    a = (stm_word_t *)addr;
    while (size-- > 0) {
      /* Acquire lock and update version number */
      stm_store2(mi->tx, a++, 0, 0);
    }
  }
  /* Schedule for removal */
  if ((mb = (mem_block_t *)malloc(sizeof(mem_block_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  mb->addr = addr;
  mb->next = mi->freed;
  mi->freed = mb;
}

/*
 * Commit memory operations performed by transaction.
 */
void mem_commit(mem_info_t *mi)
{
  mem_block_t *mb, *next;

  /* Keep memory allocated during transaction */
  if (mi->allocated != NULL) {
    mb = mi->allocated;
    while (mb != NULL) {
      next = mb->next;
      free(mb);
      mb = next;
    }
    mi->allocated = NULL;
  }

  /* Dispose of memory freed during transaction */
  if (mi->freed != NULL) {
    mb = mi->freed;
    while (mb != NULL) {
      next = mb->next;
      free(mb->addr);
      free(mb);
      mb = next;
    }
    mi->freed = NULL;
  }
}

/*
 * Abort memory operations performed by transaction.
 */
void mem_abort(mem_info_t *mi)
{
  mem_block_t *mb, *next;

  /* Dispose of memory allocated during transaction */
  if (mi->allocated != NULL) {
    mb = mi->allocated;
    while (mb != NULL) {
      next = mb->next;
      free(mb->addr);
      free(mb);
      mb = next;
    }
    mi->allocated = NULL;
  }

  /* Keep memory freed during transaction */
  if (mi->freed != NULL) {
    mb = mi->freed;
    while (mb != NULL) {
      next = mb->next;
      free(mb);
      mb = next;
    }
    mi->freed = NULL;
  }
}
