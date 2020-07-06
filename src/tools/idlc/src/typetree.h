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

/* lower four bits reserved for subtype stuff */
/* higher four bits indicate type class */

#define IDL_BASE_TYPE (0x10u)
#define IDL_TEMPL_TYPE (0x20u)
#define IDL_CONSTR_TYPE (0x30u)
#define IDL_MODULE (0x40u)
#define IDL_SCOPED_NAME (0x50u)

#define IDL_INT8 (IDL_BASE_TYPE | 0x01u)
#define IDL_INT16 (IDL_BASE_TYPE | 0x02u)
#define IDL_SHORT IDL_INT16
#define IDL_INT32 (IDL_BASE_TYPE | 0x03u)
#define IDL_LONG IDL_INT32
#define IDL_INT64 (IDL_BASE_TYPE | 0x04u)
#define IDL_LLONG IDL_INT64
#define IDL_FLOAT (IDL_BASE_TYPE | 0x05u)
#define IDL_DOUBLE (IDL_BASE_TYPE | 0x06u)
#define IDL_LDOUBLE (IDL_BASE_TYPE | 0x07u)
#define IDL_CHAR (IDL_BASE_TYPE | 0x08u)
#define IDL_WCHAR (IDL_BASE_TYPE | 0x09u)
#define IDL_BOOL (IDL_BASE_TYPE | 0x0au)
#define IDL_OCTET (IDL_BASE_TYPE | 0x0bu)

#define IDL_SEQUENCE_TYPE (IDL_TEMPL_TYPE | 0x01u)
#define IDL_STRING_TYPE (IDL_TEMPL_TYPE | 0x02u)
#define IDL_WSTRING_TYPE (IDL_TEMPL_TYPE | 0x03u)
#define IDL_FIXED_PT_TYPE (IDL_TEMPL_TYPE | 0x04u)

#define IDL_STRUCT (IDL_CONSTR_TYPE | 0x01u)
#define IDL_UNION (IDL_CONSTR_TYPE | 0x02u)
#define IDL_ENUM (IDL_CONSTR_TYPE | 0x03u)

#define IDL_FLAG_UNSIGNED (1u<<8)
#define IDL_FLAG_FORWARD (1u<<9)
#define IDL_FLAG_MEMBER (1u<<10)
#define IDL_FLAG_KEY (1u<<11)

typedef int32_t idl_retcode_t;

#define IDL_PUSH_MORE (-1)
#define IDL_NEED_REFILL (-2)
#define IDL_SCAN_ERROR (-3)
#define IDL_PARSE_ERROR IDL_SCAN_ERROR
#define IDL_MEMORY_EXHAUSTED (-5)
#define IDL_READ_ERROR (-6)

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
  uint32_t flags;
  uint32_t weight; /**< number of references to node */
  char *name; /**< type name (if applicable) */
  idl_location_t location;
  idl_node_t *parent; /**< parent */
  idl_node_t *previous; /**< previous sibling */
  idl_node_t *next; /**< next sibling */
  union {
    /* module, constructed type, constant expression */
    struct {
      struct {
        idl_node_t *first;
        idl_node_t *last;
      } children;
    } module_dcl, constr_type_dcl, const_dcl;
    /* scoped name, forward declaration, member declaration */
    struct {
      idl_node_t *reference;
    } scoped_name, forward_dcl;
    struct {
      char *name;
      idl_node_t *reference;
    } member_dcl;
  } type; /**< type of node */
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
