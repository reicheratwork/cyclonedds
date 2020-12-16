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

#include "idlc/generator.h"

typedef struct idlc_generator idlc_generator_t;
struct idlc_generator {
  void *handle;
  idlc_generator_options_t generator_options; /* optional */
  idlc_generator_annotations_t generator_annotations; /* optional */
  idlc_generate_t generate;
};

int idlc_load_generator(idlc_generator_t *gen, const char *lang);
void idlc_unload_generator(idlc_generator_t *gen);

#endif /* IDLC_GENERATOR_H */
