// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/version.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery_spdp.h"
#include "ddsi__discovery_addrset.h"
#include "ddsi__discovery_endpoint.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__topic.h"
#include "ddsi__vendor.h"
#include "ddsi__xevent.h"
#include "ddsi__transmit.h"
#include "ddsi__lease.h"
#include "ddsi__xqos.h"
#include "ddsi__spdp_schedule.h"

static bool get_pp_and_spdp_wr (struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, struct ddsi_participant **pp, struct ddsi_writer **spdp_wr)
  ddsrt_nonnull_all;

struct locators_builder {
  ddsi_locators_t *dst;
  struct ddsi_locators_one *storage;
  size_t storage_n;
};

static struct locators_builder locators_builder_init (ddsi_locators_t *dst, struct ddsi_locators_one *storage, size_t storage_n)
{
  dst->n = 0;
  dst->first = NULL;
  dst->last = NULL;
  return (struct locators_builder) {
    .dst = dst,
    .storage = storage,
    .storage_n = storage_n
  };
}

static bool locators_add_one (struct locators_builder *b, const ddsi_locator_t *loc, uint32_t port_override)
{
  if (b->dst->n >= b->storage_n)
    return false;
  if (b->dst->n == 0)
    b->dst->first = b->storage;
  else
    b->dst->last->next = &b->storage[b->dst->n];
  b->dst->last = &b->storage[b->dst->n++];
  b->dst->last->loc = *loc;
  if (port_override != DDSI_LOCATOR_PORT_INVALID)
    b->dst->last->loc.port = port_override;
  b->dst->last->next = NULL;
  return true;
}

void ddsi_get_participant_builtin_topic_data (const struct ddsi_participant *pp, ddsi_plist_t *dst, struct ddsi_participant_builtin_topic_data_locators *locs)
{
  struct ddsi_domaingv * const gv = pp->e.gv;
  size_t size;
  char node[64];
  uint64_t qosdiff;

  ddsi_plist_init_empty (dst);
  dst->present |= PP_PARTICIPANT_GUID | PP_BUILTIN_ENDPOINT_SET |
    PP_PROTOCOL_VERSION | PP_VENDORID | PP_DOMAIN_ID;
  dst->participant_guid = pp->e.guid;
  dst->builtin_endpoint_set = pp->bes;
  dst->protocol_version = gv->config.protocol_version;
  dst->vendorid = DDSI_VENDORID_ECLIPSE;
  dst->domain_id = gv->config.extDomainId.value;
  /* Be sure not to send a DOMAIN_TAG when it is the default (an empty)
     string: it is an "incompatible-if-unrecognized" parameter, and so
     implementations that don't understand the parameter will refuse to
     discover us, and so sending the default would break backwards
     compatibility. */
  if (strcmp (gv->config.domainTag, "") != 0)
  {
    dst->present |= PP_DOMAIN_TAG;
    dst->aliased |= PP_DOMAIN_TAG;
    dst->domain_tag = gv->config.domainTag;
  }

  // Construct unicast locator parameters
  {
    struct locators_builder def_uni = locators_builder_init (&dst->default_unicast_locators, locs->def_uni, MAX_XMIT_CONNS);
    struct locators_builder meta_uni = locators_builder_init (&dst->metatraffic_unicast_locators, locs->meta_uni, MAX_XMIT_CONNS);
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (!gv->xmit_conns[i]->m_factory->m_enable_spdp)
      {
        // skip any interfaces where the address kind doesn't match the selected transport
        // as a reasonablish way of not advertising PSMX locators here
        continue;
      }
#ifndef NDEBUG
      int32_t kind;
#endif
      uint32_t data_port, meta_port;
      if (gv->config.many_sockets_mode != DDSI_MSM_MANY_UNICAST)
      {
#ifndef NDEBUG
        kind = gv->loc_default_uc.kind;
#endif
        assert (kind == gv->loc_meta_uc.kind);
        data_port = gv->loc_default_uc.port;
        meta_port = gv->loc_meta_uc.port;
      }
      else
      {
#ifndef NDEBUG
        kind = pp->m_locator.kind;
#endif
        // FIXME: if the switches strip off VLAN tags on egress ports, then we using the packet's source address is probably wrong in cases where VLANs are being used
        data_port = meta_port = pp->m_locator.port;
      }
      assert (kind == gv->interfaces[i].extloc.kind);
      locators_add_one (&def_uni, &gv->interfaces[i].extloc, data_port);
      locators_add_one (&meta_uni, &gv->interfaces[i].extloc, meta_port);
    }
    if (gv->config.publish_uc_locators)
    {
      dst->present |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
      dst->aliased |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
    }
  }

  if (ddsi_include_multicast_locator_in_discovery (gv))
  {
    dst->present |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
    dst->aliased |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
    struct locators_builder def_mc = locators_builder_init (&dst->default_multicast_locators, &locs->def_multi, 1);
    struct locators_builder meta_mc = locators_builder_init (&dst->metatraffic_multicast_locators, &locs->meta_multi, 1);
    locators_add_one (&def_mc, &gv->loc_default_mc, DDSI_LOCATOR_PORT_INVALID);
    locators_add_one (&meta_mc, &gv->loc_meta_mc, DDSI_LOCATOR_PORT_INVALID);
  }

  /* Add Adlink specific version information */
  {
    dst->present |= PP_ADLINK_PARTICIPANT_VERSION_INFO;
    memset (&dst->adlink_participant_version_info, 0, sizeof (dst->adlink_participant_version_info));
    dst->adlink_participant_version_info.version = 0;
    dst->adlink_participant_version_info.flags = 0;
#if DDSRT_HAVE_GETHOSTNAME
    if (ddsrt_gethostname(node, sizeof(node)-1) < 0)
#endif
      (void) ddsrt_strlcpy (node, "unknown", sizeof (node));
    size = strlen(node) + strlen(DDS_VERSION) + strlen(DDS_HOST_NAME) + strlen(DDS_TARGET_NAME) + 4; // + '///' + '\0';
    dst->adlink_participant_version_info.internals = ddsrt_malloc(size);
    (void) snprintf(dst->adlink_participant_version_info.internals, size, "%s/%s/%s/%s", node, DDS_VERSION, DDS_HOST_NAME, DDS_TARGET_NAME);
    ETRACE (pp, "ddsi_spdp_write("PGUIDFMT") - internals: %s\n", PGUID (pp->e.guid), dst->adlink_participant_version_info.internals);
  }

  /* Add Cyclone specific information */
  {
    const uint32_t bufsz = ddsi_receive_buffer_size (gv->m_factory);
    if (bufsz > 0)
    {
      dst->present |= PP_CYCLONE_RECEIVE_BUFFER_SIZE;
      dst->cyclone_receive_buffer_size = bufsz;
    }
  }
  if (gv->config.redundant_networking)
  {
    dst->present |= PP_CYCLONE_REDUNDANT_NETWORKING;
    dst->cyclone_redundant_networking = true;
  }

