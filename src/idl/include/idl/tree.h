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

#include "idl/retcode.h"
#include "idl/export.h"

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

#define IDL_ANNOTATION_APPL (1<<27)
#define IDL_ANNOTATION_APPL_PARAM (1<<26)

#define IDL_CONST_DCL (1u<<26)
#define IDL_BINARY_EXPR (1u<<25)
#define IDL_OR_EXPR (IDL_BINARY_EXPR | 1u)
#define IDL_XOR_EXPR (IDL_BINARY_EXPR | 2u)
#define IDL_AND_EXPR (IDL_BINARY_EXPR | 3u)
#define IDL_LSHIFT_EXPR (IDL_BINARY_EXPR | 4u)
#define IDL_RSHIFT_EXPR (IDL_BINARY_EXPR | 5u)
#define IDL_ADD_EXPR (IDL_BINARY_EXPR | 6u)
#define IDL_SUB_EXPR (IDL_BINARY_EXPR | 7u)
#define IDL_MULT_EXPR (IDL_BINARY_EXPR | 8u)
#define IDL_DIV_EXPR (IDL_BINARY_EXPR | 9u)
#define IDL_MOD_EXPR (IDL_BINARY_EXPR | 10u)
#define IDL_UNARY_EXPR (1u<<24)
#define IDL_MINUS_EXPR (IDL_UNARY_EXPR | 1u)
#define IDL_PLUS_EXPR (IDL_UNARY_EXPR | 2u)
#define IDL_NOT_EXPR (IDL_UNARY_EXPR | 3u)

#define IDL_TYPEDEF (1u<<23)
#define IDL_DECLARATOR (1u<<22)
#define IDL_ARRAY_SIZE (1u<<21)
#define IDL_CONST (1u<<20) /* FIXME: not needed anymore? */
#define IDL_MODULE (1u<<19)
#define IDL_LITERAL (1u<<16)
#define IDL_FORWARD_DCL (1u<<15)
#define IDL_MEMBER (1u<<14)
#define IDL_SWITCH_TYPE_SPEC (1u<<13) /* FIXME: not needed anymore? */
#define IDL_CASE (1u<<12)
#define IDL_CASE_LABEL (1u<<11)
#define IDL_ENUMERATOR (1u<<10)

/* constructed types */
#define IDL_CONSTR_TYPE (1u<<9)
#define IDL_STRUCT_TYPE (IDL_CONSTR_TYPE | 1u)
#define IDL_UNION_TYPE (IDL_CONSTR_TYPE | 2u)
#define IDL_ENUM_TYPE (IDL_CONSTR_TYPE | 3u)
/* template types */
#define IDL_TEMPL_TYPE (1u<<8)
#define IDL_SEQUENCE_TYPE (IDL_TEMPL_TYPE | 1u)
#define IDL_STRING_TYPE (IDL_TEMPL_TYPE | 2u)
#define IDL_WSTRING_TYPE (IDL_TEMPL_TYPE | 3u)
#define IDL_FIXED_PT_TYPE (IDL_TEMPL_TYPE | 4u)
/* simple types */
#define IDL_SCOPED_NAME (1u<<7)
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

typedef uint32_t idl_kind_t;

typedef struct idl_node idl_node_t;

typedef void(*idl_print_t)(idl_node_t *);
typedef void(*idl_delete_t)(idl_node_t *);

typedef struct idl_annotation_appl idl_annotation_appl_t;

struct idl_node {
  idl_kind_t kind; /**< node type, e.g. struct, module or expression */
  /* FIXME: refcount maybe required */
  idl_location_t location;
  idl_annotation_appl_t *annotations;
  idl_node_t *parent; /**< pointer to parent node */
  idl_node_t *previous, *next; /**< pointers to sibling nodes */
  idl_print_t printer;
  idl_delete_t destructor;
};

/* syntactic sugar */
typedef union idl_definition idl_definition_t;
typedef union idl_const_expr idl_const_expr_t;
typedef union idl_type_spec idl_type_spec_t;
typedef union idl_simple_type_spec idl_simple_type_spec_t;
typedef union idl_constr_type_spec idl_constr_type_spec_t;
typedef union idl_switch_type_spec idl_switch_type_spec_t;
typedef union idl_const_type idl_const_type_t;

typedef struct idl_tree idl_tree_t;
struct idl_tree {
  idl_node_t *root;
  idl_file_t *files;
};

typedef struct idl_variant idl_variant_t;
struct idl_variant {
  idl_kind_t kind;
  union {
    int64_t signed_int;
    uint64_t unsigned_int;
    long double floating_pt;
    char *string;
    bool boolean;
  } value;
};

typedef struct idl_literal idl_literal_t;
struct idl_literal {
  idl_node_t node;
  idl_variant_t variant;
};

typedef struct idl_binary_expr idl_binary_expr_t;
struct idl_binary_expr {
  idl_node_t node;
  idl_const_expr_t *left;
  idl_const_expr_t *right;
};

