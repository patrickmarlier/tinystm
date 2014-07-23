/*
 * File:
 *   tinySTM-wt.c
 * Author(s):
 *   Pascal Felber <Pascal.Felber@unine.ch>
 * Description:
 *   STM functions (write-through version).
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

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "atomic.h"
#include "memory.h"
#include "tinySTM.h"

#define COMPILE_TIME_ASSERT(pred)       switch (0) { case 0: case pred: ; }

#ifdef DEBUG2
#ifndef DEBUG
#define DEBUG
#endif
#endif

#ifdef DEBUG
/* Note: stdio is thread-safe */
#define IO_FLUSH                        fflush(NULL)
#define PRINT_DEBUG(...)                printf(__VA_ARGS__); fflush(NULL)
#else
#define IO_FLUSH
#define PRINT_DEBUG(...)
#endif

#ifdef DEBUG2
#define PRINT_DEBUG2(...)               PRINT_DEBUG(__VA_ARGS__)
#else
#define PRINT_DEBUG2(...)
#endif

#ifndef RW_SET_SIZE
#define RW_SET_SIZE 4096
#endif

/* ################################################################### *
 * TYPES
 * ################################################################### */

enum {                                  /* Transaction status */
  TX_IDLE = 0,
  TX_ACTIVE = 1,
  TX_COMMITTED = 2,
  TX_ABORTED = 3
};

typedef struct r_entry {                /* Read set entry */
  stm_word_t version;                   /* Version read */
  volatile stm_word_t *lock;            /* Pointer to lock (for fast access) */
} r_entry_t;

typedef struct r_set {                  /* Read set */
  r_entry_t *entries;                   /* Array of entries */
  int nb_entries;                       /* Number of entries */
  int size;                             /* Size of array */
} r_set_t;

typedef struct w_entry {                /* Write set entry */
  volatile stm_word_t *addr;            /* Address written */
  stm_word_t value;                     /* Old value (for undo) */
  stm_word_t mask;                      /* Write mask */
  stm_word_t version;                   /* Old version (for undo) */
  volatile stm_word_t *lock;            /* Pointer to lock (for fast access) */
  int no_drop;                          /* Should we drop lock upon undo? */
} w_entry_t;

typedef struct w_set {                  /* Write set */
  w_entry_t *entries;                   /* Array of entries */
  int nb_entries;                       /* Number of entries */
  int size;                             /* Size of array */
} w_set_t;

struct stm_tx {                         /* Transaction descriptor */
  stm_word_t status;                    /* Transaction status (not read by other threads) */
  stm_word_t start;                     /* Start timestamp */
  stm_word_t end;                       /* End timestamp (validity range) */
  r_set_t r_set;                        /* Read set */
  w_set_t w_set;                        /* Write set */
  sigjmp_buf env;                       /* Environment for setjmp/longjmp */
  sigjmp_buf *jmp;                      /* Pointer to environment (NULL when not using setjmp/longjmp) */
  int nesting;                          /* Nesting level */
  int *ro_hint;                         /* Is the transaction read-only (hint)? */
  int ro;                               /* Is this execution read-only? */
  int must_free;                        /* Did we allocate memory for this descriptor? */
  mem_info_t *memory;                   /* Memory allocated/freed by this transation */
  void *data;                           /* Transaction-specific data */
#ifdef STATS
  unsigned long aborts;                 /* Total number of aborts (cumulative) */
#endif
};

/*
 * Transaction nesting is supported in a minimalist way (flat nesting):
 * - When a transaction is started in the context of another
 *   transaction, we simply increment a nesting counter but do not
 *   actually start a new transaction.
 * - The environment to be used for setjmp/longjmp is only returned when
 *   no transaction is active so that it is not overwritten by nested
 *   transactions. This allows for composability as the caller does not
 *   need to know whether it executes inside another transaction.
 * - The commit of a nested transaction simply decrements the nesting
 *   counter. Only the commit of the top-level transaction will actually
 *   carry through updates to shared memory.
 * - An abort of a nested transaction will rollback the top-level
 *   transaction and reset the nesting counter. The call to longjmp will
 *   restart execution before the top-level transaction.
 * Using nested transactions without setjmp/longjmp is not recommended
 * as one would need to explicitly jump back outside of the top-level
 * transaction upon abort of a nested transaction. This breaks
 * composability.
 */

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### */

