/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <string.h>
#include "dds__entity.h"
#include "dds__reader.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_sertopic.h" // for extern ddsi_sertopic_serdata_ops_wrap

#include "dds/ddsc/dds_virtual_interface.h"

/*
  dds_read_impl: Core read/take function. Usually maxs is size of buf and si
  into which samples/status are written, when set to zero is special case
  indicating that size set from number of samples in cache and also that cache
  has been locked. This is used to support C++ API reading length unlimited
  which is interpreted as "all relevant samples in cache".
*/
static dds_return_t dds_read_impl (bool take, dds_entity_t reader_or_condition, void **buf, size_t bufsz, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool lock, bool only_reader, bool loan)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_entity *entity;
  struct dds_reader *rd;
  struct dds_readcond *cond;

  if (buf == NULL || si == NULL || maxs == 0 || bufsz == 0 || bufsz < maxs || maxs > INT32_MAX)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (reader_or_condition, &entity)) < 0) {
    goto fail;
  } else if (dds_entity_kind (entity) == DDS_KIND_READER) {
    rd = (dds_reader *) entity;
    cond = NULL;
  } else if (only_reader) {
    ret = DDS_RETCODE_ILLEGAL_OPERATION;
    goto fail_pinned;
  } else if (dds_entity_kind (entity) != DDS_KIND_COND_READ && dds_entity_kind (entity) != DDS_KIND_COND_QUERY) {
    ret = DDS_RETCODE_ILLEGAL_OPERATION;
    goto fail_pinned;
  } else {
    rd = (dds_reader *) entity->m_parent;
    cond = (dds_readcond *) entity;
  }

  thread_state_awake (ts1, &entity->m_domain->gv);


  if (loan || buf[0] == NULL)
  {
    /* reset supplied pointers to NULL if we are dealing with a loan
     * or the first pointer is null, indicating the reader needs to
     * manage the samples' memory */
    memset(buf, 0, sizeof(void*)*bufsz);
  }
  else
  {
    for (uint32_t i = 0; i < maxs; i++)
    {
      dds_loaned_sample_t *s = dds_loan_manager_find_loan(rd->m_loans, buf[i]);
      /* refs(0): the reader has already returned the reference
         refs(1): the reader has the reference still */
      if (s && dds_loaned_sample_decr_refs(s))
        buf[i] = NULL;
    }
  }

  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  const uint32_t sm = dds_entity_status_reset_ov (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);
  /* reset DATA_ON_READERS status on subscriber after successful read/take if materialized */
  if (sm & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT))
    dds_entity_status_reset (rd->m_entity.m_parent, DDS_DATA_ON_READERS_STATUS);

  if (take)
    ret = dds_rhc_take (rd->m_rhc, lock, buf, si, maxs, mask, hand, cond, rd->m_loans);
  else
    ret = dds_rhc_read (rd->m_rhc, lock, buf, si, maxs, mask, hand, cond, rd->m_loans);

  dds_entity_unpin (entity);
  thread_state_asleep (ts1);
fail:
  return ret;

fail_pinned:
  dds_entity_unpin (entity);
  return ret;
}

static dds_return_t dds_readcdr_impl (bool take, dds_entity_t reader_or_condition, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask, dds_instance_handle_t hand, bool lock)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_reader *rd;
  struct dds_entity *entity;

  if (buf == NULL || si == NULL || maxs == 0 || maxs > INT32_MAX)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (reader_or_condition, &entity)) < 0) {
    return ret;
  } else if (dds_entity_kind (entity) == DDS_KIND_READER) {
    rd = (dds_reader *) entity;
  } else if (dds_entity_kind (entity) != DDS_KIND_COND_READ && dds_entity_kind (entity) != DDS_KIND_COND_QUERY) {
    dds_entity_unpin (entity);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  } else {
    rd = (dds_reader *) entity->m_parent;
  }

  thread_state_awake (ts1, &entity->m_domain->gv);

  /* read/take resets data available status -- must reset before reading because
     the actual writing is protected by RHC lock, not by rd->m_entity.m_lock */
  const uint32_t sm_old = dds_entity_status_reset_ov (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);
  /* reset DATA_ON_READERS status on subscriber after successful read/take if materialized */
  if (sm_old & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT))
    dds_entity_status_reset (rd->m_entity.m_parent, DDS_DATA_ON_READERS_STATUS);

  if (take)
    ret = dds_rhc_takecdr (rd->m_rhc, lock, buf, si, maxs, mask & DDS_ANY_SAMPLE_STATE, mask & DDS_ANY_VIEW_STATE, mask & DDS_ANY_INSTANCE_STATE, hand);
  else
    ret = dds_rhc_readcdr (rd->m_rhc, lock, buf, si, maxs, mask & DDS_ANY_SAMPLE_STATE, mask & DDS_ANY_VIEW_STATE, mask & DDS_ANY_INSTANCE_STATE, hand);

  if (rd->m_wrapped_sertopic)
  {
    for (int32_t i = 0; i < ret; i++)
    {
      assert (buf[i]->ops == &ddsi_sertopic_serdata_ops_wrap);
      struct ddsi_serdata_wrapper *wrapper = (struct ddsi_serdata_wrapper *) buf[i];
      buf[i] = ddsi_serdata_ref (wrapper->compat_wrap);
      // Lazily setting statusinfo/timestamp in the wrapped serdata because we don't
      // propagate it eagerly. This incurs the cost only in the rare case that an
      // application uses readcdr/takecdr, the other would incur it always.
      //
      // It seems a reasonable assumption on common hardware that storing a value
      // to memory that was there already won't allow observing a different one
      // temporarily. I don't think C guarantees it, but I do think all modern CPUs
      // do.
      buf[i]->statusinfo = wrapper->c.statusinfo;
      buf[i]->timestamp = wrapper->c.timestamp;
      ddsi_serdata_unref (&wrapper->c);
    }
  }

  dds_entity_unpin (entity);
  thread_state_asleep (ts1);
  return ret;
}