#ifdef DDS_HAS_SECURITY
  /* Add Security specific information. */
  if (ddsi_omg_get_participant_security_info (pp, &(dst->participant_security_info))) {
    dst->present |= PP_PARTICIPANT_SECURITY_INFO;
    dst->aliased |= PP_PARTICIPANT_SECURITY_INFO;
  }
#endif

  /* Participant QoS's insofar as they are set, different from the default, and mapped to the SPDP data, rather than to the Adlink-specific CMParticipant endpoint. */
  qosdiff = ddsi_xqos_delta (&pp->plist->qos, &ddsi_default_qos_participant, DDSI_QP_USER_DATA | DDSI_QP_ENTITY_NAME | DDSI_QP_PROPERTY_LIST | DDSI_QP_LIVELINESS);
  if (gv->config.explicitly_publish_qos_set_to_default)
    qosdiff |= ~(DDSI_QP_UNRECOGNIZED_INCOMPATIBLE_MASK | DDSI_QP_LIVELINESS);

  assert (dst->qos.present == 0);
  ddsi_plist_mergein_missing (dst, pp->plist, 0, qosdiff);
#ifdef DDS_HAS_SECURITY
  if (ddsi_omg_participant_is_secure(pp))
    ddsi_plist_mergein_missing (dst, pp->plist, PP_IDENTITY_TOKEN | PP_PERMISSIONS_TOKEN, 0);
#endif
}

struct ddsi_spdp_directed_xevent_cb_arg {
  ddsi_guid_t pp_guid;
  int nrepeats;
  ddsi_guid_prefix_t dest_proxypp_guid_prefix;
};