#ifdef TLS
static __thread stm_tx_t* thread_tx;
#else
static pthread_key_t thread_tx;
#endif

/* ################################################################### *
 * LOCKS
 * ################################################################### */

/*
 * A lock is a unsigned int of the size of a pointer.
 * The LSB is the lock bit. If it is set, this means:
 * - At least some covered memory addresses is being written.
 * - All bits of the lock apart from the lock bit form a pointer that
 *   points to the transaction descriptor containing the write-set (undo
 *   values).
 * If the lock bit is not set, then:
 * - All covered memory addresses contain consistent values.
 * - All bits of the lock besides the lock bit contain a version number.
 *   - The high order bits contain the commit time.
 *   - The low order bits contain an incarnation number (incremented
 *     upon abort while writing the covered memory addresses).
 */

#define OWNED_MASK                      0x01                /* 1 bit */
#define INCARNATION_MASK                0x0E                /* 3 bits */
#define INCARNATION_MAX                 7
#define LOCK_GET_OWNED(lock)            (lock & OWNED_MASK)
#define LOCK_GET_ADDR(lock)             (lock & ~(stm_word_t)OWNED_MASK)
#define LOCK_GET_INCARNATION(lock)      ((lock & INCARNATION_MASK) >> 1)
#define LOCK_GET_TIMESTAMP(lock)        (lock >> 4)         /* Logical shift (unsigned) */
#define LOCK_SET_ADDR_OWNED(a)          (a | OWNED_MASK)    /* WRITE bit set */
#define LOCK_SET_TIMESTAMP(t)           (t << 4)            /* WRITE bit not set */
#define LOCK_SET_INCARNATION(i)         (i << 1)            /* WRITE bit not set */
#define LOCK_UPD_INCARNATION(lock, i)   ((lock & ~(stm_word_t)(INCARNATION_MASK | OWNED_MASK)) | LOCK_SET_INCARNATION(i))

#define VERSION_MAX                     (~(stm_word_t)0 >> 4)

/*
 * We use an array of locks and hash the address to find the location of the lock.
 * We try to avoid collisions as much as possible (two addresses covered by the same lock).
 */
#define LOCK_ARRAY_SIZE                 (1 << 20)           /* 2^20 = 1M */
#define LOCK_MASK                       (LOCK_ARRAY_SIZE - 1)
#define LOCK_SHIFT                      ((sizeof(stm_word_t) == 4) ? 2 : 3)
#define LOCK_IDX(addr)                  (((stm_word_t)addr >> LOCK_SHIFT) & LOCK_MASK)
#define GET_LOCK(addr)                  (locks + LOCK_IDX(addr))

static volatile stm_word_t locks[LOCK_ARRAY_SIZE];

/* ################################################################### *
 * CLOCK
 * ################################################################### */

#ifdef CLOCK_IN_CACHE_LINE
/* At least twice a cache line (512 bytes to be on the safe side) */
static volatile stm_word_t gclock[1024 / sizeof(stm_word_t)];
#define CLOCK                           (gclock[512 / sizeof(stm_word_t)])
#else
static volatile stm_word_t gclock;
#define CLOCK                           (gclock)
#endif

#define GET_CLOCK                       (ATOMIC_LOAD_MB(&CLOCK))
#define FETCH_AND_INC_CLOCK             (ATOMIC_FETCH_AND_INC_MB(&CLOCK))

/* ################################################################### *
 * STATIC
 * ################################################################### */

