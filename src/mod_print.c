/*
 * File:
 *   mod_print.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module to test callbacks.
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

#include "mod_print.h"

#include "stm.h"

/*
 * Called upon thread creation.
 */
static void mod_print_on_thread_init(TXPARAMS void *arg)
{
  printf("==> on_thread_init()\n");
  fflush(NULL);
}

/*
 * Called upon thread deletion.
 */
static void mod_print_on_thread_exit(TXPARAMS void *arg)
{
  printf("==> on_thread_exit()\n");
  fflush(NULL);
}

/*
 * Called upon transaction start.
 */
static void mod_print_on_start(TXPARAMS void *arg)
{
  printf("==> on_start()\n");
  fflush(NULL);
}

/*
 * Called before transaction try to commit.
 */
static void mod_print_on_precommit(TXPARAMS void *arg)
{
  printf("==> on_precommit()\n");
  fflush(NULL);
}

/*
 * Called upon transaction commit.
 */
static void mod_print_on_commit(TXPARAMS void *arg)
{
  printf("==> on_commit()\n");
  fflush(NULL);
}

/*
 * Called upon transaction abort.
 */
static void mod_print_on_abort(TXPARAMS void *arg)
{
  printf("==> on_abort()\n");
  fflush(NULL);
}

/*
 * Initialize module.
 */
void mod_print_init()
{
  stm_register(mod_print_on_thread_init, mod_print_on_thread_exit, mod_print_on_start, mod_print_on_precommit, mod_print_on_commit, mod_print_on_abort, NULL);
}
