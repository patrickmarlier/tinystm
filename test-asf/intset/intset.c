/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Integer set stress test.
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
#define __USE_GNU
#include <sched.h>
#include <unistd.h>

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#ifdef EARLY_RELEASE
#include <asf-highlevel.h>
#endif /* EARLY_RELEASE */

#define PTLSIM_HYPERVISOR
#include "ptlcalls.h"
#define RO                              1
#define RW                              0

#if defined(TM_GCC) 
# include "../../abi/gcc/tm_macros.h"
/* XXX we force to do the initialization outside of simulation because
 * it takes a long time. */
int _ITM_initializeThread(void);
void _ITM_finalizeThread(void);
void _ITM_finalizeProcess(void);
int _ITM_initializeProcess(void);
# undef TM_INIT
# undef TM_EXIT
# undef TM_INIT_THREAD
# undef TM_EXIT_THREAD
# define TM_INIT                            _ITM_initializeProcess()
# define TM_EXIT                            _ITM_finalizeProcess()
# define TM_INIT_THREAD                     _ITM_initializeThread()
# define TM_EXIT_THREAD                     _ITM_finalizeThread()
#elif defined(TM_DTMC) 
# include "../../abi/dtmc/tm_macros.h"
/* TODO to be removed when DTMC will support annotations (should be ok now) */
//static double tanger_wrapperpure_erand48(unsigned short int __xsubi[3]) __attribute__ ((weakref("erand48")));
#elif defined(TM_INTEL)
# include "../../abi/intel/tm_macros.h"
#endif /* defined(TM_INTEL) */

#if defined(TM_GCC) || defined(TM_DTMC) || defined(TM_INTEL)
/* Add some attributes to library function */
TM_PURE 
void exit(int status);
TM_PURE 
void perror(const char *s);
#else /* Compile with explicit calls to tinySTM */

# include "stm.h"
# include "mod_mem.h"
# include "mod_ab.h"

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
# define TM_START(id, ro)                   { stm_tx_attr_t _a = {id, ro, 0, 0, 0}; \
                                              sigjmp_buf *_e = tm_start(&_a); \
                                              if (_e != NULL) sigsetjmp(*_e, 0)
# define TM_LOAD(addr)                      tm_load((stm_word_t *)addr)
# define TM_STORE(addr, value)              tm_store((stm_word_t *)addr, (stm_word_t)value)
# define TM_COMMIT                          tm_commit(); }
/* in this intset, there is no malloc/free inside transaction. */
# define TM_MALLOC(size)                    malloc(size)
# define TM_FREE(addr)                      free(addr)
# define TM_FREE2(addr, size)               free(addr)

# define TM_INIT                            stm_init()
# define TM_EXIT                            stm_exit()
# define TM_INIT_THREAD                     stm_init_thread()
# define TM_EXIT_THREAD                     stm_exit_thread()

/* Annotations used in this benchmark */
# define TM_SAFE
# define TM_PURE

#endif /* Compile with explicit calls to tinySTM */

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#if !(defined(USE_LINKEDLIST) || defined(USE_RBTREE) || defined(USE_SKIPLIST) || defined(USE_HASHSET))
# error "Must define USE_LINKEDLIST or USE_RBTREE or USE_SKIPLIST or USE_HASHSET"
#endif /* !(defined(USE_LINKEDLIST) || defined(USE_RBTREE) || defined(USE_SKIPLIST) || defined(USE_HASHSET)) */


#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#ifndef CACHE_LINE_SIZE
# define CACHE_LINE_SIZE                64
#endif /* CACHE_LINE_SIZE */

#define MAX(a, b)                       ((a) > (b) ? (a) : (b))

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile int stop;

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);
  return v;
}

static void pin_cpu(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);
}

#if defined(__i386__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}
#elif defined(__x86_64__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return ((unsigned long long)lo) | ((unsigned long long)hi << 32);
}
#endif /* defined(__x86_64__) */

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### */

#ifdef TLS
static __thread unsigned short *rng_seed;
#else /* ! TLS */
static pthread_key_t rng_seed_key;
#endif /* ! TLS */

