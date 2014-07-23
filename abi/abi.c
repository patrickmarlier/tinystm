/*
 * File:
 *   abi.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
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
#define _GNU_SOURCE
#include <alloca.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#ifdef __SSE__
#include <xmmintrin.h>
#endif /* __SSE__ */

#include "abi.h"

#ifndef FUNC_ATTR
//#define FUNC_ATTR(STR) inline STR __attribute__((always_inline))
#define FUNC_ATTR(STR) STR __attribute__((noinline))
#endif

/* FIXME STACK_CHECK: DTMC and GCC can use ITM_W to write on stack variable */
/* TODO STACK_CHECK: to finish and test */

#include "stm.h"
#include "atomic.h"
#include "mod_mem.h"
#include "mod_log.h"
#include "mod_cb.h"
#include "mod_stats.h"
#include "wrappers.h"
#ifdef TM_DTMC
#include "dtmc/tanger-stm-internal.h"
#include "dtmc/tanger.h"
#endif

/* TODO undef siglongjmp at the end or do a better workaround */
#define siglongjmp _ITM_siglongjmp
extern void _ITM_CALL_CONVENTION _ITM_siglongjmp(sigjmp_buf env, int val) __attribute__ ((noreturn));

#include "stm.c"
#include "mod_mem.c"
#ifdef TM_GCC
#include "gcc/alloc_cpp.c"
#endif
#include "mod_log.c"
#include "mod_cb.c"
#include "mod_stats.c"
#include "wrappers.c"
#ifdef EPOCH_GC
#include "gc.c"
#endif

/* pthread wrapper */
#ifdef PTHREAD_WRAPPER
# include "pthread_wrapper.h"
#endif /* PTHREAD_WRAPPER */

#ifdef HYBRID_ASF
# define TM_START    tm_start
# define TM_COMMIT   tm_commit
# define TM_ABORT    tm_abort
#else /* ! HYBRID_ASF */
# define TM_START    stm_start
# define TM_COMMIT   stm_commit
# define TM_ABORT    stm_abort
#endif /* ! HYBRID_ASF */ 

/* ################################################################### *
 * VARIABLES
 * ################################################################### */
/* Status of the ABI */
enum {
  ABI_NOT_INITIALIZED,
  ABI_INITIALIZING,
  ABI_INITIALIZED,
  ABI_FINALIZING,
};
/* TODO need padding and alignment */
static volatile unsigned long abi_status = ABI_NOT_INITIALIZED;

/* TODO need padding and alignment */
static volatile long thread_counter = 0;

typedef struct {
  int thread_id;
#ifdef STACK_CHECK
/* XXX STACK_CHECK could be moved to stm.c */
  void *stack_addr_low;
  void *stack_addr_high;
#endif /* STACK_CHECK */
} thread_abi_t;

#ifdef TLS
static __thread thread_abi_t *thread_abi = NULL;
#else /* ! TLS */
static pthread_key_t thread_abi;
#endif /* ! TLS */

/* Statistics */
typedef struct stats {
  int thread_id;
  unsigned long nb_commits;
  unsigned long nb_aborts;
  double nb_retries_avg;
  unsigned long nb_retries_min;
  unsigned long nb_retries_max;

  struct stats * next;
} stats_t;

/* TODO ask pascal to have duplicated method for tx paramter or without
 *      will be easier to retrieved stats from another thread
 * */
/* Thread statistics managed as a linked list */
/* TODO align + padding */
stats_t * thread_stats = NULL;


/* ################################################################### *
 * COMPATIBILITY FUNCTIONS
 * ################################################################### */
