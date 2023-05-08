// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "dds__reader.h"
#include "test_common.h"

static dds_entity_t g_domain      = 0;
static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_topic       = 0;

#define MAX_SAMPLES 2

static void setup (void)
{
  const char *config = "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<Discovery>\
  <Tag>\\${CYCLONEDDS_PID}</Tag>\
</Discovery>";
  char *conf = ddsrt_expand_envvars (config, 0);
  g_domain = dds_create_domain (0, conf);
  CU_ASSERT_FATAL (g_domain > 0);
  ddsrt_free (conf);

  g_participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_FATAL(g_participant > 0);
  g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
  CU_ASSERT_FATAL(g_topic > 0);
  g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_subscriber > 0);
  g_publisher = dds_create_publisher(g_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_publisher > 0);
  g_writer = dds_create_writer(g_publisher, g_topic, NULL, NULL);
  CU_ASSERT_FATAL(g_writer > 0);
  g_reader = dds_create_reader(g_subscriber, g_topic, NULL, NULL);
  CU_ASSERT_FATAL(g_reader > 0);
}

static void teardown (void)
{
  dds_delete (g_domain);
}

enum cdqobe_kind {
  CDQOBE_READER,
  CDQOBE_SUBSCRIBER,
  CDQOBE_TOPIC
};

static void check_default_qos_of_builtin_entity (dds_entity_t entity, enum cdqobe_kind kind)
{
  dds_return_t ret;
  int64_t deadline;
  int64_t liveliness_lease_duration;
  int64_t minimum_separation;
  int64_t max_blocking_time;
  int64_t autopurge_nowriter_samples_delay;
  int64_t autopurge_disposed_samples_delay;

  dds_durability_kind_t durability_kind;
  dds_presentation_access_scope_kind_t presentation_access_scope_kind;
  bool presentation_coherent_access;
  bool presentation_ordered_access;
  dds_ownership_kind_t ownership_kind;
  dds_liveliness_kind_t liveliness_kind;
  dds_reliability_kind_t reliability_kind;
  dds_destination_order_kind_t destination_order_kind;
  dds_history_kind_t history_kind;
  int32_t history_depth;
  int32_t resource_limits_max_samples;
  int32_t resource_limits_max_instances;
  int32_t resource_limits_max_samples_per_instance;

  char **partitions = NULL;
  uint32_t plen = 0;

  dds_qos_t *qos = dds_create_qos();
  CU_ASSERT_FATAL(qos != NULL);

  ret = dds_get_qos(entity, qos);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);

  bool x;
  x = dds_qget_durability(qos, &durability_kind);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_presentation(qos, &presentation_access_scope_kind, &presentation_coherent_access, &presentation_ordered_access);
  CU_ASSERT_FATAL(x || kind != CDQOBE_READER);
  x = dds_qget_deadline(qos, &deadline);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_ownership(qos, &ownership_kind);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_liveliness(qos, &liveliness_kind, &liveliness_lease_duration);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_time_based_filter(qos, &minimum_separation);
  CU_ASSERT_FATAL(x || kind != CDQOBE_READER);
  x = dds_qget_reliability(qos, &reliability_kind, &max_blocking_time);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_destination_order(qos, &destination_order_kind);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_history(qos, &history_kind, &history_depth);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_resource_limits(qos, &resource_limits_max_samples, &resource_limits_max_instances, &resource_limits_max_samples_per_instance);
  CU_ASSERT_FATAL(x || kind == CDQOBE_SUBSCRIBER);
  x = dds_qget_reader_data_lifecycle(qos, &autopurge_nowriter_samples_delay, &autopurge_disposed_samples_delay);
  CU_ASSERT_FATAL(x || kind != CDQOBE_READER);
  x = dds_qget_partition(qos, &plen, &partitions);
  CU_ASSERT_FATAL(x || kind == CDQOBE_TOPIC);

  if (kind == CDQOBE_READER || kind == CDQOBE_TOPIC)
  {
    CU_ASSERT_FATAL(durability_kind == DDS_DURABILITY_TRANSIENT_LOCAL);
    CU_ASSERT_FATAL(deadline == DDS_INFINITY);
    CU_ASSERT_FATAL(ownership_kind == DDS_OWNERSHIP_SHARED);
    CU_ASSERT_FATAL(liveliness_kind == DDS_LIVELINESS_AUTOMATIC);
    CU_ASSERT_FATAL(reliability_kind == DDS_RELIABILITY_RELIABLE);
    CU_ASSERT_FATAL(max_blocking_time == DDS_MSECS(100));
    CU_ASSERT_FATAL(destination_order_kind == DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
    CU_ASSERT_FATAL(history_kind == DDS_HISTORY_KEEP_LAST);
    CU_ASSERT_FATAL(history_depth == 1);
    CU_ASSERT_FATAL(resource_limits_max_instances == DDS_LENGTH_UNLIMITED);
    CU_ASSERT_FATAL(resource_limits_max_samples == DDS_LENGTH_UNLIMITED);
    CU_ASSERT_FATAL(resource_limits_max_samples_per_instance == DDS_LENGTH_UNLIMITED);
  }

  if (kind == CDQOBE_READER || kind == CDQOBE_SUBSCRIBER)
  {
    CU_ASSERT_FATAL(presentation_access_scope_kind == DDS_PRESENTATION_TOPIC);
    CU_ASSERT_FATAL(presentation_coherent_access == false);
    CU_ASSERT_FATAL(presentation_ordered_access == false);
    CU_ASSERT_FATAL(plen == 1);
    CU_ASSERT_STRING_EQUAL_FATAL(partitions[0], "__BUILT-IN PARTITION__");
  }

  if (kind == CDQOBE_READER)
  {
    CU_ASSERT_FATAL(minimum_separation == 0);
    CU_ASSERT_FATAL(autopurge_nowriter_samples_delay == DDS_INFINITY);
    CU_ASSERT_FATAL(autopurge_disposed_samples_delay == DDS_INFINITY);
  }

  if (plen > 0) {
    for (uint32_t i = 0; i < plen; i++) {
      dds_free(partitions[i]);
    }
    dds_free(partitions);
  }
  dds_delete_qos(qos);
}

