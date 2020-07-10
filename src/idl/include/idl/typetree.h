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
#ifndef IDL_TYPETREE_H
#define IDL_TYPETREE_H

#include <stdbool.h>
#include <stdint.h>

#include "idl/retcode.h"

// * generate a parse tree (not a tree consisting only of types)
// * tree is constructed of "abstract" nodes
//   * a node describes a type of node, not a type perse. tree describes IDL
//     * for instance, nodes can also describe an expression
// * a combination of flags describes the type of node
//   * for instance, you cannot have a base type node, unless it is also
//     marked a union or struct member (or a constant)
// * normally a parse tree is used to generate derivative tree. e.g. an
//   abstract syntax tree (ast).
//   * for IDL, it is probably enough to generate a decent parse tree and use
//     a second pass to resolve scoped names, expressions etc

#define IDL_MODULE (1u<<19)

/** #pragma keylist */
#define IDL_KEYLIST (1u<<18)
/** #pragma keylist key */
#define IDL_KEY (1u<<17)

#define IDL_LITERAL (1u<<16)
#define IDL_FORWARD_DCL (1u<<15)
#define IDL_SCOPED_NAME (1u<<14)
#define IDL_MEMBER (1u<<13)
#define IDL_SWITCH_TYPE_SPEC (1u<<12)
#define IDL_CASE (1u<<11)
#define IDL_CASE_LABEL (1u<<10)
#define IDL_ENUMERATOR (1u<<9)

/* constructed types */
#define IDL_CONSTR_TYPE (1u<<8)
#define IDL_STRUCT (IDL_CONSTR_TYPE | 1u)
#define IDL_UNION (IDL_CONSTR_TYPE | 2u)
#define IDL_ENUM (IDL_CONSTR_TYPE | 3u)
/* template types */
#define IDL_TEMPL_TYPE (1u<<7)
#define IDL_SEQUENCE (IDL_TEMPL_TYPE | 1u)
#define IDL_STRING (IDL_TEMPL_TYPE | 2u)
#define IDL_WSTRING (IDL_TEMPL_TYPE | 3u)
#define IDL_FIXED_PT (IDL_TEMPL_TYPE | 4u)
/* base types */
#define IDL_BASE_TYPE (1u<<6)
#define IDL_FLOATING_PT_TYPE (IDL_BASE_TYPE | (1u<<5))
#define IDL_INTEGER_TYPE (IDL_BASE_TYPE | (1u<<4))
#define IDL_UNSIGNED (1u<<0)
/* integer types */
#define IDL_INT8 (IDL_INTEGER_TYPE | (1u<<1))
#define IDL_UINT8 (IDL_INT8 | IDL_UNSIGNED)
#define IDL_INT16 (IDL_INTEGER_TYPE | (2u<<1))
#define IDL_UINT16 (IDL_INT16 | IDL_UNSIGNED)
#define IDL_SHORT IDL_INT16
#define IDL_USHORT IDL_UINT16
#define IDL_INT32 (IDL_INTEGER_TYPE | (3u<<1))
#define IDL_UINT32 (IDL_INT32 | IDL_UNSIGNED)
#define IDL_LONG IDL_INT32
#define IDL_ULONG IDL_UINT32
#define IDL_INT64 (IDL_INTEGER_TYPE | (4u<<1))
#define IDL_UINT64 (IDL_INT64 | IDL_UNSIGNED)
#define IDL_LLONG IDL_INT64
#define IDL_ULLONG IDL_UINT64
/* floating point types */
#define IDL_FLOAT (IDL_FLOATING_PT_TYPE | 1u)
#define IDL_DOUBLE (IDL_FLOATING_PT_TYPE | 2u)
#define IDL_LDOUBLE (IDL_FLOATING_PT_TYPE | 3u)
/* miscellaneous base types */
#define IDL_CHAR (IDL_BASE_TYPE | 1u)
#define IDL_WCHAR (IDL_BASE_TYPE | 2u)
#define IDL_BOOL (IDL_BASE_TYPE | 3u)
#define IDL_OCTET (IDL_BASE_TYPE | 4u)


/** @private */
typedef struct idl_file idl_file_t;
struct idl_file {
  idl_file_t *next;
  char *name;
};

/** @private */
typedef struct idl_position idl_position_t;
struct idl_position {
  const char *file;
  uint32_t line;
  uint32_t column;
};

/** @private */
typedef struct idl_location idl_location_t;
struct idl_location {
  idl_position_t first;
  idl_position_t last;
};

typedef struct idl_node idl_node_t;
typedef void(*idl_free_t)(idl_node_t *);

struct idl_node {
  uint32_t flags; /**< node type, e.g. struct, module or expression */
  uint32_t weight; /**< number of references to node */
  char *name; /**< type name, e.g. name of struct or scoped name */
  idl_location_t location;
  idl_node_t *parent; /**< parent */
  idl_node_t *previous; /**< previous sibling */
  idl_node_t *next; /**< next sibling */
  idl_node_t *children;
  union {
    struct {
      char *declarator;
    } member, _case;
    struct {
      char *identifier;
    } enumerator;
    union {
      char *string;
      long long integer;
    } literal;
    struct {
      uint32_t bound;
    } sequence, string, wstring;
    struct {
      uint32_t digits;
      uint32_t scale;
    } map;
    struct {
      char *data_type;
    } keylist;
    struct {
      char *identifier;
    } key;
  } type;
};

typedef struct idl_tree idl_tree_t;
struct idl_tree {
  idl_node_t *root;
  idl_file_t *files;
};

idl_node_t *idl_find_node(idl_node_t *root, const char *scoped_name);

typedef idl_retcode_t(*idl_visit_t)(idl_node_t *, void *user_data);

#define IDL_VISIT_RECURSE (1u<<0)
#define IDL_VISIT_FOLLOW (1u<<1)

idl_retcode_t
idl_walk(
  idl_node_t *root,
  uint32_t flags,
  idl_visit_t function,
  uint32_t filter,
  void *user_data);

#endif /* IDL_TYPETREE_H */
