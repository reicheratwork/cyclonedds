// Copyright(c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include "test_common.h"
#include "test_oneliner.h"

// Use no_shm variant because the use of shared memory may result in asynchronous delivery
// of data published by a local reader/writer and these tests are written on the assumption
// that it is always synchronous
#define dotest(ops) CU_ASSERT_FATAL (test_oneliner_no_shm (ops) > 0)

/// @brief This test checks whether the destination order by_source_timestamp is correctly adhered to. As by_source_timestamp results in the following ordering hierarchy of samples: historycache > timestamp > GUID.
/// @methodology
/// - Create 2 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 1 QoS.
/// - Write a sample on the first writer with a given timestamp.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only get the sample written by the first writer, as the second writer has a higher GUID and cannot overwrite the first writer.
/// - Clean up the DDS space.
/// - Create 2 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 1 QoS.
/// - Write a sample on the second writer with a given timestamp.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only get the sample written by the first writer, as the second writer has a higher GUID and is therefore overwritten by the first.
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 1 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only get the sample written by the first writer, as the second and third writer have a higher GUID and cannot overwrite the first writer.
CU_Test (ddsc_destorder, by_source)
{
  // Assumes GUIDs are allocated in increasing order in a participant.

  // Now that we're at it: also do some simple sanity checks that never caused a problem
  dotest ("w(do=s) x(do=s) r(do=s,h=1)"
          "  wr w (1,1,0)@1"    // initializes instance: GUID w
          "  wr x (1,2,0)@1"    // may not update instance: GUID x > GUID w
          "  take{(1,1,0)} r"); // expect to read sample written by w
  dotest ("w(do=s) x(do=s) r(do=s,h=1)"
          "  wr x (1,0,0)@1"
          "  wr w (1,1,0)@1"
          "  wr x (1,2,0)@1"
          "  take{(1,1,0)} r");
  dotest ("w(do=s) x(do=s) y(do=s) r(do=s,h=1)"
          "  wr y (1,0,0)@1"    // initializes instance: GUID y
          "  wr w (1,1,0)@1"    // updates instance: GUID w < GUID y
          "  wr x (1,2,0)@1"    // may not update instance: GUID x > GUID w
          "  take{(1,1,0)} r");
}

/// @brief This test checks whether the destination order by_reception_timestamp is correctly adhered to.
/// @methodology
/// - Create 2 writers with destination order by_reception_timestamp QoS.
/// - Create reader with destination order by_reception_timestamp and history keep_last 1 QoS.
/// - Write a sample on the first writer with a given timestamp.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only get the sample written by the second writer, as it was the last received.
/// - Clean up the DDS space.
/// - Create 2 writers with destination order by_reception_timestamp QoS.
/// - Create reader with destination order by_reception_timestamp and history keep_last 1 QoS.
/// - Write a sample on the second writer with a given timestamp.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only get the last sample written by the second writer, as it was the last received.
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_reception_timestamp QoS.
/// - Create reader with destination order by_reception_timestamp and history keep_last 1 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only get the sample written by the second writer, as it was the last received.
CU_Test (ddsc_destorder, by_reception)
{
  // While we're at it: also do some simple sanity checks that never caused a problem
  dotest ("w(do=r) x(do=r) r(do=r,h=1)"
          "  wr w (1,1,0)@1"
          "  wr x (1,2,0)@1"
          "  take{(1,2,0)} r");
  dotest ("w(do=r) x(do=r) r(do=r,h=1)"
          "  wr x (1,0,0)@1"
          "  wr w (1,1,0)@1"
          "  wr x (1,2,0)@1"
          "  take{(1,2,0)} r");
  dotest ("w(do=r) x(do=r) y(do=r) r(do=r,h=1)"
          "  wr y (1,0,0)@1"
          "  wr w (1,1,0)@1"
          "  wr x (1,2,0)@1"
          "  take{(1,2,0)} r");
}

