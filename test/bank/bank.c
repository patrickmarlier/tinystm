/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Bank stress test.
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

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#define RO                              1
#define RW                              0
#define START(id, ro)                   { sigjmp_buf *_e = stm_get_env(); stm_tx_attr_t _a = {id, ro}; sigsetjmp(*_e, 0); stm_start(_e, &_a)
#define LOAD(addr)                      stm_load((stm_word_t *)addr)
#define STORE(addr, value)              stm_store((stm_word_t *)addr, (stm_word_t)value)
#define COMMIT                          stm_commit(); }

#define DEFAULT_DURATION                10000
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_SEED                    0
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile AO_t stop;

/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account {
  int number;
  int balance;
} account_t;

typedef struct bank {
  account_t *accounts;
  int size;
} bank_t;

int transfer(account_t *src, account_t *dst, int amount)
{
  int i;

  /* Allow overdrafts */
  START(0, RW);
  i = (int)LOAD(&src->balance);
  i -= amount;
  STORE(&src->balance, i);
  i = (int)LOAD(&dst->balance);
  i += amount;
  STORE(&dst->balance, i);
  COMMIT;

  return amount;
}

int total(bank_t *bank, int transactional)
{
  int i, total;

  if (!transactional) {
    total = 0;
    for (i = 0; i < bank->size; i++) {
      total += bank->accounts[i].balance;
    }
  } else {
    START(1, RO);
    total = 0;
    for (i = 0; i < bank->size; i++) {
      total += (int)LOAD(&bank->accounts[i].balance);
    }
    COMMIT;
  }

  return total;
}

void reset(bank_t *bank)
{
  int i;

  START(2, RW);
  for (i = 0; i < bank->size; i++) {
    STORE(&bank->accounts[i].balance, 0);
  }
  COMMIT;
}

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
  int id;
  int read_all;
  int read_threads;
  int write_all;
  int write_threads;
  unsigned long nb_transfer;
  unsigned long nb_read_all;
  unsigned long nb_write_all;
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
  unsigned int seed;
  bank_t *bank;
  barrier_t *barrier;
} thread_data_t;

