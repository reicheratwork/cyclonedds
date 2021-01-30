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
#ifndef IDL_SYMBOL_H
#define IDL_SYMBOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "idl/export.h"

typedef struct idl_file idl_file_t;
struct idl_file {
  idl_file_t *next;
  char *name;
};

typedef struct idl_source idl_source_t;
struct idl_source {
  idl_source_t *parent;
  idl_source_t *previous, *next;
  idl_source_t *includes;
  bool system; /**< system include */
  const idl_file_t *path; /**< normalized path of filename in #line directive */
  const idl_file_t *file; /**< filename in #line directive */
};

typedef struct idl_position {
  const idl_source_t *source;
  /* for error reporting purposes, the "filename" provided in the #line
     directive must be kept. on includes, idlpp provides a (relative) filename
     with the proper flags, which becomes the source. user provided #line
     directives in the file are used merely in error reporting */
  const idl_file_t *file; /**< (alternate) filename in latest #line directive */
  uint32_t line;
  uint32_t column;
} idl_position_t;

typedef struct idl_location {
  idl_position_t first;
  idl_position_t last;
} idl_location_t;

/* symbols are there for the parser(s), nodes are there for the tree.
   all nodes are symbols, not all symbols are nodes */

typedef struct idl_symbol idl_symbol_t;
struct idl_symbol {
  idl_location_t location;
};

IDL_EXPORT const idl_location_t *idl_location(const void *symbol);

struct idl_pstate;

//
// would be nice to make it easier for users to construct a scoped name
// themselves... at that point the thing doesn't have location because its
// spontaneously materialized...
//   >> we need to have an internal representation that uses something like
//      struct name { idl_location_t *location; char *identifier; }
//      >> users can then pass a plain identifier or scoped name, we know
//         that internally registered names have a location prepended!
// >> we can then create things like idl_to_field_name
//

typedef struct idl_name {
  idl_symbol_t symbol;
  char *identifier;
} idl_name_t;

typedef struct idl_scoped_name {
  idl_symbol_t symbol;
  bool absolute;
  size_t length;
  idl_name_t **names;
} idl_scoped_name_t;

typedef struct idl_field_name {
  idl_symbol_t symbol;
  size_t length;
  idl_name_t **names;
} idl_field_name_t;

#endif /* IDL_SYMBOL_H */
