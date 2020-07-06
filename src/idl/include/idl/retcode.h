/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_RETCODE_H
#define IDL_RETCODE_H

#include <stdint.h>

typedef int32_t idl_retcode_t;

/**
 * @name IDL_Return_Code
 */
/** @{ */
/** Success */
#define IDL_RETCODE_OK (0)
#define IDL_RETCODE_PUSH_MORE (-1)
/** Processor needs refill in order to continue */
#define IDL_RETCODE_NEED_REFILL (-2)
/** Syntax or semantic error */
#define IDL_RETCODE_SCAN_ERROR (-3)
#define IDL_RETCODE_PARSE_ERROR IDL_RETCODE_SCAN_ERROR
/** Operation failed due to lack of resources */
#define IDL_RETCODE_NO_MEMORY (-5)
/** @} */

#endif /* IDL_RETCODE_H */
