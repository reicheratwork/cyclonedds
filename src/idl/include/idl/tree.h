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

#define IDL_KEYLIST (1llu<<38)
#define IDL_KEY (1llu<<37)
#define IDL_INHERIT_SPEC (1llu<<36)
#define IDL_SWITCH_TYPE_SPEC (1llu<<35)
/* declarations */
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

/* bits 16 - 19 are reserved for expressions (not exposed in tree) */

typedef enum idl_type idl_type_t;
enum idl_type {
  IDL_NULL = 0u,
  IDL_TYPEDEF = (1llu<<15),
  /* constructed types */
  IDL_STRUCT = (1u<<14),
  IDL_UNION = (1u<<13),
  IDL_ENUM = (1u<<12),
  /* template types */
#define IDL_TEMPL_TYPE (1llu<<11)
  IDL_SEQUENCE = (IDL_TEMPL_TYPE | 1u),
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
struct idl_declaration;
struct idl_annotation_appl;

typedef void idl_const_expr_t;
typedef void idl_definition_t;
typedef void idl_type_spec_t;

typedef uint64_t idl_mask_t;
typedef void(*idl_delete_t)(void *);
typedef const void *(*idl_iterate_t)(const void *root, const void *node);

typedef struct idl_node idl_node_t;
struct idl_node {
  idl_symbol_t symbol;
  idl_mask_t mask;
  idl_delete_t destructor;
  idl_iterate_t iterator;
  int32_t references;
  struct idl_annotation_appl *annotations;
  const struct idl_scope *scope; /**< enclosing scope */
  idl_node_t *parent;
  idl_node_t *previous, *next;
};

typedef struct idl_path idl_path_t;
struct idl_path {
  size_t length;
  const idl_node_t **nodes;
};

typedef struct idl_id idl_id_t;
struct idl_id {
  enum {
    IDL_AUTOID, /**< value assigned automatically */
    IDL_ID, /**< value assigned by @id */
    IDL_HASHID /**< value assigned by @hashid */
  } annotation;
  uint32_t value;
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

/* constructed types are not considered @nested types by default, implicitly
   stating the intent to use it as a topic. extensible and dynamic topic types
   added @default_nested and @topic to explicitly state the intent to use a
   type as a topic. for ease of use, the sum-total is provided as a single
   boolean */
typedef struct idl_nested idl_nested_t;
struct idl_nested {
  enum {
    IDL_DEFAULT_NESTED, /**< implicit through @default_nested (or not) */
    IDL_NESTED, /**< annotated with @nested */
    IDL_TOPIC /**< annotated with @topic (overrides @nested) */
  } annotation;
  bool value;
};

/* nullable boolean, like Boolean object in e.g. JavaScript or Java */
typedef enum idl_boolean idl_boolean_t;
enum idl_boolean {
  IDL_DEFAULT,
  IDL_FALSE,
  IDL_TRUE
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
  /* metadata */
  idl_boolean_t default_nested;
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
  /* metadata */
  idl_boolean_t key;
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
  /* store entire field name as context matters for embedded struct fields */
  idl_field_name_t *field_name;
};

typedef struct idl_keylist idl_keylist_t;
struct idl_keylist {
  idl_node_t node;
  idl_key_t *keys;
};

typedef struct idl_struct idl_struct_t;
struct idl_struct {
  idl_node_t node;
  idl_inherit_spec_t *inherit_spec;
  struct idl_name *name;
  idl_member_t *members;
  /* metadata */
  idl_nested_t nested; /**< if type is a topic (sum-total of annotations) */
  idl_keylist_t *keylist; /**< if type is a topic (#pragma keylist) */
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
  /* metadata */
  idl_boolean_t key;
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

#if 0
typedef struct idl_forward idl_forward_t;
struct idl_forward {
  idl_node_t node;
  struct idl_name *name;
  void *constr_type_decl;
};
#endif

typedef struct idl_typedef idl_typedef_t;
struct idl_typedef {
  idl_node_t node;
  void *type_spec;
  idl_declarator_t *declarators;
};

struct idl_pstate;
typedef idl_retcode_t (*idl_annotation_callback_t)(
  struct idl_pstate *,
  struct idl_annotation_appl *,
  idl_node_t *);

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

/**
 * @brief return if node is a declaration
 *
 * a node is a declaration if it introduces an identifier into a scope. hence,
 * declarations have a scope by definition. @idl_scope can be used to retrieve
 * the enclosing scope
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @return true if node is a declaration, false otherwise
 */
IDL_EXPORT bool idl_is_declaration(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a module
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a module, false otherwise
 */
IDL_EXPORT bool idl_is_module(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a type specifier
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a type specifier, false otherwise
 */
IDL_EXPORT bool idl_is_type_spec(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is an array, i.e. complex declarator
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node of kind declarator
 *
 * @returns true if node is a complex declarator, false otherwise
 */
IDL_EXPORT bool idl_is_array(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a typedef
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a typedef, false otherwise
 */
IDL_EXPORT bool idl_is_typedef(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node a constructed type
 *
 * return if node is a constructed type, e.g. struct, union or enum
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a constructed type, false otherwise
 */
IDL_EXPORT bool idl_is_constr_type(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a struct
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a struct, false otherwise
 */
IDL_EXPORT bool idl_is_struct(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a member
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a member, false otherwise
 */
IDL_EXPORT bool idl_is_member(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a union
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a union, false otherwise
 */
IDL_EXPORT bool idl_is_union(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a union case
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a union case, false otherwise
 */
IDL_EXPORT bool idl_is_case(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is the default union case
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is the default union case, false otherwise
 */
IDL_EXPORT bool idl_is_default_case(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is a case label
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is a union case label, false otherwise
 */
IDL_EXPORT bool idl_is_case_label(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if node is an enum
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns true if node is an enum, false otherwise
 */
IDL_EXPORT bool idl_is_enum(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_enumerator(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_templ_type(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_sequence(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_string(
  const struct idl_pstate *pstate, const void *node);

// can be called on template types
// >> so strings and sequences for now!
IDL_EXPORT bool idl_is_bounded(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_floating_pt_type(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_integer_type(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_base_type(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_declarator(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_const(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_constval(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_annotation_member(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_annotation_appl(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_inherit_spec(
  const struct idl_pstate *pstate, const void *node);

IDL_EXPORT bool idl_is_switch_type_spec(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief Return if node is a topic
 *
 * IDL version agnostic function to determine if @node is a topic.
 *
 * @param[in]  pstate  Processor state
 * @param[in]  node    Tree node
 *
 * @returns true if node is a topic, false otherwise
 */
IDL_EXPORT bool
idl_is_topic(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return if nested node is a topic key
 *
 * IDL version agnostic function to determine if nested node pointed to by
 * @path is a key in topic pointed to by @node. paths constructed by
 * @idl_visit can be used for convience
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node that denotes a topic
 * @param[in]  path    relative path from topic to key
 *
 * @returns true if path points to a topic key, false otherwise
 */
IDL_EXPORT bool
idl_is_topic_key(
  const struct idl_pstate *pstate, const void *node, const idl_path_t *path);


/* accessors */
IDL_EXPORT idl_mask_t idl_mask(const void *node);
IDL_EXPORT size_t idl_degree(const void *node);

IDL_EXPORT idl_type_t idl_type(const void *node);
IDL_EXPORT const char *idl_identifier(const void *node);
IDL_EXPORT const idl_name_t *idl_name(const void *node);

/**
 * @brief Return type specifier for node
 *
 * Retrieve type specifier for instances, elements, sequences and aliases.
 * Accepts declarators for struct members, union elements and type
 * definitions for convenience.
 *
 * @param[in]  pstate  Processor state
 * @param[in]  node    Tree node
 *
 * @returns type specifier if applicable, NULL otherwise
 */
IDL_EXPORT idl_type_spec_t *idl_type_spec(
  const struct idl_pstate *pstate, const void *node);

// can be called on declarators
IDL_EXPORT uint32_t idl_array_size(const void *node);


/* navigation */

/**
 * @brief return ancestor node
 *
 * return ancestor node @levels up. useful for retrieving e.g. the struct for
 * a member declarator
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 * @param[in]  levels  number of levels to go up
 *
 * @returns ancestor node or NULL if ancestor node at @level does not exist
 */
IDL_EXPORT void *idl_ancestor(
  const struct idl_pstate *pstate, const void *node, size_t levels);

/**
 * @brief return parent node
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns parent node or NULL if there is no parent node
 */
IDL_EXPORT void *idl_parent(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return next sibling node
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns next sibling node or NULL if there is no next sibling node
 */
IDL_EXPORT void *idl_next(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief return previous sibling node
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    tree node
 *
 * @returns previous sibling node or NULL if there is no previous sibling node
 */
IDL_EXPORT void *idl_previous(
  const struct idl_pstate *pstate, const void *node);

/**
 * @brief iterate over nodes contained by node
 *
 * iterate over nodes contained by @root. retrieve first node by passing NULL
 * for @node, pass node retrieved last on consecutive calls.
 *
 * @param[in]  pstate  processor state
 * @param[in]  root    (sub)root, e.g. module, struct or member
 * @param[in]  node    node enclosed by root
 *
 * @returns next node contained by (sub)root or NULL if there are no more
 */
IDL_EXPORT void *idl_iterate(
  const struct idl_pstate *pstate, const void *root, const void *node);

/**
 * @brief unalias type specifier
 *
 * @param[in]  pstate  processor state
 * @param[in]  node    type specifier
 * @param[in]  flags   flags
 *
 * @returns unaliased type specifier if applicable, NULL otherwise
 */
IDL_EXPORT void *idl_unalias(
  const struct idl_pstate *pstate, const void *node, uint32_t flags);

#define IDL_UNALIAS_IGNORE_ARRAY (1u<<0) /**< ignore array declarators */

#endif /* IDL_TREE_H */