/// @brief This test will check that the dds_find_topic function does not return the builtintopic handles when used.
/// @methodology
/// - Use dds_find_topic for all builtin topic names in participant scope.
/// - Check that the handle returned is 0.
CU_Test(ddsc_builtin_topics, availability_builtin_topics, .init = setup, .fini = teardown)
{
  dds_entity_t topic;

  topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSParticipant", NULL, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
  topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSTopic", NULL, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
  topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSSubscription", NULL, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
  topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSPublication", NULL, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
}

/// @brief This test will check that the publication builtin topic returns notifications with the correct topic name when a writer is created.
/// @methodology
/// - Create a reader on the builtin publication topic.
/// - Read notification messages on this topic.
/// - Check that there are notification messages returned.
/// - Check that the name of the topic writers are reported created on is called "RoundTrip".
CU_Test(ddsc_builtin_topics, read_publication_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  dds_builtintopic_endpoint_t *data;
  void *samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = NULL;
  ret = dds_read(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  data = samples[0];
  CU_ASSERT_FATAL(ret > 0);
  CU_ASSERT_STRING_EQUAL_FATAL(data->topic_name, "RoundTrip");
  dds_return_loan(reader, samples, ret);
}

/// @brief This test will check that the subscription builtin topic returns notifications with the correct topic name and qos when a reader is created.
/// @methodology
/// - Create a reader on the builtin subscription topic.
/// - Read notification messages on this topic.
/// - Check that there are notification messages returned.
/// - Check that the name of the topics readers are reported created on is called "RoundTrip" and "DCPSSubscription".
CU_Test(ddsc_builtin_topics, read_subscription_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];
  const char *exp[] = { "DCPSSubscription", "RoundTrip" };
  unsigned seen = 0;
  dds_qos_t *qos;

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = NULL;
  ret = dds_read(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret == 2);
  qos = dds_create_qos();
  for (int i = 0; i < ret; i++) {
    dds_builtintopic_endpoint_t *data = samples[i];
    for (size_t j = 0; j < sizeof (exp) / sizeof (exp[0]); j++) {
      if (strcmp (data->topic_name, exp[j]) == 0) {
        seen |= 1u << j;
        dds_return_t get_qos_ret = dds_get_qos(j == 0 ? reader : g_reader, qos);
        CU_ASSERT_FATAL(get_qos_ret == DDS_RETCODE_OK);
        const bool eq = dds_qos_equal(qos, data->qos);
        CU_ASSERT_FATAL(eq);
      }
    }
  }
  CU_ASSERT_FATAL(seen == 3);
  dds_delete_qos(qos);

  dds_return_loan(reader, samples, ret);
}