void *test(void *data)
{
  int src, dst, nb;
  thread_data_t *d = (thread_data_t *)data;

  /* Create transaction */
  stm_init_thread();
  /* Wait on barrier */
  barrier_cross(d->barrier);

  while (stop == 0) {
    if (d->id < d->read_threads) {
      /* Read all */
      total(d->bank, 1);
      d->nb_read_all++;
    } else if (d->id < d->read_threads + d->write_threads) {
      /* Write all */
      reset(d->bank);
      d->nb_write_all++;
    } else {
      nb = rand_r(&d->seed) % 100;
      if (nb < d->read_all) {
        /* Read all */
        total(d->bank, 1);
        d->nb_read_all++;
      } else if (nb < d->read_all + d->write_all) {
        /* Write all */
        reset(d->bank);
        d->nb_write_all++;
      } else {
        /* Choose random accounts */
        src = rand_r(&d->seed) % d->bank->size;
        dst = rand_r(&d->seed) % d->bank->size;
        if (dst == src)
          dst = (src + 1) % d->bank->size;
        transfer(&d->bank->accounts[src], &d->bank->accounts[dst], 1);
        d->nb_transfer++;
      }
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
    {"accounts",                  required_argument, NULL, 'a'},
    {"duration",                  required_argument, NULL, 'd'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"read-all-rate",             required_argument, NULL, 'r'},
    {"read-threads",              required_argument, NULL, 'R'},
    {"seed",                      required_argument, NULL, 's'},
    {"write-all-rate",            required_argument, NULL, 'w'},
    {"write-threads",             required_argument, NULL, 'W'},
    {NULL, 0, NULL, 0}
  };

  bank_t *bank;
  int i, c;
  char *s;
  unsigned long reads, writes, updates, aborts, aborts_locked_read, aborts_locked_write,
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
  int nb_accounts = DEFAULT_NB_ACCOUNTS;
  int nb_threads = DEFAULT_NB_THREADS;
  int read_all = DEFAULT_READ_ALL;
  int read_threads = DEFAULT_READ_THREADS;
  int seed = DEFAULT_SEED;
  int write_all = DEFAULT_WRITE_ALL;
  int write_threads = DEFAULT_WRITE_THREADS;
  sigset_t block_set;

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "ha:d:n:r:R:s:w:W:", long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("bank -- STM stress test\n"
              "\n"
              "Usage:\n"
              "  bank [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -a, --accounts <int>\n"
              "        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
              "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -r, --read-all-rate <int>\n"
              "        Percentage of read-all transactions (default=" XSTR(DEFAULT_READ_ALL) ")\n"
              "  -R, --read-threads <int>\n"
              "        Number of threads issuing only read-all transactions (default=" XSTR(DEFAULT_READ_THREADS) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
              "  -w, --write-all-rate <int>\n"
              "        Percentage of write-all transactions (default=" XSTR(DEFAULT_WRITE_ALL) ")\n"
              "  -W, --write-threads <int>\n"
              "        Number of threads issuing only write-all transactions (default=" XSTR(DEFAULT_WRITE_THREADS) ")\n"
         );
       exit(0);
     case 'a':
       nb_accounts = atoi(optarg);
       break;
     case 'd':
       duration = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'r':
       read_all = atoi(optarg);
       break;
     case 'R':
       read_threads = atoi(optarg);
       break;
     case 's':
       seed = atoi(optarg);
       break;
     case 'w':
       write_all = atoi(optarg);
       break;
     case 'W':
       write_threads = atoi(optarg);
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  assert(duration >= 0);
  assert(nb_accounts >= 2);
  assert(nb_threads > 0);
  assert(read_all >= 0 && write_all >= 0 && read_all + write_all <= 100);
  assert(read_threads + write_threads <= nb_threads);

  printf("Nb accounts    : %d\n", nb_accounts);
  printf("Duration       : %d\n", duration);
  printf("Nb threads     : %d\n", nb_threads);
  printf("Read-all rate  : %d\n", read_all);
  printf("Read threads   : %d\n", read_threads);
  printf("Seed           : %d\n", seed);
  printf("Write-all rate : %d\n", write_all);
  printf("Write threads  : %d\n", write_threads);
  printf("Type sizes     : int=%d/long=%d/ptr=%d/word=%d\n",
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

  bank = (bank_t *)malloc(sizeof(bank_t));
  bank->accounts = (account_t *)malloc(nb_accounts * sizeof(account_t));
  bank->size = nb_accounts;
  for (i = 0; i < bank->size; i++) {
    bank->accounts[i].number = i;
    bank->accounts[i].balance = 0;
  }

  stop = 0;

  /* Init STM */
  printf("Initializing STM\n");
  stm_init();

  if (stm_get_parameter("compile_flags", &s))
    printf("STM flags      : %s\n", s);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].id = i;
    data[i].read_all = read_all;
    data[i].read_threads = read_threads;
    data[i].write_all = write_all;
    data[i].write_threads = write_threads;
    data[i].nb_transfer = 0;
    data[i].nb_read_all = 0;
    data[i].nb_write_all = 0;
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
    data[i].seed = rand();
    data[i].bank = bank;
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
  writes = 0;
  updates = 0;
  max_retries = 0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #transfer   : %lu\n", data[i].nb_transfer);
    printf("  #read-all   : %lu\n", data[i].nb_read_all);
    printf("  #write-all  : %lu\n", data[i].nb_write_all);
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
    updates += data[i].nb_transfer;
    reads += data[i].nb_read_all;
    writes += data[i].nb_write_all;
    if (max_retries < data[i].max_retries)
      max_retries = data[i].max_retries;
  }
  printf("Bank total    : %d (expected: 0)\n", total(bank, 0));
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + writes + updates, (reads + writes + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#write txs    : %lu (%f / s)\n", writes, writes * 1000.0 / duration);
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

  /* Delete bank and accounts */
  free(bank->accounts);
  free(bank);

  /* Cleanup STM */
  stm_exit();

  free(threads);
  free(data);

  return 0;
}
