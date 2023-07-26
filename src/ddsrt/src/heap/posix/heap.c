// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>

#include "dds/ddsrt/heap.h"

static dds_return_t
ddsrt_heap_fini_impl()
{
  return DDS_RETCODE_OK;
}

dds_return_t
ddsrt_heap_init_impl(heap_ops_t *ops)
{
  ops->malloc = malloc;
  ops->calloc = calloc;
  ops->realloc = realloc;
  ops->free = free;
  ops->fini = ddsrt_heap_fini_impl;

  return DDS_RETCODE_OK;
}
