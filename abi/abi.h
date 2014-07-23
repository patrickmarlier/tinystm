/*
 * File:
 *   abi.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   ABI for tinySTM.
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
#include <stdint.h>
#include <stdbool.h>
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#include "arch.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef void *_ITM_transaction;

typedef void (*_ITM_userUndoFunction)(void *);
typedef void (*_ITM_userCommitFunction)(void *);

typedef uint32_t _ITM_transactionId;
#define _ITM_noTransactionId 1		/* Id for non-transactional code. */

#define _ITM_VERSION_NO_STR "1.0.3"
#define _ITM_VERSION_NO 103

typedef enum
{
  outsideTransaction = 0,
  inRetryableTransaction,
  inIrrevocableTransaction
} _ITM_howExecuting;

/* TODO useless! */
struct _ITM_mementoS;
typedef struct _ITM_mementoS _ITM_memento;

struct _ITM_srcLocationS
{
  int32_t reserved_1;
  int32_t flags;
  int32_t reserved_2;
  int32_t reserved_3;
  const char *psource;
};

typedef struct _ITM_srcLocationS _ITM_srcLocation;

typedef enum {
  pr_instrumentedCode = 0x0001,
  pr_uninstrumentedCode = 0x0002,
  pr_multiwayCode = pr_instrumentedCode | pr_uninstrumentedCode,
  pr_hasNoXMMUpdate = 0x0004,
  pr_hasNoAbort = 0x0008,
  pr_hasNoRetry = 0x0010,
  pr_hasNoIrrevocable = 0x0020,
  pr_doesGoIrrevocable = 0x0040,
  pr_hasNoSimpleReads = 0x0080,
  pr_aWBarriersOmitted = 0x0100,
  pr_RaRBarriersOmitted = 0x0200,
  pr_undoLogCode = 0x0400,
  pr_preferUninstrumented = 0x0800,
  pr_exceptionBlock = 0x1000,
  pr_hasElse = 0x2000,
  pr_readOnly = 0x4000 /* TODO GNU gcc specific */
} _ITM_codeProperties;

typedef enum {
  a_runInstrumentedCode = 0x01,
  a_runUninstrumentedCode = 0x02,
  a_saveLiveVariables = 0x04,
  a_restoreLiveVariables = 0x08,
  a_abortTransaction = 0x10,
} _ITM_actions;

typedef enum {
  modeSerialIrrevocable,
  modeObstinate,
  modeOptimistic,
  modePessimistic,
} _ITM_transactionState;

typedef enum {
  unknown = 0,
  userAbort = 1,
  userRetry = 2,
  TMConflict= 4,
  exceptionBlockAbort = 8
} _ITM_abortReason;


/* FIXME in stm.h but not export with no EXPLICIT_TX_PARAMETER */
struct stm_tx *stm_current_tx();

/* ################################################################### *
 * DEFINES
 * ################################################################### */

#ifdef EXPLICIT_TX_PARAMETER
#define TX_ARG1 _ITM_transaction *__td
#define TX_ARG2 (struct stm_tx *)__td
#define TX_ARGS1 _ITM_transaction *__td,
#define TX_ARGS2 (struct stm_tx *)__td,
#else
#define TX_ARG1
#define TX_ARG2
#define TX_ARGS1 
#define TX_ARGS2 
#endif

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */
/* TODO Remove all variable name in definition (useless) */

_ITM_transaction * _ITM_CALL_CONVENTION _ITM_getTransaction(void);

_ITM_howExecuting _ITM_CALL_CONVENTION _ITM_inTransaction(TX_ARG1);

int _ITM_CALL_CONVENTION _ITM_getThreadnum(void);

void _ITM_CALL_CONVENTION _ITM_addUserCommitAction(TX_ARGS1
                              _ITM_userCommitFunction __commit,
                              _ITM_transactionId resumingTransactionId,
                              void *__arg);

void _ITM_CALL_CONVENTION _ITM_addUserUndoAction(TX_ARGS1
                            const _ITM_userUndoFunction __undo, void * __arg);

_ITM_transactionId _ITM_CALL_CONVENTION _ITM_getTransactionId(TX_ARG1);

void _ITM_CALL_CONVENTION _ITM_dropReferences(TX_ARGS1 const void *__start, size_t __size);

void _ITM_CALL_CONVENTION _ITM_userError(const char *errString, int exitCode);

void * _ITM_malloc(size_t size);

void * _ITM_calloc(size_t nm, size_t size);

void _ITM_free(void *ptr);

const char * _ITM_CALL_CONVENTION _ITM_libraryVersion(void);

int _ITM_CALL_CONVENTION _ITM_versionCompatible(int version);


int _ITM_CALL_CONVENTION _ITM_initializeThread(void);

void _ITM_CALL_CONVENTION _ITM_finalizeThread(void);

void _ITM_CALL_CONVENTION _ITM_finalizeProcess(void);

int _ITM_CALL_CONVENTION _ITM_initializeProcess(void);

