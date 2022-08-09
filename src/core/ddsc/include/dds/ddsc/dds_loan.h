/*
 * Copyright(c) 2021 ZettaScale Technology
 * Copyright(c) 2021 Apex.AI, Inc
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

// API extension
// defines functions needed for loaning and shared memory usage

#ifndef _DDS_LOAN_API_H_
#define _DDS_LOAN_API_H_

#include "dds/ddsc/dds_basic_types.h"
#include "dds/ddsrt/retcode.h"
#include "dds/export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*state of the data contained in a memory block*/
typedef enum loaned_sample_state {
  LOANED_SAMPLE_STATE_UNITIALIZED,
  LOANED_SAMPLE_STATE_RAW,
  LOANED_SAMPLE_STATE_SERIALIZED_KEY,
  LOANED_SAMPLE_STATE_SERIALIZED_DATA
} loaned_sample_state_t;

/*identifier used to distinguish between raw data types (C/C++/Python/...)*/
typedef uint32_t loan_data_type_t;

/*identifier used to distinguish between types of loans (heap/iceoryx/...)*/
typedef uint32_t loan_origin_type_t;

/*forward declarations of struct, so pointer can be made*/
typedef struct dds_loan_manager dds_loan_manager_t;
typedef struct dds_loaned_sample dds_loaned_sample_t;
typedef struct ddsi_virtual_interface_pipe ddsi_virtual_interface_pipe_t;

/*implementation specific loaned sample cleanup function*/
typedef bool (*dds_loaned_sample_fini_f)(
  dds_loaned_sample_t *to_fini);

/*implementation specific loaned sample reference increment function*/
typedef bool (*dds_loaned_sample_incr_refs_f)(
  dds_loaned_sample_t *to_incr);

/*implementation specific loaned sample reference decrement function*/
typedef bool (*dds_loaned_sample_decr_refs_f)(
  dds_loaned_sample_t *to_decr);

/*implementation specific loaned sample zero reference callback function*/
typedef bool (*dds_loaned_sample_on_no_refs_f)(
  dds_loaned_sample_t *to_on_no_refs);

/*container for implementation specific operations*/
typedef struct dds_loaned_sample_ops {
  dds_loaned_sample_fini_f        fini;
  dds_loaned_sample_incr_refs_f   incr;
  dds_loaned_sample_decr_refs_f   decr;
  dds_loaned_sample_on_no_refs_f  on_no_refs;
} dds_loaned_sample_ops_t;

/* the definition of a block of memory originating
* from a virtual interface
*/
typedef struct dds_loaned_sample {
  dds_loaned_sample_ops_t ops; /*the implementation specific ops for this sample*/
  ddsi_virtual_interface_pipe_t *loan_origin; /*the origin of the loan*/
  dds_loan_manager_t *manager; /*the origin of the loan*/
  uint32_t block_size; /*size of the loaned block*/
  void * block_ptr; /*pointer to the loaned block*/
  uint32_t sample_size; /*size of the loaned sample*/
  void * sample_ptr; /*pointer to the loaned sample*/
  loaned_sample_state_t sample_state; /*the state of the memory block*/
  loan_data_type_t data_type; /*the data type of the raw samples read/written (used to determine whether raw samples are of the same local type)*/
  loan_origin_type_t data_origin; /*origin of data (ddsi_sertype*)*/
  uint32_t refs; /*number of references held to this sample (make atomic?)*/
  uint32_t loan_idx; /*the storage index of the loan*/
  bool can_be_dereffed; /*whether the loan is still safe to be accessed*/
} dds_loaned_sample_t;

/* generic loaned sample cleanup function will be called
   when the loaned sample runs out of refs or is retracted,
   calls the implementation specific functions */
bool dds_loaned_sample_fini(
  dds_loaned_sample_t *to_fini);

/* generic function which increases the references for this sample,
   calls the implementation specific functions*/
bool dds_loaned_sample_incr_refs(
  dds_loaned_sample_t *to_incr);

/* generic function which decreases the references for this sample,
   calls the implementation specific functions*/
bool dds_loaned_sample_decr_refs(
  dds_loaned_sample_t *to_decr);

/*an implementation specific loan manager*/
typedef struct dds_loan_manager {
  dds_loaned_sample_t **samples;
  uint32_t n_samples_cap;
  //mutex?
} dds_loan_manager_t;

/*loan manager create function*/
dds_loan_manager_t *dds_loan_manager_create(
  uint32_t initial_cap);

/** loan manager fini function ensures that the containers are
  * cleaned up and all loans are returned*/
bool dds_loan_manager_fini(
  dds_loan_manager_t *to_fini);

/** add a loan to be stored by this manager */
bool dds_loan_manager_add_loan(
  dds_loan_manager_t *manager,
  dds_loaned_sample_t *to_add);

/** removes a loan from storage by this manager */
bool dds_loan_manager_remove_loan(
  dds_loan_manager_t *manager,
  dds_loaned_sample_t *to_remove);

/** finds a whether a sample corresponds to a loan on this manager */
dds_loaned_sample_t *dds_loan_manager_find_loan(
  const dds_loan_manager_t *manager,
  const void *sample);

#if defined(__cplusplus)
}
#endif
#endif