#ifdef STACK_CHECK
static int get_stack_attr(void *low, void *high)
{
  /* GNU Pthread specific */
  pthread_attr_t attr;
  uintptr_t stackaddr;
  size_t stacksize;
  if (pthread_getattr_np(pthread_self(), &attr)) {
    return 1;
  }
  if (pthread_attr_getstack(&attr, (void *)&stackaddr, &stacksize)) {
    return 1;
  }
  *(uintptr_t *)low = stackaddr;
  *(uintptr_t *)high = stackaddr + stacksize;

  return 0;
}
/* Hints for other platforms
#if PLATFORM(DARWIN) 
pthread_get_stackaddr_np(pthread_self()); 
#endif
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(MSVC) 
// offset 0x18 from the FS segment register gives a pointer to 
// the thread information block for the current thread 
NT_TIB* pTib; 
__asm { 
MOV EAX, FS:[18h] 
MOV pTib, EAX 
} 
return (void*)pTib->StackBase; 
#elif PLATFORM(WIN_OS) && PLATFORM(X86_64) && COMPILER(MSVC) 
PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb()); 
return (void*)pTib->StackBase; 
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(GCC) 
// offset 0x18 from the FS segment register gives a pointer to 
// the thread information block for the current thread 
NT_TIB* pTib; 
asm ( "movl %%fs:0x18, %0\n" 
: "=r" (pTib) 
); 
return (void*)pTib->StackBase; 
#endif
*/
static inline int on_stack(void *a)
{
#ifdef TLS
  thread_abi_t *t = thread_abi;
#else /* ! TLS */
  thread_abi_t *t = pthread_getspecific(thread_abi);
#endif /* ! TLS */
  if ((t->stack_addr_low <= (uintptr_t)a) && ((uintptr_t)a < t->stack_addr_high)) { 
    return 1;
  } 
  return 0;
}
#endif /* STACK_CHECK */


#if defined(__APPLE__)
/* OS X */
# include <malloc/malloc.h>
inline size_t block_size(void *ptr)
{
  return malloc_size(ptr);
}
#elif defined(__linux__) || defined(__CYGWIN__)
/* Linux, WIN32 (CYGWIN) */
# include <malloc.h>
inline size_t block_size(void *ptr)
{
  return malloc_usable_size(ptr);
}
#else /* ! (defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)) */
# error "Target OS does not provide size of allocated blocks"
#endif /* ! (defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)) */



/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

_ITM_transaction * _ITM_CALL_CONVENTION _ITM_getTransaction(void)
{
  struct stm_tx *tx = stm_current_tx();
  if (unlikely(tx == NULL)) {
    /* Thread not initialized: must create transaction */
    _ITM_initializeThread();
    tx = stm_current_tx();
  }

  return (_ITM_transaction *)tx;
}

_ITM_howExecuting _ITM_CALL_CONVENTION _ITM_inTransaction(TX_ARG1)
{
  if (stm_irrevocable(TX_ARG2))
    return inIrrevocableTransaction;
  if (stm_active(TX_ARG2))
    return inRetryableTransaction;
  return outsideTransaction;
}

int _ITM_CALL_CONVENTION _ITM_getThreadnum(void)
{
#ifdef TLS
  return thread_abi->thread_id;
#else /* ! TLS */
  return ((thread_abi_t *)pthread_getspecific(thread_abi))->thread_id;
#endif /* ! TLS */
}

void _ITM_CALL_CONVENTION _ITM_addUserCommitAction(TX_ARGS1
                              _ITM_userCommitFunction __commit,
                              _ITM_transactionId resumingTransactionId,
                              void *__arg)
{
  stm_on_commit(TX_ARGS2 __commit, __arg);
}

void _ITM_CALL_CONVENTION _ITM_addUserUndoAction(TX_ARGS1
                            const _ITM_userUndoFunction __undo, void * __arg)
{
  stm_on_abort(TX_ARGS2 __undo, __arg);
}

/*
 * Specification: The getTransactionId function returns a sequence number for the current transaction. Within a transaction, nested transactions are numbered sequentially in the order in which they start, with the outermost transaction getting the lowest number, and non-transactional code the value _ITM_NoTransactionId, which is less than any transaction id for transactional code.
 */
_ITM_transactionId _ITM_CALL_CONVENTION _ITM_getTransactionId(TX_ARG1)
{
#ifndef EXPLICIT_TX_PARAMETER
  stm_tx_t *__td = stm_current_tx();
#endif
  if (__td == NULL)
    return _ITM_noTransactionId;
  /* TODO getNestingLevel? (NOTE: _ITM_noTransactionId is 1) */
  return (_ITM_transactionId) ((stm_tx_t *)__td)->nesting + 1;
}

