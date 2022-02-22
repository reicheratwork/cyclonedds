#include "dds/ddsc/dds_loan_api.h"

#include "dds__entity.h"
#include "dds__loan.h"
#include "dds__reader.h"
#include "dds__types.h"
#include "dds__writer.h"

#include "dds/ddsi/ddsi_sertype.h"


bool dds_is_shared_memory_available(const dds_entity_t entity) {
  bool ret = false;
  dds_entity *e;

  if (DDS_RETCODE_OK != dds_entity_pin(entity, &e)) {
    return ret;
  }

  switch (dds_entity_kind(e)) {
    case DDS_KIND_READER: {
      struct dds_reader * reader = (struct dds_reader *)e;
      struct dds_reader_source_pipe_listelem * pipe = reader->m_source_pipes;
      while(pipe) {
        if (pipe->interface->loan_supported) {
          ret = true;
          break;
        }
        pipe = pipe->next;
      }
      break;
    }
    case DDS_KIND_WRITER: {
      struct dds_writer * writer = (struct dds_writer *)e;
      struct dds_writer_sink_pipe_listelem * pipe = writer->m_sink_pipes;
      while(pipe) {
        if (pipe->interface->loan_supported) {
          ret = true;
          break;
        }
        pipe = pipe->next;
      }
      break;
    }
    default:
      break;
  }

  dds_entity_unpin(e);
  return ret;
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

  struct dds_writer_sink_pipe_listelem * pipe = wr->m_sink_pipes;
  while(pipe) {
    if (pipe->interface->loan_supported) {
      break;
    }
    pipe = pipe->next;
  }

  if (!pipe) {
    ret = DDS_RETCODE_ERROR;
  } else {
    *buffer = pipe->interface->ops.sink_pipe_chunk_loan(pipe->interface, pipe, size);
    if (*buffer == NULL) {
      ret = DDS_RETCODE_ERROR;
    }
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

  struct dds_writer_sink_pipe_listelem * pipe = wr->m_sink_pipes;
  while(pipe) {
    if (pipe->interface->loan_supported) {
      break;
    }
    pipe = pipe->next;
  }

  if (!pipe) {
    ret = DDS_RETCODE_ERROR;
  } else {
    *sample = pipe->interface->ops.sink_pipe_chunk_loan(pipe->interface, pipe, wr->m_topic->m_stype->zerocopy_size);
    if (*sample == NULL) {
      ret = DDS_RETCODE_ERROR;
    }
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

  struct dds_writer_sink_pipe_listelem * pipe = wr->m_sink_pipes;
  while(pipe) {
    if (pipe->interface->loan_supported) {
      break;
    }
    pipe = pipe->next;
  }

  if (!pipe) {
    ret = DDS_RETCODE_ERROR;
  } else {
    for (int32_t i = 0; i < bufsz; i++) {
      if (buf[i] == NULL) {
        ret = DDS_RETCODE_BAD_PARAMETER;
        break;
      } else if (!pipe->interface->ops.sink_pipe_chunk_return(pipe->interface, pipe, buf[i])) {
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
