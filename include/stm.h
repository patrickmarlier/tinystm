/*
 * File:
 *   stm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   STM functions.
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

#ifndef _STM_H_
#define _STM_H_

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

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

void stm_init();
void stm_exit();

stm_word_t stm_load(volatile stm_word_t *addr);
void stm_store(volatile stm_word_t *addr, stm_word_t value);
void stm_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

stm_word_t stm_unit_load(volatile stm_word_t *addr);
void stm_unit_store(volatile stm_word_t *addr, stm_word_t value);
void stm_unit_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

void stm_init_thread();
void stm_exit_thread();
sigjmp_buf *stm_get_env();

void stm_start(sigjmp_buf *env, int *ro);
int stm_commit();
void stm_abort();

int stm_active();
int stm_aborted();

int stm_get_parameter(const char *name, void *val);
int stm_set_parameter(const char *name, void *val);

int stm_create_specific();
void stm_set_specific(int key, void *data);
void *stm_get_specific(int key);

int stm_register(void (*on_thread_init)(void *arg),
                 void (*on_thread_exit)(void *arg),
                 void (*on_start)(void *arg),
                 void (*on_commit)(void *arg),
                 void (*on_abort)(void *arg),
                 void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _STM_H_ */
