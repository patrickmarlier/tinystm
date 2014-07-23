/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Integer set stress test.
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

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include <atomic_ops.h>

#include "stm.h"
#include "mod_mem.h"

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#if !(defined(USE_LINKEDLIST) || defined(USE_RBTREE) || defined(USE_SKIPLIST))
# error "Must define USE_LINKEDLIST or USE_RBTREE or USE_SKIPLIST"
#endif /* !(defined(USE_LINKEDLIST) || defined(USE_RBTREE) || defined(USE_SKIPLIST)) */

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#define RO                              1
#define RW                              0
#define START(id, ro)                   { sigjmp_buf *_e = stm_get_env(); stm_tx_attr_t _a = {id, ro}; sigsetjmp(*_e, 0); stm_start(_e, &_a)
#define START_TS(ts, label)             { sigjmp_buf *_e = stm_get_env(); if (sigsetjmp(*_e, 0)) goto label; stm_start(_e, NULL); stm_set_extension(0, &ts)
#define LOAD(addr)                      stm_load((stm_word_t *)addr)
#define UNIT_LOAD(addr, ts)             stm_unit_load((stm_word_t *)addr, ts)
#define STORE(addr, value)              stm_store((stm_word_t *)addr, (stm_word_t)value)
#define UNIT_STORE(addr, value, ts)     stm_unit_store((stm_word_t *)addr, (stm_word_t)value, ts)
#define COMMIT                          stm_commit(); }
#define MALLOC(size)                    stm_malloc(size)
#define FREE(addr, size)                stm_free(addr, size)

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0xFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile AO_t stop;
static unsigned int global_seed;

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### */

#ifdef TLS
static __thread unsigned int *rng_seed;
#else /* ! TLS */
static pthread_key_t rng_seed_key;
#endif /* ! TLS */

#if defined(USE_LINKEDLIST)

/* ################################################################### *
 * LINKEDLIST
 * ################################################################### */

# define TRANSACTIONAL                  (d->unit_tx + 1)

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

node_t *new_node(val_t val, node_t *next, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t));
  } else {
    node = (node_t *)MALLOC(sizeof(node_t));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->next = next;

  return node;
}

intset_t *set_new()
{
  intset_t *set;
  node_t *min, *max;

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(VAL_MAX, NULL, 0);
  min = new_node(VAL_MIN, max, 0);
  set->head = min;

  return set;
}

void set_delete(intset_t *set)
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

int set_size(intset_t *set)
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

int set_contains(intset_t *set, val_t val, int transactional)
{
  int result;
  node_t *prev, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (transactional == 0) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
  } else if (transactional == 1) {
    START(0, RO);
    prev = (node_t *)LOAD(&set->head);
    next = (node_t *)LOAD(&prev->next);
    while (1) {
      v = LOAD(&next->val);
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)LOAD(&prev->next);
    }
    result = (v == val);
    COMMIT;
  } else {
    /* Unit transactions */
    stm_word_t ts, start_ts, val_ts;
  restart:
    start_ts = stm_get_clock();
    /* Head node is never removed */
    prev = (node_t *)UNIT_LOAD(&set->head, &ts);
    next = (node_t *)UNIT_LOAD(&prev->next, &ts);
    if (ts > start_ts)
      start_ts = ts;
    while (1) {
      v = UNIT_LOAD(&next->val, &val_ts);
      if (val_ts > start_ts) {
        /* Restart traversal (could also backtrack) */
        goto restart;
      }
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)UNIT_LOAD(&prev->next, &ts);
      if (ts > start_ts) {
        /* Verify that node has not been modified (value and pointer are updated together) */
        UNIT_LOAD(&prev->val, &val_ts);
        if (val_ts > start_ts) {
          /* Restart traversal (could also backtrack) */
          goto restart;
        }
        start_ts = ts;
      }
    }
    result = (v == val);
  }

  return result;
}

