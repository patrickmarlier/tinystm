/*
 * File:
 *   mod_ab.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for gathering statistics about atomic blocks.
 *
 * Copyright (c) 2007-2010.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "mod_ab.h"

#include "atomic.h"
#include "stm.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

#define NB_ATOMIC_BLOCKS                64
#define BUFFER_SIZE                     1024
#define DEFAULT_SAMPLING_PERIOD         1024

typedef struct smart_counter {          /* Smart counter */
  unsigned long samples;                /* Number of samples */
  double mean;                          /* Mean */
  double variance;                      /* Variance */
  double min;                           /* Minimum */
  double max;                           /* Maximum */
} smart_counter_t;

typedef struct ab_stats {               /* Atomic block statistics */
  int id;                               /* Atomic block identifier */
  struct ab_stats *next;                /* Next atomic block */
  smart_counter_t stats;                /* Length statistics */
} ab_stats_t;

typedef struct samples_buffer {         /* Buffer to hold samples */
  struct {
    int id;                             /* Atomic block identifier */
    unsigned long length;               /* Transaction length */
  } buffer[BUFFER_SIZE];                /* Buffer */
  int nb;                               /* Number of samples */
  unsigned long total;                  /* Total number of valid samples seen by thread so far */
  uint64_t start;                       /* Start time of the current transaction */
} samples_buffer_t;

static int mod_ab_key;
static int mod_ab_initialized = 0;
static int sampling_period;             /* Inverse sampling frequency */
static int (*check_fn)(TXPARAM);        /* Function to check sample validity */

static pthread_mutex_t update_mutex;    /* Mutex to update global statistics */

static ab_stats_t *ab_list[NB_ATOMIC_BLOCKS];

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Initialize smart counter.
 */
static void sc_init(smart_counter_t *c)
{
  c->samples = 0;
  c->mean = c->variance = c->min = c->max = 0;
}

/*
 * Add sample in smart counter.
 */
static void sc_add_sample(smart_counter_t *c, double n)
{
  double prev = c->mean;
  if (c->samples == 0)
    c->min = c->max = n;
  else if (n < c->min)
    c->min = n;
  else if (n > c->max)
    c->max = n;
  c->samples++;
  c->mean = c->mean + (n - c->mean) / (double)c->samples;
  c->variance = c->variance + (n - prev) * (n - c->mean);
}

/*
 * Get number of samples of smart counter.
 */
static int sc_samples(smart_counter_t *c)
{
  return c->samples;
}

/*
 * Get mean of smart counter.
 */
static double sc_mean(smart_counter_t *c)
{
  return c->mean;
}

/*
 * Get variance of smart counter.
 */
static double sc_variance(smart_counter_t *c)
{
  if(c->samples <= 1)
    return 0.0;
  return c->variance / (c->samples - 1);
}

/*
 * Get min of smart counter.
 */
static double sc_min(smart_counter_t *c)
{
  return c->min;
}

/*
 * Get max of smart counter.
 */
static double sc_max(smart_counter_t *c)
{
  return c->max;
}

/*
 * Returns the instruction count.
 */
static inline uint64_t rdtsc() {
  uint32_t lo, hi;
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return (uint64_t)hi << 32 | lo;
}

/*
 * Add samples to global stats.
 */
static void sc_add_samples(samples_buffer_t *samples)
{
  int i, id, bucket;
  ab_stats_t *ab;

  pthread_mutex_lock(&update_mutex);
  for (i = 0; i < samples->nb; i++) {
    id = samples->buffer[i].id;
    /* Find bucket */
    bucket = abs(id) % NB_ATOMIC_BLOCKS;
    /* Search for entry in bucket */
    ab = ab_list[bucket];
    while (ab != NULL && ab->id != id)
      ab = ab->next;
    if (ab == NULL) {
      /* No entry yet: create one */
      if ((ab = (ab_stats_t *)malloc(sizeof(ab_stats_t))) == NULL) {
        perror("malloc");
        exit(1);
      }
      ab->id = id;
      ab->next = ab_list[bucket];
      sc_init(&ab->stats);
      ab_list[bucket] = ab;
    }
    sc_add_sample(&ab->stats, samples->buffer[i].length);
  }
  samples->nb = 0;
  pthread_mutex_unlock(&update_mutex);
}

