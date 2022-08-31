#ifndef _DDS_LOAN_H_
#define _DDS_LOAN_H_

#include "dds/ddsc/dds_loan.h"
#include "dds__types.h"

#if defined(__cplusplus)
extern "C" {
#endif

dds_loaned_sample_t* dds_heap_loan(const struct ddsi_sertype *type);

#if defined(__cplusplus)
}

#endif
#endif