#if defined(USE_LINKEDLIST)

/* ################################################################### *
 * LINKEDLIST
 * ################################################################### */

# define TRANSACTIONAL                  1

# define INIT_SET_PARAMETERS            /* Nothing */

typedef intptr_t val_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
  val_t val;
  struct node *next;
} node_t;

typedef struct intset {
  node_t *head;
} intset_t;

TM_SAFE
static node_t *new_node(val_t val, node_t *next)
{
  node_t *node;

#ifdef EARLY_RELEASE
  if (posix_memalign((void **)&node, CACHE_LINE_SIZE, MAX(sizeof(node_t), CACHE_LINE_SIZE / 2 + 1)) != 0) {
    perror("posix_memalign");
    exit(1);
  }
#else /* ! EARLY_RELEASE */
  node = (node_t *)malloc(sizeof(node_t));
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }
#endif /* ! EARLY_RELEASE */

  node->val = val;
  node->next = next;

  return node;
}

static intset_t *set_new()
{
  intset_t *set;
  node_t *min, *max;

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(VAL_MAX, NULL);
  min = new_node(VAL_MIN, max);
  set->head = min;

  return set;
}

static void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next;
    free(node);
    node = next;
  }
  free(set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}

static int set_contains(intset_t *set, val_t val, int transactional)
{
  int result;
  node_t *prev, *next;
  val_t v;
#ifdef EARLY_RELEASE
  node_t *t;
#endif /* EARLY_RELEASE */

# ifdef DEBUG
  printf("++> set_contains(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
  } else {
    TM_START(0, RO);
    prev = (node_t *)TM_LOAD(&set->head);
    next = (node_t *)TM_LOAD(&prev->next);
    while (1) {
      v = TM_LOAD(&next->val);
      if (v >= val)
        break;
#ifdef EARLY_RELEASE
      t = prev;
#endif /* EARLY_RELEASE */
      prev = next;
      next = (node_t *)TM_LOAD(&prev->next);
#ifdef EARLY_RELEASE
      asf_release((void *)t);
#endif /* EARLY_RELEASE */
    }
    TM_COMMIT;
    result = (v == val);
  }

# ifdef DEBUG
  printf("--> set_contains(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

static int set_add(intset_t *set, val_t val, int transactional)
{
  int result;
  node_t *prev, *next;
  val_t v;
#ifdef EARLY_RELEASE
  node_t *t;
#endif /* EARLY_RELEASE */

# ifdef DEBUG
  printf("++> set_add(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val != val);
    if (result) {
      prev->next = new_node(val, next);
    }
  } else {
    node_t *n = new_node(val, NULL);
    TM_START(1, RW);
    prev = (node_t *)TM_LOAD(&set->head);
    next = (node_t *)TM_LOAD(&prev->next);
    while (1) {
      v = TM_LOAD(&next->val);
      if (v >= val)
        break;
#ifdef EARLY_RELEASE
      t = prev;
#endif /* EARLY_RELEASE */
      prev = next;
      next = (node_t *)TM_LOAD(&prev->next);
#ifdef EARLY_RELEASE
      asf_release((void *)t);
#endif /* EARLY_RELEASE */
    }
    result = (v != val);
    if (result) {
      n->next = next;
      TM_STORE(&prev->next, n);
    }
    TM_COMMIT;
    if (!result)
      free(n);
  }

# ifdef DEBUG
  printf("--> set_add(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

static int set_remove(intset_t *set, val_t val, int transactional)
{
  int result;
  node_t *prev, *next, *n;
  val_t v;
#ifdef EARLY_RELEASE
  node_t *t;
#endif /* EARLY_RELEASE */

# ifdef DEBUG
  printf("++> set_remove(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
    if (result) {
      prev->next = next->next;
      free(next);
    }
  } else {
    TM_START(2, RW);
    prev = (node_t *)TM_LOAD(&set->head);
    next = (node_t *)TM_LOAD(&prev->next);
    while (1) {
      v = TM_LOAD(&next->val);
      if (v >= val)
        break;
#ifdef EARLY_RELEASE
      t = prev;
#endif /* EARLY_RELEASE */
      prev = next;
      next = (node_t *)TM_LOAD(&prev->next);
#ifdef EARLY_RELEASE
      asf_release((void *)t);
#endif /* EARLY_RELEASE */
    }
    result = (v == val);
    if (result) {
      n = (node_t *)TM_LOAD(&next->next);
      TM_STORE(&prev->next, n);
      /* Overwrite deleted node */
      TM_STORE(&next->next, next);
      TM_STORE(&next->val, 123456789);
    }
    TM_COMMIT;
    /* Free memory after committed execution */
    if (result)
      free(next);
  }

# ifdef DEBUG
  printf("--> set_remove(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

#elif defined(USE_RBTREE)

/* ################################################################### *
 * RBTREE
 * ################################################################### */

# define TRANSACTIONAL                  1

# define INIT_SET_PARAMETERS            /* Nothing */

# define TM_ARGDECL_ALONE               /* Nothing */
# define TM_ARGDECL                     /* Nothing */
# define TM_ARG                         /* Nothing */
# define TM_ARG_ALONE                   /* Nothing */
# define TM_CALLABLE                    /* Nothing */

# define TM_SHARED_READ(var)            TM_LOAD(&(var))
# define TM_SHARED_READ_P(var)          TM_LOAD(&(var))

# define TM_SHARED_WRITE(var, val)      TM_STORE(&(var), val)
# define TM_SHARED_WRITE_P(var, val)    TM_STORE(&(var), val)

# include "rbtree.h"

# include "rbtree.c"

typedef rbtree_t intset_t;
typedef intptr_t val_t;

static long compare(const void *a, const void *b)
{
  return ((val_t)a - (val_t)b);
}

static intset_t *set_new()
{
  return rbtree_alloc(&compare);
}

static void set_delete(intset_t *set)
{
  rbtree_free(set);
}

static int set_size(intset_t *set)
{
  int size;
  node_t *n;

  if (!rbtree_verify(set, 0)) {
    printf("Validation failed!\n");
    exit(1);
  }

  size = 0;
  for (n = firstEntry(set); n != NULL; n = successor(n))
    size++;

  return size;
}

static int set_contains(intset_t *set, val_t val, int transactional)
{
  int result;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    result = rbtree_contains(set, (void *)val);
  } else {
    result = TMrbtree_contains(set, (void *)val);
  }

  return result;
}

static int set_add(intset_t *set, val_t val, int transactional)
{
  int result;

# ifdef DEBUG
  printf("++> set_add(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    result = rbtree_insert(set, (void *)val, (void *)val);
  } else {
    result = TMrbtree_insert(set, (void *)val, (void *)val);
  }

  return result;
}

static int set_remove(intset_t *set, val_t val, int transactional)
{
  int result;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    result = rbtree_delete(set, (void *)val);
  } else {
    result = TMrbtree_delete(set, (void *)val);
  }

  return result;
}

#elif defined(USE_SKIPLIST)

/* ################################################################### *
 * SKIPLIST
 * ################################################################### */

# define TRANSACTIONAL                  1

# define MAX_LEVEL                      64

# define INIT_SET_PARAMETERS            8, 50

typedef intptr_t val_t;
typedef intptr_t level_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
  val_t val;
  level_t level;
  struct node *forward[1];
} node_t;

typedef struct intset {
  node_t *head;
  node_t *tail;
  level_t level;
  int prob;
  int max_level;
} intset_t;

TM_PURE
static inline int rand_100()
{
  unsigned short *seed;
# ifdef TLS
  seed = rng_seed;
# else /* ! TLS */
  seed = (unsigned short *)pthread_getspecific(rng_seed_key);
# endif /* ! TLS */
  return rand_range(100, seed);
}

TM_PURE
static int random_level(intset_t *set)
{
  int l = 0;
  while (l < set->max_level && rand_100() < set->prob)
    l++;
  return l;
}

TM_SAFE
static node_t *new_node(val_t val, level_t level)
{
  node_t *node;

  node = (node_t *)malloc(sizeof(node_t) + level * sizeof(node_t *));
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->level = level;

  return node;
}

static intset_t *set_new(level_t max_level, int prob)
{
  intset_t *set;
  int i;

  assert(max_level <= MAX_LEVEL);
  assert(prob >= 0 && prob <= 100);

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  set->max_level = max_level;
  set->prob = prob;
  set->level = 0;
  /* Set head and tail are immutable */
  set->tail = new_node(VAL_MAX, max_level);
  set->head = new_node(VAL_MIN, max_level);
  for (i = 0; i <= max_level; i++) {
    set->head->forward[i] = set->tail;
    set->tail->forward[i] = NULL;
  }

  return set;
}

static void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->forward[0];
    free(node);
    node = next;
  }
  free(set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->forward[0];
  while (node->forward[0] != NULL) {
    size++;
    node = node->forward[0];
  }

  return size;
}

static int set_contains(intset_t *set, val_t val, int transactional)
{
  int result, i;
  node_t *node, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
    }
    node = node->forward[0];
    result = (node->val == val);
  } else {
    TM_START(0, RO);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    for (i = TM_LOAD(&set->level); i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
    }
    TM_COMMIT;
    result = (v == val);
  }

# ifdef DEBUG
  printf("--> set_contains(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

static int set_add(intset_t *set, val_t val, int transactional)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level, l;
  val_t v;

# ifdef DEBUG
  printf("++> set_add(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
      update[i] = node;
    }
    node = node->forward[0];

    if (node->val == val) {
      result = 0;
    } else {
      l = random_level(set);
      if (l > set->level) {
        for (i = set->level + 1; i <= l; i++)
          update[i] = set->head;
        set->level = l;
      }
      node = new_node(val, l);
      for (i = 0; i <= l; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
      }
      result = 1;
    }
  } else {
    node_t *n;
    l = random_level(set);
    n = new_node(val, l);
    TM_START(1, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = TM_LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
      update[i] = node;
    }

    if (v == val) {
      result = 0;
    } else {
      if (l > level) {
        for (i = level + 1; i <= l; i++)
          update[i] = set->head;
        TM_STORE(&set->level, l);
      }
      for (i = 0; i <= l; i++) {
        n->forward[i] = (node_t *)TM_LOAD(&update[i]->forward[i]);
        TM_STORE(&update[i]->forward[i], n);
      }
      result = 1;
    }
    TM_COMMIT;
    if (!result)
      free(n);
  }

# ifdef DEBUG
  printf("--> set_add(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

static int set_remove(intset_t *set, val_t val, int transactional)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level;
  val_t v;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", (int)val);
  IO_FLUSH;
# endif

  if (!transactional) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
      update[i] = node;
    }
    node = node->forward[0];

    if (node->val != val) {
      result = 0;
    } else {
      for (i = 0; i <= set->level; i++) {
        if (update[i]->forward[i] == node)
          update[i]->forward[i] = node->forward[i];
      }
      while (set->level > 0 && set->head->forward[set->level]->forward[0] == NULL)
        set->level--;
      free(node);
      result = 1;
    }
  } else {
    TM_START(2, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = TM_LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
      update[i] = node;
    }

    if (v != val) {
      result = 0;
    } else {
      node = (node_t *)TM_LOAD(&node->forward[0]);
      for (i = 0; i <= level; i++) {
        if ((node_t *)TM_LOAD(&update[i]->forward[i]) == node)
          TM_STORE(&update[i]->forward[i], (node_t *)TM_LOAD(&node->forward[i]));
      }
      i = level;
      while (i > 0 && (node_t *)TM_LOAD(&set->head->forward[i]) == set->tail)
        i--;
      if (i != level)
        TM_STORE(&set->level, i);
      /* Overwrite deleted node */
      for (i = 0; i <= node->level; i++)
        TM_STORE(&node->forward[i], node);
      node->val = 123456789;
      node->level = 123456789;
      result = 1;
    }
    TM_COMMIT;
    /* Free memory after committed execution */
    if (result)
      free(node);
  }

