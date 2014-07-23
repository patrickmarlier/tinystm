/*
 * File:
 *   tinySTM.h
 * Author(s):
 *   Pascal Felber <Pascal.Felber@unine.ch>
 * Description:
 *   STM functions.
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

#ifndef _TINY_STM_H_
#define _TINY_STM_H_

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ################################################################### *
 * TYPES
 * ################################################################### */

/* Size of a word (accessible atomically) on the target architecture */
typedef uintptr_t stm_word_t;

/* Transaction descriptor */
typedef struct stm_tx stm_tx_t;

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

void stm_init(int flags);
void stm_exit(int flags);

void *stm_malloc(stm_tx_t *tx, size_t size);
void stm_free(stm_tx_t *tx, void *addr, size_t size);

stm_word_t stm_load(stm_tx_t *tx, volatile stm_word_t *addr);
void stm_store(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value);
void stm_store2(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

stm_tx_t *stm_new(stm_tx_t *tx);
void stm_delete(stm_tx_t *tx);
stm_tx_t *stm_get_tx();
sigjmp_buf *stm_get_env(stm_tx_t *tx);

void stm_start(stm_tx_t *tx, sigjmp_buf *env, int *ro);
int stm_commit(stm_tx_t *tx);
void stm_abort(stm_tx_t *tx);

int stm_active(stm_tx_t *tx);
int stm_aborted(stm_tx_t *tx);

int stm_get_parameter(stm_tx_t *tx, const char *key, void *val);
void *stm_get_specific(stm_tx_t *tx);
void stm_set_specific(stm_tx_t *tx, void *data);

#ifdef __cplusplus
}
#endif

#endif /* _TINY_STM_H_ */