/// @brief This test will check that the participant builtin topic returns notifications when a participant is created.
/// @methodology
/// - Create a reader on the builtin participant topic.
/// - Read notification messages on this topic.
/// - Check that there are notification messages returned.
CU_Test(ddsc_builtin_topics, read_participant_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = NULL;
  ret = dds_read(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);
  dds_return_loan(reader, samples, ret);
}

/// @brief This test will check that the topic builtin topic returns notifications with the correct topic name when a topic is created.
/// @methodology - Create a reader on the topic builtin topic.
/// - Read notification messages on this topic.
/// - Check that there are notification messages returned and that none of the notifications mention the builtin topic names.
CU_Test(ddsc_builtin_topics, read_topic_data, .init = setup, .fini = teardown)
{
#ifdef DDS_HAS_TOPIC_DISCOVERY
  const char *exp[] = { "RoundTrip", "DCPSPublication", "DCPSSubscription", "DCPSTopic" };
  unsigned seen = 0;
  dds_entity_t reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  void * samples[MAX_SAMPLES] = { NULL };
  dds_sample_info_t info[MAX_SAMPLES];
  dds_return_t ret = dds_read(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret >= 0);
  for (int i = 0; i < ret; i++)
  {
    dds_builtintopic_topic_t *data = samples[i];
    for (size_t j = 0; j < sizeof (exp) / sizeof (exp[0]); j++)
    {
      if (strcmp (data->topic_name, exp[j]) == 0)
        seen |= 1u << j;
    }
  }
  CU_ASSERT_FATAL(seen == 1); // built-in topics should not be reported as DCPSTopic samples
  dds_return_loan(reader, samples, ret);
#endif /* DDS_HAS_TOPIC_DISCOVERY */
}

/// @brief This test will check that the subscribers for the publication, subscription, participant and topic builtin topics are all the same entity.
/// @methodology
/// - Create readers for the subscription, publication, participant and topic builtin topics.
/// - Get the parents of said readers, these should be the subscribers.
/// - Check that all these entities are the same.
CU_Test(ddsc_builtin_topics, same_subscriber, .init = setup, .fini = teardown)
{
  dds_entity_t subscription_rdr;
  dds_entity_t subscription_subscriber;

  dds_entity_t publication_rdr;
  dds_entity_t publication_subscriber;

  dds_entity_t participant_rdr;
  dds_entity_t participant_subscriber;

  dds_entity_t topic_rdr;
  dds_entity_t topic_subscriber;

  subscription_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(subscription_rdr > 0);
  subscription_subscriber = dds_get_parent(subscription_rdr);
  CU_ASSERT_FATAL(subscription_subscriber > 0);

  publication_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(publication_rdr > 0);
  publication_subscriber = dds_get_parent(publication_rdr);
  CU_ASSERT_FATAL(publication_subscriber > 0);

  CU_ASSERT_FATAL(subscription_subscriber == publication_subscriber);

  participant_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(participant_rdr > 0);
  participant_subscriber = dds_get_parent(participant_rdr);
  CU_ASSERT_FATAL(participant_subscriber > 0);

  CU_ASSERT_FATAL(publication_subscriber == participant_subscriber);

  topic_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
  CU_ASSERT_FATAL(topic_rdr > 0);
  topic_subscriber = dds_get_parent(topic_rdr);
  CU_ASSERT_FATAL(topic_subscriber > 0);
  CU_ASSERT_FATAL(participant_subscriber == topic_subscriber);
#else
  (void) topic_subscriber;
  CU_ASSERT_EQUAL_FATAL(topic_rdr, DDS_RETCODE_UNSUPPORTED);
#endif
}

