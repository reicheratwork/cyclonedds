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
#ifndef IDL_TREE_H
#define IDL_TREE_H

#include <stdbool.h>
#include <stdint.h>

#include "idl/export.h"
#include "idl/retcode.h"
#include "idl/symbol.h"

/* the parser constructs a tree representing the idl document from
   specialized nodes. each node is derived from the same base node, which
   contains properties common accross nodes, and is either a declaration,
   specifier, expression, constant, pragma, or combination thereof. constants
   contain the result from an expression, pragmas contain compiler-specific
   instructions that apply to a specific declaration, much like annotations.
   the exact type of a node is stored in the mask property of the base node
   and is an constructed by combining preprocessor defines. unique bits are
   reserved for categories and properties that generators are likely to filter
   on when applying a visitor pattern. */

#define IDL_KEY (1llu<<37)
#define IDL_INHERIT_SPEC (1llu<<36)
#define IDL_SWITCH_TYPE_SPEC (1llu<<35)
/* specifiers */
#define IDL_TYPE (1llu<<34)
/* declarations */
#define IDL_DECLARATION (1llu<<33)
#define IDL_MODULE (1llu<<32)
#define IDL_CONST (1llu<<31)
#define IDL_MEMBER (1llu<<30)
#define IDL_FORWARD (1llu<<29)
#define IDL_CASE (1llu<<28)
#define IDL_CASE_LABEL (1llu<<27)
#define IDL_ENUMERATOR (1llu<<26)
#define IDL_DECLARATOR (1llu<<25)
/* annotations */
#define IDL_ANNOTATION (1llu<<24)
#define IDL_ANNOTATION_MEMBER (1llu<<23)
#define IDL_ANNOTATION_APPL (1llu<<22)
#define IDL_ANNOTATION_APPL_PARAM (1llu<<21)

typedef enum idl_type idl_type_t;
enum idl_type {
  IDL_NULL = 0u,
  IDL_TYPEDEF = (1llu<<16),                   /* IDL_TYPE | IDL_DECLARATION */
  /* constructed types */
#define IDL_CONSTR_TYPE (1llu<<15)            /* IDL_TYPE | IDL_DECLARATION */
  IDL_STRUCT = (1u<<14),                                 /* IDL_CONSTR_TYPE */
  IDL_UNION = (1u<<13),                                  /* IDL_CONSTR_TYPE */
  IDL_ENUM = (1u<<12),                                   /* IDL_CONSTR_TYPE */
  /* template types */
#define IDL_TEMPL_TYPE (1llu<<11)
  IDL_SEQUENCE = (IDL_TEMPL_TYPE | 1u),                         /* IDL_TYPE */
  IDL_STRING = (IDL_TEMPL_TYPE | 2u),               /* IDL_CONST / IDL_TYPE */
  IDL_WSTRING = (IDL_TEMPL_TYPE | 3u),              /* IDL_CONST / IDL_TYPE */
  IDL_FIXED_PT = (IDL_TEMPL_TYPE | 4u),                         /* IDL_TYPE */
  /* simple types */
  /* miscellaneous base types */
#define IDL_BASE_TYPE (1llu<<10)                    /* IDL_CONST / IDL_TYPE */
#define IDL_UNSIGNED (1llu<<0)
  IDL_CHAR = (IDL_BASE_TYPE | (1u<<1)),
  IDL_WCHAR = (IDL_BASE_TYPE | (2u<<1)),
  IDL_BOOL = (IDL_BASE_TYPE | (3u<<1)),
  IDL_OCTET = (IDL_BASE_TYPE | (4u<<1) | IDL_UNSIGNED),
  IDL_ANY = (IDL_BASE_TYPE | (5u<<1)),
  /* integer types */
#define IDL_INTEGER_TYPE (IDL_BASE_TYPE | (1llu<<9))       /* IDL_BASE_TYPE */
  IDL_SHORT = (IDL_INTEGER_TYPE | (1u<<1)),
  IDL_USHORT = (IDL_SHORT | IDL_UNSIGNED),
  IDL_LONG = (IDL_INTEGER_TYPE | (2u<<1)),
  IDL_ULONG = (IDL_LONG | IDL_UNSIGNED),
  IDL_LLONG = (IDL_INTEGER_TYPE | (3u<<1)),
  IDL_ULLONG = (IDL_LLONG | IDL_UNSIGNED),
  /* fixed size integer types overlap with legacy integer types in size, but
     unique identifiers are required for proper syntax errors. language
     bindings may choose to map onto different types as well */
  IDL_INT8 = (IDL_INTEGER_TYPE | (4u<<1)),
  IDL_UINT8 = (IDL_INT8 | IDL_UNSIGNED),
  IDL_INT16 = (IDL_INTEGER_TYPE | (5u<<1)),
  IDL_UINT16 = (IDL_INT16 | IDL_UNSIGNED),
  IDL_INT32 = (IDL_INTEGER_TYPE | (6u<<1)),
  IDL_UINT32 = (IDL_INT32 | IDL_UNSIGNED),
  IDL_INT64 = (IDL_INTEGER_TYPE | (7u<<1)),
  IDL_UINT64 = (IDL_INT64 | IDL_UNSIGNED),
  /* floating point types */
#define IDL_FLOATING_PT_TYPE (IDL_BASE_TYPE | (1llu<<8))   /* IDL_BASE_TYPE */
  IDL_FLOAT = (IDL_FLOATING_PT_TYPE | 1u),
  IDL_DOUBLE = (IDL_FLOATING_PT_TYPE | 2u),
  IDL_LDOUBLE = (IDL_FLOATING_PT_TYPE | 3u)
};