static bool get_pp_and_spdp_wr (struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, struct ddsi_participant **pp, struct ddsi_writer **spdp_wr)
{
  if ((*pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, pp_guid)) == NULL)
  {
    GVTRACE ("handle_xevk_spdp "PGUIDFMT" - unknown guid\n", PGUID (*pp_guid));
    return false;
  }
  if (ddsi_get_builtin_writer (*pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER, spdp_wr) != DDS_RETCODE_OK)
  {
    GVTRACE ("handle_xevk_spdp "PGUIDFMT" - spdp writer of participant not found\n", PGUID (*pp_guid));
    return false;
  }
  if (*spdp_wr == NULL)
    return false;
  return true;
}

static void ddsi_spdp_directed_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, UNUSED_ARG (struct ddsi_xpack *xp), void *varg, ddsrt_mtime_t tnow)
{
  struct ddsi_spdp_directed_xevent_cb_arg * const arg = varg;
  struct ddsi_participant *pp;
  struct ddsi_writer *spdp_wr;

  if (!get_pp_and_spdp_wr (gv, &arg->pp_guid, &pp, &spdp_wr))
  {
    ddsi_delete_xevent (ev);
    return;
  }

  const ddsi_guid_t guid = { .prefix = arg->dest_proxypp_guid_prefix, .entityid = { .u = DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER } };
  struct ddsi_proxy_reader *prd;
  if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gv->entity_index, &guid)) == NULL)
  {
    GVTRACE ("xmit spdp: no proxy reader "PGUIDFMT"\n", PGUID (guid));
    ddsi_delete_xevent (ev);
  }
  else if (!ddsi_spdp_force_republish (gv->spdp_schedule, pp, prd))
  {
    // it is just a local race, so a few milliseconds should be plenty
    ddsrt_mtime_t tnext = ddsrt_mtime_add_duration (tnow, DDS_MSECS (10));
    GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x too early (resched %gs)\n",
             PGUID (pp->e.guid),
             PGUIDPREFIX (arg->dest_proxypp_guid_prefix), DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
             (double)(tnext.v - tnow.v) / 1e9);
    (void) ddsi_resched_xevent_if_earlier (ev, tnext);
  }
  else if (--arg->nrepeats == 0 ||
           pp->plist->qos.liveliness.lease_duration < DDS_SECS (1) ||
           (!gv->config.spdp_interval.isdefault && gv->config.spdp_interval.value < DDS_SECS (1)))
  {
    ddsi_delete_xevent (ev);
  }
  else
  {
    ddsrt_mtime_t tnext = ddsrt_mtime_add_duration (tnow, DDS_SECS (1));
    GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x (resched %gs)\n",
             PGUID (pp->e.guid),
             PGUIDPREFIX (arg->dest_proxypp_guid_prefix), DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
             (double)(tnext.v - tnow.v) / 1e9);
    (void) ddsi_resched_xevent_if_earlier (ev, tnext);
  }
}

static unsigned pseudo_random_delay (const ddsi_guid_t *x, const ddsi_guid_t *y, ddsrt_mtime_t tnow)
{
  /* You know, an ordinary random generator would be even better, but
     the C library doesn't have a reentrant one and I don't feel like
     integrating, say, the Mersenne Twister right now. */
  static const uint64_t cs[] = {
    UINT64_C (15385148050874689571),
    UINT64_C (17503036526311582379),
    UINT64_C (11075621958654396447),
    UINT64_C ( 9748227842331024047),
    UINT64_C (14689485562394710107),
    UINT64_C (17256284993973210745),
    UINT64_C ( 9288286355086959209),
    UINT64_C (17718429552426935775),
    UINT64_C (10054290541876311021),
    UINT64_C (13417933704571658407)
  };
  uint32_t a = x->prefix.u[0], b = x->prefix.u[1], c = x->prefix.u[2], d = x->entityid.u;
  uint32_t e = y->prefix.u[0], f = y->prefix.u[1], g = y->prefix.u[2], h = y->entityid.u;
  uint32_t i = (uint32_t) ((uint64_t) tnow.v >> 32), j = (uint32_t) tnow.v;
  uint64_t m = 0;
  m += (a + cs[0]) * (b + cs[1]);
  m += (c + cs[2]) * (d + cs[3]);
  m += (e + cs[4]) * (f + cs[5]);
  m += (g + cs[6]) * (h + cs[7]);
  m += (i + cs[8]) * (j + cs[9]);
  return (unsigned) (m >> 32);
}