typedef struct idl_unary_expr idl_unary_expr_t;
struct idl_unary_expr {
  idl_node_t node;
  idl_const_expr_t *right;
};

typedef struct idl_base_type idl_base_type_t;
struct idl_base_type {
  idl_node_t node;
};

typedef struct idl_scoped_name idl_scoped_name_t;
struct idl_scoped_name {
  idl_node_t node;
  char *name;
  idl_node_t *reference;
};

typedef struct idl_sequence_type idl_sequence_type_t;
struct idl_sequence_type {
  idl_node_t node;
  idl_simple_type_spec_t *type_spec;
  idl_const_expr_t *const_expr;
};

typedef struct idl_string_type idl_string_type_t;
struct idl_string_type {
  idl_node_t node;
  idl_const_expr_t *const_expr;
};

/* annotations */
typedef struct idl_annotation_appl_param idl_annotation_appl_param_t;
struct idl_annotation_appl_param {
  idl_node_t node;
  char *identifier;
  idl_const_expr_t *const_expr;
};

struct idl_annotation_appl {
  idl_node_t node;
  char *name;
  /* FIXME: either an expression or a list of parameters, needs work */
  idl_annotation_appl_param_t *parameters;
};

typedef struct idl_const_dcl idl_const_dcl_t;
struct idl_const_dcl {
  idl_node_t node;
  idl_const_type_t *type_spec;
  char *identifier;
  idl_const_expr_t *expression;
};

/* definitions */
typedef struct idl_module idl_module_t;
struct idl_module {
  idl_node_t node;
  char *identifier;
  idl_definition_t *definitions;
  idl_module_t *previous;
};

typedef struct idl_array_size idl_array_size_t;
struct idl_array_size {
  idl_node_t node;
  idl_const_expr_t *const_expr;
};

typedef struct idl_declarator idl_declarator_t;
struct idl_declarator {
  idl_node_t node;
  char *identifier;
  idl_array_size_t *array_sizes;
};

/* #pragma keylist directives and @key annotations can be mixed if the
   key members and ordering match. both are converted to populate the key
   member in the second pass */
typedef struct idl_member idl_member_t;
struct idl_member {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarators;
  uint32_t id; /* FIXME: for @id?, paired with flag? */
  /** non-zero if member is a key, zero otherwise. value dictates order */
  uint32_t key;
};

/**
 * Complements @id annotation and is applicable to any set containing elements
 * to which allocating a 32-bit integer identifier makes sense. It instructs
 * to automatically allocate identifiers to the elements.
 */
typedef enum idl_autoid idl_autoid_t;
enum idl_autoid {
  IDL_AUTOID_HASH, /**< Identifier computed by a hashing algorithm */
  IDL_AUTOID_SEQUENTIAL /**< Identifier computed by incrementing previous */
};

typedef enum idl_extensibility idl_extensibility_t;
enum idl_extensibility {
  /** Type is not allowed to evolve */
  IDL_EXTENSIBILITY_FINAL,
  /** Type may be complemented (elements may be appended, not reorganized) */
  IDL_EXTENSIBILITY_APPENDABLE,
  /** Type may evolve */
  IDL_EXTENSIBILITY_MUTABLE
};

typedef struct idl_struct_type idl_struct_type_t;
struct idl_struct_type {
  idl_node_t node;
  char *identifier;
  idl_member_t *members;
  idl_autoid_t autoid; /* FIXME: for @autoid?, paired with flag? */
  idl_extensibility_t extensibility; /* FIXME: for @extensibility?, paired with flag? */
};

typedef struct idl_struct_forward_dcl idl_struct_forward_dcl_t;
struct idl_struct_forward_dcl {
  idl_node_t node;
  char *identifier;
  idl_struct_type_t *struct_dcl;
};

typedef struct idl_case_label idl_case_label_t;
struct idl_case_label {
  idl_node_t node;
  idl_const_expr_t *const_expr;
};

typedef struct idl_case idl_case_t;
struct idl_case {
  idl_node_t node;
  idl_case_label_t *case_labels;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarator;
};

typedef struct idl_union_type idl_union_type_t;
struct idl_union_type {
  idl_node_t node;
  char *identifier;
  idl_switch_type_spec_t *switch_type_spec;
  idl_case_t *cases;
};

typedef struct idl_union_forward_dcl idl_union_forward_dcl_t;
struct idl_union_forward_dcl {
  idl_node_t node;
  char *identifier;
  idl_union_type_t *union_dcl;
};

typedef struct idl_enumerator idl_enumerator_t;
struct idl_enumerator {
  idl_node_t node;
  char *identifier;
  uint32_t value; /* FIXME: for @value, paired with a flag? */
};

typedef struct idl_enum_type idl_enum_type_t;
struct idl_enum_type {
  idl_node_t node;
  char *identifier;
  idl_enumerator_t *enumerators;
};

typedef struct idl_typedef idl_typedef_t;
struct idl_typedef {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarators;
};

