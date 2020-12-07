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

typedef uint64_t idl_mask_t;

typedef void(*idl_print_t)(const void *);
typedef void(*idl_delete_t)(void *);

typedef struct idl_symbol idl_symbol_t;
struct idl_symbol {
  idl_mask_t mask;
  idl_location_t location;
  idl_print_t printer;
  idl_delete_t destructor;
};

IDL_EXPORT void idl_delete_symbol(void *symbol);
IDL_EXPORT idl_mask_t idl_mask(const void *symbol);
IDL_EXPORT bool idl_is_masked(const void *symbol, idl_mask_t mask);
IDL_EXPORT const idl_location_t *idl_location(const void *symbol);

#define IDL_NAME (1llu<<63)
#define IDL_SCOPED_NAME (1llu<<62)

struct idl_pstate;

typedef struct idl_name {
  idl_symbol_t symbol;
  char *identifier;
} idl_name_t;

typedef struct idl_scoped_name {
  idl_symbol_t symbol;
  bool absolute;
  struct {
    size_t length;
    idl_name_t **names;
  } path;
} idl_scoped_name_t;

#endif /* IDL_SYMBOL_H */