static void respond_to_spdp (const struct ddsi_domaingv *gv, const ddsi_guid_t *dest_proxypp_guid)
{
  struct ddsi_entity_enum_participant est;
  struct ddsi_participant *pp;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  ddsi_entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = ddsi_entidx_enum_participant_next (&est)) != NULL)
  {
    /* delay_base has 32 bits, so delay_norm is approximately 1s max;
       delay_max <= 1s by gv.config checks */
    unsigned delay_base = pseudo_random_delay (&pp->e.guid, dest_proxypp_guid, tnow);
    unsigned delay_norm = delay_base >> 2;
    int64_t delay_max_ms = gv->config.spdp_response_delay_max / 1000000;
    int64_t delay = (int64_t) delay_norm * delay_max_ms / 1000;
    ddsrt_mtime_t tsched = ddsrt_mtime_add_duration (tnow, delay);
    GVTRACE (" %"PRId64, delay);
    struct ddsi_spdp_directed_xevent_cb_arg arg = {
      .pp_guid = pp->e.guid,
      .nrepeats = 4, .dest_proxypp_guid_prefix = dest_proxypp_guid->prefix
    };
    ddsi_qxev_callback (gv->xevents, tsched, ddsi_spdp_directed_xevent_cb, &arg, sizeof (arg), false);
  }
  ddsi_entidx_enum_participant_fini (&est);
}

static void handle_spdp_dead (const struct ddsi_receiver_state *rst, ddsi_entityid_t pwr_entityid, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap, unsigned statusinfo)
{
  struct ddsi_domaingv * const gv = rst->gv;
  ddsi_guid_t guid;

  GVLOGDISC ("SPDP ST%x", statusinfo);

  if (datap->present & PP_PARTICIPANT_GUID)
  {
    guid = datap->participant_guid;
    GVLOGDISC (" %"PRIx32":%"PRIx32":%"PRIx32":%"PRIx32, PGUID (guid));
    assert (guid.entityid.u == DDSI_ENTITYID_PARTICIPANT);
    if (ddsi_is_proxy_participant_deletion_allowed(gv, &guid, pwr_entityid))
    {
      if (ddsi_delete_proxy_participant_by_guid (gv, &guid, timestamp, false) < 0)
      {
        GVLOGDISC (" unknown");
      }
      else
      {
        GVLOGDISC (" delete");
      }
    }
    else
    {
      GVLOGDISC (" not allowed");
    }
  }
  else
  {
    GVWARNING ("data (SPDP, vendor %u.%u): no/invalid payload\n", rst->vendor.id[0], rst->vendor.id[1]);
  }
}

enum find_internal_interface_index_result {
  FIIIR_NO_INFO,
  FIIIR_NO_MATCH,
  FIIIR_MATCH
};

static enum find_internal_interface_index_result find_internal_interface_index (const struct ddsi_domaingv *gv, uint32_t nwstack_if_index, int *internal_if_index) ddsrt_nonnull_all;

static enum find_internal_interface_index_result find_internal_interface_index (const struct ddsi_domaingv *gv, uint32_t nwstack_if_index, int *internal_if_index)
{
  if (nwstack_if_index == 0)
    return FIIIR_NO_INFO;
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    if (gv->interfaces[i].if_index == nwstack_if_index)
    {
      *internal_if_index = i;
      return FIIIR_MATCH;
    }
  }
  return FIIIR_NO_MATCH;
}

static bool accept_packet_from_interface (const struct ddsi_domaingv *gv, const struct ddsi_receiver_state *rst)
{
  int internal_if_index;
  switch (find_internal_interface_index (gv, rst->pktinfo.if_index, &internal_if_index))
  {
    case FIIIR_NO_MATCH:
      // Don't accept SPDP packets received on a interface outside the enabled ones
      break;
    case FIIIR_MATCH:
      // Accept all unicast packets (except those manifestly received over an interface we are not using)
      // and multicast packets if we chose to do multicast discovery on the interface over we received it
      if (!ddsi_is_mcaddr (gv, &rst->pktinfo.dst) || gv->interfaces[internal_if_index].allow_multicast & DDSI_AMC_SPDP)
        return true;
      break;
    case FIIIR_NO_INFO:
      // We could try to match the source address with an interface. Perhaps the destination address
      // is available even though the interface index is not, allowing some tricks.  On Linux, Windows
      // and macOS this shouldn't happen, so rather than complicate things unnecessarily, just accept
      // the packet like we always used to do.
      return true;
  }
  return false;
}

