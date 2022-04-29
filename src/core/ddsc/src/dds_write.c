/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include "dds__writer.h"
#include "dds__write.h"
#include "dds__loan.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_deliver_locally.h"
#include "dds/ddsi/q_addrset.h"

#include "dds/ddsc/dds_loan.h"

dds_return_t dds_write (dds_entity_t writer, const void *data)
{
  dds_return_t ret;
  dds_writer *wr;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  ret = dds_write_impl (wr, data, dds_time (), 0);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_writecdr (dds_entity_t writer, struct ddsi_serdata *serdata)
{
  dds_return_t ret;
  dds_writer *wr;

  if (serdata == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  if (wr->m_topic->m_filter.mode != DDS_TOPIC_FILTER_NONE)
  {
    dds_writer_unlock (wr);
    return DDS_RETCODE_ERROR;
  }
  serdata->statusinfo = 0;
  serdata->timestamp.v = dds_time ();
  ret = dds_writecdr_impl (wr, wr->m_xp, serdata, !wr->whc_batch);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_forwardcdr (dds_entity_t writer, struct ddsi_serdata *serdata)
{
  dds_return_t ret;
  dds_writer *wr;

  if (serdata == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  if (wr->m_topic->m_filter.mode != DDS_TOPIC_FILTER_NONE)
  {
    dds_writer_unlock (wr);
    return DDS_RETCODE_ERROR;
  }
  ret = dds_writecdr_impl (wr, wr->m_xp, serdata, !wr->whc_batch);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_write_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  dds_return_t ret;
  dds_writer *wr;

  if (data == NULL || timestamp < 0)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  ret = dds_write_impl (wr, data, timestamp, 0);
  dds_writer_unlock (wr);
  return ret;
}

static struct reader *writer_first_in_sync_reader (struct entity_index *entity_index, struct entity_common *wrcmn, ddsrt_avl_iter_t *it)
{
  assert (wrcmn->kind == EK_WRITER);
  struct writer *wr = (struct writer *) wrcmn;
  struct wr_rd_match *m = ddsrt_avl_iter_first (&wr_local_readers_treedef, &wr->local_readers, it);
  return m ? entidx_lookup_reader_guid (entity_index, &m->rd_guid) : NULL;
}

static struct reader *writer_next_in_sync_reader (struct entity_index *entity_index, ddsrt_avl_iter_t *it)
{
  struct wr_rd_match *m = ddsrt_avl_iter_next (it);
  return m ? entidx_lookup_reader_guid (entity_index, &m->rd_guid) : NULL;
}

struct local_sourceinfo {
  const struct ddsi_sertype *src_type;
  struct ddsi_serdata *src_payload;
  struct ddsi_tkmap_instance *src_tk;
  ddsrt_mtime_t timeout;
};

static struct ddsi_serdata *local_make_sample (struct ddsi_tkmap_instance **tk, struct ddsi_domaingv *gv, struct ddsi_sertype const * const type, void *vsourceinfo)
{
  struct local_sourceinfo *si = vsourceinfo;
  struct ddsi_serdata *d = ddsi_serdata_ref_as_type (type, si->src_payload);
  if (d == NULL)
  {
    DDS_CWARNING (&gv->logconfig, "local: deserialization %s failed in type conversion\n", type->type_name);
    return NULL;
  }
  if (type != si->src_type)
    *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, d);
  else
  {
    // if the type is the same, we can avoid the lookup
    ddsi_tkmap_instance_ref (si->src_tk);
    *tk = si->src_tk;
  }
  return d;
}

static dds_return_t local_on_delivery_failure_fastpath (struct entity_common *source_entity, bool source_entity_locked, struct local_reader_ary *fastpath_rdary, void *vsourceinfo)
{
  (void) fastpath_rdary;
  (void) source_entity_locked;
  assert (source_entity->kind == EK_WRITER);
  struct writer *wr = (struct writer *) source_entity;
  struct local_sourceinfo *si = vsourceinfo;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  if (si->timeout.v == 0)
    si->timeout = ddsrt_mtime_add_duration (tnow, wr->xqos->reliability.max_blocking_time);
  if (tnow.v >= si->timeout.v)
    return DDS_RETCODE_TIMEOUT;
  else
  {
    dds_sleepfor (DDS_HEADBANG_TIMEOUT);
    return DDS_RETCODE_OK;
  }
}

static dds_return_t deliver_locally (struct writer *wr, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk)
{
  static const struct deliver_locally_ops deliver_locally_ops = {
    .makesample = local_make_sample,
    .first_reader = writer_first_in_sync_reader,
    .next_reader = writer_next_in_sync_reader,
    .on_failure_fastpath = local_on_delivery_failure_fastpath
  };
  struct local_sourceinfo sourceinfo = {
    .src_type = wr->type,
    .src_payload = payload,
    .src_tk = tk,
    .timeout = { 0 },
  };
  dds_return_t rc;
  struct ddsi_writer_info wrinfo;
  ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, payload->statusinfo);
  rc = deliver_locally_allinsync (wr->e.gv, &wr->e, false, &wr->rdary, &wrinfo, &deliver_locally_ops, &sourceinfo);
  if (rc == DDS_RETCODE_TIMEOUT)
    DDS_CERROR (&wr->e.gv->logconfig, "The writer could not deliver data on time, probably due to a local reader resources being full\n");
  return rc;
}

static struct ddsi_serdata *convert_serdata(struct writer *ddsi_wr, struct ddsi_serdata *din) {
  struct ddsi_serdata *dout;
  if (ddsi_wr->type == din->type)
  {
    dout = din;
    // dout refc: must consume 1
    // din refc: must consume 0 (it is an alias of dact)
  }
  else if (din->type->ops->version == ddsi_sertype_v0)
  {
    // deliberately allowing mismatches between d->type and ddsi_wr->type:
    // that way we can allow transferring data from one domain to another
    dout = ddsi_serdata_ref_as_type (ddsi_wr->type, din);
    // dout refc: must consume 1
    // din refc: must consume 1 (independent of dact: types are distinct)
  }
  else
  {
    // hope for the best (the type checks/conversions were missing in the
    // sertopic days anyway, so this is simply bug-for-bug compatibility
    dout = ddsi_sertopic_wrap_serdata (ddsi_wr->type, din->kind, din);
    // dout refc: must consume 1
    // din refc: must consume 1
  }
  return dout;
}

static dds_return_t deliver_data (struct writer *ddsi_wr, struct ddsi_serdata *d, struct nn_xpack *xp, bool flush) {
  struct thread_state1 * const ts1 = lookup_thread_state ();

  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, d);
  // write_sample_gc always consumes 1 refc from d
  int ret = write_sample_gc (ts1, xp, ddsi_wr, d, tk);
  if (ret >= 0)
  {
    /* Flush out write unless configured to batch */
    if (flush && xp != NULL)
      nn_xpack_send (xp, false);
    ret = DDS_RETCODE_OK;
  }
  else
  {
    if (ret != DDS_RETCODE_TIMEOUT)
      ret = DDS_RETCODE_ERROR;
  }

  if (ret == DDS_RETCODE_OK)
    ret = deliver_locally (ddsi_wr, d, tk);

  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);

  return ret;
}