int set_add(intset_t *set, val_t val, int transactional)
{
  int result;
  node_t *prev, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (transactional == 0) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val != val);
    if (result) {
      prev->next = new_node(val, next, transactional);
    }
  } else if (transactional <= 2) {
    START(1, RW);
    prev = (node_t *)LOAD(&set->head);
    next = (node_t *)LOAD(&prev->next);
    while (1) {
      v = LOAD(&next->val);
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)LOAD(&prev->next);
    }
    result = (v != val);
    if (result) {
      STORE(&prev->next, new_node(val, next, transactional));
    }
    COMMIT;
  } else {
    /* Unit transactions */
    stm_word_t ts, start_ts, val_ts;
  restart:
    start_ts = stm_get_clock();
    /* Head node is never removed */
    prev = (node_t *)UNIT_LOAD(&set->head, &ts);
    next = (node_t *)UNIT_LOAD(&prev->next, &ts);
    if (ts > start_ts)
      start_ts = ts;
    while (1) {
      v = UNIT_LOAD(&next->val, &val_ts);
      if (val_ts > start_ts) {
        /* Restart traversal (could also backtrack) */
        goto restart;
      }
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)UNIT_LOAD(&prev->next, &ts);
      if (ts > start_ts) {
        /* Verify that node has not been modified (value and pointer are updated together) */
        UNIT_LOAD(&prev->val, &val_ts);
        if (val_ts > start_ts) {
          /* Restart traversal (could also backtrack) */
          goto restart;
        }
        start_ts = ts;
      }
    }
    result = (v != val);
    if (result) {
      node_t *n = new_node(val, next, 0);
      /* Make sure that there are no concurrent updates to that memory location */
      if (!UNIT_STORE(&prev->next, n, &ts)) {
        free(n);
        goto restart;
      }
    }
  }

  return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
  int result;
  node_t *prev, *next;
  val_t v;
  node_t *n;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (transactional == 0) {
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
  } else if (transactional <= 3) {
    START(2, RW);
    prev = (node_t *)LOAD(&set->head);
    next = (node_t *)LOAD(&prev->next);
    while (1) {
      v = LOAD(&next->val);
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)LOAD(&prev->next);
    }
    result = (v == val);
    if (result) {
      n = (node_t *)LOAD(&next->next);
      STORE(&prev->next, n);
      /* Free memory (delayed until commit) */
      FREE(next, sizeof(node_t));
    }
    COMMIT;
  } else {
    /* Unit transactions */
    stm_word_t ts, start_ts, val_ts;
  restart:
    start_ts = stm_get_clock();
    /* Head node is never removed */
    prev = (node_t *)UNIT_LOAD(&set->head, &ts);
    next = (node_t *)UNIT_LOAD(&prev->next, &ts);
    if (ts > start_ts)
      start_ts = ts;
    while (1) {
      v = UNIT_LOAD(&next->val, &val_ts);
      if (val_ts > start_ts) {
        /* Restart traversal (could also backtrack) */
        goto restart;
      }
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)UNIT_LOAD(&prev->next, &ts);
      if (ts > start_ts) {
        /* Verify that node has not been modified (value and pointer are updated together) */
        UNIT_LOAD(&prev->val, &val_ts);
        if (val_ts > start_ts) {
          /* Restart traversal (could also backtrack) */
          goto restart;
        }
        start_ts = ts;
      }
    }
    result = (v == val);
    if (result) {
      /* Make sure that the transaction does not access versions more recent than start_ts */
      START_TS(start_ts, restart);
      n = (node_t *)LOAD(&next->next);
      STORE(&prev->next, n);
      /* Free memory (delayed until commit) */
      FREE(next, sizeof(node_t));
      COMMIT;
    }
  }

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

# define TM_SHARED_READ(var)            LOAD(&(var))
# define TM_SHARED_READ_P(var)          LOAD(&(var))

# define TM_SHARED_WRITE(var, val)      STORE(&(var), val)
# define TM_SHARED_WRITE_P(var, val)    STORE(&(var), val)

# define TM_MALLOC(size)                MALLOC(size)
# define TM_FREE(ptr)                   FREE(ptr, sizeof(*ptr))

# include "rbtree.h"

# include "rbtree.c"