enum participant_guid_is_known_result {
  PGIKR_UNKNOWN,
  PGIKR_KNOWN,
  PGIKR_KNOWN_BUT_INTERESTING
};

static enum participant_guid_is_known_result participant_guid_is_known (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap)
{
  struct ddsi_domaingv * const gv = rst->gv;
  struct ddsi_entity_common *existing_entity;
  if ((existing_entity = ddsi_entidx_lookup_guid_untyped (gv->entity_index, &datap->participant_guid)) == NULL)
  {
    /* Local SPDP packets may be looped back, and that can include ones
       for participants currently being deleted.  The first thing that
       happens when deleting a participant is removing it from the hash
       table, and consequently the looped back packet may appear to be
       from an unknown participant.  So we handle that. */
    if (ddsi_is_deleted_participant_guid (gv->deleted_participants, &datap->participant_guid))
    {
      RSTTRACE ("SPDP ST0 "PGUIDFMT" (recently deleted)", PGUID (datap->participant_guid));
      return PGIKR_KNOWN;
    }
  }
  else if (existing_entity->kind == DDSI_EK_PARTICIPANT)
  {
    RSTTRACE ("SPDP ST0 "PGUIDFMT" (local)", PGUID (datap->participant_guid));
    return PGIKR_KNOWN;
  }
  else if (existing_entity->kind == DDSI_EK_PROXY_PARTICIPANT)
  {
    struct ddsi_proxy_participant *proxypp = (struct ddsi_proxy_participant *) existing_entity;
    struct ddsi_lease *lease;
    int interesting = 0;
    RSTTRACE ("SPDP ST0 "PGUIDFMT" (known)", PGUID (datap->participant_guid));
    /* SPDP processing is so different from normal processing that we are
       even skipping the automatic lease renewal. Note that proxy writers
       that are not alive are not set alive here. This is done only when
       data is received from a particular pwr (in handle_regular) */
    if ((lease = ddsrt_atomic_ldvoidp (&proxypp->minl_auto)) != NULL)
      ddsi_lease_renew (lease, ddsrt_time_elapsed ());
    ddsrt_mutex_lock (&proxypp->e.lock);
    if (seq > proxypp->seq)
    {
      interesting = 1;
      if (!(gv->logconfig.c.mask & DDS_LC_TRACE))
        GVLOGDISC ("SPDP ST0 "PGUIDFMT, PGUID (datap->participant_guid));
      GVLOGDISC (" (update)");
      ddsi_update_proxy_participant_plist_locked (proxypp, seq, datap, timestamp);
    }
    ddsrt_mutex_unlock (&proxypp->e.lock);
    return interesting ? PGIKR_KNOWN_BUT_INTERESTING : PGIKR_KNOWN;
  }
  else
  {
    /* mismatch on entity kind: that should never have gotten past the input validation */
    GVWARNING ("data (SPDP, vendor %u.%u): "PGUIDFMT" kind mismatch\n", rst->vendor.id[0], rst->vendor.id[1], PGUID (datap->participant_guid));
    return PGIKR_KNOWN;
  }
  return PGIKR_UNKNOWN;
}

static uint32_t get_builtin_endpoint_set (const struct ddsi_receiver_state *rst, const ddsi_plist_t *datap, bool is_secure)
{
  struct ddsi_domaingv * const gv = rst->gv;
  uint32_t builtin_endpoint_set;
  assert (datap->present & PP_BUILTIN_ENDPOINT_SET);
  /* At some point the RTI implementation didn't mention
     BUILTIN_ENDPOINT_DDSI_PARTICIPANT_MESSAGE_DATA_READER & ...WRITER, or
     so it seemed; and yet they are necessary for correct operation,
     so add them. */
  builtin_endpoint_set = datap->builtin_endpoint_set;
  if (ddsi_vendor_is_rti (rst->vendor) &&
      ((builtin_endpoint_set &
        (DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
         DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER))
       != (DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
           DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER)) &&
      gv->config.assume_rti_has_pmd_endpoints)
  {
    GVLOGDISC ("data (SPDP, vendor %u.%u): assuming unadvertised PMD endpoints do exist\n",
               rst->vendor.id[0], rst->vendor.id[1]);
    builtin_endpoint_set |=
    DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
    DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
  }
  /* Make sure we don't create any security builtin endpoint when it's considered unsecure. */
  if (!is_secure)
    builtin_endpoint_set &= DDSI_BES_MASK_NON_SECURITY;
  return builtin_endpoint_set;
}

