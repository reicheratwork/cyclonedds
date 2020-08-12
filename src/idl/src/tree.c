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
#include <stdio.h>
#include <stdlib.h>

#include "idl/tree.h"

// FIXME: use X macro trick to generate constructors with the preprocessor

bool idl_is_declaration(const void *node)
{
  switch (((const idl_node_t *)node)->kind) {
    case IDL_TYPEDEF:
    case IDL_DECLARATOR:
    case IDL_MODULE:
    case IDL_MEMBER:
    case IDL_ENUMERATOR:
      return true;
    default:
      break;
  }
  return ((const idl_node_t *)node)->kind & IDL_CONSTR_TYPE;
}

bool idl_is_module(const void *node)
{
  return (((const idl_node_t *)node)->kind & IDL_MODULE);
}

bool idl_is_struct(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_STRUCT_TYPE);
}

bool idl_is_struct_forward_dcl(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_STRUCT_TYPE | IDL_FORWARD_DCL);
}

bool idl_is_union(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_UNION_TYPE);
}

bool idl_is_union_forward_dcl(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_UNION_TYPE | IDL_FORWARD_DCL);
}

bool idl_is_enum(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_ENUM_TYPE);
}

bool idl_is_declarator(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_DECLARATOR);
}

bool idl_is_enumerator(const void *node)
{
  return ((const idl_node_t *)node)->kind == (IDL_ENUMERATOR);
}

bool idl_is_literal(const void *node)
{
  return ((const idl_node_t *)node)->kind & IDL_LITERAL;
}

bool idl_is_floating_pt(const void *node)
{
  return ((const idl_node_t *)node)->kind & IDL_FLOATING_PT_TYPE;
}

bool idl_is_integer(const void *node)
{
  return ((const idl_node_t *)node)->kind & IDL_INTEGER_TYPE;
}

const char *idl_identifier(const void *node)
{
  if (!idl_is_declaration(node))
    return NULL;
  if (idl_is_module(node))
    return ((const idl_module_t *)node)->identifier;
  if (idl_is_struct(node))
    return ((const idl_struct_type_t *)node)->identifier;
  if (idl_is_struct_forward_dcl(node))
    return ((const idl_struct_forward_dcl_t *)node)->identifier;
  if (idl_is_union(node))
    return ((const idl_union_type_t *)node)->identifier;
  if (idl_is_union_forward_dcl(node))
    return ((const idl_union_forward_dcl_t *)node)->identifier;
  if (idl_is_enum(node))
    return ((const idl_enum_type_t *)node)->identifier;
  if (idl_is_declarator(node))
    return ((const idl_declarator_t *)node)->identifier;
  if (idl_is_enumerator(node))
    return ((const idl_enumerator_t *)node)->identifier;
  return NULL;
}

const char *idl_type(void *node)
{
  assert(node);

  switch (((idl_node_t *)node)->kind) {
    case IDL_INT8:
      return "int8";
    case IDL_UINT8:
      return "uint8";
    case IDL_INT16:
      return "int16";
    case IDL_UINT16:
      return "uint16";
    case IDL_INT32:
      return "int32";
    case IDL_UINT32:
      return "uint32";
    case IDL_INT64:
      return "int64";
    case IDL_UINT64:
      return "uint64";
    default:
      break;
  }

  return "unknown";
}

void idl_delete(void *node)
{
  // FIXME: implement
  (void)node;
  return;
}

static void *
make_node(
  size_t size,
  idl_kind_t kind,
  idl_print_t printer,
  idl_delete_t destructor)
{
  idl_node_t *node;
  assert(size >= sizeof(node));
  if ((node = calloc(1, size))) {
    node->kind = kind;
    node->printer = printer;
    node->destructor = destructor;
  }
  return node;
}

idl_literal_t *idl_create_integer_literal(uint64_t unsigned_int)
{
  idl_kind_t kind = unsigned_int > UINT32_MAX ? IDL_UINT64 : IDL_UINT32;
  idl_literal_t *node;
  if ((node = make_node(sizeof(*node), IDL_LITERAL | kind, 0, 0))) {
    node->variant.kind = kind;
    node->variant.value.unsigned_int = unsigned_int;
  }
  return node;
}

idl_literal_t *idl_create_boolean_literal(bool boolean)
{
  idl_literal_t *node;
  if ((node = make_node(sizeof(*node), IDL_LITERAL | IDL_BOOL, 0, 0))) {
    node->variant.kind = IDL_BOOL;
    node->variant.value.boolean = boolean;
  }
  return node;
}

