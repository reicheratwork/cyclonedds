#include "dds/ddsc/dds_loan.h"

#include "dds__entity.h"
#include "dds__loan.h"
#include "dds__reader.h"
#include "dds__types.h"
#include "dds__writer.h"
#include "dds/ddsi/q_entity.h"

#include "dds/ddsi/ddsi_sertype.h"
#include <string.h>

dds_return_t dds_writer_loan_samples(dds_entity_t writer, void **samples_ptr, uint32_t n_samples) {
  dds_return_t ret;
  dds_writer *wr;

  if (!samples_ptr)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock(writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  uint32_t sz = wr->m_topic->m_stype->fixed_size;
  if (sz) {
    ddsi_virtual_interface_pipe_t *pipe = NULL;
    for (uint32_t i = 0; i < wr->m_wr->c.n_virtual_pipes; i++) {
      if (wr->m_wr->c.m_pipes[i]->topic->supports_loan)
        pipe = wr->m_wr->c.m_pipes[i];
    }

    uint32_t loans_out = 0;
    for (uint32_t i = 0; i < wr->n_virtual_interface_loan_cap; i++) {
      if (wr->m_virtual_interface_loans[i])
        loans_out++;
    }

    if (wr->n_virtual_interface_loan_cap-loans_out < n_samples) {
      uint32_t newsamples = n_samples-wr->n_virtual_interface_loan_cap+loans_out;
      wr->n_virtual_interface_loan_cap = loans_out+n_samples;
      wr->m_virtual_interface_loans = realloc(
        wr->m_virtual_interface_loans,
        sizeof(*(wr->m_virtual_interface_loans))*wr->n_virtual_interface_loan_cap);
      memset(wr->m_virtual_interface_loans+newsamples, 0x0, newsamples*sizeof(*(wr->m_virtual_interface_loans)));
    }

    dds_loaned_sample_t **ptr = wr->m_virtual_interface_loans;
    for (uint32_t i = 0; i < n_samples; i++) {
      while (*ptr)
        ptr++;

      *ptr = loaned_sample_create(pipe, sz);
      if (!*ptr) {
        goto fail_cleanup_loans;
      } else {
        samples_ptr[i] = (*ptr)->sample_ptr;
      }
    }
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
  }

  dds_writer_unlock(wr);

  return (dds_return_t)n_samples;

fail_cleanup_loans:
  for (uint32_t i = 0; i < wr->n_virtual_interface_loan_cap; i++)
    loaned_sample_cleanup(wr->m_virtual_interface_loans[i]);

  dds_writer_unlock(wr);

  return DDS_RETCODE_ERROR;
}

bool loaned_sample_cleanup(dds_loaned_sample_t *sample)
{
  if (NULL == sample)
    return true;

  if (sample->sample_origin) {
    sample->sample_origin->ops.unref_block(sample->sample_origin, sample);
  } else {
    dds_free(sample->sample_ptr);
    dds_free(sample);
  }

  return true;
}

dds_loaned_sample_t * loaned_sample_create(ddsi_virtual_interface_pipe_t *pipe, size_t size)
{
  dds_loaned_sample_t * ptr = NULL;

  if (pipe) {
    ptr = pipe->ops.request_loan(pipe, size);
    if (!ptr)
      goto fail;
  } else {
    ptr = dds_alloc(sizeof(dds_loaned_sample_t));
    if (!ptr)
      goto fail;
    memset(ptr, 0x0, sizeof(dds_loaned_sample_t));
    ptr->sample_ptr = dds_alloc(size);
    ptr->sample_size = size;
    if (!ptr->sample_ptr)
      goto fail;
  }

  return ptr;

fail:
  if (ptr)
    loaned_sample_cleanup(ptr);
  return NULL;
}