void _ITM_CALL_CONVENTION _ITM_dropReferences(TX_ARGS1 const void *__start, size_t __size)
{
  /* TODO stm_release(__start); + manage size paramter */
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

void _ITM_CALL_CONVENTION _ITM_userError(const char *errString, int exitCode)
{
  fprintf(stderr, "%s", errString);
  exit(exitCode);
}

void * _ITM_malloc(size_t size)
{
#ifdef EXPLICIT_TX_PARAMETER
  stm_tx_t *tx = stm_current_tx();
  if (tx == NULL || !stm_active(tx))
    return malloc(size);
  return stm_malloc(tx, size);
#else
  if (!stm_active())
    return malloc(size);
  return stm_malloc(size);
#endif
}

void * _ITM_calloc(size_t nm, size_t size)
{
#ifdef EXPLICIT_TX_PARAMETER
  stm_tx_t *tx = stm_current_tx();
  if (tx == NULL || !stm_active(tx))
    return calloc(nm, size);
  return stm_calloc(tx, nm, size);
#else
  if (!stm_active())
    return calloc(nm, size);
  return stm_calloc(nm, size);
#endif
}

void _ITM_free(void *ptr)
{
#ifdef EXPLICIT_TX_PARAMETER
  stm_tx_t *tx = stm_current_tx();
  if (tx == NULL || !stm_active(tx)) {
    free(ptr);
    return;
  }
# ifdef NO_WRITE_ON_FREE
  stm_free(tx, ptr, 0);
# else
  stm_free(tx, ptr, block_size(ptr));
# endif
#else
  if (!stm_active()) {
    free(ptr);
    return;
  }
# ifdef NO_WRITE_ON_FREE
  stm_free(ptr, 0);
# else
  stm_free(ptr, block_size(ptr));
# endif
#endif
}

const char * _ITM_CALL_CONVENTION _ITM_libraryVersion(void)
{
  return _ITM_VERSION_NO_STR " using TinySTM " STM_VERSION "";
}

int _ITM_CALL_CONVENTION _ITM_versionCompatible(int version)
{
  return version == _ITM_VERSION_NO;
}

int _ITM_CALL_CONVENTION _ITM_initializeThread(void)
{
  /* Make sure that the main initilization is done */
  _ITM_initializeProcess();
#ifdef TLS
  if (thread_abi == NULL) {
    thread_abi_t * t = malloc(sizeof(thread_abi));
    thread_abi = t;
#else /* ! TLS */
  if (pthread_getspecific(thread_abi) == NULL) {
    thread_abi_t *t = malloc(sizeof(thread_abi));
    pthread_setspecific(thread_abi, t);
#endif /* ! TLS */
    t->thread_id = (int)ATOMIC_FETCH_INC_FULL(&thread_counter);
    stm_init_thread();
#ifdef STACK_CHECK
    get_stack_attr(&t->stack_addr_low, &t->stack_addr_high);
#endif /* STACK_CHECK */
  }
  return 0;
}

void _ITM_CALL_CONVENTION _ITM_finalizeThread(void)
{
  stm_tx_t * __td = stm_current_tx();
  
  if (__td == NULL)
    return;
  if (getenv("ITM_STATISTICS") != NULL) {
    stats_t * ts = malloc(sizeof(stats_t));
#ifdef TLS
    thread_abi_t *t = thread_abi;
#else /* ! TLS */
    thread_abi_t *t = pthread_getspecific(thread_abi);
#endif /* ! TLS */
    ts->thread_id = t->thread_id;
    stm_get_local_stats(TX_ARGS2 "nb_commits", &ts->nb_commits);
    stm_get_local_stats(TX_ARGS2 "nb_aborts", &ts->nb_aborts);
    stm_get_local_stats(TX_ARGS2 "nb_retries_avg", &ts->nb_retries_avg);
    stm_get_local_stats(TX_ARGS2 "nb_retries_min", &ts->nb_retries_min);
    stm_get_local_stats(TX_ARGS2 "nb_retries_max", &ts->nb_retries_max);
    /* Register thread-statistics to global */
    do {
	ts->next = (stats_t *)ATOMIC_LOAD(&thread_stats);
    } while (ATOMIC_CAS_FULL(&thread_stats, ts->next, ts) == 0);
    /* ts will be freed on _ITM_finalizeProcess. */
#ifdef TLS
    thread_abi = NULL;
#else /* ! TLS */
    pthread_setspecific(thread_abi, NULL);
#endif
    /* Free thread_abi_t structure. */
    free(t);
  }

  stm_exit_thread(TX_ARG2);

#ifdef TM_DTMC
  /* Free the saved stack */
  tanger_stm_free_stack();
#endif

}

#ifdef __PIC__
/* Add call when the library is loaded and unloaded */
#define ATTR_CONSTRUCTOR __attribute__ ((constructor))  
#define ATTR_DESTRUCTOR __attribute__ ((destructor))  
#else
#define ATTR_CONSTRUCTOR 
#define ATTR_DESTRUCTOR 
#endif

void ATTR_DESTRUCTOR _ITM_CALL_CONVENTION _ITM_finalizeProcess(void)
{
  char * statistics;

  _ITM_finalizeThread();

  /* Ensure thread safety */
reload:
  if (ATOMIC_LOAD_ACQ(&abi_status) == ABI_INITIALIZED) {
    if (ATOMIC_CAS_FULL(&abi_status, ABI_INITIALIZED, ABI_FINALIZING) == 0)
      goto reload;
  } else {
    return;
  }

  if ((statistics = getenv("ITM_STATISTICS")) != NULL) {
    FILE * f;
    int i = 0;
    stats_t * ts;
    if (statistics[0] == '-')
      f = stdout;
    else if ((f = fopen("itm.log", "w")) == NULL) {
      fprintf(stderr, "can't open itm.log for writing\n");
      goto finishing;
    }
    fprintf(f, "STATS REPORT\n");
    fprintf(f, "THREAD TOTALS\n");

    while (1) {
      do {
        ts = (stats_t *)ATOMIC_LOAD(&thread_stats);
	if (ts == NULL)
	  goto no_more_stat;
      } while(ATOMIC_CAS_FULL(&thread_stats, ts, ts->next) == 0);
      /* Skip stats if not a transactional thread */
      if (ts->nb_commits == 0)
        continue;
      fprintf(f, "Thread %-4i                : %12s %12s %12s %12s\n", i, "Min", "Mean", "Max", "Total");
      fprintf(f, "  Transactions             : %12lu\n", ts->nb_commits);
      fprintf(f, "  %-25s: %12lu %12.2f %12lu %12lu\n", "Retries", ts->nb_retries_min, ts->nb_retries_avg, ts->nb_retries_max, ts->nb_aborts);
      fprintf(f,"\n");
      /* Free the thread stats structure */
      free(ts);
      i++;
    }
no_more_stat:
    if (f != stdout) {
      fclose(f);
    }
  }
finishing:
#ifndef TLS
  pthread_key_delete(thread_abi);
#else /* TLS */
  thread_abi = NULL;
#endif /* TLS */
  stm_exit();

  ATOMIC_STORE(&abi_status, ABI_NOT_INITIALIZED);
}

int ATTR_CONSTRUCTOR _ITM_CALL_CONVENTION _ITM_initializeProcess(void)
{
  /* thread safe */
reload:
  if (ATOMIC_LOAD_ACQ(&abi_status) == ABI_NOT_INITIALIZED) {
    if (ATOMIC_CAS_FULL(&abi_status, ABI_NOT_INITIALIZED, ABI_INITIALIZING) != 0) {
      /* TODO temporary to be sure to use tinySTM */
      printf("TinySTM-ABI v%s.\n", _ITM_libraryVersion());
      atexit((void (*)(void))(_ITM_finalizeProcess));

      /* TinySTM initialization */
      stm_init();
      mod_mem_init(0);
# ifdef TM_GCC
      mod_alloc_cpp();
# endif /* TM_GCC */
      mod_log_init();
      mod_cb_init();
      if (getenv("ITM_STATISTICS") != NULL) {
        mod_stats_init();
      }
#ifndef TLS
      if (pthread_key_create(&thread_abi, NULL) != 0) {
        fprintf(stderr, "Error creating thread local\n");
        exit(1);
      }
#endif /* ! TLS */
      ATOMIC_STORE(&abi_status, ABI_INITIALIZED);
      /* Also initialize thread as specify in the specification */
      _ITM_initializeThread();
      return 0; 
    } else {
      goto reload;
    }
  } else if (ATOMIC_LOAD_ACQ(&abi_status) != ABI_INITIALIZED) {
    /* Wait the end of the initialization */
    goto reload;
  }

  return 0;
}

void _ITM_CALL_CONVENTION _ITM_error(const _ITM_srcLocation *__src, int errorCode)
{
  fprintf(stderr, "Error: %s (%d)\n", (__src == NULL || __src->psource == NULL ? "?" : __src->psource), errorCode);
  exit(1);
}

/* The _ITM_beginTransaction is defined in assembly (context.S)  */

/*
 * Intel Def
 * uint32_t _ITM_beginTransaction(_ITM_transaction *td,
                               uint32_t __properties,
                               const _ITM_srcLocation *__src)
 * GNU GCC Def
 * uint32_t _ITM_beginTransaction(uint32_t, ...) REGPARM;
 */
uint32_t _ITM_CALL_CONVENTION GTM_begin_transaction(TX_ARGS1 uint32_t attr, sigjmp_buf * buf) 
{
  /* FIXME if attr & a_saveLiveVariable +> siglongjmp must return a_restoreLiveVariable (and set a_saveLiveVariable)
   *       check a_abortTransaction attr
   * */
  uint32_t ret;
#ifndef EXPLICIT_TX_PARAMETER
  sigjmp_buf * env;
#endif /* ! EXPLICIT_TX_PARAMETER */
  /* This variable is in the stack but stm_start copies the content. */
  stm_tx_attr_t _a = {0,0,0,0,0};

#ifndef EXPLICIT_TX_PARAMETER
  if (unlikely(stm_current_tx() == NULL)) {
    _ITM_initializeThread();
  }
#endif

#ifdef TM_DTMC
  /* DTMC prior or equal to Velox R3 did not use regparm(2) with x86-32. */
  /* TODO to be removed when new release of DTMC fix it. */
  attr = 3;
  ret = a_runInstrumentedCode;
#else /* !TM_DTMC */
  /* Manage attribute for the transaction */
  if ((attr & pr_doesGoIrrevocable) || !(attr & pr_instrumentedCode))
  {
    /* TODO Add an attribute to specify irrevocable TX */
    stm_set_irrevocable(TX_ARGS2 1);
    ret = a_runUninstrumentedCode;
    if ((attr & pr_multiwayCode) == pr_instrumentedCode)
      ret = a_runInstrumentedCode;
  } else {
#ifdef TM_GCC
    /* GCC gives read only information (to be tested, implementation should be done) */
    if (attr & pr_readOnly)
      _a.read_only = 1;
    else
      _a.read_only = 0; /* Reset the value */
#endif /* TM_GCC */

    ret = a_runInstrumentedCode | a_saveLiveVariables;
  }
#endif /* !TM_DTMC */

#ifdef TM_DTMC
  /* if (ret & a_runInstrumentedCode) */
  tanger_stm_save_stack();
#endif /* TM_DTMC */

#ifdef EXPLICIT_TX_PARAMETER
  TM_START(TX_ARGS2 &_a);
#else /* ! EXPLICIT_TX_PARAMETER */
  env = TM_START(TX_ARGS2 &_a);
  /* Save thread context to retry (Already copied in case of EXPLICIT_TX_PARAMETER, see _ITM_beginTransaction) */
  if (env != NULL)
    memcpy(env, buf, sizeof(sigjmp_buf));
#endif /* ! EXPLICIT_TX_PARAMETER */

  return ret;
}

void _ITM_CALL_CONVENTION _ITM_commitTransaction(TX_ARGS1
                            const _ITM_srcLocation *__src)
{
  TM_COMMIT(TX_ARG2);
#ifdef TM_DTMC
  tanger_stm_reset_stack();
#endif /* TM_DTMC */
}

bool _ITM_CALL_CONVENTION _ITM_tryCommitTransaction(TX_ARGS1
                                   const _ITM_srcLocation *__src)
{
  return (TM_COMMIT(TX_ARG2) != 0);
}

/**
 * Commits all inner transactions nested within the transaction specified by the transaction id parameter
 */
void _ITM_CALL_CONVENTION _ITM_commitTransactionToId(TX_ARGS1
                                const _ITM_transactionId tid,
                                const _ITM_srcLocation *__src)
{
  /* TODO commit multiple levels in one time  -> TODO to test*/
#ifndef EXPLICIT_TX_PARAMETER
  stm_tx_t *__td = stm_current_tx();
#endif
  while ( ((stm_tx_t *)__td)->nesting+1 > tid )
    TM_COMMIT(TX_ARG2);
}

void _ITM_CALL_CONVENTION _ITM_abortTransaction(TX_ARGS1 _ITM_abortReason __reason, const _ITM_srcLocation *__src)
{
  // TODO to test
  if( __reason == userAbort) {
    /* __tm_abort was invoked. */
    __reason = a_abortTransaction;
  } else if(__reason == userRetry) {
    /* __tm_retry was invoked. */
    __reason = 0;
  }
  TM_ABORT(TX_ARGS2 __reason);
}

void _ITM_CALL_CONVENTION _ITM_rollbackTransaction(TX_ARGS1
                              const _ITM_srcLocation *__src)
{
  /* TODO check exactly the purpose of this function */
  TM_ABORT(TX_ARGS2 0);
}

void _ITM_CALL_CONVENTION _ITM_registerThrownObject(TX_ARGS1 const void *__obj, size_t __size)
{
  // TODO A rollback of the tx will not roll back the registered object
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

void _ITM_CALL_CONVENTION _ITM_changeTransactionMode(TX_ARGS1
                                _ITM_transactionState __mode,
                                const _ITM_srcLocation *__loc)
{
  /* FIXME: it seems there is a problem with irrevocable and intel c */
  switch (__mode) {
    case modeSerialIrrevocable:
      stm_set_irrevocable(TX_ARGS2 1);
      /* TODO a_runUninstrumentedCode must be set at rollback! */
      break;
    case modeObstinate:
    case modeOptimistic:
    case modePessimistic:
    default:
	fprintf(stderr, "This mode %d is not implemented yet\n", __mode);
  }
}

/**** TM GCC Specific ****/
#ifdef TM_GCC
#include "gcc/clone.c"
#include "gcc/eh.c"

/* TODO This function is not fully compatible, need to delete exception on abort. */
void _ITM_CALL_CONVENTION _ITM_commitTransactionEH(TX_ARGS1
                            void *exc_ptr)
{
  TM_COMMIT(TX_ARG2);
}

#endif /* TM_GCC */


/**** LOAD STORE LOG FUNCTIONS ****/

#define TM_LOAD(F, T, WF, WT) \
  T _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr) \
  { \
    return (WT)WF(TX_ARGS2 (volatile WT *)addr); \
  }

#define TM_LOAD_GENERIC(F, T) \
  T _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr) \
  { \
    union { T d; uint8_t s[sizeof(T)]; } c; \
    stm_load_bytes(TX_ARGS2 (volatile uint8_t *)addr, c.s, sizeof(T)); \
    return c.d; \
  }

/* TODO if WRITE_BACK/ALL?, write to stack must be saved and written directly
 * TODO must use stm_log is addresses are under the beginTransaction
  if (on_stack(addr)) { stm_log_u64(addr); *addr = val; } 
not enough because if we abort and restore -> stack can be corrupted
*/
#ifdef STACK_CHECK
#define TM_STORE(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr, T val) \
  { \
    if (on_stack(addr)) *((T*)addr) = val; \
    else WF(TX_ARGS2 (volatile WT *)addr, (WT)val); \
  }
#else /* !STACK_CHECK */
#define TM_STORE(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr, T val) \
  { \
    WF(TX_ARGS2 (volatile WT *)addr, (WT)val); \
  }
#endif /* !STACK_CHECK */

#define TM_STORE_GENERIC(F, T) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr, T val) \
  { \
    union { T d; uint8_t s[sizeof(T)]; } c; \
    c.d = val; \
    stm_store_bytes(TX_ARGS2 (volatile uint8_t *)addr, c.s, sizeof(T)); \
  }

#define TM_LOG(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr) \
  { \
    WF(TX_ARGS2 (WT *)addr); \
  }

#define TM_LOG_GENERIC(F, T) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const T *addr) \
  { \
    stm_log_bytes(TX_ARGS2 (uint8_t *)addr, sizeof(T));    \
  }

#define TM_STORE_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size) \
  { \
    stm_store_bytes(TX_ARGS2 (volatile uint8_t *)dst, (uint8_t *)src, size); \
  }

#define TM_LOAD_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size) \
  { \
    stm_load_bytes(TX_ARGS2 (volatile uint8_t *)src, (uint8_t *)dst, size); \
  }

#define TM_LOG_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 const void *addr, size_t size) \
  { \
    stm_log_bytes(TX_ARGS2 (uint8_t *)addr, size); \
  }

#ifdef STACK_CHECK
#define TM_SET_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, int val, size_t count) \
  { \
    if (on_stack(dst)) memset(dst, val, count); \
    else stm_set_bytes(TX_ARGS2 (volatile uint8_t *)dst, val, count); \
  }
#else /* !STACK_CHECK */
#define TM_SET_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, int val, size_t count) \
  { \
    stm_set_bytes(TX_ARGS2 (volatile uint8_t *)dst, val, count); \
  }
#endif /* !STACK_CHECK */

#ifdef STACK_CHECK
#define TM_COPY_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    if (on_stack(src)) memcpy(buf, src, size); \
    stm_load_bytes(TX_ARGS2 (volatile uint8_t *)src, buf, size); \
    if (on_stack(dst)) memcpy(dst, buf, size); \
    else stm_store_bytes(TX_ARGS2 (volatile uint8_t *)dst, buf, size); \
  }
