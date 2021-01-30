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
  name->symbol.location = *location;
  name->identifier = identifier;
  *namep = name;
  return IDL_RETCODE_OK;
}

void idl_delete_scoped_name(idl_scoped_name_t *scoped_name)
{
  if (scoped_name) {
    for (size_t i=0; i < scoped_name->length; i++)
      idl_delete_name(scoped_name->names[i]);
    free(scoped_name->names);
    free(scoped_name);
  }
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
  if (!(scoped_name->names = calloc(1, sizeof(idl_name_t*)))) {
    free(scoped_name);
    return IDL_RETCODE_NO_MEMORY;
  }
  scoped_name->symbol.location.first = location->first;
  scoped_name->symbol.location.last = name->symbol.location.last;
  scoped_name->absolute = absolute;
  scoped_name->length = 1;
  scoped_name->names[0] = name;
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
  assert(scoped_name->length >= 1);
  assert(name);

  size = (scoped_name->length + 1) * sizeof(idl_name_t*);
  if (!(names = realloc(scoped_name->names, size)))
    return IDL_RETCODE_NO_MEMORY;
  scoped_name->symbol.location.last = name->symbol.location.last;
  scoped_name->names = names;
  scoped_name->names[scoped_name->length++] = name;
  return IDL_RETCODE_OK;
}

void idl_delete_field_name(idl_field_name_t *field_name)
{
  if (field_name) {
    for (size_t i=0; i < field_name->length; i++)
      idl_delete_name(field_name->names[i]);
    free(field_name->names);
    free(field_name);
  }
}

idl_retcode_t
idl_create_field_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_field_name_t **field_namep)
{
  idl_field_name_t *field_name;

  (void)pstate;
  if (!(field_name = malloc(sizeof(*field_name))))
    goto err_name;
  if (!(field_name->names = calloc(1, sizeof(idl_name_t*))))
    goto err_names;
  field_name->symbol.location = *location;
  field_name->symbol.location.last = name->symbol.location.last;
  field_name->length = 1;
  field_name->names[0] = name;
  *field_namep = field_name;
  return IDL_RETCODE_OK;
err_names:
  free(field_name);
err_name:
  return IDL_RETCODE_NO_MEMORY;
}

idl_retcode_t
idl_append_to_field_name(
  idl_pstate_t *pstate,
  idl_field_name_t *field_name,
  idl_name_t *name)
{
  size_t size;
  idl_name_t **names;

  (void)pstate;
  assert(field_name);
  assert(field_name->length >= 1);
  assert(name);

  size = (field_name->length + 1) * sizeof(idl_name_t*);
  if (!(names = realloc(field_name->names, size)))
    return IDL_RETCODE_NO_MEMORY;
  field_name->symbol.location.last = name->symbol.location.last;
  field_name->names = names;
  field_name->names[field_name->length++] = name;
  return IDL_RETCODE_OK;
}