void _ITM_CALL_CONVENTION _ITM_error(const _ITM_srcLocation *__src, int errorCode);

uint32_t _ITM_CALL_CONVENTION _ITM_beginTransaction(TX_ARGS1
                               uint32_t __properties,
                               const _ITM_srcLocation *__src);

void _ITM_CALL_CONVENTION _ITM_commitTransaction(TX_ARGS1
                            const _ITM_srcLocation *__src);

bool _ITM_CALL_CONVENTION _ITM_tryCommitTransaction(TX_ARGS1
                                   const _ITM_srcLocation *__src);

void _ITM_CALL_CONVENTION _ITM_commitTransactionToId(TX_ARGS1
                                const _ITM_transactionId tid,
                                const _ITM_srcLocation *__src);

void _ITM_CALL_CONVENTION _ITM_abortTransaction(TX_ARGS1 _ITM_abortReason __reason, const _ITM_srcLocation *__src);

void _ITM_CALL_CONVENTION _ITM_rollbackTransaction(TX_ARGS1
                              const _ITM_srcLocation *__src);

void _ITM_CALL_CONVENTION _ITM_registerThrownObject(TX_ARGS1 const void *__obj, size_t __size);

void _ITM_CALL_CONVENTION _ITM_changeTransactionMode(TX_ARGS1
                                _ITM_transactionState __mode,
                                const _ITM_srcLocation *__loc);


/**** TM GCC Specific ****/
#ifdef TM_GCC
/* TODO Check if all these definitions (included above) need "extern" ? */
_ITM_CALL_CONVENTION
void *_ITM_getTMCloneOrIrrevocable (void *);
_ITM_CALL_CONVENTION
void *_ITM_getTMCloneSafe (void *);
void _ITM_registerTMCloneTable (void *, size_t);
void _ITM_deregisterTMCloneTable (void *);
_ITM_CALL_CONVENTION
void _ITM_commitTransactionEH(void *);
#endif


/**** LOAD STORE LOG FUNCTIONS ****/
#define TM_LOAD(F, T, WF, WT) \
  T _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr); 

#define TM_LOAD_GENERIC(F, T) \
  T _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr);

#define TM_STORE(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr, T val);

#define TM_STORE_GENERIC(F, T) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr, T val); 

#define TM_LOG(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr);

#define TM_LOG_GENERIC(F, T) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr); 

#define TM_STORE_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size);

#define TM_LOAD_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size);

#define TM_LOG_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const void *addr, size_t size);

#define TM_SET_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, int val, size_t count);

#define TM_COPY_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size);

#define TM_COPY_BYTES_RN_WT(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size);

#define TM_COPY_BYTES_RT_WN(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size); 

