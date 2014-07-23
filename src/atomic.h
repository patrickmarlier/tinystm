/*
 * File:
 *   atomic.h
 * Author(s):
 *   Pascal Felber <Pascal.Felber@unine.ch>
 * Description:
 *   Atomic operations.
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

#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#include <atomic_ops.h>

typedef AO_t atomic_t;

#ifdef NO_AO
/* Use only for testing purposes (single thread benchmarks) */
#define ATOMIC_CAS_MB(addr, e, v)       (*(addr) = (v), 1)
#define ATOMIC_FETCH_AND_INC_MB(addr)   ((*(addr))++)
#define ATOMIC_LOAD_MB(addr)            (*(addr))
#define ATOMIC_LOAD(addr)               ATOMIC_LOAD_MB(addr)
#define ATOMIC_STORE_MB(addr, v)        (*(addr) = (v))
#define ATOMIC_STORE(addr, v)           ATOMIC_STORE_MB(addr, v)
#else /* NO_AO */
#define ATOMIC_CAS_MB(addr, e, v)       (AO_compare_and_swap_full((volatile AO_t *)(addr), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_FETCH_AND_INC_MB(addr)   (AO_fetch_and_add1_full((volatile AO_t *)(addr)))
#ifdef SAFE
#define ATOMIC_LOAD_MB(addr)            (AO_load_full((volatile AO_t *)(addr)))
#define ATOMIC_LOAD(addr)               ATOMIC_LOAD_MB(addr)
#define ATOMIC_STORE_MB(addr, v)        (AO_store_full((volatile AO_t *)(addr), (AO_t)(v)))
#define ATOMIC_STORE(addr, v)           ATOMIC_STORE_MB(addr, v)
#else /* SAFE */
#define ATOMIC_LOAD_MB(addr)            (AO_load_acquire_read((volatile AO_t *)(addr)))
#define ATOMIC_LOAD(addr)               (AO_load((volatile AO_t *)(addr)))
#define ATOMIC_STORE_MB(addr, v)        (AO_store_release((volatile AO_t *)(addr), (AO_t)(v)))
#define ATOMIC_STORE(addr, v)           (AO_store((volatile AO_t *)(addr), (AO_t)(v)))
#endif /* SAFE */
#endif /* NO_AO */

#endif /* _ATOMIC_H_ */
