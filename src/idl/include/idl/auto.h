/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_AUTO_H
#define IDL_AUTO_H

#include "idl/export.h"
#if WIN32
# include <intrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* very crude garbage collection based on stack depth and return address */

/* !!!!DO NOT USE!!!! */

#if _WIN32
# define return_address() _ReturnAddress()
#else
# define return_address() __builtin_return_address(0)
#endif

IDL_EXPORT void *idl_auto(const void *address, void *block);

#define IDL_AUTO(pointer) idl_auto(return_address(), pointer)

IDL_EXPORT void idl_collect_auto(const void *address);

#define IDL_COLLECT_AUTO() idl_collect_auto(return_address())

#ifdef __cplusplus
}
#endif

#endif /* IDL_AUTO_H */
