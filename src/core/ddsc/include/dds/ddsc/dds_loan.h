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

struct dds_writer;

/**
 * @ingroup loan
 * @brief Loan a sample from the writer.
 *
 * @note This function is to be used with dds_write to publish the loaned
 * sample.
 * @note The function can only be used if dds_is_loan_available is
 *       true for the writer.
 *
 * @param[in] writer the writer to loan the buffer from
 * @param[out] sample the loaned sample
 *
 * @returns DDS_RETCODE_OK if successful, DDS_RETCODE_ERROR otherwise
 */
DDS_EXPORT dds_return_t dds_writer_loan_samples(dds_entity_t writer, void **samples_ptr, uint32_t n_samples);

DDS_EXPORT memory_block_t * dds_writer_check_for_loan(dds_writer *writer, const void *sample_ptr);

#if defined(__cplusplus)
}
#endif
#endif
