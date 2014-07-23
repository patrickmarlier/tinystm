/*
 * File:
 *   util.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Various utility functions.
 *
 * Copyright (c) 2008.
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include "stm.h"

#ifdef __cplusplus
extern "C" {
#endif

static __inline__ void *w2vp(stm_word_t val)
{
  union { stm_word_t w; void *v; } convert;
  convert.w = val;
  return convert.v;
}

static __inline__ stm_word_t vp2w(void *val)
{
  union { stm_word_t w; void *v; } convert;
  convert.v = val;
  return convert.w;
}

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H_ */