# ifdef DEBUG
  printf("--> set_remove(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

#elif defined(USE_HASHSET)

/* ################################################################### *
 * HASHSET
 * ################################################################### */

# define TRANSACTIONAL                  1

# define INIT_SET_PARAMETERS            /* Nothing */

# define NB_BUCKETS                     (1UL << 17)

# define HASH(a)                        (hash((uint32_t)a) & (NB_BUCKETS - 1))

typedef intptr_t val_t;

typedef struct bucket {
  val_t val;
  struct bucket *next;
} bucket_t;

typedef struct intset {
  char pad1[64];
  bucket_t **buckets;
  char pad2[64];
} intset_t;

TM_PURE
static uint32_t hash(uint32_t a)
{
  /* Knuth's multiplicative hash function */
  a *= 2654435761UL;
  return a;
}

TM_SAFE
static bucket_t *new_entry(val_t val, bucket_t *next)
{
  bucket_t *b;

  b = (bucket_t *)malloc(sizeof(bucket_t));
  if (b == NULL) {
    perror("malloc");
    exit(1);
  }

  b->val = val;
  b->next = next;

  return b;
}

static intset_t *set_new()
{
  intset_t *set;

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((set->buckets = (bucket_t **)calloc(NB_BUCKETS, sizeof(bucket_t *))) == NULL) {
    perror("calloc");
    exit(1);
  }

  return set;
}

static void set_delete(intset_t *set)
{
  unsigned int i;
  bucket_t *b, *next;

  for (i = 0; i < NB_BUCKETS; i++) {
    b = set->buckets[i];
    while (b != NULL) {
      next = b->next;
      free(b);
      b = next;
    }
  }
  free(set->buckets);
  free(set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  unsigned int i;
  bucket_t *b;

  for (i = 0; i < NB_BUCKETS; i++) {
    b = set->buckets[i];
    while (b != NULL) {
      size++;
      b = b->next;
    }
  }

  return size;
}

static int set_contains(intset_t *set, val_t val, int transactional)
{
  int result, i;
  bucket_t *b;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (!transactional) {
    i = HASH(val);
    b = set->buckets[i];
    result = 0;
    while (b != NULL) {
      if (b->val == val) {
        result = 1;
        break;
      }
      b = b->next;
    }
  } else {
    TM_START(0, RO);
    i = HASH(val);
    b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    result = 0;
    while (b != NULL) {
      if (TM_LOAD(&b->val) == val) {
        result = 1;
        break;
      }
      b = (bucket_t *)TM_LOAD(&b->next);
    }
    TM_COMMIT;
  }

# ifdef DEBUG
  printf("--> set_contains(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

static int set_add(intset_t *set, val_t val, int transactional)
{
  int result, i;
  bucket_t *b, *first;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (!transactional) {
    i = HASH(val);
    first = b = set->buckets[i];
    result = 1;
    while (b != NULL) {
      if (b->val == val) {
        result = 0;
        break;
      }
      b = b->next;
    }
    if (result) {
      set->buckets[i] = new_entry(val, first);
    }
  } else {
    bucket_t *n = new_entry(val, NULL);
    TM_START(0, RW);
    i = HASH(val);
    first = b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    result = 1;
    while (b != NULL) {
      if (TM_LOAD(&b->val) == val) {
        result = 0;
        break;
      }
      b = (bucket_t *)TM_LOAD(&b->next);
    }
    if (result) {
      n->next = first;
      TM_STORE(&set->buckets[i], n);
    }
    TM_COMMIT;
    if (!result)
      free(n);
  }

# ifdef DEBUG
  printf("--> set_add(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

static int set_remove(intset_t *set, val_t val, int transactional)
{
  int result, i;
  bucket_t *b, *prev;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (!transactional) {
    i = HASH(val);
    prev = b = set->buckets[i];
    result = 0;
    while (b != NULL) {
      if (b->val == val) {
        result = 1;
        break;
      }
      prev = b;
      b = b->next;
    }
    if (result) {
      if (prev == b) {
        /* First element of bucket */
        set->buckets[i] = b->next;
      } else {
        prev->next = b->next;
      }
      free(b);
    }
  } else {
    TM_START(0, RW);
    i = HASH(val);
    prev = b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    result = 0;
    while (b != NULL) {
      if (TM_LOAD(&b->val) == val) {
        result = 1;
        break;
      }
      prev = b;
      b = (bucket_t *)TM_LOAD(&b->next);
    }
    if (result) {
      if (prev == b) {
        /* First element of bucket */
        TM_STORE(&set->buckets[i], TM_LOAD(&b->next));
      } else {
        TM_STORE(&prev->next, TM_LOAD(&b->next));
      }
      /* Overwrite deleted node */
      TM_STORE(&b->next, b);
      TM_STORE(&b->val, 123456789);
    }
    TM_COMMIT;
    /* Free memory after committed execution */
    if (result)
      free(b);
  }

# ifdef DEBUG
  printf("--> set_remove(%d): %d\n", (int)val, result);
  IO_FLUSH;
# endif

  return result;
}

#endif /* defined(USE_HASHSET) */

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct {
  volatile unsigned long wait;
} barrier_t;

// TODO: We should be able to run this w/o the fences on x86, but maybe not on PTLsim!
static void inline atomic_inc(volatile unsigned long *a) {
  asm volatile ("lock incq %0"::"m"(*a):"memory");
}
static void inline atomic_dec(volatile unsigned long *a) {
  asm volatile ("lock decq %0"::"m"(*a):"memory");
}
static void barrier_init(barrier_t* b, unsigned long wait) {
  b->wait = wait;
  asm volatile ("mfence");
}
static void barrier_cross_nice(barrier_t* b) {
  atomic_dec(&b->wait);
  asm volatile ("mfence");
  while (b->wait)
    sched_yield();
  asm volatile ("mfence");
}
static void barrier_cross(barrier_t* b) {
  atomic_dec(&b->wait);
  asm volatile ("mfence");
  while (b->wait);
  asm volatile ("mfence");
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
  intset_t *set;
  barrier_t *init_barrier;
  barrier_t *go_barrier;
  barrier_t *stop_barrier;
  unsigned long nb_add;
  unsigned long nb_remove;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long total;
  unsigned short seed[3];
  int diff;
  int core;
  int range;
  int update;
  int alternate;
  unsigned long time;
  unsigned long ticks;
  const char *ptlcmd;
  char padding[64];
} thread_data_t;

void *test(void *data)
{
  int op, val, last = -1;
  thread_data_t *d = (thread_data_t *)data;
  struct timeval ts, te;
  unsigned long long is = 0, ie;

#ifdef TLS
  rng_seed = d->seed;
#else /* ! TLS */
  pthread_setspecific(rng_seed_key, d->seed);
#endif /* ! TLS */

  /* Create transaction */
  TM_INIT_THREAD;

  pin_cpu((int)d->core);

  barrier_cross_nice(d->init_barrier);
  if (d->ptlcmd != NULL) {
    /* Start simulator */
    ptlcall_single_flush(d->ptlcmd);
    /* Get time in simulation mode */
    gettimeofday(&ts, NULL);
    is = rdtsc();
  }
  barrier_cross(d->go_barrier);

  while (stop == 0) {
    op = rand_range(100, d->seed);
    if (op < d->update) {
      if (d->alternate) {
        /* Alternate insertions and removals */
        if (last < 0) {
          /* Add random value */
          val = rand_range(d->range, d->seed) + 1;
          if (set_add(d->set, val, TRANSACTIONAL)) {
            d->diff++;
            last = val;
          }
          d->nb_add++;
        } else {
          /* Remove last value */
          if (set_remove(d->set, last, TRANSACTIONAL))
            d->diff--;
          d->nb_remove++;
          last = -1;
        }
      } else {
        /* Randomly perform insertions and removals */
        val = rand_range(d->range, d->seed) + 1;
        if ((op & 0x01) == 0) {
          /* Add random value */
          if (set_add(d->set, val, TRANSACTIONAL))
            d->diff++;
          d->nb_add++;
        } else {
          /* Remove random value */
          if (set_remove(d->set, val, TRANSACTIONAL))
            d->diff--;
          d->nb_remove++;
        }
      }
    } else {
      /* Look for random value */
      val = rand_range(d->range, d->seed) + 1;
      if (set_contains(d->set, val, TRANSACTIONAL))
        d->nb_found++;
      d->nb_contains++;
    }
    if (d->nb_contains + d->nb_add + d->nb_remove >= d->total)
      stop = 1;
  }

  barrier_cross(d->stop_barrier);
  if (d->ptlcmd != NULL) {
    /* Get time in simulation mode */
    gettimeofday(&te, NULL);
    ie = rdtsc();
    /* Stop simulator */
    ptlcall_switch_to_native();
    d->time = (te.tv_sec * 1000000UL + te.tv_usec) - (ts.tv_sec * 1000000UL + ts.tv_usec);
    d->ticks = (unsigned long)(ie - is);
  }
  /* Free transaction */
  TM_EXIT_THREAD;

  return NULL;
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"do-not-alternate",          no_argument,       NULL, 'a'},
    {"contention-manager",        required_argument, NULL, 'c'},
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"ptl-cmd",                   required_argument, NULL, 'p'},
    {"range",                     required_argument, NULL, 'r'},
    {"seed",                      required_argument, NULL, 's'},
    {"update-rate",               required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0}
  };

  intset_t *set;
  int i, c, val, size, ret;
  unsigned long reads, updates;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  cpu_set_t cpuset;
  barrier_t init_barrier, go_barrier, stop_barrier;
  int duration = DEFAULT_DURATION;
  int initial = DEFAULT_INITIAL;
  int nb_threads = DEFAULT_NB_THREADS;
  const char* ptlcmd = NULL;
  int range = DEFAULT_RANGE;
  int seed = DEFAULT_SEED;
  int update = DEFAULT_UPDATE;
  char *cm = NULL;
  int alternate = 1;
  unsigned short main_seed[3];

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "hac:d:i:n:p:r:s:u:"
#ifdef USE_LINKEDLIST
                    "x"
#endif /* LINKEDLIST */
                    , long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("intset -- STM stress test "
#if defined(USE_LINKEDLIST)
              "(linked list)\n"
#elif defined(USE_RBTREE)
              "(red-black tree)\n"
#elif defined(USE_SKIPLIST)
              "(skip list)\n"
#elif defined(USE_HASHSET)
              "(hash set)\n"
#endif /* defined(USE_HASHSET) */
              "\n"
              "Usage:\n"
              "  intset [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -a, --do-not-alternate\n"
              "        Do not alternate insertions and removals\n"
              "  -c, --contention-manager <string>\n"
              "        Contention manager for resolving conflicts (default=suicide)\n"
              "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --initial-size <int>\n"
              "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -p, --ptl-cmd <int>\n"
              "        Comment for PTLsim (default=\"\")\n"
              "  -r, --range <int>\n"
              "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
              "  -u, --update-rate <int>\n"
              "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
         );
       exit(0);
     case 'a':
       alternate = 0;
       break;
     case 'c':
       cm = optarg;
       break;
     case 'd':
       duration = atoi(optarg);
       break;
     case 'i':
       initial = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'p':
       ptlcmd = optarg;
       break;
     case 'r':
       range = atoi(optarg);
       break;
     case 's':
       seed = atoi(optarg);
       break;
     case 'u':
       update = atoi(optarg);
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(range > 0 && range >= initial);
  assert(update >= 0 && update <= 100);

#if defined(USE_LINKEDLIST)
  printf("Set type     : linked list\n");
#elif defined(USE_RBTREE)
  printf("Set type     : red-black tree\n");
#elif defined(USE_SKIPLIST)
  printf("Set type     : skip list\n");
#elif defined(USE_HASHSET)
  printf("Set type     : hash set\n");
#endif /* defined(USE_HASHSET) */
  printf("CM           : %s\n", (cm == NULL ? "DEFAULT" : cm));
  printf("Duration     : %d\n", duration);
  printf("Initial size : %d\n", initial);
  printf("Nb threads   : %d\n", nb_threads);
  printf("Value range  : %d\n", range);
  printf("PTL command  : %s\n", ptlcmd);
  printf("Seed         : %d\n", seed);
  printf("Update rate  : %d\n", update);
  printf("Alternate    : %d\n", alternate);
  printf("Type sizes   : int=%d/long=%d/ptr=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *));

  if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }

  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);

  set = set_new(INIT_SET_PARAMETERS);

  stop = 0;

  /* Thread-local seed for main thread */
  rand_init(main_seed);
#ifdef TLS
  rng_seed = main_seed;
#else /* ! TLS */
  if (pthread_key_create(&rng_seed_key, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
  pthread_setspecific(rng_seed_key, main_seed);
#endif /* ! TLS */

  /* Init STM */
  printf("Initializing STM\n");
  TM_INIT;

  if (alternate == 0 && range != initial * 2)
    printf("WARNING: range is not twice the initial set size\n");

  /* Populate set */
  printf("Adding %d entries to set\n", initial);
  i = 0;
  while (i < initial) {
    val = rand_range(range, main_seed) + 1;
    if (set_add(set, val, 0))
      i++;
  }
  size = set_size(set);
  printf("Set size     : %d\n", size);

  /* Access set from all threads */
  barrier_init(&init_barrier, nb_threads);
  barrier_init(&go_barrier, nb_threads);
  barrier_init(&stop_barrier, nb_threads);

  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].range = range;
    data[i].update = update;
    data[i].alternate = alternate;
    data[i].time = data[i].ticks = 0;
    data[i].ptlcmd = (i == 0 ? ptlcmd : NULL);
    data[i].nb_add = 0;
    data[i].nb_remove = 0;
    data[i].nb_contains = 0;
    data[i].nb_found = 0;
    data[i].total = duration;
    data[i].diff = 0;
    data[i].core = i;
    rand_init(data[i].seed);
    data[i].set = set;
    data[i].init_barrier = &init_barrier;
    data[i].go_barrier = &go_barrier;
    data[i].stop_barrier = &stop_barrier;
  }

  /* Thread 0 will be run by main thread */
  for (i = 1; i < nb_threads; i++) {
    /* Pin threads early to CPUs to avoid additional balancing runs through Linux' scheduler */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);

    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
    pthread_attr_destroy(&attr);
  }

  printf("STARTING...\n");

  /* Start main thread */
  test((void *)(&data[0]));

  printf("STOPPING...\n");

  /* Wait for thread completion */
  for (i = 1; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

  reads = 0;
  updates = 0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %lu\n", data[i].nb_add);
    printf("  #remove     : %lu\n", data[i].nb_remove);
    printf("  #contains   : %lu\n", data[i].nb_contains);
    printf("  #found      : %lu\n", data[i].nb_found);
    reads += data[i].nb_contains;
    updates += (data[i].nb_add + data[i].nb_remove);
    size += data[i].diff;
  }
  printf("Set size      : %d (expected: %d)\n", set_size(set), size);
  ret = (set_size(set) != size);
  printf("Duration      : %lu us (%lu ticks)\n", data[0].time, data[0].ticks);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (double)(reads + updates) * 1000000.0 / (double)data[0].time);
  printf("#read txs     : %lu (%f / s)\n", reads, (double)reads * 1000000.0 / (double)data[0].time);
  printf("#update txs   : %lu (%f / s)\n", updates, (double)updates * 1000000.0 / (double)data[0].time);

  /* Delete set */
  set_delete(set);

  /* Cleanup STM */
  TM_EXIT;

#ifndef TLS
  pthread_key_delete(rng_seed_key);
#endif /* ! TLS */

  free(threads);
  free(data);

  return ret;
}