static bool get_locators (const struct ddsi_receiver_state *rst, const ddsi_plist_t *datap, struct ddsi_addrset **as_default, struct ddsi_addrset **as_meta)
{
  struct ddsi_domaingv * const gv = rst->gv;
  const ddsi_locators_t emptyset = { .n = 0, .first = NULL, .last = NULL };
  const ddsi_locators_t *uc;
  const ddsi_locators_t *mc;
  bool allow_srcloc;
  ddsi_interface_set_t inherited_intfs;

  uc = (datap->present & PP_DEFAULT_UNICAST_LOCATOR) ? &datap->default_unicast_locators : &emptyset;
  mc = (datap->present & PP_DEFAULT_MULTICAST_LOCATOR) ? &datap->default_multicast_locators : &emptyset;
  if (gv->config.tcp_use_peeraddr_for_unicast)
    uc = &emptyset; // force use of source locator
  allow_srcloc = (uc == &emptyset) && !ddsi_is_unspec_locator (&rst->pktinfo.src);
  ddsi_interface_set_init (&inherited_intfs);
  *as_default = ddsi_addrset_from_locatorlists (gv, uc, mc, &rst->pktinfo, allow_srcloc, &inherited_intfs);

  uc = (datap->present & PP_METATRAFFIC_UNICAST_LOCATOR) ? &datap->metatraffic_unicast_locators : &emptyset;
  mc = (datap->present & PP_METATRAFFIC_MULTICAST_LOCATOR) ? &datap->metatraffic_multicast_locators : &emptyset;
  if (gv->config.tcp_use_peeraddr_for_unicast)
    uc = &emptyset; // force use of source locator
  allow_srcloc = (uc == &emptyset) && !ddsi_is_unspec_locator (&rst->pktinfo.src);
  ddsi_interface_set_init (&inherited_intfs);
  *as_meta = ddsi_addrset_from_locatorlists (gv, uc, mc, &rst->pktinfo, allow_srcloc, &inherited_intfs);

  ddsi_log_addrset (gv, DDS_LC_DISCOVERY, " (data", *as_default);
  ddsi_log_addrset (gv, DDS_LC_DISCOVERY, " meta", *as_meta);
  GVLOGDISC (")");

  if (ddsi_addrset_contains_non_psmx_uc (*as_default) && ddsi_addrset_contains_non_psmx_uc (*as_meta))
    return true;
  else
  {
    GVLOGDISC (" (no unicast address");
    ddsi_unref_addrset (*as_default);
    ddsi_unref_addrset (*as_meta);
    return false;
  }
}

// Result for handle_spdp_alive, "interesting" vs "not interesting" affects the
// logging category for subsequent logging output
enum handle_spdp_result {
  HSR_NOT_INTERESTING,
  HSR_INTERESTING
};

static enum handle_spdp_result handle_spdp_alive (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap)
{
  struct ddsi_domaingv * const gv = rst->gv;

  // Don't just process any SPDP packet but look at the network interface and uni/multicast
  // One could refine this even further by also looking at the locators advertised in the
  // packet, but this should suffice to drop unwanted multicast packets, which is the only
  // use case I am currently aware of.
  if (!accept_packet_from_interface (gv, rst))
    return HSR_NOT_INTERESTING;

  /* If advertised domain id or domain tag doesn't match, ignore the message.  Do this first to
     minimize the impact such messages have. */
  {
    const uint32_t domain_id = (datap->present & PP_DOMAIN_ID) ? datap->domain_id : gv->config.extDomainId.value;
    const char *domain_tag = (datap->present & PP_DOMAIN_TAG) ? datap->domain_tag : "";
    if (domain_id != gv->config.extDomainId.value || strcmp (domain_tag, gv->config.domainTag) != 0)
    {
      GVTRACE ("ignore remote participant in mismatching domain %"PRIu32" tag \"%s\"\n", domain_id, domain_tag);
      return HSR_NOT_INTERESTING;
    }
  }

