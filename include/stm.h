/*
 * File:
 *   stm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM functions.
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
 *   STM functions.  This library contains the core functions for
 *   programming with STM.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * @date
 *   2007-2011
 */

/**
 * @mainpage TinySTM
 *
 * @section overview_sec Overview
 *
 *   TinySTM is a lightweight but efficient word-based STM
 *   implementation.  This distribution includes three versions of
 *   TinySTM: write-back (updates are buffered until commit time),
 *   write-through (updates are directly written to memory), and
 *   commit-time locking (locks are only acquired upon commit).  The
 *   version can be selected by editing the makefile, which documents
 *   all the different compilation options.
 *
 *   TinySTM compiles and runs on 32 or 64-bit architectures.  It was
 *   tested on various flavors of Unix, on Mac OS X, and on Windows
 *   using cygwin.  It comes with a few test applications, notably a
 *   linked list, a skip list, and a red-black tree.
 *
 * @section install_sec Installation
 *
 *   TinySTM requires the atomic_ops library, freely available from
 *   http://www.hpl.hp.com/research/linux/atomic_ops/.  A stripped-down
 *   version of the library is included in the TinySTM distribution.  If you
 *   wish to use another version, you must set the environment variable
 *   <c>LIBAO_HOME</c> to the installation directory of atomic_ops.
 *
 *   If your system does not support GCC thread-local storage, set the
 *   environment variable <c>NOTLS</c> to a non-empty value before
 *   compilation.
 *
 *   To compile TinySTM libraries, execute <c>make</c> in the main
 *   directory.  To compile test applications, execute <c>make test</c>.
 *
 * @section contact_sec Contact
 *
 *   - E-mail : tinystm@tinystm.org
 *   - Web    : http://tinystm.org
 */

#ifndef _STM_H_
# define _STM_H_

# include <setjmp.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>

# include "../src/atomic.h" // FIXME atomic.h should be in include/ ?

/* Version string */
# define STM_VERSION                    "1.0.3"
/* Version number (times 100) */
# define STM_VERSION_NB                 103

