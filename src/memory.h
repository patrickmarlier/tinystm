/*
 * File:
 *   memory.h
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

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "tinySTM.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ################################################################### *
 * TYPES
 * ################################################################### */

/* Memory descriptor */
typedef struct mem_info mem_info_t;

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

mem_info_t *mem_new(stm_tx_t *tx);
void mem_delete(mem_info_t *mi);

void *mem_alloc(mem_info_t *mi, size_t size);
void mem_free(mem_info_t *mi, void *addr, size_t size);

void mem_commit(mem_info_t *mi);
void mem_abort(mem_info_t *mi);

#ifdef __cplusplus
}
#endif

#endif /* _MEMORY_H_ */
