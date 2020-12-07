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
#include <assert.h>
#include <stdlib.h>

#include "idl/processor.h"
#include "symbol.h"

void idl_delete_symbol(void *symbol)
{
  if (symbol)
    ((idl_symbol_t *)symbol)->destructor(symbol);
}

idl_mask_t idl_mask(const void *symbol)
{
  return symbol ? ((idl_symbol_t *)symbol)->mask : 0u;
}

bool idl_is_masked(const void *symbol, idl_mask_t mask)
{
  return symbol && (((idl_symbol_t *)symbol)->mask & mask) == mask;
}

const idl_location_t *idl_location(const void *symbol)
{
  return &((const idl_symbol_t *)symbol)->location;
}

void idl_delete_name(idl_name_t *name)
{
  if (name) {
    if (name->identifier)
      free(name->identifier);
    free(name);
  }
}

static void delete_name(void *symbol)
{
  idl_delete_name(symbol);
}

idl_retcode_t
idl_create_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  char *identifier,
  idl_name_t **namep)
{
  idl_name_t *name;

  (void)pstate;
  if (!(name = malloc(sizeof(*name))))
    return IDL_RETCODE_NO_MEMORY;
  name->symbol.mask = IDL_NAME;
  name->symbol.location = *location;
  name->symbol.destructor = &delete_name;
  name->symbol.printer = 0;
  name->identifier = identifier;
  *namep = name;
  return IDL_RETCODE_OK;
}

void idl_delete_scoped_name(idl_scoped_name_t *scoped_name)
{
  if (scoped_name) {
    for (size_t i=0; i < scoped_name->path.length; i++)
      idl_delete_name(scoped_name->path.names[i]);
    free(scoped_name->path.names);
    free(scoped_name);
  }
}

static void delete_scoped_name(void *symbol)
{
  idl_delete_scoped_name(symbol);
}

idl_retcode_t
idl_create_scoped_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  bool absolute,
  idl_scoped_name_t **scoped_namep)
{
  idl_scoped_name_t *scoped_name;

  (void)pstate;
  if (!(scoped_name = malloc(sizeof(*scoped_name)))) {
    return IDL_RETCODE_NO_MEMORY;
  }
  if (!(scoped_name->path.names = calloc(1, sizeof(idl_name_t*)))) {
    free(scoped_name);
    return IDL_RETCODE_NO_MEMORY;
  }
  scoped_name->symbol.location.first = location->first;
  scoped_name->symbol.location.last = name->symbol.location.last;
  scoped_name->symbol.destructor = &delete_scoped_name;
  scoped_name->symbol.printer = 0;
  scoped_name->path.length = 1;
  scoped_name->path.names[0] = name;
  scoped_name->absolute = absolute;
  *scoped_namep = scoped_name;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_append_to_scoped_name(
  idl_pstate_t *pstate,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name)
{
  size_t size;
  idl_name_t **names;

  (void)pstate;
  assert(scoped_name);
  assert(scoped_name->path.length >= 1);
  assert(name);

  size = (scoped_name->path.length + 1) * sizeof(idl_name_t*);
  if (!(names = realloc(scoped_name->path.names, size)))
    return IDL_RETCODE_NO_MEMORY;
  scoped_name->symbol.location.last = name->symbol.location.last;
  scoped_name->path.names = names;
  scoped_name->path.names[scoped_name->path.length++] = name;
  return IDL_RETCODE_OK;
}
