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
  dds_return_t ret = DDS_RETCODE_OK;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_allocator_impl_t *d = (dds_allocator_impl_t *) data_allocator->opaque.bytes;

  // special case, the allocator treats this entity as an allocation on the heap
  if (entity != DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ddsi_virtual_interface_pipe_list_elem * pipes = NULL;
    if ((ret = dds_entity_pin(entity, &e)) != DDS_RETCODE_OK)
      return ret;
    switch (dds_entity_kind(e)) {
      case DDS_KIND_READER:
        pipes = ((struct dds_reader *)e)->m_pipes;
        break;
      case DDS_KIND_WRITER:
        pipes = ((struct dds_writer *)e)->m_pipes;
        break;
      default:
        ret = DDS_RETCODE_ILLEGAL_OPERATION;
        break;
    }

    while(pipes && !pipes->pipe->supports_loan) {
      pipes = pipes->next;
    }

    if (pipes) {
      assert(pipes->pipe->supports_loan);
      d->kind = DDS_ALLOCATOR_KIND_LOAN;
      d->pipe = pipes->pipe;
    } else {
      d->kind = DDS_ALLOCATOR_KIND_HEAP;
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
  void *outptr;
  switch (d->kind)
  {
    case DDS_ALLOCATOR_KIND_HEAP:
      outptr = ddsrt_malloc (size);
      break;
    case DDS_ALLOCATOR_KIND_LOAN:
        if (!d->pipe->virtual_interface->ops.pipe_request_loan(d->pipe, &outptr, size))
          outptr = NULL;  /*something went wrong...*/
      break;
    default:
      outptr = NULL;
  }
  return outptr;
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
      case DDS_ALLOCATOR_KIND_HEAP:
        ddsrt_free(ptr);
        break;
      case DDS_ALLOCATOR_KIND_LOAN:
        if (ptr &&
            !d->pipe->virtual_interface->ops.pipe_return_loan(d->pipe, ptr))
          ret = DDS_RETCODE_ERROR;
        break;
      default:
        ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }
  return ret;
}
