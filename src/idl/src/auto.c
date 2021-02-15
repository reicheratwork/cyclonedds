/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#if !_WIN32
#include <pthread.h>
#endif

#include "idl/auto.h"

#include "config.h"

struct list {
  struct list *next;
  const void *address;
  const void *depth;
  void *block;
};

#if _WIN32
static __declspec(thread) struct list *auto_list = NULL;

static struct list *fetch(void)
{
  return auto_list;
}

static void stash(struct list *last)
{
  auto_list = last;
}
#else
static pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void collect(void *ptr)
{
  struct list *last, *next;
  for (last = ptr; last; last = next) {
    next = last->next;
    free(last->block);
    free(last);
  }
}

static void make_key(void)
{
  (void)pthread_key_create(&key, collect);
}

static struct list *fetch(void)
{
  (void)pthread_once(&once, make_key);
  return pthread_getspecific(key);
}

static void stash(struct list *last)
{
  (void)pthread_once(&once, make_key);
  pthread_setspecific(key, last);
}
#endif

/* if return address is not the same, we are definitly not executing the same
   function and can free blocks that have the same depth */
static bool
out_of_scope(const void *address, const void *depth, const struct list *item)
{
  if (!address || !depth)
    return true;
#if STACK_DIRECTION == 1
  return address == item->address ? item->depth > depth : item->depth >= depth;
#elif STACK_DIRECTION == -1
  return address == item->address ? item->depth < depth : item->depth <= depth;
#else
  return false;
#endif
}

#if _WIN32
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void idl_collect_auto(const void *address)
{
  char probe; /* probes stack depth */
  const void *depth = &probe;
  struct list *last, *next;

  for (last = fetch(), next = NULL; last; last = next) {
    if (!out_of_scope(address, depth, last))
      break;
    next = last->next;
    free(last->block);
    free(last);
  }

  stash(last);
}

#if _WIN32
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void *idl_auto(const void *address, void *block)
{
  char probe; /* probes stack depth */
  const void *depth = &probe;
  struct list *last, *next;

  if (!block) {
    return NULL;
  }

  for (last = fetch(); last; last = next) {
    if (!out_of_scope(address, depth, last))
      break;
    next = last->next;
    free(last->block);
    free(last);
  }

  stash(last);

  next = last;
  if (!(last = malloc(sizeof(*last)))) {
    free(block);
    return NULL;
  }

  last->next = next;
  last->address = address;
  last->depth = depth;
  last->block = block;

  stash(last);

  return block;
}
