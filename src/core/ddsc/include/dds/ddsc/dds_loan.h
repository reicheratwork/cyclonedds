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

typedef struct dds_writer dds_writer;

/*state of the data contained in a memory block*/
typedef enum loaned_sample_state {
  LOANED_SAMPLE_STATE_UNITIALIZED,
  LOANED_SAMPLE_STATE_RAW,
  LOANED_SAMPLE_STATE_SERIALIZED
} loaned_sample_state_t;

/* the definition of a block of memory originating
* from a virtual interface
*/
typedef struct dds_loaned_sample {
  ddsi_virtual_interface_pipe_t *sample_origin; /*the local pipe this block originates from*/
  loaned_sample_state_t sample_state; /*the state of the memory block*/
  size_t sample_size; /*size of the block*/
  void * sample_ptr; /*pointer to the block*/
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

/**
 * @ingroup loan
 * @brief Returns a sample loaned from a writer.
 *
 * @note This function is to be used with dds_write to publish the loaned
 * sample.
 * @note Will attempt to loan from any virtual interfaces available, but will fall
 * on heap-based loans
 *
 * @param[in] writer the writer to loan the buffer from
 * @param[out] samples_ptr pointer to the loaned samples
 * @param[in] n_samples the number of samples to be returned
 *
 * @returns DDS_RETCODE_OK if successful, DDS_RETCODE_ERROR otherwise
 */
DDS_EXPORT dds_return_t dds_return_writer_loan(dds_writer *writer, void **samples_ptr,
                                               int32_t bufsz);

/**
 * @ingroup loan
 * @brief Check whether a sample is loaned from a writer.
 *
 * @note This function is to be used with dds_write to publish the loaned
 * sample.
 *
 * @param[in] writer pointer to the writer to check for loan
 * @param[out] sample_ptr pointer to the sample to check
 *
 * @returns pointer to loaned block found, NULL otherwise
 */
DDS_EXPORT dds_loaned_sample_t * dds_writer_check_for_loan(dds_writer *writer, const void *sample_ptr);

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
DDS_EXPORT bool loaned_sample_cleanup(dds_loaned_sample_t *sample);

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
DDS_EXPORT dds_loaned_sample_t * loaned_sample_create(ddsi_virtual_interface_pipe_t *pipe, size_t size);

#if defined(__cplusplus)
}
#endif
#endif