#else /* !STACK_CHECK */
#define TM_COPY_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    stm_load_bytes(TX_ARGS2 (volatile uint8_t *)src, buf, size); \
    stm_store_bytes(TX_ARGS2 (volatile uint8_t *)dst, buf, size); \
  }
#endif /* !STACK_CHECK */

#define TM_COPY_BYTES_RN_WT(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    memcpy(buf, src, size); \
    stm_store_bytes(TX_ARGS2 (volatile uint8_t *)dst, buf, size); \
  }

#define TM_COPY_BYTES_RT_WN(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS1 void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    stm_load_bytes(TX_ARGS2 (volatile uint8_t *)src, buf, size); \
    memcpy(dst, buf, size); \
  }

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
#ifdef __SSE__
TM_LOAD_GENERIC_ALL(M64, __m64)
TM_LOAD_GENERIC_ALL(M128, __m128)
#endif /* __SSE__ */
TM_LOAD_GENERIC_ALL(CF, float _Complex)
TM_LOAD_GENERIC_ALL(CD, double _Complex)
TM_LOAD_GENERIC_ALL(CE, long double _Complex)

TM_STORE_ALL(U1, uint8_t, stm_store_u8, uint8_t)
TM_STORE_ALL(U2, uint16_t, stm_store_u16, uint16_t)
TM_STORE_ALL(U4, uint32_t, stm_store_u32, uint32_t)
TM_STORE_ALL(U8, uint64_t, stm_store_u64, uint64_t)
TM_STORE_ALL(F, float, stm_store_float, float)
TM_STORE_ALL(D, double, stm_store_double, double)
#ifdef __SSE__
TM_STORE_GENERIC_ALL(M64, __m64)
TM_STORE_GENERIC_ALL(M128, __m128)
#endif /* __SSE__ */
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
#if defined __SSE__ && !defined(TM_DTMC)
/* TODO LLVM-GCC doesn't compile this (run in infinite loop) */
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

#ifdef TM_DTMC
/* DTMC file uses this macro name for other thing */
# undef TM_LOAD
# undef TM_STORE
# include "dtmc/tanger.c"
#endif