/// @brief This test will check that the default qos settings for the topic builtin topic reader and its subscriber parent.
/// @methodology
/// - Create a reader for the topic builtin topic.
/// - Check whether the QoS settings for this reader are the same as the default reader QoS settings.
/// - Get the subscriber parent of the reader.
/// - Check whether the QoS settings for this subscriber are the same as the default subscriber QoS settings.
CU_Test(ddsc_builtin_topics, builtin_qos, .init = setup, .fini = teardown)
{
  dds_entity_t dds_sub_rdr;
  dds_entity_t dds_sub_subscriber;

  dds_sub_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(dds_sub_rdr > 0);
  check_default_qos_of_builtin_entity(dds_sub_rdr, CDQOBE_READER);

  dds_sub_subscriber = dds_get_parent(dds_sub_rdr);
  CU_ASSERT_FATAL(dds_sub_subscriber > 0);
  check_default_qos_of_builtin_entity(dds_sub_subscriber, CDQOBE_SUBSCRIBER);
}

/// @brief This test will check that taking samples from the subscription builtin topic will return something.
/// @methodology
/// - Create a participant and a reader on the subscription builtin topic.
/// - Take a sample from the reader, and then take another sample.
/// - Check that both takes were completed succesfully.
/// - Check that returning both samples as loans completes succesfully.
CU_Test(ddsc_builtin_topics, read_nothing)
{
  dds_entity_t pp;
  dds_entity_t rd;
  dds_return_t ret;
  dds_sample_info_t si;
  void *raw1, *raw2;
  int32_t n1, n2;

  pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  rd = dds_create_reader (pp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  /* Can't guarantee there's no other process around with a publication, but
     we can take until nothing remains.  The point is checking handling of
     freeing memory when a loan was outstanding, memory had to be allocated,
     and subsequently had to be freed because of an absence of data. */
  raw1 = raw2 = NULL;
  n1 = dds_take (rd, &raw1, &si, 1, 1);
  CU_ASSERT_FATAL (n1 >= 0);
  n2 = dds_take (rd, &raw2, &si, 1, 1);
  CU_ASSERT_FATAL (n2 >= 0);
  ret = dds_return_loan (rd, &raw1, n1);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_return_loan (rd, &raw2, n2);
  CU_ASSERT_FATAL (ret == 0);

  ret = dds_delete (pp);
  CU_ASSERT_FATAL (ret == 0);
}

static bool querycond_true (const void *sample)
{
  (void) sample;
  return true;
}

/// @brief This test will check that the dds_get_topic function returns the correct topic for readers and conditions created on builtin topics.
/// @methodology
/// - Create a participant.
/// - Create a reader for each of the builtin topics.
/// - Create a readcondition for the reader.
/// - Create a querycondition for the reader.
/// - Check that the topic through the dds_get_topic function is the same as the builtin topic.
CU_Test(ddsc_builtin_topics, get_topic)
{
  static const dds_entity_t tps[] = {
    DDS_BUILTIN_TOPIC_DCPSPARTICIPANT,
    DDS_BUILTIN_TOPIC_DCPSTOPIC,
    DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
    DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION,
    0
  };
  const dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  for (int i = 0; tps[i]; i++)
  {
    const dds_entity_t rd = dds_create_reader (pp, tps[i], NULL, NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    CU_ASSERT_FATAL (rd > 0);
#else // expect failure for DCPSTopic if topic discovery isn't enabled, but verify the error code
    if (tps[i] != DDS_BUILTIN_TOPIC_DCPSTOPIC)
    {
      CU_ASSERT_FATAL (rd > 0);
    }
    else
    {
      CU_ASSERT_FATAL (rd == DDS_RETCODE_UNSUPPORTED);
      continue;
    }
#endif
    // get_topic must return the pseudo handle
    {
      const dds_entity_t rdtp = dds_get_topic (rd);
      CU_ASSERT_FATAL (rdtp == tps[i]);
    }
    // ... also on a read condition
    {
      const dds_entity_t rdc = dds_create_readcondition (rd, DDS_ANY_STATE);
      CU_ASSERT_FATAL (rdc > 0);
      const dds_entity_t rdctp = dds_get_topic (rdc);
      CU_ASSERT_FATAL (rdctp == tps[i]);
    }
    // ... and on a query condition
    {
      const dds_entity_t qc = dds_create_querycondition (rd, DDS_ANY_STATE, querycond_true);
      CU_ASSERT_FATAL (qc > 0);
      const dds_entity_t qctp = dds_get_topic (qc);
      CU_ASSERT_FATAL (qctp == tps[i]);
    }
  }
  dds_return_t rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
}

/// @brief This test will check that the dds_get_name function returns the correct name for builtin topics.
/// @methodology
/// - Check that the output of dds_get_name returns the correct name for builtin topics.
CU_Test(ddsc_builtin_topics, get_name)
{
  static const struct { dds_entity_t h; const char *n; } tps[] = {
    { DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, "DCPSParticipant" },
    { DDS_BUILTIN_TOPIC_DCPSTOPIC, "DCPSTopic" },
    { DDS_BUILTIN_TOPIC_DCPSPUBLICATION, "DCPSPublication" },
    { DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, "DCPSSubscription" },
    { 0, NULL }
  };
  // pseudo handles always exist and it actually works even in the absence of a domain
  // not sure whether that's a feature or a bug ...
  for (int i = 0; tps[i].h; i++)
  {
    char buf[100];
    dds_return_t rc = dds_get_name (tps[i].h, buf, sizeof (buf));
    CU_ASSERT_FATAL (rc == (dds_return_t) strlen (tps[i].n));
    CU_ASSERT_FATAL (strcmp (buf, tps[i].n) == 0);
  }
}

/// @brief This test will check that the dds_get_type_name function returns the correct type name for builtin topics.
/// @methodology
/// - Check that the output of dds_get_type_name returns the correct name for builtin topics.
CU_Test(ddsc_builtin_topics, get_type_name)
{
  static const struct { dds_entity_t h; const char *n; } tps[] = {
    { DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, "org::eclipse::cyclonedds::builtin::DCPSParticipant" },
    { DDS_BUILTIN_TOPIC_DCPSTOPIC, "org::eclipse::cyclonedds::builtin::DCPSTopic" },
    { DDS_BUILTIN_TOPIC_DCPSPUBLICATION, "org::eclipse::cyclonedds::builtin::DCPSPublication" },
    { DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, "org::eclipse::cyclonedds::builtin::DCPSSubscription" },
    { 0, NULL }
  };
  // pseudo handles always exist and it actually works even in the absence of a domain
  // not sure whether that's a feature or a bug ...
  for (int i = 0; tps[i].h; i++)
  {
    char buf[100];
    dds_return_t rc = dds_get_type_name (tps[i].h, buf, sizeof (buf));
    CU_ASSERT_FATAL (rc == (dds_return_t) strlen (tps[i].n));
    CU_ASSERT_FATAL (strcmp (buf, tps[i].n) == 0);
  }
}

/// @brief This test will check that readers created on the builtin topics only give rise to single child entity on their participant, their subscriber.
/// @methodology
/// - Create a participant.
/// - Create a reader for each builtin topic.
/// - Check that the output of dds_get_children for the participant returns only a single entity.
/// - Check that the entity returned is a subscriber.
CU_Test(ddsc_builtin_topics, get_children)
{
  static const dds_entity_t tps[] = {
    DDS_BUILTIN_TOPIC_DCPSPARTICIPANT,
    DDS_BUILTIN_TOPIC_DCPSTOPIC,
    DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
    DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION,
    0
  };
  const dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  for (int i = 0; tps[i]; i++)
  {
    const dds_entity_t rd = dds_create_reader (pp, tps[i], NULL, NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    CU_ASSERT_FATAL (rd > 0);
#else // expect failure for DCPSTopic if topic discovery isn't enabled, but verify the error code
    if (tps[i] != DDS_BUILTIN_TOPIC_DCPSTOPIC)
    {
      CU_ASSERT_FATAL (rd > 0);
    }
    else
    {
      CU_ASSERT_FATAL (rd == DDS_RETCODE_UNSUPPORTED);
      continue;
    }
#endif
  }
  // get_children on must not return the topics we just created, there should be just one
  // subscriber in the result
  dds_entity_t cs[1];
  const int32_t ncs = dds_get_children (pp, cs, sizeof (cs) / sizeof (cs[0]));
  CU_ASSERT_FATAL (ncs == 1);
  struct dds_entity *ptr = NULL;
  dds_return_t r = dds_entity_pin(cs[0], &ptr);
  CU_ASSERT_EQUAL_FATAL (r, DDS_RETCODE_OK);
  dds_entity_kind_t k = dds_entity_kind(ptr);
  CU_ASSERT_EQUAL_FATAL (k, DDS_KIND_SUBSCRIBER);
  dds_entity_unpin(ptr);
  dds_return_t rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
}

/// @brief This test will check that the real entity handle associated with the builtin topic cannot be used for creating a reader or deleting the entity.
/// @methodology
/// - Create a participant.
/// - Create a reader for each builtin topic.
/// - Get the real builtin topic entity handle from this reader, not the constant placeholder.
/// - Attempt to delete the entity associated with this real handle.
/// - Attempt to create a reader on this real handle, it should fail.
CU_Test(ddsc_builtin_topics, cant_use_real_topic)
{
  static const dds_entity_t tps[] = {
    DDS_BUILTIN_TOPIC_DCPSPARTICIPANT,
    DDS_BUILTIN_TOPIC_DCPSTOPIC,
    DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
    DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION,
    0
  };
  const dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  for (int i = 0; tps[i]; i++)
  {
    const dds_entity_t rd = dds_create_reader (pp, tps[i], NULL, NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    CU_ASSERT_FATAL (rd > 0);
#else // expect failure for DCPSTopic if topic discovery isn't enabled, but verify the error code
    if (tps[i] != DDS_BUILTIN_TOPIC_DCPSTOPIC)
    {
      CU_ASSERT_FATAL (rd > 0);
    }
    else
    {
      CU_ASSERT_FATAL (rd == DDS_RETCODE_UNSUPPORTED);
      continue;
    }
#endif

    // extract real topic handle by digging underneath the API
    // as a very efficient alternative to a lucky guess
    dds_return_t rc;
    dds_reader *rd_ent = NULL;
    rc = dds_reader_lock (rd, &rd_ent);
    CU_ASSERT_FATAL (rc == 0 && rd_ent != NULL);
    assert (rc == 0 && rd_ent != NULL);
    const dds_entity_t real_topic = rd_ent->m_topic->m_entity.m_hdllink.hdl;
    dds_reader_unlock (rd_ent);

    // can't delete
    rc = dds_delete (real_topic);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

    rc = dds_create_reader (pp, real_topic, NULL, NULL);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  }
  dds_return_t rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
}

/// @brief This test will check that the qos of the builtin topics is the same as the default topic qos.
/// @methodology
/// - Retrieve the qos for the builtin topic.
/// - Check that the QoSPolicies are equal to that of the default topic QoS.
CU_Test(ddsc_builtin_topics, get_qos)
{
  static const struct { dds_entity_t h; const char *n; } tps[] = {
    { DDS_BUILTIN_TOPIC_DCPSPARTICIPANT },
    { DDS_BUILTIN_TOPIC_DCPSTOPIC },
    { DDS_BUILTIN_TOPIC_DCPSPUBLICATION },
    { DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION },
    { 0, NULL }
  };
  // pseudo handles always exist and it actually works even in the absence of a domain
  // not sure whether that's a feature or a bug ...
  for (int i = 0; tps[i].h; i++)
  {
    check_default_qos_of_builtin_entity (tps[i].h, CDQOBE_TOPIC);
  }
}