typedef rbtree_t intset_t;
typedef intptr_t val_t;

static long compare(const void *a, const void *b)
{
  return ((val_t)a - (val_t)b);
}

intset_t *set_new()
{
  return rbtree_alloc(&compare);
}

void set_delete(intset_t *set)
{
  rbtree_free(set);
}

int set_size(intset_t *set)
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

int set_contains(intset_t *set, val_t val, int transactional)
{
  int result;

  if (!transactional) {
    result = rbtree_contains(set, (void *)val);
  } else {
    START(0, RO);
    result = TMrbtree_contains(set, (void *)val);
    COMMIT;
  }

  return result;
}

int set_add(intset_t *set, val_t val, int transactional)
{
  int result;

  if (!transactional) {
    result = rbtree_insert(set, (void *)val, (void *)val);
  } else {
    START(1, RW);
    result = TMrbtree_insert(set, (void *)val, (void *)val);
    COMMIT;
  }

  return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
  int result;

  if (!transactional) {
    result = rbtree_delete(set, (void *)val);
  } else {
    START(2, RW);
    result = TMrbtree_delete(set, (void *)val);
    COMMIT;
  }

  return result;
}

#elif defined(USE_SKIPLIST)

/* ################################################################### *
 * SKIPLIST
 * ################################################################### */

# define TRANSACTIONAL                  1

# define MAX_LEVEL                      64

# define INIT_SET_PARAMETERS            32, 50

typedef intptr_t val_t;
typedef intptr_t level_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
  val_t val;
  level_t level;
  struct node **forward;
} node_t;

typedef struct intset {
  node_t *head;
  node_t *tail;
  level_t level;
  int prob;
  int max_level;
} intset_t;

inline int rand_100()
{
# ifdef TLS
  return rand_r(rng_seed) % 100;
# else /* ! TLS */
  return rand_r((unsigned int *)pthread_getspecific(rng_seed_key)) % 100;
# endif /* ! TLS */
}

int random_level(intset_t *set)
{
  int l = 0;
  while (l < set->max_level && rand_100() < set->prob)
    l++;
  return l;
}

node_t *new_node(val_t val, level_t level, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t));
  } else {
    node = (node_t *)MALLOC(sizeof(node_t));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }
  if (!transactional) {
    node->forward = (node_t **)malloc((level + 1) * sizeof(node_t *));
  } else {
    node->forward = (node_t **)MALLOC((level + 1) * sizeof(node_t *));
  }
  if (node->forward == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->level = level;

  return node;
}

intset_t *set_new(level_t max_level, int prob)
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
  set->tail = new_node(VAL_MAX, max_level, 0);
  set->head = new_node(VAL_MIN, max_level, 0);
  for (i = 0; i <= max_level; i++) {
    set->head->forward[i] = set->tail;
    set->tail->forward[i] = NULL;
  }

  return set;
}

void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->forward[0];
    free(node->forward);
    free(node);
    node = next;
  }
  free(set);
}

int set_size(intset_t *set)
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

int set_contains(intset_t *set, val_t val, int transactional)
{
  int result, i;
  node_t *node, *next;
  val_t v;

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
    START(0, RO);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    for (i = LOAD(&set->level); i >= 0; i--) {
      next = (node_t *)LOAD(&node->forward[i]);
      while (1) {
        v = LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)LOAD(&node->forward[i]);
      }
    }
    node = (node_t *)LOAD(&node->forward[0]);
    result = (v == val);
    COMMIT;
  }

  return result;
}