static dds_return_t dds_writecdr_impl_common (struct writer *ddsi_wr, struct nn_xpack *xp, struct ddsi_serdata *din, bool flush)
{
  // consumes 1 refc from din in all paths (weird, but ... history ...)
  // let refc(din) be r, so upon returning it must be r-1
  struct thread_state1 * const ts1 = lookup_thread_state ();
  int ret = DDS_RETCODE_OK;

  struct ddsi_serdata *d = convert_serdata(ddsi_wr, din);

  if (d == NULL)
  {
    ddsi_serdata_unref(din); // refc(din) = r - 1 as required
    return DDS_RETCODE_ERROR;
  }

  // d = din: refc(d) = r, otherwise refc(d) = 1

  thread_state_awake (ts1, ddsi_wr->e.gv);
  ddsi_serdata_ref(d); // d = din: refc(d) = r + 1, otherwise refc(d) = 2

  ret = deliver_data(ddsi_wr, d, xp, flush); // d = din: refc(d) = r, otherwise refc(d) = 1

  if(d != din)
    ddsi_serdata_unref(din); // d != din: refc(din) = r - 1 as required, refc(d) unchanged
  ddsi_serdata_unref(d); // d = din: refc(d) = r - 1, otherwise refc(din) = r-1 and refc(d) = 0

  thread_state_asleep (ts1);
  return ret;
}

static bool evalute_topic_filter (const dds_writer *wr, const void *data, bool writekey)
{
  // false if data rejected by filter
  if (wr->m_topic->m_filter.mode == DDS_TOPIC_FILTER_NONE || writekey)
    return true;

  const struct dds_topic_filter *f = &wr->m_topic->m_filter;
  switch (f->mode)
  {
    case DDS_TOPIC_FILTER_NONE:
    case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
      break;
    case DDS_TOPIC_FILTER_SAMPLE:
      if (!f->f.sample (data))
        return false;
      break;
    case DDS_TOPIC_FILTER_SAMPLE_ARG:
      if (!f->f.sample_arg (data, f->arg))
        return false;
      break;
    case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
      struct dds_sample_info si;
      memset (&si, 0, sizeof (si));
      if (!f->f.sample_sampleinfo_arg (data, &si, f->arg))
        return false;
      break;
    }
  }
  return true;
}

