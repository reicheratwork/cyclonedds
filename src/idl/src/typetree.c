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
#include <stddef.h>

#include "idl/typetree.h"

idl_node_t *idl_find_node(idl_node_t *root, const char *name)
{
  (void)root;
  (void)name;

  // .. implement ..

  return NULL;
}

idl_retcode_t
idl_walk(
  idl_node_t *root,
  uint32_t flags,
  idl_visit_t callback,
  uint32_t filter,
  void *user_data)
{
  (void)root;
  (void)flags;
  (void)callback;
  (void)filter;
  (void)user_data;

  // .. implement ..

  return 0;
}