#define TM_LOAD_ALL(E, T, WF, WT) \
  TM_LOAD(_ITM_R##E, T, WF, WT) \
  TM_LOAD(_ITM_RaR##E, T, WF, WT) \
  TM_LOAD(_ITM_RaW##E, T, WF, WT) \
  TM_LOAD(_ITM_RfW##E, T, WF, WT)

#define TM_LOAD_GENERIC_ALL(E, T) \
  TM_LOAD_GENERIC(_ITM_R##E, T) \
  TM_LOAD_GENERIC(_ITM_RaR##E, T) \
  TM_LOAD_GENERIC(_ITM_RaW##E, T) \
  TM_LOAD_GENERIC(_ITM_RfW##E, T)

#define TM_STORE_ALL(E, T, WF, WT) \
  TM_STORE(_ITM_W##E, T, WF, WT) \
  TM_STORE(_ITM_WaR##E, T, WF, WT) \
  TM_STORE(_ITM_WaW##E, T, WF, WT)

#define TM_STORE_GENERIC_ALL(E, T) \
  TM_STORE_GENERIC(_ITM_W##E, T) \
  TM_STORE_GENERIC(_ITM_WaR##E, T) \
  TM_STORE_GENERIC(_ITM_WaW##E, T)



TM_LOAD_ALL(U1, uint8_t, stm_load_u8, uint8_t)
TM_LOAD_ALL(U2, uint16_t, stm_load_u16, uint16_t)
TM_LOAD_ALL(U4, uint32_t, stm_load_u32, uint32_t)
TM_LOAD_ALL(U8, uint64_t, stm_load_u64, uint64_t)
TM_LOAD_ALL(F, float, stm_load_float, float)
TM_LOAD_ALL(D, double, stm_load_double, double)
#if defined(__SSE__) && !defined(TM_DTMC)
/* FIXME dtmc (llvm-gcc) doesn't support it */
TM_LOAD_GENERIC_ALL(M64, __m64)
TM_LOAD_GENERIC_ALL(M128, __m128)
#endif
TM_LOAD_GENERIC_ALL(CF, float _Complex)
TM_LOAD_GENERIC_ALL(CD, double _Complex)
TM_LOAD_GENERIC_ALL(CE, long double _Complex)

TM_STORE_ALL(U1, uint8_t, stm_store_u8, uint8_t)
TM_STORE_ALL(U2, uint16_t, stm_store_u16, uint16_t)
TM_STORE_ALL(U4, uint32_t, stm_store_u32, uint32_t)
TM_STORE_ALL(U8, uint64_t, stm_store_u64, uint64_t)
TM_STORE_ALL(F, float, stm_store_float, float)
TM_STORE_ALL(D, double, stm_store_double, double)
#if defined(__SSE__) && !defined(TM_DTMC)
TM_STORE_GENERIC_ALL(M64, __m64)
TM_STORE_GENERIC_ALL(M128, __m128)
#endif
TM_STORE_GENERIC_ALL(CF, float _Complex)
TM_STORE_GENERIC_ALL(CD, double _Complex)
TM_STORE_GENERIC_ALL(CE, long double _Complex)

TM_STORE_BYTES(_ITM_memcpyRnWt)
TM_STORE_BYTES(_ITM_memcpyRnWtaR)
TM_STORE_BYTES(_ITM_memcpyRnWtaW)

TM_LOAD_BYTES(_ITM_memcpyRtWn)
TM_LOAD_BYTES(_ITM_memcpyRtaRWn)
TM_LOAD_BYTES(_ITM_memcpyRtaWWn)

TM_COPY_BYTES(_ITM_memcpyRtWt)
TM_COPY_BYTES(_ITM_memcpyRtWtaR)
TM_COPY_BYTES(_ITM_memcpyRtWtaW)
TM_COPY_BYTES(_ITM_memcpyRtaRWt)
TM_COPY_BYTES(_ITM_memcpyRtaRWtaR)
TM_COPY_BYTES(_ITM_memcpyRtaRWtaW)
TM_COPY_BYTES(_ITM_memcpyRtaWWt)
TM_COPY_BYTES(_ITM_memcpyRtaWWtaR)
TM_COPY_BYTES(_ITM_memcpyRtaWWtaW)

TM_LOG(_ITM_LU1, uint8_t, stm_log_u8, uint8_t)
TM_LOG(_ITM_LU2, uint16_t, stm_log_u16, uint16_t)
TM_LOG(_ITM_LU4, uint32_t, stm_log_u32, uint32_t)
TM_LOG(_ITM_LU8, uint64_t, stm_log_u64, uint64_t)
TM_LOG(_ITM_LF, float, stm_log_float, float)
TM_LOG(_ITM_LD, double, stm_log_double, double)
TM_LOG_GENERIC(_ITM_LE, long double)
#if defined(__SSE__) && !defined(TM_DTMC)
TM_LOG_GENERIC(_ITM_LM64, __m64)
TM_LOG_GENERIC(_ITM_LM128, __m128)
#endif
TM_LOG_GENERIC(_ITM_LCF, float _Complex)
TM_LOG_GENERIC(_ITM_LCD, double _Complex)
TM_LOG_GENERIC(_ITM_LCE, long double _Complex)

TM_LOG_BYTES(_ITM_LB)

TM_SET_BYTES(_ITM_memsetW)
TM_SET_BYTES(_ITM_memsetWaR)
TM_SET_BYTES(_ITM_memsetWaW)

TM_COPY_BYTES_RN_WT(_ITM_memmoveRnWt)
TM_COPY_BYTES_RN_WT(_ITM_memmoveRnWtaR)
TM_COPY_BYTES_RN_WT(_ITM_memmoveRnWtaW)

TM_COPY_BYTES_RT_WN(_ITM_memmoveRtWn)
TM_COPY_BYTES_RT_WN(_ITM_memmoveRtaRWn)
TM_COPY_BYTES_RT_WN(_ITM_memmoveRtaWWn)

TM_COPY_BYTES(_ITM_memmoveRtWt)
TM_COPY_BYTES(_ITM_memmoveRtWtaR)
TM_COPY_BYTES(_ITM_memmoveRtWtaW)
TM_COPY_BYTES(_ITM_memmoveRtaRWt)
TM_COPY_BYTES(_ITM_memmoveRtaRWtaR)
TM_COPY_BYTES(_ITM_memmoveRtaRWtaW)
TM_COPY_BYTES(_ITM_memmoveRtaWWt)
TM_COPY_BYTES(_ITM_memmoveRtaWWtaR)
TM_COPY_BYTES(_ITM_memmoveRtaWWtaW)


/* Clean macros */
#undef TM_LOG
#undef TM_LOG_GENERIC
#undef TM_STORE_BYTES
#undef TM_LOAD_BYTES
#undef TM_LOG_BYTES
#undef TM_SET_BYTES
#undef TM_COPY_BYTES
#undef TM_COPY_BYTES_RN_WT
#undef TM_COPY_BYTES_RT_WN
#undef TM_LOAD
#undef TM_LOAD_ALL
#undef TM_LOAD_GENERIC_ALL
#undef TM_LOAD_GENERIC
#undef TM_STORE_ALL
#undef TM_STORE
#undef TM_STORE_GENERIC_ALL
#undef TM_STORE_GENERIC
