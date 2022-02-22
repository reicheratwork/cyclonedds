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
#include "dds/dds.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsrt/heap.h"
#include "dds__data_allocator.h"
#include "dds__entity.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_shm_transport.h"
#endif

#include "dds/ddsc/dds_loan_api.h"

dds_return_t dds_data_allocator_init_heap (dds_data_allocator_t *data_allocator)
{
  // Use special entity handle to allocate on heap
  return dds_data_allocator_init(DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP, data_allocator);
}

dds_return_t dds_data_allocator_init (dds_entity_t entity, dds_data_allocator_t *data_allocator)
{
  dds_entity *e;
  dds_return_t ret;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_allocator_impl_t *d = (dds_allocator_impl_t *) data_allocator->opaque.bytes;

  // special case, the allocator treats this entity as an allocation on the heap
  if (entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ret = DDS_RETCODE_OK;
  } else {
    if ((ret = dds_entity_pin(entity, &e)) != DDS_RETCODE_OK)
      return ret;
    switch (dds_entity_kind(e)) {
      case DDS_KIND_READER: {
        struct dds_reader * reader = (struct dds_reader *)e;
        struct dds_reader_source_pipe_listelem * pipe = reader->m_source_pipes;
        while(pipe) {
          if (pipe->interface->loan_supported) {
            ret = DDS_RETCODE_OK;
            d->kind = DDS_ALLOCATOR_KIND_SUBSCRIBER;
            d->ref.source_pipe = pipe;
            break;
          }
          pipe = pipe->next;
          if (!pipe) {
            // no virtual interfaces with loan support
            ret = DDS_RETCODE_OK;
            d->kind = DDS_ALLOCATOR_KIND_NONE;
          }
        }
        break;
      }
      case DDS_KIND_WRITER: {
        struct dds_writer * writer = (struct dds_writer *)e;
        struct dds_writer_sink_pipe_listelem * pipe = writer->m_sink_pipes;
        while(pipe) {
          if (pipe->interface->loan_supported) {
            ret = DDS_RETCODE_OK;
            d->kind = DDS_ALLOCATOR_KIND_PUBLISHER;
            d->ref.sink_pipe = pipe;
          }
          pipe = pipe->next;
          if (!pipe) {
            // no virtual interfaces with loan support
            ret = DDS_RETCODE_OK;
            d->kind = DDS_ALLOCATOR_KIND_NONE;
          }
        }
        break;
      }
      default:
        ret = DDS_RETCODE_ILLEGAL_OPERATION;
        break;
    }
    dds_entity_unpin (e);
  }
  if (ret == DDS_RETCODE_OK)
    data_allocator->entity = entity;
  return ret;
}

dds_return_t dds_data_allocator_fini (dds_data_allocator_t *data_allocator)
{
  dds_entity *e;
  dds_return_t ret;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  // special case, the allocator treats this entity as an allocation on the heap
  if (data_allocator->entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ret = DDS_RETCODE_OK;
  } else {
    if ((ret = dds_entity_pin(data_allocator->entity, &e)) != DDS_RETCODE_OK)
      return ret;
    switch (dds_entity_kind(e)) {
      case DDS_KIND_READER:
        break;
      case DDS_KIND_WRITER:
        break;
      default:
        ret = DDS_RETCODE_ILLEGAL_OPERATION;
        break;
    }
    dds_entity_unpin(e);
  }
  if (ret == DDS_RETCODE_OK)
    data_allocator->entity = 0;
  return ret;
}

void *dds_data_allocator_alloc (dds_data_allocator_t *data_allocator, size_t size)
{
  if (data_allocator == NULL)
    return NULL;

  if(data_allocator->entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP)
    return ddsrt_malloc (size);

  dds_allocator_impl_t *d = (dds_allocator_impl_t *) data_allocator->opaque.bytes;
  switch (d->kind)
  {
    case DDS_ALLOCATOR_KIND_FINI:
      return NULL;
    case DDS_ALLOCATOR_KIND_NONE:
      return ddsrt_malloc (size);
    case DDS_ALLOCATOR_KIND_SUBSCRIBER:
      return NULL;
    case DDS_ALLOCATOR_KIND_PUBLISHER:
        return d->ref.sink_pipe->interface->ops.sink_pipe_chunk_loan(
          d->ref.sink_pipe->interface,
          d->ref.sink_pipe,
          size
        );
    default:
      return NULL;
  }
}

dds_return_t dds_data_allocator_free (dds_data_allocator_t *data_allocator, void *ptr)
{
  dds_return_t ret = DDS_RETCODE_OK;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if(data_allocator->entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ddsrt_free(ptr);
  } else {
    dds_allocator_impl_t *d = (dds_allocator_impl_t *)data_allocator->opaque.bytes;
    switch (d->kind) {
      case DDS_ALLOCATOR_KIND_FINI:
        ret = DDS_RETCODE_PRECONDITION_NOT_MET;
        break;
      case DDS_ALLOCATOR_KIND_NONE:
        ddsrt_free(ptr);
        break;
      case DDS_ALLOCATOR_KIND_SUBSCRIBER:
        if (ptr != NULL) {
          d->ref.source_pipe->interface->ops.source_pipe_chunk_return(
            d->ref.source_pipe->interface,
            d->ref.source_pipe,
            ptr
          );
        }
        break;
      case DDS_ALLOCATOR_KIND_PUBLISHER:
        if (ptr != NULL) {
          d->ref.sink_pipe->interface->ops.sink_pipe_chunk_return(
            d->ref.sink_pipe->interface,
            d->ref.sink_pipe,
            ptr
          );
        }
        break;
      default:
        ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }
  return ret;
}