struct idl_scope;
struct idl_annotation_appl;

typedef void idl_const_expr_t;
typedef void idl_definition_t;
typedef void idl_type_spec_t;

typedef struct idl_id idl_id_t;
struct idl_id {
  enum {
    IDL_AUTOID, /** value assigned automatically */
    IDL_ID, /**< value assigned by @id */
    IDL_HASHID /**< value assigned by @hashid */
  } annotation;
  uint32_t value;
};

typedef struct idl_node idl_node_t;
struct idl_node {
  idl_symbol_t symbol;
  idl_delete_t destructor; /**< destructor */
  int32_t references;
  struct idl_annotation_appl *annotations;
  const struct idl_scope *scope; /**< enclosing scope */
  idl_node_t *parent;
  idl_node_t *previous, *next;
};

typedef enum idl_autoid idl_autoid_t;
enum idl_autoid {
  IDL_AUTOID_SEQUENTIAL,
  IDL_AUTOID_HASH
};

typedef enum idl_extensibility idl_extensibility_t;
enum idl_extensibility {
  IDL_EXTENSIBILITY_FINAL,
  IDL_EXTENSIBILITY_APPENDABLE,
  IDL_EXTENSIBILITY_MUTABLE
};

/* annotations */

/* constants contain the value of resolved constant expressions and are used
   if the resulting constant value can be of more than one type, e.g. in
   constant declarations, case labels, etc. language native types are used if
   the resulting constant value is required to be of a specific base type,
   e.g. bounds in sequences. */
typedef struct idl_constval idl_constval_t;
struct idl_constval {
  idl_node_t node;
  union {
    bool bln;
    char chr;
    int8_t int8;
    uint8_t uint8;
    int16_t int16;
    uint16_t uint16;
    int32_t int32;
    uint32_t uint32;
    int64_t int64;
    uint64_t uint64;
    float flt;
    double dbl;
    long double ldbl;
    char *str;
  } value;
};

typedef struct idl_const idl_const_t;
struct idl_const {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  struct idl_name *name;
  idl_const_expr_t *const_expr;
};

typedef struct idl_base_type idl_base_type_t;
struct idl_base_type {
  idl_node_t node;
  /* empty */
};

typedef struct idl_sequence idl_sequence_t;
struct idl_sequence {
  idl_node_t node;
  void *type_spec;
  uint32_t maximum;
};

typedef struct idl_string idl_string_t;
struct idl_string {
  idl_node_t node;
  uint32_t maximum;
};

typedef struct idl_module idl_module_t;
struct idl_module {
  idl_node_t node;
  struct idl_name *name;
  idl_definition_t *definitions;
  const idl_module_t *previous; /**< previous module if module was reopened */
};

typedef struct idl_declarator idl_declarator_t;
struct idl_declarator {
  idl_node_t node;
  struct idl_name *name;
  idl_const_expr_t *const_expr;
};

typedef struct idl_member idl_member_t;
struct idl_member {
  idl_node_t node;
  void *type_spec;
  idl_declarator_t *declarators;
  bool key;
  idl_id_t id;
};

/* types can inherit from and extend other types (interfaces, values and
   structs). declarations in the base type that become available in the
   derived type as a consequence are imported into the scope */
typedef struct idl_inherit_spec idl_inherit_spec_t;
struct idl_inherit_spec {
  idl_node_t node;
  void *base;
};

/* keylist directives can use dotted names, e.g. "#pragma keylist foo bar.baz"
   in "struct foo { struct bar { long baz; }; };" to declare a member as key.
   this notation makes it possible for "baz" to only be a key member if "bar"
   is embedded in "foo" and for key order to differ from field order */
typedef struct idl_key idl_key_t;
struct idl_key {
  idl_node_t node;
  idl_declarator_t *declarator;
};

