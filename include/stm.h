/*
 * File:
 *   stm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   STM functions.
 *
 * Copyright (c) 2007-2009.
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
 * @date
 *   2007-2009
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
 *   http://www.hpl.hp.com/research/linux/atomic_ops/.  The environment
 *   variable <c>LIBAO_HOME</c> must be set to the installation
 *   directory of atomic_ops.
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
# ifdef EXPLICIT_TX_PARAMETER
struct stm_tx;
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
   * value of the read-only flag is changed to false.
   */
  int ro;
} stm_tx_attr_t;

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
 * @param env
 *   Specifies the environment (stack context) to be used to jump back
 *   upon abort.  If null, the transaction will continue even after
 *   abort and the application should explicitely check its status.  If
 *   the transaction is nested, this parameter is ignored as an abort
 *   will restart the top-level transaction (flat nesting).
 * @param attr
 *   Specifies optional attributes associated to the transaction.  If
 *   null, the transaction uses default attributes.
 */
void stm_start(TXPARAMS sigjmp_buf *env, stm_tx_attr_t *attr);

/**
 * Try to commit a transaction.  If successful, the function returns 1.
 * Otherwise, execution continues at the point specified by the
 * environment passed as parameter to stm_start() (for the outermost
 * transaction upon nesting).  If the environment was null, the function
 * returns 0 if commit is unsuccessful.
 */
int stm_commit(TXPARAM);

/**
 * Explicitly abort a transaction.  Execution continues at the point
 * specified by the environment passed as parameter to stm_start() (for
 * the outermost transaction upon nesting), unless the environment was
 * null.
 */
void stm_abort(TXPARAM) /* __attribute__ ((noreturn)) */;

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
 * Register application-specific callbacks that are triggered when
 * particular events occur.
 *
 * @param on_thread_init
 *   Function called upon initialization of a transactional thread.
 * @param on_thread_exit
 *   Function called upon cleanup of a transactional thread.
 * @param on_start
 *   Function called upon start of a transaction.
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

#ifdef __cplusplus
}
#endif

#endif /* _STM_H_ */