int set_add(intset_t *set, val_t val, int transactional)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level, l;
  val_t v;

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
      node = new_node(val, l, transactional);
      for (i = 0; i <= l; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
      }
      result = 1;
    }
  } else {
    START(1, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)LOAD(&node->forward[i]);
      while (1) {
        v = LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)LOAD(&node->forward[i]);
      }
      update[i] = node;
    }
    node = (node_t *)LOAD(&node->forward[0]);

    if (v == val) {
      result = 0;
    } else {
      l = random_level(set);
      if (l > level) {
        for (i = level + 1; i <= l; i++)
          update[i] = set->head;
        STORE(&set->level, l);
      }
      node = new_node(val, l, transactional);
      for (i = 0; i <= l; i++) {
        node->forward[i] = (node_t *)LOAD(&update[i]->forward[i]);
        STORE(&update[i]->forward[i], node);
      }
      result = 1;
    }
    COMMIT;
  }

  return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level;
  val_t v;

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
      free(node->forward);
      free(node);
      result = 1;
    }
  } else {
    START(2, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)LOAD(&node->forward[i]);
      while (1) {
        v = LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)LOAD(&node->forward[i]);
      }
      update[i] = node;
    }
    node = (node_t *)LOAD(&node->forward[0]);

    if (v != val) {
      result = 0;
    } else {
      for (i = 0; i <= level; i++) {
        if ((node_t *)LOAD(&update[i]->forward[i]) == node)
          STORE(&update[i]->forward[i], (node_t *)LOAD(&node->forward[i]));
      }
      i = level;
      while (i > 0 && (node_t *)LOAD(&set->head->forward[i]) == set->tail)
        i--;
      if (i != level)
        STORE(&set->level, i);
      /* Free memory (delayed until commit) */
      FREE(node->forward, (node->level + 1) * sizeof(node_t *));
      FREE(node, sizeof(node_t));
      result = 1;
    }
    COMMIT;
  }

  return result;
}

#endif /* defined(USE_SKIPLIST) */

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
  int range;
  int update;
#ifdef USE_LINKEDLIST
  int unit_tx;
#endif /* LINKEDLIST */
  unsigned long nb_add;
  unsigned long nb_remove;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long nb_aborts;
  unsigned long nb_aborts_locked_read;
  unsigned long nb_aborts_locked_write;
  unsigned long nb_aborts_validate_read;
  unsigned long nb_aborts_validate_write;
  unsigned long nb_aborts_validate_commit;
  unsigned long nb_aborts_invalid_memory;
  unsigned long nb_aborts_reallocate;
  unsigned long nb_aborts_rollover;
  unsigned long locked_reads_ok;
  unsigned long locked_reads_failed;
  unsigned long max_retries;
  int diff;
  unsigned int seed;
  intset_t *set;
  barrier_t *barrier;
} thread_data_t;

void *test(void *data)
{
  int val, last = 0;
  thread_data_t *d = (thread_data_t *)data;

#ifdef TLS
  rng_seed = &d->seed;
#else /* ! TLS */
  pthread_setspecific(rng_seed_key, &d->seed);
#endif /* ! TLS */

  /* Create transaction */
  stm_init_thread();
  /* Wait on barrier */
  barrier_cross(d->barrier);

  last = -1;
  while (stop == 0) {
    val = rand_r(&d->seed) % 100;
    if (val < d->update) {
      if (last < 0) {
        /* Add random value */
        val = (rand_r(&d->seed) % d->range) + 1;
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
      /* Look for random value */
      val = (rand_r(&d->seed) % d->range) + 1;
      if (set_contains(d->set, val, TRANSACTIONAL))
        d->nb_found++;
      d->nb_contains++;
    }
  }
  stm_get_stats("nb_aborts", &d->nb_aborts);
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read);
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write);
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit);
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory);
  stm_get_stats("nb_aborts_reallocate", &d->nb_aborts_reallocate);
  stm_get_stats("nb_aborts_rollover", &d->nb_aborts_rollover);
  stm_get_stats("locked_reads_ok", &d->locked_reads_ok);
  stm_get_stats("locked_reads_failed", &d->locked_reads_failed);
  stm_get_stats("max_retries", &d->max_retries);
  /* Free transaction */
  stm_exit_thread();

  return NULL;
}