/* syntactic sugar */
union idl_const_expr {
  idl_kind_t kind;
  idl_node_t node;
  idl_literal_t literal;
  idl_scoped_name_t scoped_name;
  idl_binary_expr_t binary_expr;
  idl_unary_expr_t unary_expr;
};

/* syntactic sugar */
union idl_const_type {
  idl_kind_t kind;
  idl_node_t node;
  idl_base_type_t base_type;
};

/* syntactic sugar */
union idl_simple_type_spec {
  idl_kind_t kind;
  idl_node_t node;
  idl_base_type_t base_type;
  idl_scoped_name_t scoped_name;
};

/* syntactic sugar */
union idl_constr_type_spec {
  idl_kind_t kind;
  idl_node_t node;
  idl_struct_type_t struct_type;
  idl_union_type_t union_type;
  idl_enum_type_t enum_type;
};

/* syntactic sugar */
union idl_type_spec {
  idl_kind_t kind;
  idl_node_t node;
  idl_base_type_t base_type;
  idl_scoped_name_t scoped_name;
  idl_sequence_type_t sequence_type;
  idl_string_type_t string_type;
  idl_struct_type_t struct_type;
  idl_union_type_t union_type;
  idl_enum_type_t enum_type;
};

/* syntactic sugar */
union idl_switch_type_spec {
  idl_kind_t kind;
  idl_node_t node;
  idl_base_type_t base_type;
  idl_scoped_name_t scoped_name;
};

/* syntactive sugar */
union idl_definition {
  idl_kind_t kind;
  idl_node_t node;
  idl_module_t module_dcl;
  idl_struct_type_t struct_dcl;
  idl_struct_forward_dcl_t struct_forward_dcl;
  idl_union_type_t union_dcl;
  idl_union_forward_dcl_t union_forward_dcl;
  idl_enum_type_t enum_dcl;
};

IDL_EXPORT bool idl_is_declaration(const void *node);
IDL_EXPORT bool idl_is_module(const void *node);
IDL_EXPORT bool idl_is_struct(const void *node);
IDL_EXPORT bool idl_is_struct_forward_dcl(const void *node);
IDL_EXPORT bool idl_is_union(const void *node);
IDL_EXPORT bool idl_is_union_forward_dcl(const void *node);
IDL_EXPORT bool idl_is_enum(const void *node);
IDL_EXPORT bool idl_is_declarator(const void *node);
IDL_EXPORT bool idl_is_enumerator(const void *node);
IDL_EXPORT bool idl_is_literal(const void *node);
IDL_EXPORT bool idl_is_integer(const void *node);
IDL_EXPORT bool idl_is_floating_pt(const void *node);

IDL_EXPORT const char *idl_identifier(const void *node);
IDL_EXPORT const char *idl_type(void *node);

IDL_EXPORT idl_const_dcl_t *idl_create_const_dcl(void);
IDL_EXPORT idl_binary_expr_t *idl_create_binary_expr(idl_kind_t kind);
IDL_EXPORT idl_unary_expr_t *idl_create_unary_expr(idl_kind_t kind);
IDL_EXPORT idl_literal_t *idl_create_integer_literal(uint64_t uint);
IDL_EXPORT idl_literal_t *idl_create_boolean_literal(bool bln);
IDL_EXPORT idl_literal_t *idl_create_string_literal(char *str);
IDL_EXPORT idl_module_t *idl_create_module(void);
IDL_EXPORT idl_base_type_t *idl_create_base_type(idl_kind_t kind);
IDL_EXPORT idl_scoped_name_t *idl_create_scoped_name(char *name);
IDL_EXPORT idl_sequence_type_t *idl_create_sequence_type(void);
IDL_EXPORT idl_string_type_t *idl_create_string_type(void);
IDL_EXPORT idl_struct_type_t *idl_create_struct(void);
IDL_EXPORT idl_member_t *idl_create_member(void);
IDL_EXPORT idl_struct_forward_dcl_t *idl_create_struct_forward_dcl(void);
IDL_EXPORT idl_union_type_t *idl_create_union(void);
IDL_EXPORT idl_case_label_t *idl_create_case_label(void);
IDL_EXPORT idl_case_t *idl_create_case(void);
IDL_EXPORT idl_union_forward_dcl_t *idl_create_union_forward_dcl(void);
IDL_EXPORT idl_enum_type_t *idl_create_enum(void);
IDL_EXPORT idl_enumerator_t *idl_create_enumerator(void);
IDL_EXPORT idl_annotation_appl_t *idl_create_annotation_appl(void);
IDL_EXPORT idl_annotation_appl_param_t *idl_create_annotation_appl_param(void);
IDL_EXPORT idl_array_size_t *idl_create_array_size(void);
IDL_EXPORT idl_declarator_t *idl_create_declarator(void);
IDL_EXPORT idl_typedef_t *idl_create_typedef(void);

IDL_EXPORT void idl_delete(void *node);

#endif /* IDL_TREE_H */
