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
#include <string.h>

#include "table.h"
#include "idl/string.h"

const idl_symbol_t *
idl_add_symbol(
  idl_processor_t *proc,
  const char *scope,
  const char *name,
  const idl_node_t *node)
{
  idl_symbol_t *sym;

  assert(proc);
  assert(name);
  assert(node);

  if (!(sym = malloc(sizeof(*sym))))
    return NULL;

  if (name[0] == ':' && name[1] == ':') {
    if (!(sym->name = strdup(name))) {
      free(sym);
      return NULL;
    }
  } else {
    assert(scope);
    assert(scope[0] == ':' && scope[1] == ':');
    if (idl_asprintf(&sym->name, "%s::%s", strcmp(scope, "::") == 0 ? "" : scope, name) == -1) {
      free(sym);
      return NULL;
    }
  }

  sym->node = node;
  if (proc->table.first) {
    assert(proc->table.last);
    proc->table.last->next = sym;
  } else {
    assert(!proc->table.last);
    proc->table.first = proc->table.last = sym;
  }

  return sym;
}

const idl_symbol_t *
idl_find_symbol(
  const idl_processor_t *proc,
  const char *scope,
  const char *name,
  const idl_symbol_t *whence)
{
  idl_symbol_t *sym;

  assert(proc);
  assert(name);

  assert(!whence || whence->next || whence == proc->table.last);
  if (whence == proc->table.last)
    return NULL;

  sym = whence ? whence->next : proc->table.first;

  if (name[0] == ':' && name[1] == ':') {
    for (; sym && strcmp(name, sym->name) != 0; sym = sym->next) ;
  } else {
    size_t len;

    assert(scope);
    assert(scope[0] == ':' && scope[1] == ':');
    len = strlen(scope);
    for (; sym; sym = sym->next) {
      if (strncmp(scope, sym->name, len) != 0)
        continue;
      if (!(sym->name[len+0] == ':' && sym->name[len+1] == ':'))
        continue;
      if (strcmp(name, sym->name + len + 2) == 0)
        break;
    }
  }

  return sym;
}
