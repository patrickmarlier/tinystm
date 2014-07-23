/*
 * File:
 *   mod_mem.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module for dynamic memory management.
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

#ifndef _MOD_MEM_H_
#define _MOD_MEM_H_

#include "stm.h"

#ifdef __cplusplus
extern "C" {
#endif

void *stm_malloc(size_t size);
void stm_free(void *addr, size_t size);

void mod_mem_init();

#ifdef __cplusplus
}
#endif

#endif /* _MOD_MEM_H_ */