dds_return_t dds_read (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t) bufsz;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false, false);
}

dds_return_t dds_read_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false, true);
}

dds_return_t dds_read_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t)bufsz;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, lock, false, false);
}

dds_return_t dds_read_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, mask, DDS_HANDLE_NIL, lock, false, true);
}

dds_return_t dds_readcdr (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_readcdr_impl (false, rd_or_cnd, buf, maxs, si, mask, DDS_HANDLE_NIL, lock);
}

dds_return_t dds_read_instance (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, handle, lock, false, false);
}

dds_return_t dds_read_instance_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, handle, lock, false, true);
}

dds_return_t dds_read_instance_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t)bufsz;
  }
  return dds_read_impl (false, rd_or_cnd, buf, bufsz, maxs, si, mask, handle, lock, false, false);
}

dds_return_t dds_read_instance_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (false, rd_or_cnd, buf, maxs, maxs, si, mask, handle, lock, false, true);
}

dds_return_t dds_readcdr_instance (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_readcdr_impl(false, rd_or_cnd, buf, maxs, si, mask, handle, lock);
}

dds_return_t dds_read_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (false, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true, false);
}

dds_return_t dds_read_next_wl (
                 dds_entity_t reader,
                 void **buf,
                 dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (false, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true, true);
}

dds_return_t dds_take (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t)bufsz;
  }
  return dds_read_impl (true, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false, false);
}

dds_return_t dds_take_wl (dds_entity_t rd_or_cnd, void ** buf, dds_sample_info_t * si, uint32_t maxs)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (true, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, DDS_HANDLE_NIL, lock, false, true);
}

dds_return_t dds_take_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t) bufsz;
  }
  return dds_read_impl (true, rd_or_cnd, buf, bufsz, maxs, si, mask, DDS_HANDLE_NIL, lock, false, false);
}

dds_return_t dds_take_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl (true, rd_or_cnd, buf, maxs, maxs, si, mask, DDS_HANDLE_NIL, lock, false, true);
}

dds_return_t dds_takecdr (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, uint32_t mask)
{
  bool lock = true;
  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_readcdr_impl (true, rd_or_cnd, buf, maxs, si, mask, DDS_HANDLE_NIL, lock);
}

dds_return_t dds_take_instance (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, bufsz, maxs, si, NO_STATE_MASK_SET, handle, lock, false, false);
}

dds_return_t dds_take_instance_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, maxs, maxs, si, NO_STATE_MASK_SET, handle, lock, false, true);
}

dds_return_t dds_take_instance_mask (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = (uint32_t)bufsz;
  }
  return dds_read_impl(true, rd_or_cnd, buf, bufsz, maxs, si, mask, handle, lock, false, false);
}

dds_return_t dds_take_instance_mask_wl (dds_entity_t rd_or_cnd, void **buf, dds_sample_info_t *si, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_read_impl(true, rd_or_cnd, buf, maxs, maxs, si, mask, handle, lock, false, true);
}

dds_return_t dds_takecdr_instance (dds_entity_t rd_or_cnd, struct ddsi_serdata **buf, uint32_t maxs, dds_sample_info_t *si, dds_instance_handle_t handle, uint32_t mask)
{
  bool lock = true;

  if (handle == DDS_HANDLE_NIL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (maxs == DDS_READ_WITHOUT_LOCK)
  {
    lock = false;
    /* FIXME: Fix the interface. */
    maxs = 100;
  }
  return dds_readcdr_impl(true, rd_or_cnd, buf, maxs, si, mask, handle, lock);
}

dds_return_t dds_take_next (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (true, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true, false);
}

dds_return_t dds_take_next_wl (dds_entity_t reader, void **buf, dds_sample_info_t *si)
{
  uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  return dds_read_impl (true, reader, buf, 1u, 1u, si, mask, DDS_HANDLE_NIL, true, true, true);
}

dds_return_t dds_return_reader_loan (dds_reader *rd, void **buf, int32_t bufsz)
{
  dds_return_t ret = 0;
  if (bufsz <= 0)
  {
    /* No data whatsoever, or an invocation following a failed read/take call.  Read/take
       already take care of restoring the state prior to their invocation if they return
       no data.  Return late so invalid handles can be detected. */
    return ret;
  }
  ddsrt_mutex_lock (&rd->m_entity.m_mutex);

  for (int32_t s = 0; s < bufsz && ret >= 0; s++)
  {
    void *sample = buf[s];
    if (!sample)
      continue;

    dds_loaned_sample_t *loan = dds_loan_manager_find_loan(rd->m_loans, sample);

    if (loan)
    {
      /* refs(0)  reader has discarded the sample already,
         refs(1)  the reader still has the loan*/
      if (dds_loaned_sample_decr_refs(loan))
      {
        buf[s] = NULL;
        ret++;
      }
      else
      {
        ret = DDS_RETCODE_ERROR;
      }
    }
    else
    {
      ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }

  ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
  return ret;
}