/// @brief This test checks whether the destination order by_source_timestamp is correctly causes the samples to be stored in the correct order.
/// @methodology
/// - Create 3 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 3 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only expect to see the ones from the third and first writers, due to the sample from the second writer not being able to "update" the instance as it is attempting to update an instance with the same timestamp, but lower GUID (the first writer).
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 3 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: to see all samples, since the updating is each time being done by a lower GUID.
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 3 QoS.
/// - Write a sample on the first writer with a given timestamp.
/// - Write a sample on the second writer with a later timestamp and the only different content is non-key members.
/// - Write a sample on the third writer with an even later timestamp and the only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: to see all samples, since the updating is each time being done by with a later timestamp.
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_source_timestamp QoS.
/// - Create reader with destination order by_source_timestamp and history keep_last 3 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the second writer with an earlier timestamp and the only different content is non-key members.
/// - Write a sample on the first writer with an even earlier timestamp and the only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: only see the sample from the third writer, since the updating is each time being done by with an earlier timestamp.
CU_Test (ddsc_destorder, by_source_history)
{
  // Deeper history: it accepts/rejects samples based on how it compares with the current
  // state, it doesn't rewrite history.  This is a point of debate with regards to the
  // spec: what precisely is covered by eventual consistency?
  //
  // Cyclone DDS treats the reader as something that in a sense "samples" the data space,
  // OpenSplice tries to make the history eventually consistent, but then has to break
  // that anyway because of some other requirement on "take".  The specs are completely
  // oblivious to the possibility of non-monotonically increasing timestamps and the
  // cosequences thereof and the DCPS spec is essentially silent on the topic anyway.  The
  // DDSI says it concerns the full history, but then only considers a single writer and
  // so almost by definition only makes some subtly wrong suggestions about the system
  // behaviour.
  //
  // In my humble view of things, the only sane approach is to consider DDS as a data
  // space that writers update and where the writer history cache settings affect which
  // updates can be counted upon to be visible (for however briefly a time), and where the
  // readers sample the current state and update their own local history.
  //
  // Not-quite-coincidentally, that fits with what Cyclone does.  (Well, I tried ...)
  dotest ("w(do=s) x(do=s) y(do=s) r(do=s,h=3)"
          "  wr y (1,0,0)@1"
          "  wr w (1,1,0)@1"
          "  wr x (1,2,0)@1"
          "  take{(1,0,0),(1,1,0)} r");

  // Same with GUIDs monotonically decreasing (so in increasing priority): we expect them
  // all
  dotest ("w(do=s) x(do=s) y(do=s) r(do=s,h=3)"
          "  wr y (1,0,0)@1"
          "  wr x (1,1,0)@1"
          "  wr w (1,2,0)@1"
          "  take{(1,0,0),(1,1,0),(1,2,0)} r");

  // Monotonically increasing GUIDs (decreasing priority), but increasing time stamps:
  // expect them all
  dotest ("w(do=s) x(do=s) y(do=s) r(do=s,h=3)"
          "  wr w (1,0,0)@1"
          "  wr x (1,1,0)@1.1"
          "  wr y (1,2,0)@1.2"
          "  take{(1,0,0),(1,1,0),(1,2,0)} r");

  // GUIDs monotonically decreasing (so in increasing priority), timestamps monotonically
  // decreasing: only the first
  dotest ("w(do=s) x(do=s) y(do=s) r(do=s,h=3)"
          "  wr y (1,0,0)@1.2"
          "  wr x (1,1,0)@1.1"
          "  wr w (1,2,0)@1.0"
          "  take{(1,0,0)} r");
}

/// @brief This test checks whether the destination order by_reception_timestamp causes samples to be stored in the correct order.
/// @methodology
/// - Create 3 writers with destination order by_reception_timestamp QoS.
/// - Create reader with destination order by_reception_timestamp and history keep_last 3 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the second writer with a later timestamp with only different content is non-key members.
/// - Write a sample on the first writer with an even later timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: you expect to see samples in the same order they were written, since the history capacity is there and only the reception order matters.
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_reception_timestamp QoS.
/// - Create reader with destination order by_reception_timestamp and history keep_last 3 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the first writer with the same timestamp with only different content is non-key members.
/// - Write a sample on the second writer with the same timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: you expect to see samples in the same order they were written, since the history capacity is there and only the reception order matters.
/// - Clean up the DDS space.
/// - Create 3 writers with destination order by_reception_timestamp QoS.
/// - Create reader with destination order by_reception_timestamp and history keep_last 3 QoS.
/// - Write a sample on the third writer with a given timestamp.
/// - Write a sample on the first writer with an earlier timestamp with only different content is non-key members.
/// - Write a sample on the second writer with an even earlier timestamp with only different content is non-key members.
/// - Take the samples from the reader.
/// - Expectation: you expect to see samples in the same order they were written, since the history capacity is there and only the reception order matters.
CU_Test (ddsc_destorder, by_reception_history)
{
  // timestamps don't matter
  dotest ("w(do=r) x(do=r) y(do=r) r(do=r,h=3)"
          "  wr y (1,0,0)@1"
          "  wr w (1,1,0)@1.1"
          "  wr x (1,2,0)@1.2"
          "  take{(1,0,0),(1,1,0),(1,2,0)} r");
  dotest ("w(do=r) x(do=r) y(do=r) r(do=r,h=3)"
          "  wr y (1,0,0)@1"
          "  wr w (1,1,0)@1"
          "  wr x (1,2,0)@1"
          "  take{(1,0,0),(1,1,0),(1,2,0)} r");
  dotest ("w(do=r) x(do=r) y(do=r) r(do=r,h=3)"
          "  wr y (1,0,0)@1.2"
          "  wr w (1,1,0)@1.1"
          "  wr x (1,2,0)@1"
          "  take{(1,0,0),(1,1,0),(1,2,0)} r");
}
