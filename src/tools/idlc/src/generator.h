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
#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>

#include "idl/processor.h"

struct generator {
  const char *path;
  struct {
    FILE *handle;
    char *path;
  } header;
  struct {
    FILE *handle;
    char *path;
  } source;
};

#if _WIN32
__declspec(dllexport)
#endif
idl_retcode_t idlc_generate(const idl_pstate_t *pstate);

#if _WIN32
__declspec(dllexport)
#endif
idl_retcode_t generate_nosetup(const idl_pstate_t *pstate, struct generator *generator);

#endif /* GENERATOR_H */