# ifdef __cplusplus
extern "C" {
# endif

/*
 * The library does not require to pass the current transaction as a
 * parameter to the functions (the current transaction is stored in a
 * thread-local variable).  One can, however, compile the library with
 * explicit transaction parameters.  This is useful, for instance, for
 * performance on architectures that do not support TLS or for easier
 * compiler integration.
 */
struct stm_tx;
# ifdef EXPLICIT_TX_PARAMETER
#  define TXTYPE                        struct stm_tx *
#  define TXPARAM                       struct stm_tx *tx
#  define TXPARAMS                      struct stm_tx *tx,
#  define TXARG                         (struct stm_tx *)tx
#  define TXARGS                        (struct stm_tx *)tx,
struct stm_tx *stm_current_tx();
# else /* ! EXPLICIT_TX_PARAMETER */
#  define TXTYPE                        void
#  define TXPARAM                       /* Nothing */
#  define TXPARAMS                      /* Nothing */
#  define TXARG                         /* Nothing */
#  define TXARGS                        /* Nothing */
#endif /* ! EXPLICIT_TX_PARAMETER */

/* ################################################################### *
 * TYPES
 * ################################################################### */

/**
 * Size of a word (accessible atomically) on the target architecture.
 * The library supports 32-bit and 64-bit architectures.
 */
typedef uintptr_t stm_word_t;

/**
 * Transaction attributes specified by the application.
 */
typedef struct stm_tx_attr {
  /**
   * Application-specific identifier for the transaction.  Typically,
   * each transactional construct (atomic block) should have a different
   * identifier.  This identifier can be used by the infrastructure for
   * improving performance, for instance by not scheduling together
   * atomic blocks that have conflicted often in the past.
   */
  int id;
  /**
   * Indicates whether the transaction is read-only.  This information
   * is used as a hint.  If a read-only transaction performs a write, it
   * is aborted and restarted in read-write mode.  In that case, the
   * value of the read-only flag is changed to false.  If no attributes
   * are specified when starting a transaction, it is assumed to be
   * read-write.
   */
  unsigned int read_only : 1;
  /**
   * Indicates whether the transaction should use visible reads.  This
   * information is used when the transaction starts or restarts.  If a
   * transaction automatically switches to visible read mode (e.g.,
   * after having repeatedly aborted with invisible reads), this flag is
   * updated accordingly.  If no attributes are specified when starting
   * a transaction, the default behavior is to use invisible reads.
   */
  unsigned int visible_reads : 1;
  /**
   * Indicates that the transaction should not retry execution using
   * sigsetjmp() after abort.  If no attributes are specified when
   * starting a transaction, the default behavior is to retry.
   */
  unsigned int no_retry : 1;
  /**
   * Indicates how soon the transaction should be completed.  This
   * information may be used by a contention manager or the scheduler to
   * prioritize transactions.
   */
  stm_word_t deadline;
} stm_tx_attr_t;

/**
 * Reason for aborting (returned by sigsetjmp() upon transaction
 * restart).
 */
enum {
  /**
   * Abort due to explicit call from the programmer (no retry).
   */
  STM_ABORT_EXPLICIT = (1 << 4),
  /**
   * Implicit abort (high order bits indicate more detailed reason).
   */
  STM_ABORT_IMPLICIT = (1 << 5),
  /**
   * Abort upon reading a memory location being read by another
   * transaction.
   */
  STM_ABORT_RR_CONFLICT = (1 << 5) | (0x01 << 8),
  /**
   * Abort upon writing a memory location being read by another
   * transaction.
   */
  STM_ABORT_RW_CONFLICT = (1 << 5) | (0x02 << 8),
  /**
   * Abort upon reading a memory location being written by another
   * transaction.
   */
  STM_ABORT_WR_CONFLICT = (1 << 5) | (0x03 << 8),
  /**
   * Abort upon writing a memory location being written by another
   * transaction.
   */
  STM_ABORT_WW_CONFLICT = (1 << 5) | (0x04 << 8),
  /**
   * Abort upon read due to failed validation.
   */
  STM_ABORT_VAL_READ = (1 << 5) | (0x05 << 8),
  /**
   * Abort upon write due to failed validation.
   */
  STM_ABORT_VAL_WRITE = (1 << 5) | (0x06 << 8),
  /**
   * Abort upon commit due to failed validation.
   */
  STM_ABORT_VALIDATE = (1 << 5) | (0x07 << 8),
  /**
   * Abort upon write from a transaction declared as read-only.
   */
  STM_ABORT_RO_WRITE = (1 << 5) | (0x08 << 8),
  /**
   * Abort upon deferring to an irrevocable transaction.
   */
  STM_ABORT_IRREVOCABLE = (1 << 5) | (0x09 << 8),
  /**
   * Abort due to being killed by another transaction.
   */
  STM_ABORT_KILLED = (1 << 5) | (0x0A << 8),
  /**
   * Abort due to receiving a signal.
   */
  STM_ABORT_SIGNAL = (1 << 5) | (0x0B << 8),
  /**
   * Abort due to other reasons (internal to the protocol).
   */
  STM_ABORT_OTHER = (1 << 5) | (0x0F << 8)
};

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/**
 * Initialize the STM library.  This function must be called once, from
 * the main thread, before any access to the other functions of the
 * library.
 */
void stm_init();

/**
 * Clean up the STM library.  This function must be called once, from
 * the main thread, after all transactional threads have completed.
 */
void stm_exit();

/**
 * Initialize a transactional thread.  This function must be called once
 * from each thread that performs transactional operations, before the
 * thread calls any other functions of the library.
 */
TXTYPE stm_init_thread();

/**
 * Clean up a transactional thread.  This function must be called once
 * from each thread that performs transactional operations, upon exit.
 */
void stm_exit_thread(TXPARAM);

/**
 * Start a transaction.
 *
 * @param attr
 *   Specifies optional attributes associated to the transaction.
 *   Attributes are copied in transaction-local storage.  If null, the
 *   transaction uses default attributes.
 * @return
 *   Environment (stack context) to be used to jump back upon abort.  It
 *   is the responsibility of the application to call sigsetjmp()
 *   immediately after starting the transaction.  If the transaction is
 *   nested, the function returns NULL and one should not call
 *   sigsetjmp() as an abort will restart the top-level transaction
 *   (flat nesting).
 */
sigjmp_buf *stm_start(TXPARAMS stm_tx_attr_t *attr);

/**
 * Try to commit a transaction.  If successful, the function returns 1.
 * Otherwise, execution continues at the point where sigsetjmp() has
 * been called after starting the outermost transaction (unless the
 * attributes indicate that the transaction should not retry).
 *
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_commit(TXPARAM);

/**
 * Explicitly abort a transaction.  Execution continues at the point
 * where sigsetjmp() has been called after starting the outermost
 * transaction (unless the attributes indicate that the transaction
 * should not retry).
 *
 * @param abort_reason
 *   Reason for aborting the transaction.
 */
void stm_abort(TXPARAMS int abort_reason);

/**
 * Transactional load.  Read the specified memory location in the
 * context of the current transaction and return its value.  Upon
 * conflict, the transaction may abort while reading the memory
 * location.  Note that the value returned is consistent with respect to
 * previous reads from the same transaction.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
stm_word_t stm_load(TXPARAMS volatile stm_word_t *addr);

/**
 * Transactional store.  Write a word-sized value to the specified
 * memory location in the context of the current transaction.  Upon
 * conflict, the transaction may abort while writing to the memory
 * location.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store(TXPARAMS volatile stm_word_t *addr, stm_word_t value);

/**
 * Transactional store.  Write a value to the specified memory location
 * in the context of the current transaction.  The value may be smaller
 * than a word on the target architecture, in which case a mask is used
 * to indicate the bits of the words that must be updated.  Upon
 * conflict, the transaction may abort while writing to the memory
 * location.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 * @param mask
 *   Mask specifying the bits to be written.
 */
void stm_store2(TXPARAMS volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

/**
 * Check if the current transaction is still active.
 *
 * @return
 *   True (non-zero) if the transaction is active, false (zero) otherwise.
 */
int stm_active(TXPARAM);

/**
 * Check if the current transaction has aborted.
 *
 * @return
 *   True (non-zero) if the transaction has aborted, false (zero) otherwise.
 */
int stm_aborted(TXPARAM);

/**
 * Check if the current transaction is still active and in irrevocable
 * state.
 *
 * @return 
 *   True (non-zero) if the transaction is active and irrevocable, false
 *   (zero) otherwise.
 */
int stm_irrevocable(TXPARAM);

/**
 * Get the environment used by the current thread to jump back upon
 * abort.  This environment should be used when calling sigsetjmp()
 * before starting the transaction and passed as parameter to
 * stm_start().  If the current thread is already executing a
 * transaction, i.e., the new transaction will be nested, the function
 * returns NULL and one should not call sigsetjmp().
 *
 * @return
 *   The environment to use for saving the stack context, or NULL if the
 *   transaction is nested.
 */
sigjmp_buf *stm_get_env(TXPARAM);

/**
 * Get attributes associated with the current transactions, if any.
 * These attributes were passed as parameters when starting the
 * transaction.
 *
 * @return Attributes associated with the current transaction, or NULL
 *   if no attributes were specified when starting the transaction.
 */
stm_tx_attr_t *stm_get_attributes(TXPARAM);

/**
 * Get attributes associated with the specified transaction.
 * These attributes were passed as parameters when starting the
 * transaction.
 *
 * @return Attributes associated with the specified transaction, or NULL
 *   if no attributes were specified when starting the transaction.
 */
stm_tx_attr_t *stm_get_attributes_tx(struct stm_tx *tx);

/**
 * Get various statistics about the current thread/transaction.  See the
 * source code (stm.c) for a list of supported statistics.
 *
 * @param name
 *   Name of the statistics.
 * @param val
 *   Pointer to the variable that should hold the value of the
 *   statistics.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_stats(TXPARAMS const char *name, void *val);

/**
 * Get various parameters of the STM library.  See the source code
 * (stm.c) for a list of supported parameters.
 *
 * @param name
 *   Name of the parameter.
 * @param val
 *   Pointer to the variable that should hold the value of the
 *   parameter.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_parameter(const char *name, void *val);

/**
 * Set various parameters of the STM library.  See the source code
 * (stm.c) for a list of supported parameters.
 *
 * @param name
 *   Name of the parameter.
 * @param val
 *   Pointer to a variable that holds the new value of the parameter.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_set_parameter(const char *name, void *val);

/**
 * Create a key to associate application-specific data to the current
 * thread/transaction.  This mechanism can be combined with callbacks to
 * write modules.
 *
 * @return
 *   The new key.
 */
int stm_create_specific();

/**
 * Get application-specific data associated to the current
 * thread/transaction and a given key.
 *
 * @param key
 *   Key designating the data to read.
 * @return
 *   Data stored under the given key.
 */
void *stm_get_specific(TXPARAMS int key);

/**
 * Set application-specific data associated to the current
 * thread/transaction and a given key.
 *
 * @param key
 *   Key designating the data to read.
 * @param data
 *   Data to store under the given key.
 */
void stm_set_specific(TXPARAMS int key, void *data);

/**
 * Register application-specific callbacks that are triggered each time
 * particular events occur.
 *
 * @param on_thread_init
 *   Function called upon initialization of a transactional thread.
 * @param on_thread_exit
 *   Function called upon cleanup of a transactional thread.
 * @param on_start
 *   Function called upon start of a transaction.
 * @param on_precommit
 *   Function called before transaction try to commit.
 * @param on_commit
 *   Function called upon successful transaction commit.
 * @param on_abort
 *   Function called upon transaction abort.
 * @param arg
 *   Parameter to be passed to the callback functions.
 * @return
 *   1 if the callbacks have been successfully registered, 0 otherwise.
 */
int stm_register(void (*on_thread_init)(TXPARAMS void *arg),
                 void (*on_thread_exit)(TXPARAMS void *arg),
                 void (*on_start)(TXPARAMS void *arg),
                 void (*on_precommit)(TXPARAMS void *arg),
                 void (*on_commit)(TXPARAMS void *arg),
                 void (*on_abort)(TXPARAMS void *arg),
                 void *arg);

/**
 * Transaction-safe load.  Read the specified memory location outside of
 * the context of any transaction and return its value.  The operation
 * behaves as if executed in the context of a dedicated transaction
 * (i.e., it executes atomically and in isolation) that never aborts,
 * but may get delayed.
 *
 * @param addr Address of the memory location.

 * @param timestamp If non-null, the referenced variable is updated to
 *   hold the timestamp of the memory location being read.
 * @return
 *   Value read from the specified address.
 */
stm_word_t stm_unit_load(volatile stm_word_t *addr, stm_word_t *timestamp);

/**
 * Transaction-safe store.  Write a word-sized value to the specified
 * memory location outside of the context of any transaction.  The
 * operation behaves as if executed in the context of a dedicated
 * transaction (i.e., it executes atomically and in isolation) that
 * never aborts, but may get delayed.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 * @param timestamp If non-null and the timestamp in the referenced
 *   variable is smaller than that of the memory location being written,
 *   no data is actually written and the variable is updated to hold the
 *   more recent timestamp. If non-null and the timestamp in the
 *   referenced variable is not smaller than that of the memory location
 *   being written, the memory location is written and the variable is
 *   updated to hold the new timestamp.
 * @return
 *   1 if value has been written, 0 otherwise.
 */
int stm_unit_store(volatile stm_word_t *addr, stm_word_t value, stm_word_t *timestamp);

/**
 * Transaction-safe store.  Write a value to the specified memory
 * location outside of the context of any transaction.  The value may be
 * smaller than a word on the target architecture, in which case a mask
 * is used to indicate the bits of the words that must be updated.  The
 * operation behaves as if executed in the context of a dedicated
 * transaction (i.e., it executes atomically and in isolation) that
 * never aborts, but may get delayed.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 * @param mask
 *   Mask specifying the bits to be written.
 * @param timestamp If non-null and the timestamp in the referenced
 *   variable is smaller than that of the memory location being written,
 *   no data is actually written and the variable is updated to hold the
 *   more recent timestamp. If non-null and the timestamp in the
 *   referenced variable is not smaller than that of the memory location
 *   being written, the memory location is written and the variable is
 *   updated to hold the new timestamp.
 * @return
 *   1 if value has been written, 0 otherwise.
 */
int stm_unit_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask, stm_word_t *timestamp);

/**
 * Enable or disable snapshot extensions for the current transaction,
 * and optionally set an upper bound for the snapshot.  This function is
 * useful for implementing efficient algorithms with unit loads and
 * stores while preserving compatibility with with regular transactions.
 *
 * @param enable
 *   True (non-zero) to enable snapshot extensions, false (zero) to
 *   disable them.
 * @param timestamp
 *   If non-null and the timestamp in the referenced variable is smaller
 *   than the current upper bound of the snapshot, update the upper
 *   bound to the value of the referenced variable.
 */
void stm_set_extension(TXPARAMS int enable, stm_word_t *timestamp);

/**
 * Read the current value of the global clock (used for timestamps).
 * This function is useful when programming with unit loads and stores.
 *
 * @return
 *   Value of the global clock.
 */
stm_word_t stm_get_clock();

/**
 * Enter irrevocable mode for the current transaction.  If successful,
 * the function returns 1.  Otherwise, it aborts and execution continues
 * at the point where sigsetjmp() has been called after starting the
 * outermost transaction (unless the attributes indicate that the
 * transaction should not retry).
 *
 * @param serial
 *   True (non-zero) for serial-irrevocable mode (no transaction can
 *   execute concurrently), false for parallel-irrevocable mode.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_set_irrevocable(TXPARAMS int serial);

#ifdef HYBRID_ASF

/**
 * Start an hybrid transaction.
 *
 * @param attr
 *   Specifies optional attributes associated to the transaction.
 *   Attributes are copied in transaction-local storage.  If null, the
 *   transaction uses default attributes.
 * @return
 *   Environment (stack context) to be used to jump back upon abort.  It
 *   is the responsibility of the application to call sigsetjmp()
 *   immediately after starting the transaction.  If the transaction is
 *   nested, the function returns NULL and one should not call
 *   sigsetjmp() as an abort will restart the top-level transaction
 *   (flat nesting).
 */
sigjmp_buf *hytm_start(TXPARAMS stm_tx_attr_t *attr);

/**
 * Try to commit an hybrid transaction.
 * 
 * @return
 *   1 upon success, 0 otherwise.
 */
int hytm_commit(TXPARAM);

/**
 * Explicitly abort an hybrid transaction.
 *
 * @param abort_reason
 *   Reason for aborting the transaction.
 */
void hytm_abort(TXPARAMS int abort_reason);

/**
 * Transactional hybrid load. 
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
stm_word_t hytm_load(TXPARAMS volatile stm_word_t *addr);

/**
 * Transactional hybrid store.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void hytm_store(TXPARAMS volatile stm_word_t *addr, stm_word_t value);

/**
 * Transactional hybrid store.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 * @param mask
 *   Mask specifying the bits to be written.
 */
void hytm_store2(TXPARAMS volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

/**
 * Start a transaction.
 *
 * @param attr
 *   Specifies optional attributes associated to the transaction.
 *   Attributes are copied in transaction-local storage.  If null, the
 *   transaction uses default attributes.
 * @return
 *   Environment (stack context) to be used to jump back upon abort.  It
 *   is the responsibility of the application to call sigsetjmp()
 *   immediately after starting the transaction.  If the transaction is
 *   nested, the function returns NULL and one should not call
 *   sigsetjmp() as an abort will restart the top-level transaction
 *   (flat nesting).
 */
sigjmp_buf *tm_start(TXPARAMS stm_tx_attr_t *attr);

/**
 * Try to commit a transaction.
 * 
 * @return
 *   1 upon success, 0 otherwise.
 */
int tm_commit(TXPARAM);

/**
 * Explicitly abort a transaction.
 *
 * @param abort_reason
 *   Reason for aborting the transaction.
 */
void tm_abort(TXPARAMS int abort_reason);

/**
 * Transactional load. 
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
stm_word_t tm_load(TXPARAMS volatile stm_word_t *addr);

/**
 * Transactional store.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void tm_store(TXPARAMS volatile stm_word_t *addr, stm_word_t value);

/**
 * Transactional store.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 * @param mask
 *   Mask specifying the bits to be written.
 */
void tm_store2(TXPARAMS volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

/**
 * Check if the current transaction is an hybrid transaction.
 *
 * @return
 *   True (non-zero) if the transaction is hybrid, false (zero) otherwise.
 */
int tm_hybrid(TXPARAM);

/**
 * Abort the current hybrid transaction and retry it using software mode. 
 * No effect is the transaction is already in software mode.
 */
void tm_restart_software(TXPARAM);
#endif /* HYBRID_ASF */

#ifdef __cplusplus
}
#endif

#endif /* _STM_H_ */