static bool requires_serialization(struct dds_topic *topic)
{
  return !topic->m_stype->fixed_size;
}

static bool allows_serialization_into_buffer(struct dds_topic *topic)
{
  return (NULL != topic->m_stype->ops->serialize_into) &&
         (NULL != topic->m_stype->ops->get_serialized_size);
}

static bool get_required_buffer_size(struct dds_topic *topic, const void *sample, uint32_t *sz) {
  assert (topic && sz && sample);

  if (!requires_serialization(topic))
    *sz = topic->m_stype->zerocopy_size;
  else if (allows_serialization_into_buffer(topic))
    *sz = (uint32_t)ddsi_sertype_get_serialized_size(topic->m_stype, (void*) sample);
  else
    return false;

  return true;
}

static dds_return_t dds_write_basic_impl (struct thread_state1 * const ts1, dds_writer *wr, struct ddsi_serdata *d, bool remote_delivery)
{
  struct writer *ddsi_wr = wr->m_wr;
  dds_return_t ret = DDS_RETCODE_OK;

  if (d == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (wr->m_entity.m_domain->gv.m_tkmap, d);

  if (remote_delivery) {
    ret = write_sample_gc (ts1, wr->m_xp, ddsi_wr, d, tk);
    if (ret >= 0) {
      /* Flush out write unless configured to batch */
      if (!wr->whc_batch)
        nn_xpack_send (wr->m_xp, false);
      ret = DDS_RETCODE_OK;
    } else if (ret != DDS_RETCODE_TIMEOUT) {
      ret = DDS_RETCODE_ERROR;
    }
  }

  if (ret == DDS_RETCODE_OK) {
    ret = deliver_locally (ddsi_wr, d, tk);
  }

  ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);

  return ret;
}

dds_return_t dds_request_writer_loan(dds_writer *wr, void **samples_ptr, int32_t n_samples) {
  if (n_samples < 0 || !samples_ptr)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = 0;

  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  dds_loaned_sample_t **loans_ptr = dds_alloc(sizeof(dds_loaned_sample_t*)*(size_t)n_samples);
  if (!loans_ptr)
    goto fail;

  //attempt to request loans from virtual interfaces
  struct endpoint_common *ec = &wr->m_wr->c;
  if (wr->m_topic->m_stype->fixed_size)
  {
    for (uint32_t i = 0; i < ec->n_virtual_pipes; i++)
    {
      for (; ret < n_samples; ret++)
      {
        dds_loaned_sample_t *loan = ddsi_virtual_interface_pipe_request_loan(ec->m_pipes[i], wr->m_topic->m_stype->zerocopy_size);
        if (!loan)
          break;
        loans_ptr[ret] = loan;
      }
    }
  }

  //attempt to request loans from heap based interface
  if (0 == ret)
  {
    for (; ret < n_samples; ret++)
    {
      dds_loaned_sample_t *loan = dds_heap_loan(wr->m_topic->m_stype);
      if (!loan)
        break;
      loans_ptr[ret] = loan;
    }
  }

fail:
  if (ret != n_samples)  //we couldnt get the number of loans requested
  {
    if (loans_ptr)
    {
      for (int32_t i = 0; i < ret; i++)
        dds_loaned_sample_fini(loans_ptr[i]);
    }

    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    for (int32_t i = 0; i < n_samples; i++)
    {
      dds_loan_manager_add_loan(wr->m_loans, loans_ptr[i]);
      samples_ptr[i] = loans_ptr[i]->sample_ptr;
    }
  }

  if (loans_ptr)
    dds_free(loans_ptr);

  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);

  return ret;
}