void catcher(int sig)
{
  static int nb = 0;
  printf("CAUGHT SIGNAL %d\n", sig);
  if (++nb >= 3)
    exit(1);
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"range",                     required_argument, NULL, 'r'},
    {"seed",                      required_argument, NULL, 's'},
    {"update-rate",               required_argument, NULL, 'u'},
    {"unit-tx",                   no_argument,       NULL, 'x'},
    {NULL, 0, NULL, 0}
  };

  intset_t *set;
  int i, c, val, size;
  char *s;
  unsigned long reads, updates, aborts, aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, aborts_reallocate, aborts_rollover,
    locked_reads_ok, locked_reads_failed, max_retries;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;
  int duration = DEFAULT_DURATION;
  int initial = DEFAULT_INITIAL;
  int nb_threads = DEFAULT_NB_THREADS;
  int range = DEFAULT_RANGE;
  int seed = DEFAULT_SEED;
  int update = DEFAULT_UPDATE;
#ifdef USE_LINKEDLIST
  int unit_tx = 0;
#endif /* LINKEDLIST */
  sigset_t block_set;

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "hd:i:n:r:s:u:"
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
#endif /* defined(USE_SKIPLIST) */
              "\n"
              "Usage:\n"
              "  intset [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --initial-size <int>\n"
              "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -r, --range <int>\n"
              "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
              "  -u, --update-rate <int>\n"
              "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
#ifdef USE_LINKEDLIST
              "  -x, --unit-tx\n"
              "        Use unit transactions\n"
#endif /* LINKEDLIST */
         );
       exit(0);
     case 'd':
       duration = atoi(optarg);
       break;
     case 'i':
       initial = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
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
#ifdef USE_LINKEDLIST
     case 'x':
       unit_tx++;
       break;
#endif /* LINKEDLIST */
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
#endif /* defined(USE_SKIPLIST) */
  printf("Duration     : %d\n", duration);
  printf("Initial size : %d\n", initial);
  printf("Nb threads   : %d\n", nb_threads);
  printf("Value range  : %d\n", range);
  printf("Seed         : %d\n", seed);
  printf("Update rate  : %d\n", update);
#ifdef USE_LINKEDLIST
  printf("Unit tx      : %d\n", unit_tx);
#endif /* LINKEDLIST */
  printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(stm_word_t));

  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;

  if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }

  if (seed == 0)
    srand((int)time(0));
  else
    srand(seed);

  set = set_new(INIT_SET_PARAMETERS);

  stop = 0;

  global_seed = rand();
#ifdef TLS
  rng_seed = &global_seed;
