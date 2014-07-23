/*
 * File:
 *   mod_stats.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module for statistics.
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

#ifndef _MOD_STATS_H_
#define _MOD_STATS_H_

#include "stm.h"

#ifdef __cplusplus
extern "C" {
#endif

int stm_get_stats(const char *name, void *val);

void mod_stats_init();

#ifdef __cplusplus
}
#endif

#endif /* _MOD_STATS_H_ */