dds_return_t dds_return_writer_loan(dds_writer *wr, void **samples_ptr, int32_t n_samples) {
  if (n_samples < 0 || !samples_ptr)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  for (int32_t i = 0; i < n_samples && ret == DDS_RETCODE_OK; i++)
  {
    void *sample = samples_ptr[i];
    if (!sample)
      continue;

    dds_loaned_sample_t * loan = dds_loan_manager_find_loan(wr->m_loans, sample);
    if (loan)
    {
      /* refs(0):  user has discarded the sample already*/
      if (!dds_loaned_sample_decr_refs(loan) ||
          !dds_loan_manager_remove_loan(loan))
        ret = DDS_RETCODE_ERROR;
    }
    else
    {
      ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }

  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
  return ret;
}

// has to support two cases:
// 1) data is in an external buffer allocated on the stack or dynamically
// 2) data is in an zerocopy buffer obtained by dds_loan_sample
dds_return_t dds_write_impl (dds_writer *wr, const void * data, dds_time_t tstamp, dds_write_action action)
{
  // 1. Input validation
  struct thread_state1 * const ts1 = lookup_thread_state ();
  const bool writekey = action & DDS_WR_KEY_BIT;
  struct writer *ddsi_wr = wr->m_wr;
  int ret = DDS_RETCODE_OK;
  struct ddsi_serdata *d = NULL;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  // 2. Topic filter
  if (!evalute_topic_filter (wr, data, writekey))
    return DDS_RETCODE_OK;

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);

  // 3. Check whether data is loaned
  dds_loaned_sample_t *supplied_loan = dds_loan_manager_find_loan(wr->m_loans, data);
  dds_loaned_sample_t *loan = NULL;
  if (supplied_loan && supplied_loan->loan_origin)
    loan = supplied_loan;

  // 4. If it is a heap loan, attempt to get a virtual interface loan
  uint32_t required_size = 0;
  if (!loan && get_required_buffer_size(wr->m_topic, data, &required_size))
  {
    struct endpoint_common *ec = &wr->m_wr->c;
    if (required_size)
    {
      //attempt to get a loan from a virtual interface
      for (uint32_t i = 0; i < ec->n_virtual_pipes && !loan; i++)
      {
        ddsi_virtual_interface_pipe_t *p = ec->m_pipes[i];
        loan = ddsi_virtual_interface_pipe_request_loan(p, required_size);
      }
    }
  }

  // ddsi_wr->as can be changed by the matching/unmatching of proxy readers if we don't hold the lock
  // it is rather unfortunate that this then means we have to lock here to check, then lock again to
  // actually distribute the data, so some further refactoring is needed.
  ddsrt_mutex_lock (&ddsi_wr->e.lock);
  struct addrset *as = ddsi_wr->as;
  bool remote_readers = (addrset_empty (as) == 0);  //this does not yet show the correct number of remote readers
  ddsrt_mutex_unlock (&ddsi_wr->e.lock);

  // 5. Create a correct serdata
  if (loan)
    d = ddsi_serdata_from_loaned_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data, loan, remote_readers);
  else
    d = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);

  //the supplied loan may no longer be necessary here
  if (supplied_loan && supplied_loan != loan)
  {
    dds_loaned_sample_decr_refs (supplied_loan);
    dds_loan_manager_remove_loan (supplied_loan);
  }

  if (loan && loan != supplied_loan)
    dds_loan_manager_add_loan (wr->m_loans, loan);

  if(d == NULL) {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto return_loan;
  }

  // refc(d) = 1 after successful construction
  d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) |
                  ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0));
  d->timestamp.v = tstamp;

  // 6. Deliver the data

  // 6.a Deliver via network
  if ((ret = dds_write_basic_impl(ts1, wr, d, remote_readers)) != DDS_RETCODE_OK)
    goto unref_serdata;

  // 6.b Deliver through virtual interface
  if (loan)
  {
    ddsi_virtual_interface_pipe_t *pipe = loan->loan_origin;
    
    //populate metadata fields
    dds_virtual_interface_metadata_t *md = loan->metadata;
    md->guid = ddsi_wr->e.guid;
    md->timestamp = d->timestamp.v;
    md->statusinfo = d->statusinfo;
    if (!pipe->ops.sink_data(pipe, loan))
    {
      ret = DDS_RETCODE_ERROR;
      goto unref_serdata;
    }
    else
    {
      dds_loaned_sample_decr_refs(loan); //loan refs(0)
      d->loan = NULL;
    }
  }

  thread_state_asleep (ts1);
  return ret;

unref_serdata:
  if (d)
    ddsi_serdata_unref(d); // refc(d) = 0
return_loan:
  if(loan)
    dds_loaned_sample_fini(loan);
  thread_state_asleep (ts1);
  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct nn_xpack *xp, struct ddsi_serdata *dinp, bool flush)
{
  return dds_writecdr_impl_common (wr->m_wr, xp, dinp, flush);
}

dds_return_t dds_writecdr_local_orphan_impl (struct local_orphan_writer *lowr, struct nn_xpack *xp, struct ddsi_serdata *dinp)
{
  return dds_writecdr_impl_common (&lowr->wr, xp, dinp, true);
}

void dds_write_flush (dds_entity_t writer)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_writer *wr;
  if (dds_writer_lock (writer, &wr) == DDS_RETCODE_OK)
  {
    thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
    nn_xpack_send (wr->m_xp, true);
    thread_state_asleep (ts1);
    dds_writer_unlock (wr);
  }
}