#ifdef ROLL_OVER_CLOCK
/*
 * We use a simple approach for clock roll-over:
 * - We maintain the count of (active) transactions using a counter
 *   protected by a mutex. This approach is not very efficient but the
 *   cost is quickly amortized because we only modify the counter when
 *   creating and deleting a transaction descriptor, which typically
 *   happens much less often than starting and committing a transaction.
 * - We detect overflows when reading the clock or when incrementing it.
 *   Upon overflow, we wait until all threads have blocked on a barrier.
 * - Threads can block on the barrier upon overflow when they (1) start
 *   a transaction, or (2) delete a transaction. This means that threads
 *   must ensure that they properly delete their transaction descriptor
 *   before performing any blocking operation outside of a transaction
 *   in order to guarantee liveness (our model prohibits blocking
 *   inside a transaction).
 */

pthread_mutex_t tx_count_mutex;
pthread_cond_t tx_reset;
int tx_count;
int tx_overflow;

/*
 * Enter new transaction.
 */
static inline void stm_enter_tx(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_enter_tx(%p)\n", tx);

  pthread_mutex_lock(&tx_count_mutex);
  while (tx_overflow != 0)
    pthread_cond_wait(&tx_reset, &tx_count_mutex);
  /* One more (active) transaction */
  tx_count++;
  pthread_mutex_unlock(&tx_count_mutex);
}

/*
 * Exit transaction.
 */
static inline void stm_exit_tx(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_exit_tx(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  pthread_mutex_lock(&tx_count_mutex);
  /* One less (active) transaction */
  tx_count--;
  assert(tx_count >= 0);
  /* Are all transactions stopped? */
  if (tx_overflow != 0 && tx_count == 0) {
    /* Yes: reset clock */
    memset((void *)locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));
    CLOCK = 0;
    tx_overflow = 0;
    /* Wake up all thread */
    pthread_cond_broadcast(&tx_reset);
  }
  pthread_mutex_unlock(&tx_count_mutex);
}

/*
 * Clock overflow.
 */
static inline void stm_overflow(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_overflow(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  pthread_mutex_lock(&tx_count_mutex);
  /* Set overflow flag (might already be set) */
  tx_overflow = 1;
  /* One less (active) transaction */
  tx_count--;
  assert(tx_count >= 0);
  /* Are all transactions stopped? */
  if (tx_count == 0) {
    /* Yes: reset clock */
    memset((void *)locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));
    CLOCK = 0;
    tx_overflow = 0;
    /* Wake up all thread */
    pthread_cond_broadcast(&tx_reset);
  } else {
    /* No: wait for other transactions to stop */
    pthread_cond_wait(&tx_reset, &tx_count_mutex);
  }
  /* One more (active) transaction */
  tx_count++;
  pthread_mutex_unlock(&tx_count_mutex);
}
#endif

/*
 * Implicit abort, leading to a retry.
 */
