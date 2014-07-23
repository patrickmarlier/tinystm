/*
 * File:
 *   mod_cb.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for user callback.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mod_cb.h"

#include "stm.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef struct mod_cb_entry {           /* Callback entry */
  void (*f)(void *);                    /* Function */
  void *arg;                            /* Argument to be passed to function */
  struct mod_cb_entry *next;            /* Next callback */
} mod_cb_entry_t;

typedef struct mod_cb_info {
  mod_cb_entry_t *commit;
  mod_cb_entry_t *abort;
} mod_cb_info_t;

static int mod_cb_key;
static int mod_cb_initialized = 0;

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Register abort callback for the CURRENT transaction.
 */
int stm_on_abort(TXPARAMS void (*on_abort)(void *arg), void *arg)
{
  mod_cb_info_t *icb;
  mod_cb_entry_t *ccb;
  if (!mod_cb_initialized) {
    fprintf(stderr, "Module mod_cb not initialized\n");
    exit(1);
  }

  icb = (mod_cb_info_t *)stm_get_specific(TXARGS mod_cb_key);
  assert(icb != NULL);

  if ((ccb = malloc(sizeof(mod_cb_entry_t))) == NULL) {
    perror("mod_cb: cannot allocate memory");
    exit(1);
  }
  ccb->f = on_abort;
  ccb->arg = arg;
  ccb->next = icb->abort;
  
  icb->abort = ccb;
  
  return 1;
}

/*
 * Register commit callback for the CURRENT transaction.
 */
int stm_on_commit(TXPARAMS void (*on_commit)(void *arg), void *arg)
{
  mod_cb_info_t *icb;
  mod_cb_entry_t *ccb;
  if (!mod_cb_initialized) {
    fprintf(stderr, "Module mod_cb not initialized\n");
    exit(1);
  }

  icb = (mod_cb_info_t *)stm_get_specific(TXARGS mod_cb_key);
  assert(icb != NULL);

  if ((ccb = malloc(sizeof(mod_cb_entry_t))) == NULL) {
    perror("mod_cb: cannot allocate memory");
    exit(1);
  }
  ccb->f = on_commit;
  ccb->arg = arg;
  ccb->next = icb->commit;
  
  icb->commit = ccb;

  return 1;
}

/*
 * Called upon transaction commit.
 */
static void mod_cb_on_commit(TXPARAMS void *arg)
{
  mod_cb_info_t *icb;
  mod_cb_entry_t *ccb;

  if (!mod_cb_initialized) {
    fprintf(stderr, "Module mod_cb not initialized\n");
    exit(1);
  }

  icb = (mod_cb_info_t *)stm_get_specific(TXARGS mod_cb_key);
  assert(icb != NULL);

  /* Call commit callback */
  while ((ccb = icb->commit) != NULL) {
    icb->commit->f(icb->commit->arg);
    icb->commit = ccb->next;
    free(ccb);
  }
  /* Free abort callback */
  while ((ccb = icb->abort) != NULL) {
    icb->abort = ccb->next;
    free(ccb);
  }
}

/*
 * Called upon transaction abort.
 */
static void mod_cb_on_abort(TXPARAMS void *arg)
{
  mod_cb_info_t *icb;
  mod_cb_entry_t *ccb;

  if (!mod_cb_initialized) {
    fprintf(stderr, "Module mod_cb not initialized\n");
    exit(1);
  }

  icb = (mod_cb_info_t *)stm_get_specific(TXARGS mod_cb_key);
  assert(icb != NULL);

  /* Call abort callback */
  while ((ccb = icb->abort) != NULL) {
    icb->abort->f(icb->abort->arg);
    icb->abort = ccb->next;
    free(ccb);
  }
  /* Free commit callback */
  while ((ccb = icb->commit) != NULL) {
    icb->commit = ccb->next;
    free(ccb);
  }
}

/*
 * Called upon thread creation.
 */
static void mod_cb_on_thread_init(TXPARAMS void *arg)
{
  mod_cb_info_t *icb;

  if ((icb = (mod_cb_info_t *)malloc(sizeof(mod_cb_info_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  icb->commit = icb->abort = NULL;

  stm_set_specific(TXARGS mod_cb_key, icb);
}

/*
 * Called upon thread deletion.
 */
static void mod_cb_on_thread_exit(TXPARAMS void *arg)
{
  free(stm_get_specific(TXARGS mod_cb_key));
}

/*
 * Initialize module.
 */
void mod_cb_init()
{
  if (mod_cb_initialized)
    return;

  stm_register(mod_cb_on_thread_init, mod_cb_on_thread_exit, NULL, NULL, mod_cb_on_commit, mod_cb_on_abort, NULL);
  mod_cb_key = stm_create_specific();
  if (mod_cb_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  mod_cb_initialized = 1;
}

