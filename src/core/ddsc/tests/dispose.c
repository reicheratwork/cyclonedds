// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"

#include "test_common.h"

/**************************************************************************************************
 *
 * Test fixtures
 *
 *************************************************************************************************/
#define MAX_SAMPLES                 7
#define INITIAL_SAMPLES             2

static dds_entity_t g_participant = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_waitset     = 0;

static dds_time_t   g_past        = 0;
static dds_time_t   g_present     = 0;


static void*             g_samples[MAX_SAMPLES];
static Space_Type1       g_data[MAX_SAMPLES];
static dds_sample_info_t g_info[MAX_SAMPLES];

static void
disposing_init(void)
{
    Space_Type1 sample = { 0, 0, 0 };
    dds_qos_t *qos = dds_create_qos ();
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    /* Use by source timestamp to be able to check the time related funtions. */
    dds_qset_destination_order(qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_waitset = dds_create_waitset(g_participant);
    CU_ASSERT_FATAL(g_waitset > 0);

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_disposing_test", name, sizeof name), qos, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    /* Create a reader that keeps one sample on three instances. */
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS(100));
    dds_qset_resource_limits(qos, DDS_LENGTH_UNLIMITED, 3, 1);
    g_reader = dds_create_reader(g_participant, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);

    /* Create a writer that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_participant, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_writer > 0 );

    /* Sync g_writer to g_reader. */
    ret = dds_set_status_mask(g_writer, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_writer, g_writer);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(g_writer, (dds_entity_t)(intptr_t)triggered);
    ret = dds_waitset_detach(g_waitset, g_writer);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Sync g_reader to g_writer. */
    ret = dds_set_status_mask(g_reader, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(g_reader, (dds_entity_t)(intptr_t)triggered);
    ret = dds_waitset_detach(g_waitset, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write initial samples. */
    for (int i = 0; i < INITIAL_SAMPLES; i++) {
        sample.long_1 = i;
        sample.long_2 = i*2;
        sample.long_3 = i*3;
        ret = dds_write(g_writer, &sample);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    }

    /* Initialize reading buffers. */
    memset (g_data, 0, sizeof (g_data));
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_samples[i] = &g_data[i];
    }

    /* Initialize times. */
    g_present = dds_time();
    g_past    = g_present - DDS_SECS(1);

    dds_delete_qos(qos);
}

static void
disposing_fini(void)
{
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
}


#if 0
#else
/**************************************************************************************************
 *
 * These will check the dds_writedispose() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose on a deleted writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Delete the writer.
/// - Attempt to do writedispose on the deleted writer.
/// - Expectation: the writedispose action will fail with a bad parameter error, as the writer no longer exists.
CU_Test(ddsc_writedispose, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(g_writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose on a sample pointer NULL will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do writedispose on the writer with a sample pointer NULL.
/// - Expectation: the writedispose action will fail with a bad parameter error, as there is no sample provided.
CU_Test(ddsc_writedispose, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(g_writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_writedispose, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
/// @brief This test checks whether calling dds_writedispose on an entity handle which does not point to an entity will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do writedispose on a handle which does not point to an entity.
/// - Expectation: the writedispose action will fail with a bad parameter error, as the handle is invalid.
CU_Theory((dds_entity_t writer), ddsc_writedispose, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;

    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_writedispose, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
/// @brief This test checks whether calling dds_writedispose on an entity handle which does not point to a writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do writedispose on the handle of the waitset, reader, topic and participant.
/// - Expectation: the writedispose action will fail with an illegal operation error, as the handle does not point to a writer.
CU_Theory((dds_entity_t *writer), ddsc_writedispose, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(*writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose on an existing instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Writedispose an instance with the key field the same as the first sample written, and the other fields being 22.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: the first sample has all non-key fields set to 22, due to the writedispose operation.
/// - Expectation: the first sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
/// - Expectation: the second sample has the same fields as those written.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_writedispose, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_writedispose(g_writer, &oldInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all samples that matches filter. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 22);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 22);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose on a new instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Writedispose an instance with a key field which was not written before, and the other fields being 42.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 3 samples read.
/// - Expectation: all samples have the same values as those written.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
/// - Expectation: the third sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
CU_Test(ddsc_writedispose, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose(g_writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all samples that matches filter. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 3 );
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 42);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 42);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose on more instances than allowed will cause a timeout.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Writedispose an instance with a key field which was not written before, and the other fields being 22.
/// - Expectation: this is succesful, as there are now 3 instances, within the resource_limits QoSPolicy.
/// - Writedispose an instance with a key field which was not written before, and the other fields being 42.
/// - Expectation: this fails with a timeout error, as it attempts to manage 4 instances, exceeding the resource_limits QoSPolicy.
CU_Test(ddsc_writedispose, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose(g_writer, &newInstance1);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_writedispose(g_writer, &newInstance2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_TIMEOUT);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_writedispose_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose_ts on a deleted writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Delete the writer.
/// - Attempt to do writedispose on the deleted writer with the timestamp of writing.
/// - Expectation: the writedispose action will fail with a bad parameter error, as the writer no longer exists.
CU_Test(ddsc_writedispose_ts, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(g_writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose_ts on a sample pointer NULL will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do writedispose on the writer with a sample pointer NULL with the timestamp of writing.
/// - Expectation: the writedispose action will fail with a bad parameter error, as there is no sample provided.
CU_Test(ddsc_writedispose_ts, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(g_writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose_ts on more instances than allowed will cause a timeout.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Writedispose an instance with a key field which was not written before, and the other fields being 22 with the timestamp of writing.
/// - Expectation: this is succesful, as there are now 3 instances, within the resource_limits QoSPolicy.
/// - Writedispose an instance with a key field which was not written before, and the other fields being 42.
/// - Expectation: this fails with a timeout error, as it attempts to manage 4 instances, exceeding the resource_limits QoSPolicy.
CU_Test(ddsc_writedispose_ts, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose_ts(g_writer, &newInstance1, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_writedispose_ts(g_writer, &newInstance2, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_TIMEOUT);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_writedispose_ts, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
/// @brief This test checks whether calling dds_writedispose_ts on an entity handle which does not point to an entity will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do writedispose on a handle which does not point to an entity with the timestamp of writing.
/// - Expectation: the writedispose action will fail with a bad parameter error, as the handle is invalid.
CU_Theory((dds_entity_t writer), ddsc_writedispose_ts, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;

    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_writedispose_ts, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
/// @brief This test checks whether calling dds_writedispose_ts on an entity handle which does not point to a writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do writedispose on the handle of the waitset, reader, topic and participant with the timestamp of writing.
/// - Expectation: the writedispose action will fail with an illegal operation error, as the handle does not point to a writer.
CU_Theory((dds_entity_t *writer), ddsc_writedispose_ts, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(*writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose_ts on an existing instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Writedispose an instance with the key field the same as the first sample written, and the other fields being 22 with the timestamp of writing.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: the first sample has all non-key fields set to 22, due to the writedispose operation.
/// - Expectation: the first sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
/// - Expectation: the second sample has the same fields as those written.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_writedispose_ts, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_writedispose_ts(g_writer, &oldInstance, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 22);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 22);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose_ts on a new instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Writedispose an instance with a key field which was not written before, and the other fields being 42 with the timestamp of writing.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 3 samples read.
/// - Expectation: all samples have the same values as those written.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
/// - Expectation: the third sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
CU_Test(ddsc_writedispose_ts, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose_ts(g_writer, &newInstance, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 42);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 42);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_writedispose_ts on an existing instance with an older timestamp works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Create a waitset for the sample_lost status on the reader.
/// - Writedispose the first instance with a timestamp 1 second before the original writing time.
/// - Expectation: this is succesful.
/// - Wait on the waitset.
/// - Expectation: this returns 1, indicating a single sample was lost.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same values as those written and have valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_writedispose_ts, disposing_past_sample, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_return_t ret;

    /* Disposing a sample in the past should trigger a lost sample. */
    ret = dds_set_status_mask(g_reader, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now, dispose a sample in the past. */
    ret = dds_writedispose_ts(g_writer, &oldInstance, g_past);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, NULL, 0, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }

}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_dispose() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose on a deleted writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Delete the writer.
/// - Attempt to do dispose on the deleted writer.
/// - Expectation: the dispose action will fail with a bad parameter error, as the writer no longer exists.
CU_Test(ddsc_dispose, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(g_writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose on a sample pointer NULL will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on the writer with a sample pointer NULL.
/// - Expectation: the writedispose action will fail with a bad parameter error, as there is no sample provided.
CU_Test(ddsc_dispose, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(g_writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose on more instances than allowed will cause a timeout.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Dispose an instance with a key field which was not written before, and the other fields being 22.
/// - Expectation: this is succesful, as there are now 3 instances, within the resource_limits QoSPolicy.
/// - Dispose an instance with a key field which was not written before, and the other fields being 42.
/// - Expectation: this fails with a timeout error, as it attempts to manage 4 instances, exceeding the resource_limits QoSPolicy.
CU_Test(ddsc_dispose, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose(g_writer, &newInstance1);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_dispose(g_writer, &newInstance2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_TIMEOUT);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
/// @brief This test checks whether calling dds_dispose on an entity handle which does not point to an entity will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on a handle which does not point to an entity.
/// - Expectation: the dispose action will fail with a bad parameter error, as the handle is invalid.
CU_Theory((dds_entity_t writer), ddsc_dispose, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;

    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(writer, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
/// @brief This test checks whether calling dds_dispose on an entity handle which does not point to a writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on the handle of the waitset, reader, topic and participant.
/// - Expectation: the dispose action will fail with an illegal operation error, as the handle does not point to a writer.
CU_Theory((dds_entity_t *writer), ddsc_dispose, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    /* pass a non-null pointer that'll trigger a crash if it is read */
    ret = dds_dispose(*writer, (void *) 1);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose on an existing instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Dispose an instance with the key field the same as the first sample written, and the other fields being 22.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same fields as those written, since the dispose operation does not modify contents.
/// - Expectation: the first sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_dispose, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_dispose(g_writer, &oldInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 0);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 0);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose on a new instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Dispose an instance with a key field which was not written before, and the other fields being 42.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 3 samples read.
/// - Expectation: the first 2 samples have the same values as those written.
/// - Expectation: the third sample does not contain valid data.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
/// - Expectation: the third sample has valid_data false, sample state not_read, view_state new and instance_state not_alive_disposed.
CU_Test(ddsc_dispose, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose(g_writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Don't check data; it's not valid. */

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     false);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_dispose_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ts on a deleted writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Delete the writer.
/// - Attempt to do dispose on the deleted writer with the timestamp of writing.
/// - Expectation: the dispose action will fail with a bad parameter error, as the writer no longer exists.
CU_Test(ddsc_dispose_ts, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(g_writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387); /* Disable SAL warning on intentional misuse of the API */
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ts on a sample pointer NULL will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on the writer with a sample pointer NULL with the timestamp of writing.
/// - Expectation: the writedispose action will fail with a bad parameter error, as there is no sample provided.
CU_Test(ddsc_dispose_ts, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(g_writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ts on more instances than allowed will cause a timeout.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Dispose an instance with a key field which was not written before, and the other fields being 22 with the timestamp of writing.
/// - Expectation: this is succesful, as there are now 3 instances, within the resource_limits QoSPolicy.
/// - Dispose an instance with a key field which was not written before, and the other fields being 42.
/// - Expectation: this fails with a timeout error, as it attempts to manage 4 instances, exceeding the resource_limits QoSPolicy.
CU_Test(ddsc_dispose_ts, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose_ts(g_writer, &newInstance1, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_dispose_ts(g_writer, &newInstance2, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_TIMEOUT);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ts, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
/// @brief This test checks whether calling dds_dispose_ts on an entity handle which does not point to an entity will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on a handle which does not point to an entity with the timestamp of writing.
/// - Expectation: the dispose action will fail with a bad parameter error, as the handle is invalid.
CU_Theory((dds_entity_t writer), ddsc_dispose_ts, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;

    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(writer, NULL, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ts, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
/// @brief This test checks whether calling dds_dispose_ts on an entity handle which does not point to a writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on the handle of the waitset, reader, topic and participant with the timestamp of writing.
/// - Expectation: the dispose action will fail with an illegal operation error, as the handle does not point to a writer.
CU_Theory((dds_entity_t *writer), ddsc_dispose_ts, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    /* pass a non-null pointer that'll trigger a crash if it is read */
    ret = dds_dispose_ts(*writer, (void *) 1, g_present);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ts on an existing instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Dispose an instance with the key field the same as the first sample written, and the other fields being 22 with the timestamp of writing.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same fields as those written, as the dispose operation does not modify sample contents.
/// - Expectation: the first sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_dispose_ts, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_dispose_ts(g_writer, &oldInstance, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of dispose is not used, only the key part). */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ts on a new instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Dispose an instance with a key field which was not written before, and the other fields being 42, with the timestamp of writing.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 3 samples read.
/// - Expectation: the first 2 samples have the same values as those written.
/// - Expectation: the third sample does not contain valid data.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
/// - Expectation: the third sample has valid_data false, sample state not_read, view_state new and instance_state not_alive_disposed.
CU_Test(ddsc_dispose_ts, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose_ts(g_writer, &newInstance, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Don't check data; it's not valid. */

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     false);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ts on an existing instance with an older timestamp works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Create a waitset for the sample_lost status on the reader.
/// - Dispose the first instance with a timestamp 1 second before the original writing time.
/// - Expectation: this is succesful.
/// - Wait on the waitset.
/// - Expectation: this returns 1, indicating a single sample was lost.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same values as those written and have valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_dispose_ts, disposing_past_sample, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_return_t ret;

    /* Disposing a sample in the past should trigger a lost sample. */
    ret = dds_set_status_mask(g_reader, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now, dispose a sample in the past. */
    ret = dds_dispose_ts(g_writer, &oldInstance, g_past);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, NULL, 0, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if ((sample->long_1 == 0) || (sample->long_1 == 1)) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }

}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_dispose_ih() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ih on a deleted writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Delete the writer.
/// - Attempt to do dispose on the deleted writer.
/// - Expectation: the dispose action will fail with a bad parameter error, as the writer no longer exists.
CU_Test(ddsc_dispose_ih, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_dispose_ih(g_writer, DDS_HANDLE_NIL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ih, invalid_handles) = {
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
/// @brief This test checks whether calling dds_dispose_ih on a handle which does not point to an instance will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dds_dispose_ih on an instance handle which does not point to an entity.
/// - Expectation: the dispose action will fail with a precondition not met error, as the handle does not point an instance.
CU_Theory((dds_instance_handle_t handle), ddsc_dispose_ih, invalid_handles, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih(g_writer, handle);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ih, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
/// @brief This test checks whether calling dds_dispose_ih on an entity handle which does not point to an entity will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on a handle which does not point to an entity.
/// - Expectation: the dispose action will fail with a bad parameter error, as the handle is invalid.
CU_Theory((dds_entity_t writer), ddsc_dispose_ih, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;

    ret = dds_dispose_ih(writer, DDS_HANDLE_NIL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ih, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
/// @brief This test checks whether calling dds_dispose_ih on an entity handle which does not point to a writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on the handle of the waitset, reader, topic and participant.
/// - Expectation: the dispose action will fail with an illegal operation error, as the handle does not point to a writer.
CU_Theory((dds_entity_t *writer), ddsc_dispose_ih, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih(*writer, DDS_HANDLE_NIL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ih on an existing instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Get the instance handle of an instance with the key field the same as the first instance written and the other fields being 22.
/// - Dispose this instance handle.
/// - Expectation: the dispose is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same fields as those written, as the dispose operation does not modify sample contents.
/// - Expectation: the first sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_dispose_ih, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_lookup_instance(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_dispose_ih(g_writer, hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 0);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 0);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_dispose_ih_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ih_ts on a deleted writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Delete the writer.
/// - Attempt to do dispose on the deleted writer with the timestamp of writing.
/// - Expectation: the dispose action will fail with a bad parameter error, as the writer no longer exists.
CU_Test(ddsc_dispose_ih_ts, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_dispose_ih_ts(g_writer, DDS_HANDLE_NIL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ih_ts, invalid_handles) = {
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
/// @brief This test checks whether calling dds_dispose_ih_ts on a handle which does not point to an instance will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dds_dispose_ih_ts on an instance handle which does not point to an entity with the time of writing.
/// - Expectation: the dispose action will fail with a precondition not met error, as the handle does not point an instance.
CU_Theory((dds_instance_handle_t handle), ddsc_dispose_ih_ts, invalid_handles, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih_ts(g_writer, handle, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ih_ts, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
/// @brief This test checks whether calling dds_dispose_ih_ts on an entity handle which does not point to an entity will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on a handle which does not point to an entity with the time of writing.
/// - Expectation: the dispose action will fail with a bad parameter error, as the handle is invalid.
CU_Theory((dds_entity_t writer), ddsc_dispose_ih_ts, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;

    ret = dds_dispose_ih_ts(writer, DDS_HANDLE_NIL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_dispose_ih_ts, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
/// @brief This test checks whether calling dds_dispose_ih_ts on an entity handle which does not point to a writer will fail.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Attempt to do dispose on the handle of the waitset, reader, topic and participant with the time of writing.
/// - Expectation: the dispose action will fail with an illegal operation error, as the handle does not point to a writer.
CU_Theory((dds_entity_t *writer), ddsc_dispose_ih_ts, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih_ts(*writer, DDS_HANDLE_NIL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ih_ts on an existing instance works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Get the instance handle of an instance with the key field the same as the first instance written and the other fields being 22.
/// - Dispose this handle.
/// - Expectation: this is succesful.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same fields as those written, as the dispose operation does not modify sample contents.
/// - Expectation: the first sample has valid_data true, sample state not_read, view_state new and instance_state not_alive_disposed.
/// - Expectation: the second sample has valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_dispose_ih_ts, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_lookup_instance(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_dispose_ih_ts(g_writer, hdl, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of dispose is not used, only the key part). */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
/// @brief This test checks whether calling dds_dispose_ih_ts on an existing instance with an older timestamp works as intended.
/// @methodology
/// - Create a participant, topic, reader and writer with the destination_order QoSPolicy set to by_source_timestamp, resource_limits QoSPolicy max_instances to 3 and max_samples_per_instance to 1 and writer_data_lifecycle QoSPolicy autodispose to false.
/// - Wait for the readers and writers to see eachother.
/// - Write 2 samples.
/// - Get the instance handle of an instance same contents as the first sample written.
/// - Create a waitset for the sample_lost status on the reader.
/// - Dispose the instance handle with a timestamp 1 second before the original writing time.
/// - Expectation: this is succesful.
/// - Wait on the waitset.
/// - Expectation: this returns 1, indicating a single sample was lost.
/// - Read all samples from the reader.
/// - Expectation: there are 2 samples read.
/// - Expectation: all samples have the same values as those written and have valid_data true, sample state not_read, view_state new and instance_state alive.
CU_Test(ddsc_dispose_ih_ts, disposing_past_sample, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_instance_handle_t hdl = dds_lookup_instance(g_writer, &oldInstance);
    dds_return_t ret;

    /* Disposing a sample in the past should trigger a lost sample. */
    ret = dds_set_status_mask(g_reader, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now, dispose a sample in the past. */
    ret = dds_dispose_ih_ts(g_writer, hdl, g_past);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, NULL, 0, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if ((sample->long_1 == 0) || (sample->long_1 == 1)) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }

}
/*************************************************************************************************/


#endif
