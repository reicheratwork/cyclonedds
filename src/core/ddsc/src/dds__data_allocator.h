/*
 * Copyright(c) 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS__DATA_ALLOCATOR_H
#define DDS__DATA_ALLOCATOR_H

#include "dds/ddsc/dds_data_allocator.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/sync.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef enum dds_allocator_kind {
  DDS_ALLOCATOR_KIND_FINI,
  DDS_ALLOCATOR_KIND_NONE, /* use heap */
  DDS_ALLOCATOR_KIND_PUBLISHER,
  DDS_ALLOCATOR_KIND_SUBSCRIBER
} dds_allocator_kind_t;

typedef struct dds_allocator_impl {
  enum dds_allocator_kind kind;
  union {
    struct dds_writer_sink_pipe_listelem *sink_pipe;
    struct dds_reader_source_pipe_listelem *source_pipe;
  } ref;
} dds_allocator_impl_t;

DDSRT_STATIC_ASSERT(sizeof (dds_allocator_impl_t) <= sizeof (dds_data_allocator_t));

#if defined (__cplusplus)
}
#endif

#endif
