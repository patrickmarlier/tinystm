/*
 * File:
 *   mod_log.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for logging memory accesses.
 *
 * Copyright (c) 2007-2011.
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

/**
 * @file
 *   Module for logging memory accesses.  Data is stored in an undo log.
 *   Upon abort, modifications are reverted.  Note that this module
 *   should not be used for updating shared data as there are no
 *   mechanisms to deal with concurrent accesses.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * @date
 *   2007-2011
 */

#ifndef _MOD_LOG_H_
# define _MOD_LOG_H_

# include "stm.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Log word-sized value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log(TXPARAMS stm_word_t *addr);

/**
 * Log char 8-bit value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_u8(TXPARAMS uint8_t *addr);

/**
 * Log char 16-bit value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_u16(TXPARAMS uint16_t *addr);

/**
 * Log char 32-bit value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_u32(TXPARAMS uint32_t *addr);

/**
 * Log char 64-bit value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_u64(TXPARAMS uint64_t *addr);

/**
 * Log char value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_char(TXPARAMS char *addr);

/**
 * Log unsigned char value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_uchar(TXPARAMS unsigned char *addr);

/**
 * Log short value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_short(TXPARAMS short *addr);

/**
 * Log unsigned short value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_ushort(TXPARAMS unsigned short *addr);

/**
 * Log int value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_int(TXPARAMS int *addr);

/**
 * Log unsigned int value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_uint(TXPARAMS unsigned int *addr);

/**
 * Log long value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_long(TXPARAMS long *addr);

/**
 * Log unsigned long value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_ulong(TXPARAMS unsigned long *addr);

/**
 * Log float value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_float(TXPARAMS float *addr);

/**
 * Log double value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_double(TXPARAMS double *addr);

/**
 * Log pointer value in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 */
void stm_log_ptr(TXPARAMS void **addr);

/**
 * Log memory region in transaction log.
 *
 * @param addr
 *   Address of the memory location.
 * @param size
 *   Number of bytes to log.
 */
void stm_log_bytes(TXPARAMS uint8_t *addr, size_t size);

/**
 * Initialize the module.  This function must be called once, from the
 * main thread, after initializing the STM library and before performing
 * any transactional operation.
 */
void mod_log_init();

# ifdef __cplusplus
}
# endif

#endif /* _MOD_LOG_H_ */