typedef struct idl_struct idl_struct_t;
struct idl_struct {
  idl_node_t node;
  idl_inherit_spec_t *inherit_spec;
  struct idl_name *name;
  idl_member_t *members;
  bool topic;
  idl_key_t *keys;
  idl_autoid_t autoid;
  idl_extensibility_t extensibility;
};

typedef struct idl_case_label idl_case_label_t;
struct idl_case_label {
  idl_node_t node;
  void *const_expr;
};

typedef struct idl_case idl_case_t;
struct idl_case {
  idl_node_t node;
  idl_case_label_t *case_labels;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarator;
};

typedef struct idl_switch_type_spec idl_switch_type_spec_t;
struct idl_switch_type_spec {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  bool key;
};

typedef struct idl_union idl_union_t;
struct idl_union {
  idl_node_t node;
  struct idl_name *name;
  idl_switch_type_spec_t *switch_type_spec;
  idl_case_t *cases;
  idl_extensibility_t extensibility;
};

typedef struct idl_enumerator idl_enumerator_t;
struct idl_enumerator {
  idl_node_t node;
  struct idl_name *name;
  uint32_t value;
};

typedef struct idl_enum idl_enum_t;
struct idl_enum {
  idl_node_t node;
  struct idl_name *name;
  idl_enumerator_t *enumerators;
  idl_extensibility_t extensibility;
};

typedef struct idl_forward idl_forward_t;
struct idl_forward {
  idl_node_t node;
  struct idl_name *name;
//  void *constr_type_decl;
};

typedef struct idl_typedef idl_typedef_t;
struct idl_typedef {
  idl_node_t node;
  void *type_spec;
  idl_declarator_t *declarators;
};

struct idl_pstate;
typedef idl_retcode_t (*idl_annotation_callback_t)(struct idl_pstate *, struct idl_annotation_appl *, idl_node_t *);

typedef struct idl_annotation_member idl_annotation_member_t;
struct idl_annotation_member {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarator;
  idl_const_expr_t *const_expr; /**< default value (if any) */
};

typedef void idl_annotation_definition_t;

typedef struct idl_annotation idl_annotation_t;
struct idl_annotation {
  idl_node_t node;
  idl_name_t *name;
  /** definitions that together form the body, e.g. member, enum, etc */
  idl_definition_t *definitions;
  idl_annotation_callback_t callback;
};

typedef struct idl_annotation_appl_param idl_annotation_appl_param_t;
struct idl_annotation_appl_param {
  idl_node_t node;
  idl_annotation_member_t *member;
  idl_const_expr_t *const_expr; /**< constant or enumerator */
};

typedef struct idl_annotation_appl idl_annotation_appl_t;
struct idl_annotation_appl {
  idl_node_t node;
  idl_annotation_t *annotation;
  idl_annotation_appl_param_t *parameters;
};

IDL_EXPORT bool idl_is_declaration(const void *node);
IDL_EXPORT bool idl_is_module(const void *node);
IDL_EXPORT bool idl_is_struct(const void *node);
IDL_EXPORT bool idl_is_member(const void *node);
IDL_EXPORT bool idl_is_union(const void *node);
IDL_EXPORT bool idl_is_case(const void *node);
IDL_EXPORT bool idl_is_default_case(const void *node);
IDL_EXPORT bool idl_is_case_label(const void *node);
IDL_EXPORT bool idl_is_enum(const void *node);
IDL_EXPORT bool idl_is_declarator(const void *node);
IDL_EXPORT bool idl_is_enumerator(const void *node);
IDL_EXPORT bool idl_is_typedef(const void *node);
IDL_EXPORT bool idl_is_forward(const void *node);
IDL_EXPORT bool idl_is_templ_type(const void *node);
IDL_EXPORT bool idl_is_sequence(const void *node);
IDL_EXPORT bool idl_is_string(const void *node);
IDL_EXPORT bool idl_is_base_type(const void *node);
IDL_EXPORT bool idl_is_type(const void *node, idl_type_t type);
IDL_EXPORT bool idl_is_const(const void *node);
IDL_EXPORT bool idl_is_constval(const void *node);

IDL_EXPORT bool idl_is_annotation_member(const void *node);

IDL_EXPORT idl_type_t idl_type(const void *node);
IDL_EXPORT const char *idl_identifier(const void *node);
IDL_EXPORT const idl_name_t *idl_name(const void *node);
IDL_EXPORT void *idl_parent(const void *node);
IDL_EXPORT void *idl_previous(const void *node);
IDL_EXPORT void *idl_next(const void *node);
IDL_EXPORT void *idl_unalias(const void *node);
IDL_EXPORT size_t idl_length(const void *node);

#endif /* IDL_TREE_H */
