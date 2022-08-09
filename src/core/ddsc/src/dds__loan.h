#ifndef _DDS_LOAN_H_
#define _DDS_LOAN_H_

#include "dds/ddsc/dds_loan.h"

#if defined(__cplusplus)
extern "C" {
#endif

dds_loaned_sample_t* dds_heap_loan(uint32_t sz/*sertype/sample?*/);

#if defined(__cplusplus)
}

#endif
#endif