  if (!(datap->present & PP_PARTICIPANT_GUID) || !(datap->present & PP_BUILTIN_ENDPOINT_SET))
  {
    GVWARNING ("data (SPDP, vendor %u.%u): no/invalid payload\n", rst->vendor.id[0], rst->vendor.id[1]);
    return HSR_NOT_INTERESTING;
  }

  const enum participant_guid_is_known_result pgik_result = participant_guid_is_known (rst, seq, timestamp, datap);
  if (pgik_result != PGIKR_UNKNOWN)
    return (pgik_result == PGIKR_KNOWN_BUT_INTERESTING) ? HSR_INTERESTING : HSR_NOT_INTERESTING;

  const bool is_secure = ((datap->builtin_endpoint_set & DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER) != 0 && (datap->present & PP_IDENTITY_TOKEN));

  const uint32_t builtin_endpoint_set = get_builtin_endpoint_set (rst, datap, is_secure);
  GVLOGDISC ("SPDP ST0 "PGUIDFMT" bes %"PRIx32"%s NEW", PGUID (datap->participant_guid), builtin_endpoint_set, is_secure ? " (secure)" : "");

  if (datap->present & PP_ADLINK_PARTICIPANT_VERSION_INFO)
  {
    GVLOGDISC (" (0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32" %s)",
               datap->adlink_participant_version_info.version,
               datap->adlink_participant_version_info.flags,
               datap->adlink_participant_version_info.unused[0],
               datap->adlink_participant_version_info.unused[1],
               datap->adlink_participant_version_info.unused[2],
               datap->adlink_participant_version_info.internals);
  }

  /* Can't do "mergein_missing" because of constness of *datap */
  dds_duration_t lease_duration;
  if (datap->qos.present & DDSI_QP_LIVELINESS)
    lease_duration = datap->qos.liveliness.lease_duration;
  else
  {
    assert (ddsi_default_qos_participant.present & DDSI_QP_LIVELINESS);
    lease_duration = ddsi_default_qos_participant.liveliness.lease_duration;
  }

  struct ddsi_addrset *as_meta, *as_default;
  if (!get_locators (rst, datap, &as_default, &as_meta))
  {
    GVLOGDISC (" (no unicast address");
    return HSR_INTERESTING;
  }

  GVLOGDISC (" QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, &datap->qos);
  GVLOGDISC ("}\n");

  struct ddsi_proxy_participant *proxy_participant;
  if (!ddsi_new_proxy_participant (&proxy_participant, gv, &datap->participant_guid, builtin_endpoint_set, as_default, as_meta, datap, lease_duration, rst->vendor, timestamp, seq))
  {
    /* If no proxy participant was created, don't respond */
    return HSR_NOT_INTERESTING;
  }
  else
  {
    /* Force transmission of SPDP messages - we're not very careful
       in avoiding the processing of SPDP packets addressed to others
       so filter here */
    const bool have_dst = (rst->dst_guid_prefix.u[0] != 0 || rst->dst_guid_prefix.u[1] != 0 || rst->dst_guid_prefix.u[2] != 0);
    if (have_dst)
      GVLOGDISC ("directed SPDP packet -> not responding\n");
    else
    {
      GVLOGDISC ("broadcasted SPDP packet -> answering");
      respond_to_spdp (gv, &datap->participant_guid);
    }
    return HSR_INTERESTING;
  }
}

void ddsi_handle_spdp (const struct ddsi_receiver_state *rst, ddsi_entityid_t pwr_entityid, ddsi_seqno_t seq, const struct ddsi_serdata *serdata)
{
  struct ddsi_domaingv * const gv = rst->gv;
  ddsi_plist_t decoded_data;
  if (ddsi_serdata_to_sample (serdata, &decoded_data, NULL, NULL))
  {
    enum handle_spdp_result interesting = HSR_NOT_INTERESTING;
    switch (serdata->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER))
    {
      case 0:
        interesting = handle_spdp_alive (rst, seq, serdata->timestamp, &decoded_data);
        break;

      case DDSI_STATUSINFO_DISPOSE:
      case DDSI_STATUSINFO_UNREGISTER:
      case (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER):
        handle_spdp_dead (rst, pwr_entityid, serdata->timestamp, &decoded_data, serdata->statusinfo);
        interesting = HSR_INTERESTING;
        break;
    }

    ddsi_plist_fini (&decoded_data);
    GVLOG (interesting == HSR_INTERESTING ? DDS_LC_DISCOVERY : DDS_LC_TRACE, "\n");
  }
}
