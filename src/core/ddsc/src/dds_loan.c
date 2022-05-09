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

  bool val = false;
  switch (dds_entity_kind(e)) {
    case DDS_KIND_READER: {
        struct dds_reader *ptr = (struct dds_reader *)e;
        for (uint32_t i = 0; i < ptr->n_virtual_pipes && !val; i++) {
          val = ptr->m_pipes[i]->topic->supports_loan;
        }
      }
      break;
    case DDS_KIND_WRITER: {
        struct dds_writer *ptr = (struct dds_writer *)e;
        for (uint32_t i = 0; i < ptr->n_virtual_pipes && !val; i++) {
          val = ptr->m_pipes[i]->topic->supports_loan;
        }
      }
      break;
    default:
      break;
  }

  dds_entity_unpin(e);
  return val;
}

bool dds_is_loan_available(const dds_entity_t entity) {
  return dds_is_shared_memory_available(entity);
}

bool is_loan_available(const dds_entity_t entity) {
  return dds_is_loan_available(entity);
}

static dds_return_t dds_writer_loan_impl(dds_writer *wr, size_t size, void **buffer)
{
  if (!buffer || !wr)
    return DDS_RETCODE_BAD_PARAMETER;

  ddsi_virtual_interface_pipe_t *pipe = NULL;
  for (uint32_t i = 0; i < wr->n_virtual_pipes; i++) {
    if (wr->m_pipes[i]->topic->supports_loan)
      pipe = wr->m_pipes[i];
  }

  if (pipe) {
    memory_block_t *loan = pipe->ops.request_loan(pipe, size);
    if (!loan)
      return DDS_RETCODE_ERROR;
    else
      *buffer = loan->block;
  } else {
      *buffer = dds_alloc(size);
  }

  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_shared_memory_buffer(dds_entity_t writer, size_t size, void **buffer) {
  dds_return_t ret;
  dds_writer *wr;

  if (!buffer)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  ret = dds_writer_loan_impl(wr, size, buffer);

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

  size_t sz = wr->m_topic->m_stype->fixed_size;
  if (sz) {
    ret = dds_writer_loan_impl(wr, sz, sample);
  } else {
    ret = DDS_RETCODE_BAD_PARAMETER;
  }

  dds_writer_unlock(wr);

  return ret;
}

dds_return_t dds_return_writer_loan(dds_writer *writer, void **buf, int32_t bufsz) {
  if (bufsz <= 0) {
    // analogous to long-standing behaviour for the reader case, where it makes
    // (some) sense as it allows passing in the result of a read/take operation
    // regardless of whether that operation was successful
    return DDS_RETCODE_OK;
  }

  ddsi_virtual_interface_pipe_t *pipe;
  for (uint32_t i = 0; i < writer->n_virtual_pipes && !pipe; i++) {
    if (writer->m_pipes[i]->topic->supports_loan)
      pipe = writer->m_pipes[i];
  }

  for (int32_t i = 0; i < bufsz; i++) {
    if (buf[i] == NULL) {
      // we should not be passed empty pointers, this indicates an internal inconsistency
      return DDS_RETCODE_BAD_PARAMETER;
      break;
    } else if (pipe && !pipe->ops.return_loan(pipe, buf[i])) {
      // a failure in returning the loan to the pipe
      return DDS_RETCODE_ERROR;
    } else {
      dds_free(buf[i]);
    }
    // return loan on the reader nulls buf[0], but here it makes more sense to
    // clear all successfully returned ones: then, on failure, the application
    // can figure out which ones weren't returned by looking for the first
    // non-null pointer
    buf[i] = NULL;
  }

  return DDS_RETCODE_OK;
}
