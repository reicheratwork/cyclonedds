#include "dds/ddsc/dds_loan_api.h"

#include "dds__entity.h"
#include "dds__loan.h"
#include "dds__reader.h"
#include "dds__types.h"
#include "dds__writer.h"

#include "dds/ddsi/ddsi_sertype.h"


bool dds_is_shared_memory_available(const dds_entity_t entity) {
  dds_entity *e = NULL;

  if (DDS_RETCODE_OK != dds_entity_pin(entity, &e)) {
    return false;
  }

  ddsi_virtual_interface_pipe_list_elem * pipes = NULL;
  switch (dds_entity_kind(e)) {
    case DDS_KIND_READER:
      pipes = ((struct dds_reader *)e)->m_pipes;
      break;
    case DDS_KIND_WRITER:
      pipes = ((struct dds_writer *)e)->m_pipes;
      break;
    default:
      break;
  }

  while(pipes && !pipes->pipe->supports_loan) {
    pipes = pipes->next;
  }

  dds_entity_unpin(e);
  assert(!pipes || pipes->pipe->supports_loan);
  return pipes;
}

bool dds_is_loan_available(const dds_entity_t entity) {
  return dds_is_shared_memory_available(entity);
}

bool is_loan_available(const dds_entity_t entity) {
  return dds_is_loan_available(entity);
}

dds_return_t dds_loan_shared_memory_buffer(dds_entity_t writer, size_t size, void **buffer) {
  dds_return_t ret;
  dds_writer *wr;

  if (!buffer)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  ddsi_virtual_interface_pipe_list_elem * pipes = wr->m_pipes;
  while(pipes && !pipes->pipe.supports_loan) {
    pipes = pipes->next;
  }

  if (!pipes || /*no pipes present with loan functionality*/
      !pipes->pipe->virtual_interface->ops.pipe_request_loan(pipe, buffer, size)) {  /*loan unsuccesful*/
      ret = DDS_RETCODE_ERROR;
  }

  dds_writer_unlock(wr);
  return ret;
}

dds_return_t dds_loan_sample(dds_entity_t writer, void **sample) {
  dds_return_t ret;
  dds_writer *wr;

  if (!sample)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  ddsi_virtual_interface_pipe_list_elem * pipes = wr->m_pipes;
  while(pipes && !pipes->pipe->supports_loan) {
    pipes = pipes->next;
  }

  if (!pipes ||
      !pipes->pipe->virtual_interface->ops.pipe_request_loan(pipe, sample, wr->m_topic->m_stype->zerocopy_size)) {
      ret = DDS_RETCODE_ERROR;
  }

  dds_writer_unlock(wr);
  return ret;
}

dds_return_t dds_return_writer_loan(dds_writer *writer, void **buf, int32_t bufsz) {
  dds_return_t ret;
  dds_writer *wr;

  if (bufsz <= 0) {
    // analogous to long-standing behaviour for the reader case, where it makes
    // (some) sense as it allows passing in the result of a read/take operation
    // regardless of whether that operation was successful
    return DDS_RETCODE_OK;
  }

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  ddsi_virtual_interface_pipe_list_elem * pipes = wr->m_pipes;
  while(pipes && !pipes->pipe.supports_loan) {
    pipes = pipes->next;
  }

  if (!pipes) {
    ret = DDS_RETCODE_ERROR;
  } else {
    for (int32_t i = 0; i < bufsz; i++) {
      if (buf[i] == NULL) {
        ret = DDS_RETCODE_BAD_PARAMETER;
        break;
      } else if (!pipes->pipe->virtual_interface->ops.pipe_return_loan(pipe, buf[i])) {
        ret = DDS_RETCODE_PRECONDITION_NOT_MET;
        break;
      } else {
      // return loan on the reader nulls buf[0], but here it makes more sense to
      // clear all successfully returned ones: then, on failure, the application
      // can figure out which ones weren't returned by looking for the first
      // non-null pointer
        buf[i] = NULL;
      }
    }
  }

  dds_writer_unlock(wr);
  return ret;
}