static inline void stm_abort_self(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_abort_self(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Call regular abort function */
  stm_abort(tx);
  /* Jump back to transaction start */
  if (tx->jmp != NULL)
    siglongjmp(*tx->jmp, 1);
}

/*
 * Check if address has been read previously.
 */
static inline int stm_has_read(stm_tx_t *tx, volatile stm_word_t *lock)
{
  r_entry_t *r;
  int i;

  PRINT_DEBUG("==> stm_has_read(%p[%lu-%lu],%p)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, lock);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Look for read */
  r = tx->r_set.entries;
  for (i = tx->r_set.nb_entries; i > 0; i--, r++) {
    if (r->lock == lock) {
      return 1;
    }
  }
  return 0;
}

/*
 * Validate read set (check if all read addresses are still valid now).
 */
static inline int stm_validate(stm_tx_t *tx)
{
  r_entry_t *r;
  int i;
  stm_word_t l;

  PRINT_DEBUG("==> stm_validate(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Validate reads */
  r = tx->r_set.entries;
  for (i = tx->r_set.nb_entries; i > 0; i--, r++) {
    /* Read lock */
    l = ATOMIC_LOAD_MB(r->lock);
    /* Unlocked and still the same version? */
    if (LOCK_GET_OWNED(l)) {
      /* Do we own the lock? */
      if ((stm_tx_t *)LOCK_GET_ADDR(l) != tx) {
        /* Locked by another transaction: cannot validate */
        return 0;
      }
      /* We own the lock: OK */
    } else {
      if (LOCK_GET_TIMESTAMP(l) != r->version) {
        /* Other version: cannot validate */
        return 0;
      }
      /* Same version: OK */
    }
  }
  return 1;
}

/*
 * Extend snapshot range.
 */
static inline int stm_extend(stm_tx_t *tx)
{
  stm_word_t now;

  PRINT_DEBUG("==> stm_extend(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Get current time */
  now = GET_CLOCK;
#ifdef ROLL_OVER_CLOCK
  if (now >= VERSION_MAX) {
    /* Clock overflow */
    return 0;
  }
#endif
  /* Try to validate read set */
  if (stm_validate(tx)) {
    /* It works: we can extend until now */
    tx->end = now;
    return 1;
  }
  return 0;
}

/*
 * Store a word-sized value.
 */
static inline void stm_write(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  volatile stm_word_t *lock;
  stm_word_t l, version;
  int duplicate;
  w_entry_t *w;
#ifdef NO_DUPLICATES_IN_RW_SETS
  int i;
#endif

  PRINT_DEBUG2("==> stm_write(t=%p[%lu-%lu],a=%p,d=%p-%lu,m=0x%lx)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  if (tx->ro) {
    /* Disable read-only and abort */
    *tx->ro_hint = 0;
    stm_abort_self(tx);
    return;
  }

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  duplicate = 0;

  /* Try to acquire lock */
 restart:
  l = ATOMIC_LOAD_MB(lock);
  if (LOCK_GET_OWNED(l)) {
    /* Locked */
    /* Do we own the lock? */
    if (tx == (stm_tx_t *)LOCK_GET_ADDR(l)) {
      /* Yes */
#ifdef NO_DUPLICATES_IN_RW_SETS
      /* Check if addr is in write set (a lock may cover multiple addresses) */
      w = tx->w_set.entries;
      for (i = tx->w_set.nb_entries; i > 0; i--, w++) {
        if (w->addr == addr) {
          if (mask == 0)
            return;
          if (w->mask == 0) {
            /* Remember old value */
            w->value = ATOMIC_LOAD_MB(addr);
            w->mask = mask;
          }
          /* Yes: only write to memory */
          PRINT_DEBUG2("==> stm_write(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu,m=0x%lx)\n",
                       tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value, (unsigned long)mask);
          if (mask != ~(stm_word_t)0)
            value = (ATOMIC_LOAD_MB(addr) & ~mask) | (value & mask);
          /* No need for barrier */
          ATOMIC_STORE(addr, value);
          return;
        }
      }
#endif
      /* Mark entry so that we do not drop the lock upon undo */
      duplicate = 1;
      /* Must add to write set (may add entry multiple times) */
    } else {
      /* No: abort self (TODO: try to wait first) */
      stm_abort_self(tx);
      return;
    }
  } else {
    /* Not locked */
    /* Handle write after reads (before CAS) */
    version = LOCK_GET_TIMESTAMP(l);
#ifdef ROLL_OVER_CLOCK
    if (version == VERSION_MAX) {
      /* Cannot acquire lock on address with version VERSION_MAX: abort */
      stm_abort_self(tx);
      return;
    }
#endif
    if (version > tx->end) {
      /* We might have read an older version previously */
      if (stm_has_read(tx, lock)) {
        /* Read version must be older (otherwise, tx->end >= version) */
        /* Not much we can do: abort */
        stm_abort_self(tx);
        return;
      }
    }

    if (ATOMIC_CAS_MB(lock, l, LOCK_SET_ADDR_OWNED((stm_word_t)tx)) == 0)
      goto restart;
  }
  /* We own the lock here */

  /* Add address to write set and undo log */
  if (tx->w_set.nb_entries == tx->w_set.size) {
    /* Extend write set */
    tx->w_set.size *= 2;
    PRINT_DEBUG("==> reallocate write set (%p[%lu-%lu],%d)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, tx->w_set.size);
    if ((tx->w_set.entries = (w_entry_t *)realloc(tx->w_set.entries, tx->w_set.size * sizeof(w_entry_t))) == NULL) {
      perror("realloc");
      exit(1);
    }
  }
  w = &tx->w_set.entries[tx->w_set.nb_entries++];
  w->addr = addr;
  if (mask == 0) {
    /* Do not write anything */
#ifndef NDEBUG
    w->value = 0;
#endif
  } else {
    /* Remember old value */
    w->value = ATOMIC_LOAD_MB(addr);
  }
  w->mask = mask;
  /* We store the old value of the lock (timestamp and incarnation) */
  w->version = l;
  w->lock = lock;
  w->no_drop = duplicate;

  if (mask == 0)
    return;

  PRINT_DEBUG2("==> stm_write(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu,m=0x%lx)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value, (unsigned long)mask);

  if (mask != ~(stm_word_t)0)
    value = (w->value & ~mask) | (value & mask);
  /* No need for barrier */
  ATOMIC_STORE(addr, value);
}

/*
 * Catch signal (to emulate non-faulting load).
 */
static void signal_catcher(int sig)
{
  stm_tx_t *tx;

  /* A fault might only occur upon a load concurrent with a free (read-after-free) */
  PRINT_DEBUG("Caught signal: %d", sig);

#ifdef TLS
  tx = thread_tx;
#else
  tx = (stm_tx_t *)pthread_getspecific(thread_tx);
#endif
  if (tx == NULL || tx->jmp == NULL) {
    /* There is not much we can do: execution will restart at faulty load */
    fprintf(stderr, "Error: invalid memory accessed and no longjmp destination\n");
    exit(1);
  }

  /* Will cause a longjmp */
  stm_abort_self(tx);
}

/* ################################################################### *
 * STM FUNCTIONS
 * ################################################################### */

/*
 * Called once (from main) to initialize STM infrastructure.
 */
void stm_init(int flags)
{
  PRINT_DEBUG("==> stm_init(%d)\n", flags);

  PRINT_DEBUG("\tsizeof(word)=%d\n", (int)sizeof(stm_word_t));

  PRINT_DEBUG("\tVERSION_MAX=0x%lx\n", (unsigned long)VERSION_MAX);

  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(void *));
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(atomic_t));

  memset((void *)locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));

  CLOCK = 0;
#ifdef ROLL_OVER_CLOCK
  if (pthread_mutex_init(&tx_count_mutex, NULL) != 0) {
    fprintf(stderr, "Error creating mutex\n");
    exit(1);
  }
  if (pthread_cond_init(&tx_reset, NULL) != 0) {
    fprintf(stderr, "Error creating condition variable\n");
    exit(1);
  }
  tx_count = 0;
  tx_overflow = 0;
#endif

#ifndef TLS
  if (pthread_key_create(&thread_tx, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
#endif

  /* Catch signals for non-faulting load */
  if (signal(SIGBUS, signal_catcher) == SIG_ERR ||
      signal(SIGSEGV, signal_catcher) == SIG_ERR) {
    perror("signal");
    exit(1);
  }
}

/*
 * Called once (from main) to clean up STM infrastructure.
 */
void stm_exit(int flags)
{
  PRINT_DEBUG("==> stm_exit(%d)\n", flags);

#ifndef TLS
  pthread_key_delete(thread_tx);
#endif
}

/*
 * Called by the CURRENT thread to allocate memory within a transaction.
 */
void *stm_malloc(stm_tx_t *tx, size_t size)
{
  PRINT_DEBUG2("==> stm_malloc(t=%p[%lu-%lu],s=%d)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, (int)size);

  return mem_alloc(tx->memory, size);
}

/*
 * Called by the CURRENT thread to free memory within a transaction.
 */
void stm_free(stm_tx_t *tx, void *addr, size_t size)
{
  PRINT_DEBUG2("==> stm_free(t=%p[%lu-%lu],a=%p,s=%d)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, (int)size);

  mem_free(tx->memory, addr, size);
}

/*
 * Called by the CURRENT thread to load a word-sized value.
 */
stm_word_t stm_load(stm_tx_t *tx, volatile stm_word_t *addr)
{
  volatile stm_word_t *lock;
  stm_word_t l, l2, value, version;
  r_entry_t *r;

  PRINT_DEBUG2("==> stm_load(t=%p[%lu-%lu],a=%p)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, addr);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* TODO: we could check for duplicate reads and get value from read set */

  /* Read lock, value, lock */
  l = ATOMIC_LOAD_MB(lock);
 restart:
  if (LOCK_GET_OWNED(l)) {
    /* Locked */
    /* Do we own the lock? */
    if (tx == (stm_tx_t *)LOCK_GET_ADDR(l)) {
      /* Yes: we have a version locked by us that was valid at write time */
      value = ATOMIC_LOAD_MB(addr);
      /* No need to add to read set (will remain valid) */
      PRINT_DEBUG2("==> stm_load(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu)\n",
                   tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value);
      return value;
    }
    /* TODO: check for duplicate reads and get value from read set (should be rare) */
    /* Cannot read a consistent version: abort (TODO: try to wait first) */
    stm_abort_self(tx);
    return 0;
  } else {
    /* Not locked */
    value = ATOMIC_LOAD_MB(addr);
    l2 = ATOMIC_LOAD_MB(lock);
    if (l != l2) {
      l = l2;
      goto restart;
    }
    /* Check timestamp */
    version = LOCK_GET_TIMESTAMP(l);
    /* Valid version? */
    if (version > tx->end) {
      /* No: try to extend first (except for read-only transactions: no read set) */
      if (tx->ro || !stm_extend(tx)) {
        /* Not much we can do: abort */
        stm_abort_self(tx);
        return 0;
      }
      /* Verify that version has not been overwritten (read value has not
       * yet been added to read set and may have not been checked during
       * extend) */
      l = ATOMIC_LOAD_MB(lock);
      if (l != l2)
        goto restart;
      /* Worked: we now have a good version (version <= tx->end) */
    }
  }
  /* We have a good version: add to read set (update transactions) and return value */

  if (!tx->ro) {
    /* Add address and version to read set */
    if (tx->r_set.nb_entries == tx->r_set.size) {
      /* Extend read set */
      tx->r_set.size *= 2;
      PRINT_DEBUG2("==> reallocate read set (%p[%lu-%lu],%d)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, tx->r_set.size);
      if ((tx->r_set.entries = (r_entry_t *)realloc(tx->r_set.entries, tx->r_set.size * sizeof(r_entry_t))) == NULL) {
        perror("realloc");
        exit(1);
      }
    }
    r = &tx->r_set.entries[tx->r_set.nb_entries++];
    r->version = version;
    r->lock = lock;
  }

  PRINT_DEBUG2("==> stm_load(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu,v=%lu)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value, (unsigned long)version);

  return value;
}

/*
 * Called by the CURRENT thread to store a word-sized value.
 */
void stm_store(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value)
{
  stm_write(tx, addr, value, ~(stm_word_t)0);
}

/*
 * Called by the CURRENT thread to store part of a word-sized value.
 */
void stm_store2(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  stm_write(tx, addr, value, mask);
}

/*
 * Called by the CURRENT thread to initialize or allocate a new transaction descriptor.
 */
stm_tx_t *stm_new(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_new()\n");

  if (tx == NULL) {
    /* Allocate descriptor */
    if ((tx = (stm_tx_t *)malloc(sizeof(stm_tx_t))) == NULL) {
      perror("malloc");
      exit(1);
    }
    tx->must_free = 1;
  } else {
    tx->must_free = 0;
  }
  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_IDLE;
  /* Read set */
  tx->r_set.nb_entries = 0;
  tx->r_set.size = RW_SET_SIZE;
  if ((tx->r_set.entries = (r_entry_t *)malloc(tx->r_set.size * sizeof(r_entry_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  /* Write set */
  tx->w_set.nb_entries = 0;
  tx->w_set.size = RW_SET_SIZE;
  if ((tx->w_set.entries = (w_entry_t *)malloc(tx->w_set.size * sizeof(w_entry_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  /* Nesting level */
  tx->nesting = 0;
  /* Transaction-specific data */
  tx->data = NULL;
#ifdef STATS
  /* Statistics */
  tx->aborts = 0;
#endif
  /* Memory */
  tx->memory = mem_new(tx);
  /* Store as thread-local data */
#ifdef TLS
  thread_tx = tx;
#else
  pthread_setspecific(thread_tx, tx);
#endif
#ifdef ROLL_OVER_CLOCK
  stm_enter_tx(tx);
#endif

  PRINT_DEBUG("==> %p\n", tx);

  return tx;
}

/*
 * Called by the CURRENT thread to delete a transaction descriptor.
 */
void stm_delete(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_delete(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

#ifdef ROLL_OVER_CLOCK
  stm_exit_tx(tx);
#endif
  mem_delete(tx->memory);
  free(tx->r_set.entries);
  free(tx->w_set.entries);
  if (tx->must_free) {
    free(tx);
  }
}

/*
 * Returns the transaction descriptor for the CURRENT thread.
 */
stm_tx_t *stm_get_tx()
{
#ifdef TLS
  return thread_tx;
#else
  return (stm_tx_t *)pthread_getspecific(thread_tx);
#endif
}

/*
 * Called by the CURRENT thread to obtain an environment for setjmp/longjmp.
 */
sigjmp_buf *stm_get_env(stm_tx_t *tx)
{
  /* Only return environment for top-level transaction */
  return tx->nesting == 0 ? &tx->env : NULL;
}

/*
 * Called by the CURRENT thread to start a transaction.
 */
void stm_start(stm_tx_t *tx, sigjmp_buf *env, int *ro)
{
  PRINT_DEBUG("==> stm_start(%p)\n", tx);

  /* Increment nesting level */
  if (tx->nesting++ > 0)
    return;

  /* Use setjmp/longjmp? */
  tx->jmp = env;
  /* Read-only? */
  tx->ro_hint = ro;
  tx->ro = (ro == NULL ? 0 : *ro);
  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_ACTIVE;
  /* Read/write set */
  tx->w_set.nb_entries = 0;
  tx->r_set.nb_entries = 0;
#ifdef ROLL_OVER_CLOCK
 start:
#endif
  /* Start timestamp */
  tx->start = GET_CLOCK; /* OPT: Could be delayed until first read/write */
#ifdef ROLL_OVER_CLOCK
  if (tx->start >= VERSION_MAX) {
    /* Overflow: we must reset clock */
    stm_overflow(tx);
    goto start;
  }
#endif
  tx->end = tx->start;
}

/*
 * Called by the CURRENT thread to commit a transaction.
 */
int stm_commit(stm_tx_t *tx)
{
  w_entry_t *w;
  stm_word_t t;
  int i;

  PRINT_DEBUG("==> stm_commit(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Decrement nesting level */
  if (--tx->nesting > 0)
    return 1;

  if (tx->w_set.nb_entries > 0) {
    /* Update transaction */

    /* Get commit timestamp */
    t = FETCH_AND_INC_CLOCK + 1;
    if (t >= VERSION_MAX) {
#ifdef ROLL_OVER_CLOCK
      /* Abort: will reset the clock on next transaction start or delete */
      stm_abort_self(tx);
      return 0;
#else
      fprintf(stderr, "Exceeded maximum version number: 0x%lx\n", (unsigned long)t);
      exit(1);
#endif
    }

    /* Try to validate (only if a concurrent transaction has committed since tx->start) */
    if (tx->start != t - 1 && !stm_validate(tx)) {
      /* Cannot commit */
      stm_abort_self(tx);
      return 0;
    }

    /* Drop locks and set new timestamp (traverse in reverse order) */
    i = 0;
    w = tx->w_set.entries + tx->w_set.nb_entries;
    while (w != tx->w_set.entries) {
      w--;
      if (w->no_drop)
        continue;
      /* No need for CAS (can only be modified by owner transaction) */
      if (i == 0) {
        /* Make sure that the updates become visible to other threads */
        ATOMIC_STORE_MB(w->lock, LOCK_SET_TIMESTAMP(t));
        i++;
      } else if (w == tx->w_set.entries) {
        /* Make sure that all lock releases become visible to other threads */
        /* Note: the first lock acquired cannot be "no drop" */
        ATOMIC_STORE_MB(w->lock, LOCK_SET_TIMESTAMP(t));
        /* We can directly exit (avoid one test) */
        break;
      } else {
        /* No need for barrier */
        ATOMIC_STORE(w->lock, LOCK_SET_TIMESTAMP(t));
      }
    }
  }

  mem_commit(tx->memory);

  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_COMMITTED;

  return 1;
}

/*
 * Called by the CURRENT thread to abort a transaction.
 */
void stm_abort(stm_tx_t *tx)
{
  w_entry_t *w;
  stm_word_t t;
  int inc;

  PRINT_DEBUG("==> stm_abort(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  t = 0;
  /* Undo writes and drop locks (traverse in reverse order) */
  w = tx->w_set.entries + tx->w_set.nb_entries;
  while (w != tx->w_set.entries) {
    w--;
    PRINT_DEBUG2("==> undo(t=%p[%lu-%lu],a=%p,d=%p-%lu,v=%lu,m=0x%lx)\n",
                 tx, (unsigned long)tx->start, (unsigned long)tx->end, w->addr, (void *)w->value, (unsigned long)w->value, (unsigned long)w->version, (unsigned long)w->mask);
    if (w->mask != 0) {
      /* No need for barrier */
      ATOMIC_STORE(w->addr, w->value);
    }
    if (w->no_drop)
      continue;
    /* Incarnation numbers allow readers to detect dirty reads */
    inc = LOCK_GET_INCARNATION(w->version) + 1;
    if (inc > INCARNATION_MAX) {
      /* Simple approach: write new version (might trigger unnecessary aborts) */
      if (t == 0) {
        t = FETCH_AND_INC_CLOCK + 1;
        if (t >= VERSION_MAX) {
#ifdef ROLL_OVER_CLOCK
          /* We can still use VERSION_MAX for protecting read-only trasanctions from dirty reads */
          t = VERSION_MAX;
#else
          fprintf(stderr, "Exceeded maximum version number: 0x%lx\n", (unsigned long)t);
          exit(1);
#endif
        }
      }
      ATOMIC_STORE_MB(w->lock, LOCK_SET_TIMESTAMP(t));
    } else {
      /* Use new incarnation number */
      ATOMIC_STORE_MB(w->lock, LOCK_UPD_INCARNATION(w->version, inc));
    }
  }

  mem_abort(tx->memory);

#ifdef STATS
  tx->aborts++;
#endif

  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_ABORTED;

  /* Reset nesting level */
  tx->nesting = 0;
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
int stm_active(stm_tx_t *tx)
{
  return (tx->status == TX_ACTIVE);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
int stm_aborted(stm_tx_t *tx)
{
  return (tx->status == TX_ABORTED);
}

/*
 * Returns STM parameters or statistics about a transaction.
 */
int stm_get_parameter(stm_tx_t *tx, const char *key, void *val)
{
#ifdef STATS
  if (strcmp("nb_aborts", key) == 0) {
    *(unsigned long *)val = tx->aborts;
    return 1;
  }
#endif
  return 0;
}

/*
 * Assign transaction-specific data.
 */
void stm_set_specific(stm_tx_t *tx, void *data)
{
  tx->data = data;
}

/*
 * Returns transaction-specific data.
 */
void *stm_get_specific(stm_tx_t *tx)
{
  return tx->data;
}