/*
 * Clean up module.
 */
static void cleanup()
{
  pthread_mutex_destroy(&update_mutex);
}

/*
 * Called upon thread creation.
 */
static void mod_ab_on_thread_init(TXPARAMS void *arg)
{
  samples_buffer_t *samples;

  if ((samples = (samples_buffer_t *)malloc(sizeof(samples_buffer_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  samples->nb = 0;
  stm_set_specific(TXARGS mod_ab_key, samples);
}

/*
 * Called upon thread deletion.
 */
static void mod_ab_on_thread_exit(TXPARAMS void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)stm_get_specific(TXARGS mod_ab_key);
  assert(samples != NULL);

  sc_add_samples(samples);

  free(samples);
}

/*
 * Called upon transaction start.
 */
static void mod_print_on_start(TXPARAMS void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)stm_get_specific(TXARGS mod_ab_key);
  assert(samples != NULL);

  samples->start = rdtsc();
}

/*
 * Called upon transaction commit.
 */
static void mod_ab_on_commit(TXPARAMS void *arg)
{
  samples_buffer_t *samples;
  stm_tx_attr_t *attrs;
  unsigned long length;

  samples = (samples_buffer_t *)stm_get_specific(TXARGS mod_ab_key);
  assert(samples != NULL);

  if (check_fn == NULL || check_fn(TXARG)) {
    length = rdtsc() - samples->start;
    samples->total++;
    /* Should be keep this sample? */
    if ((samples->total % sampling_period) == 0) {
      attrs = stm_get_attributes(TXARG);
      samples->buffer[samples->nb].id = (attrs == NULL ? 0 : attrs->id);
      samples->buffer[samples->nb].length = length;
      /* Is buffer full? */
      if (++samples->nb == BUFFER_SIZE) {
        /* Accumulate in global stats (and empty buffer) */
        sc_add_samples(samples);
      }
    }
  }
}

/*
 * Called upon transaction abort.
 */
static void mod_ab_on_abort(TXPARAMS void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)stm_get_specific(TXARGS mod_ab_key);
  assert(samples != NULL);

  samples->start = rdtsc();
}

/*
 * Return statistics about atomic block.
 */
int stm_get_ab_stats(int id, stm_ab_stats_t *stats)
{
  int bucket, result;
  ab_stats_t *ab;

  result = 0;
  pthread_mutex_lock(&update_mutex);
  /* Find bucket */
  bucket = abs(id) % NB_ATOMIC_BLOCKS;
  /* Search for entry in bucket */
  ab = ab_list[bucket];
  while (ab != NULL && ab->id != id)
    ab = ab->next;
  if (ab != NULL) {
    stats->samples = sc_samples(&ab->stats);
    stats->mean = sc_mean(&ab->stats);
    stats->variance = sc_variance(&ab->stats);
    stats->min = sc_min(&ab->stats);
    stats->max = sc_max(&ab->stats);
    result = 1;
  }
  pthread_mutex_unlock(&update_mutex);

  return result;
}

/*
 * Initialize module.
 */
void mod_ab_init(int freq, int (*check)(TXPARAM))
{
  int i;

  if (mod_ab_initialized)
    return;

  sampling_period = (freq <= 0 ? DEFAULT_SAMPLING_PERIOD : freq);
  check_fn = check;

  stm_register(mod_ab_on_thread_init, mod_ab_on_thread_exit, mod_print_on_start, mod_ab_on_commit, mod_ab_on_abort, NULL);
  mod_ab_key = stm_create_specific();
  if (mod_ab_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  if (pthread_mutex_init(&update_mutex, NULL) != 0) {
    fprintf(stderr, "Error creating mutex\n");
    exit(1);
  }
  for (i = 0; i < NB_ATOMIC_BLOCKS; i++)
    ab_list[i] = NULL;
  atexit(cleanup);
  mod_ab_initialized = 1;
}