#else /* ! TLS */
  if (pthread_key_create(&rng_seed_key, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
  pthread_setspecific(rng_seed_key, &global_seed);
#endif /* ! TLS */

  /* Init STM */
  printf("Initializing STM\n");
  stm_init();
  mod_mem_init();

  if (stm_get_parameter("compile_flags", &s))
    printf("STM flags    : %s\n", s);

  /* Populate set */
  printf("Adding %d entries to set\n", initial);
  i = 0;
  while (i < initial) {
    val = (rand() % range) + 1;
    if (set_add(set, val, 0))
      i++;
  }
  size = set_size(set);
  printf("Set size     : %d\n", size);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].range = range;
    data[i].update = update;
#ifdef USE_LINKEDLIST
    data[i].unit_tx = unit_tx;
#endif /* LINKEDLIST */
    data[i].nb_add = 0;
    data[i].nb_remove = 0;
    data[i].nb_contains = 0;
    data[i].nb_found = 0;
    data[i].nb_aborts = 0;
    data[i].nb_aborts_locked_read = 0;
    data[i].nb_aborts_locked_write = 0;
    data[i].nb_aborts_validate_read = 0;
    data[i].nb_aborts_validate_write = 0;
    data[i].nb_aborts_validate_commit = 0;
    data[i].nb_aborts_invalid_memory = 0;
    data[i].nb_aborts_reallocate = 0;
    data[i].nb_aborts_rollover = 0;
    data[i].locked_reads_ok = 0;
    data[i].locked_reads_failed = 0;
    data[i].max_retries = 0;
    data[i].diff = 0;
    data[i].seed = rand();
    data[i].set = set;
    data[i].barrier = &barrier;
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);

  /* Catch some signals */
  if (signal(SIGHUP, catcher) == SIG_ERR ||
      signal(SIGINT, catcher) == SIG_ERR ||
      signal(SIGTERM, catcher) == SIG_ERR) {
    perror("signal");
    exit(1);
  }
 
  /* Start threads */
  barrier_cross(&barrier);

  printf("STARTING...\n");
  gettimeofday(&start, NULL);
  if (duration > 0) {
    nanosleep(&timeout, NULL);
  } else {
    sigemptyset(&block_set);
    sigsuspend(&block_set);
  }
  AO_store_full(&stop, 1);
  gettimeofday(&end, NULL);
  printf("STOPPING...\n");

  /* Wait for thread completion */
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
  aborts = 0;
  aborts_locked_read = 0;
  aborts_locked_write = 0;
  aborts_validate_read = 0;
  aborts_validate_write = 0;
  aborts_validate_commit = 0;
  aborts_invalid_memory = 0;
  aborts_reallocate = 0;
  aborts_rollover = 0;
  locked_reads_ok = 0;
  locked_reads_failed = 0;
  reads = 0;
  updates = 0;
  max_retries = 0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %lu\n", data[i].nb_add);
    printf("  #remove     : %lu\n", data[i].nb_remove);
    printf("  #contains   : %lu\n", data[i].nb_contains);
    printf("  #found      : %lu\n", data[i].nb_found);
    printf("  #aborts     : %lu\n", data[i].nb_aborts);
    printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
    printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
    printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
    printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
    printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
    printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
    printf("    #realloc  : %lu\n", data[i].nb_aborts_reallocate);
    printf("    #r-over   : %lu\n", data[i].nb_aborts_rollover);
    printf("  #lr-ok      : %lu\n", data[i].locked_reads_ok);
    printf("  #lr-failed  : %lu\n", data[i].locked_reads_failed);
    printf("  Max retries : %lu\n", data[i].max_retries);
    aborts += data[i].nb_aborts;
    aborts_locked_read += data[i].nb_aborts_locked_read;
    aborts_locked_write += data[i].nb_aborts_locked_write;
    aborts_validate_read += data[i].nb_aborts_validate_read;
    aborts_validate_write += data[i].nb_aborts_validate_write;
    aborts_validate_commit += data[i].nb_aborts_validate_commit;
    aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
    aborts_reallocate += data[i].nb_aborts_reallocate;
    aborts_rollover += data[i].nb_aborts_rollover;
    locked_reads_ok += data[i].locked_reads_ok;
    locked_reads_failed += data[i].locked_reads_failed;
    reads += data[i].nb_contains;
    updates += (data[i].nb_add + data[i].nb_remove);
    size += data[i].diff;
    if (max_retries < data[i].max_retries)
      max_retries = data[i].max_retries;
  }
  printf("Set size      : %d (expected: %d)\n", set_size(set), size);
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);
  printf("#aborts       : %lu (%f / s)\n", aborts, aborts * 1000.0 / duration);
  printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read, aborts_locked_read * 1000.0 / duration);
  printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write, aborts_locked_write * 1000.0 / duration);
  printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read, aborts_validate_read * 1000.0 / duration);
  printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write, aborts_validate_write * 1000.0 / duration);
  printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit, aborts_validate_commit * 1000.0 / duration);
  printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory, aborts_invalid_memory * 1000.0 / duration);
  printf("  #realloc    : %lu (%f / s)\n", aborts_reallocate, aborts_reallocate * 1000.0 / duration);
  printf("  #r-over     : %lu (%f / s)\n", aborts_rollover, aborts_rollover * 1000.0 / duration);
  printf("#lr-ok        : %lu (%f / s)\n", locked_reads_ok, locked_reads_ok * 1000.0 / duration);
  printf("#lr-failed    : %lu (%f / s)\n", locked_reads_failed, locked_reads_failed * 1000.0 / duration);
  printf("Max retries   : %lu\n", max_retries);

  /* Delete set */
  set_delete(set);

  /* Cleanup STM */
  stm_exit();

#ifndef TLS
  pthread_key_delete(rng_seed_key);
#endif /* ! TLS */

  free(threads);
  free(data);

  return 0;
}
