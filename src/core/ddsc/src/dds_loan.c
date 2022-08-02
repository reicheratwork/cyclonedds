#include "dds/ddsc/dds_loan.h"

#include "dds__entity.h"
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
  dds_loaned_sample_t ** loans_created = NULL;
  if (sz)
  {
    ddsi_virtual_interface_pipe_t *pipe = NULL;
    for (uint32_t i = 0; i < wr->m_wr->c.n_virtual_pipes && !pipe; i++)
    {
      if (wr->m_wr->c.m_pipes[i]->topic->supports_loan)
        pipe = wr->m_wr->c.m_pipes[i];
    }

    loans_created = dds_alloc(sizeof(dds_loaned_sample_t*)*n_samples);
    if (!loans_created)
    {
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto fail;
    }
    
    for (uint32_t i = 0; i < n_samples; i++)
    {
      loans_created[i] = loaned_sample_create(pipe, sz);
      if (!loans_created[i])
      {
        ret = DDS_RETCODE_OUT_OF_RESOURCES;
        goto fail;
      }
    }
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
    goto fail;
  }

  uint32_t newcap = wr->m_loans_used+n_samples;
  dds_loaned_sample_t ** newloans = NULL;
  if (wr->m_loans_cap < newcap)
  {
    newloans = dds_realloc(wr->m_loans, newcap*sizeof(dds_loaned_sample_t*));
    if (!newloans)
    {
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto fail;
    }
    else
    {
      memset(newloans+wr->m_loans_cap, 0, sizeof(dds_loaned_sample_t*)*(newcap-wr->m_loans_cap));
      wr->m_loans = newloans;
      wr->m_loans_cap = newcap;
    }
  }

  for (uint32_t i = 0; i < n_samples; i++)
  {
    while (*newloans)
      newloans++;
    *newloans = loans_created[i];
  }
  wr->m_loans_used += n_samples;

  dds_free(loans_created);

  dds_writer_unlock(wr);

  return (dds_return_t)n_samples;


fail:

  for (uint32_t i = 0; i < n_samples && loans_created; i++)
    loaned_sample_cleanup(loans_created[i]);

  dds_free(loans_created);

  dds_writer_unlock(wr);

  return ret;
}

bool loaned_sample_cleanup(dds_loaned_sample_t *sample)
{
  if (NULL == sample)
    return true;

  if (sample->sample_origin)
  {
    return unref_sample(sample);
  }
  else
  {
    dds_free(sample->sample_ptr);
    dds_free(sample);
    return true;
  }

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

bool unref_sample(dds_loaned_sample_t *sample)
{
  return sample->sample_origin->ops.unref_block(sample->sample_origin, sample);
}

bool ref_sample(dds_loaned_sample_t *sample)
{
  return sample->sample_origin->ops.ref_block(sample->sample_origin, sample);
}
