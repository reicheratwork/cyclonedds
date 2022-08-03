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
#include "dds/ddsc/dds_virtual_interface.h"
#include "dds/ddsrt/retcode.h"
#include "dds/export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* the definition of a block of memory originating
* from a virtual interface
*/
typedef struct dds_loaned_sample {
  ddsi_virtual_interface_pipe_t *sample_origin; /*the local pipe this block originates from*/
  loaned_sample_state_t sample_state; /*the state of the memory block*/
  uint32_t block_size; /*size of the loaned block*/
  void * block_ptr; /*pointer to the loaned block*/
  uint32_t sample_size; /*size of the loaned sample*/
  void * sample_ptr; /*pointer to the loaned sample*/
  virtual_interface_data_type_t data_type; /*the data type of the raw samples read/written (used to determine whether raw samples are of the same local type)*/
  virtual_interface_identifier_t data_origin; /*origin of data (ddsi_sertype*)*/
} dds_loaned_sample_t;

/**
 * @ingroup loan
 * @brief Loan a sample from the writer.
 *
 * @note This function is to be used with dds_write to publish the loaned
 * sample. Will attempt to loan from any virtual interfaces available, but
 * will fall back on heap-based loans
 *
 * @param[in] writer the writer to loan the buffer from
 * @param[out] samples_ptr location to store the pointers to the loaned samples
 * @param[in] n_samples the number of samples to be loaned
 *
 * @returns DDS_RETCODE_OK if successful, DDS_RETCODE_ERROR otherwise
 */
DDS_EXPORT dds_return_t dds_writer_loan_samples(dds_entity_t writer, void **samples_ptr,
                                                uint32_t n_samples);

//move these functions to a private header?
/**
 * @ingroup loan
 * @brief Cleans up a loaned sample.
 *
 * @note Returns the sample to the virtual interface if it originates from one, otherwise it will
 * return the sample to the heap.
 *
 * @param[in] sample The sample to cleanup.
 *
 * @returns true if the sample was returned succesfully, false otherwise.
 */
bool loaned_sample_cleanup(dds_loaned_sample_t *sample);

bool unref_sample(dds_loaned_sample_t *sample);

bool ref_sample(dds_loaned_sample_t *sample);

/**
 * @ingroup loan
 * @brief Creates a loaned sample.
 *
 * @note Attempts to loan a sample from a virtual interface or the heap.
 *
 * @param[in] pipe The pipe to the virtual interface to use, or NULL to use the heap.
 * @param[in] size The size of the sample to loan.
 *
 * @returns pointer to the loaned sample if succesful.
 */
dds_loaned_sample_t * loaned_sample_create(ddsi_virtual_interface_pipe_t *pipe, uint32_t size);

#if defined(__cplusplus)
}
#endif
#endif