idl_literal_t *idl_create_string_literal(char *string)
{
  idl_literal_t *node;
  if ((node = make_node(sizeof(*node), IDL_LITERAL | IDL_STRING_TYPE, 0, 0))) {
    node->variant.kind = IDL_STRING_TYPE;
    node->variant.value.string = string;
  }
  return node;
}

idl_binary_expr_t *idl_create_binary_expr(idl_kind_t kind)
{
  return make_node(sizeof(idl_binary_expr_t), kind, 0, 0);
}

idl_unary_expr_t *idl_create_unary_expr(idl_kind_t kind)
{
  return make_node(sizeof(idl_unary_expr_t), kind, 0, 0);
}

idl_const_dcl_t *idl_create_const_dcl(void)
{
  return make_node(sizeof(idl_const_dcl_t), IDL_CONST_DCL, 0, 0);
}

idl_module_t *idl_create_module(void)
{
  return make_node(sizeof(idl_module_t), IDL_MODULE, 0, 0);
}

idl_base_type_t *idl_create_base_type(uint32_t type)
{
  return make_node(sizeof(idl_base_type_t), type, 0, 0);
}

idl_scoped_name_t *idl_create_scoped_name(char *name)
{
  idl_scoped_name_t *node = make_node(sizeof(idl_scoped_name_t), IDL_SCOPED_NAME, 0, 0);
  if (node)
    node->name = name;
  return node;
}

idl_sequence_type_t *idl_create_sequence_type(void)
{
  return make_node(sizeof(idl_sequence_type_t), IDL_SEQUENCE_TYPE, 0, 0);
}

idl_string_type_t *idl_create_string_type(void)
{
  return make_node(sizeof(idl_string_type_t), IDL_STRING_TYPE, 0, 0);
}

idl_struct_type_t *idl_create_struct(void)
{
  return make_node(sizeof(idl_struct_type_t), IDL_STRUCT_TYPE, 0, 0);
}

idl_member_t *idl_create_member(void)
{
  return make_node(sizeof(idl_member_t), IDL_MEMBER, 0, 0);
}

idl_struct_forward_dcl_t *idl_create_struct_forward_dcl(void)
{
  return make_node(sizeof(idl_struct_forward_dcl_t), IDL_STRUCT_TYPE | IDL_FORWARD_DCL, 0, 0);
}

idl_union_type_t *idl_create_union(void)
{
  return make_node(sizeof(idl_union_type_t), IDL_UNION_TYPE, 0, 0);
}

idl_case_label_t *idl_create_case_label(void)
{
  return make_node(sizeof(idl_case_label_t), IDL_CASE_LABEL, 0, 0);
}

idl_case_t *idl_create_case(void)
{
  return make_node(sizeof(idl_case_t), IDL_CASE, 0, 0);
}

idl_union_forward_dcl_t *idl_create_union_forward_dcl(void)
{
  return make_node(sizeof(idl_union_forward_dcl_t), IDL_UNION_TYPE | IDL_FORWARD_DCL, 0, 0);
}

idl_enum_type_t *idl_create_enum(void)
{
  return make_node(sizeof(idl_enum_type_t), IDL_ENUM_TYPE, 0, 0);
}

idl_enumerator_t *idl_create_enumerator(void)
{
  return make_node(sizeof(idl_enumerator_t), IDL_ENUMERATOR, 0, 0);
}

idl_annotation_appl_param_t *idl_create_annotation_appl_param(void)
{
  return make_node(sizeof(idl_annotation_appl_param_t), IDL_ANNOTATION_APPL_PARAM, 0, 0);
}

idl_annotation_appl_t *idl_create_annotation_appl(void)
{
  return make_node(sizeof(idl_annotation_appl_t), IDL_ANNOTATION_APPL, 0, 0);
}

idl_array_size_t *idl_create_array_size(void)
{
  return make_node(sizeof(idl_array_size_t), IDL_ARRAY_SIZE, 0, 0);
}

idl_typedef_t *idl_create_typedef(void)
{
  return make_node(sizeof(idl_typedef_t), IDL_TYPEDEF, 0, 0);
}

idl_declarator_t *idl_create_declarator(void)
{
  return make_node(sizeof(idl_declarator_t), IDL_DECLARATOR, 0, 0);
}
