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

#define IDL_MASK(node) (node ? ((const idl_node_t *)node)->mask : 0)

bool idl_is_declaration(const void *node)
{
  return (IDL_MASK(node) & IDL_DECL) == IDL_DECL;
}

//bool idl_is_struct_forward_dcl(const void *node)
//{
//  return IDL_MASK(node) == (IDL_DECL | IDL_FORWARD | IDL_STRUCT);
//}

bool idl_is_union(const void *node)
{
  return IDL_MASK(node) == (IDL_DECL | IDL_UNION);
}

//bool idl_is_union_forward_dcl(const void *node)
//{
//  return IDL_MASK(node) == (IDL_DECL | IDL_FORWARD | IDL_UNION);
//}

bool idl_is_enum(const void *node)
{
  return IDL_MASK(node) == (IDL_DECL | IDL_ENUM);
}

bool idl_is_enumerator(const void *node)
{
  return IDL_MASK(node) == (IDL_DECL | IDL_ENUMERATOR);
}

//bool idl_is_literal(const void *node)
//{
//  return ((const idl_node_t *)node)->kind & IDL_LITERAL;
//}
//
//bool idl_is_floating_pt(const void *node)
//{
//  return ((const idl_node_t *)node)->kind & IDL_FLOATING_PT_TYPE;
//}
//
//bool idl_is_integer(const void *node)
//{
//  return ((const idl_node_t *)node)->kind & IDL_INTEGER_TYPE;
//}

bool idl_is_module(const void *node)
{
  idl_module_t *n = (idl_module_t *)node;
  if (!n)
    return false;
  if (!(n->node.mask & IDL_MODULE))
    return false;
  assert((n->node.mask & IDL_DECL));
  return true;
}

bool idl_is_struct(const void *node)
{
  idl_struct_t *n = (idl_struct_t *)node;
  if (!n)
    return false;
  if (!(n->node.mask & IDL_STRUCT))
    return false;
  assert((n->node.mask & (IDL_DECL | IDL_TYPE)) == (IDL_DECL | IDL_TYPE));
  /* structs must have an identifier */
  assert(n->identifier);
  /* structs must have at least one member */
  assert(n->members);
  assert((n->members->node.mask & (IDL_DECL | IDL_MEMBER))
    == (IDL_DECL | IDL_MEMBER));
  return true;
}

bool idl_is_member(const void *node)
{
  idl_member_t *n = (idl_member_t *)node;
  if (!n)
    return false;
  if (!(n->node.mask & IDL_MEMBER))
    return false;
  assert(n->node.mask & IDL_DECL);
  /* members must have a parent node which is a struct */
  assert(n->node.parent);
  assert((n->node.parent->mask & (IDL_DECL | IDL_TYPE | IDL_STRUCT))
    == (IDL_DECL | IDL_TYPE | IDL_STRUCT));
  /* members must have a type specifier */
  assert(n->type_spec);
  assert(n->type_spec->mask & IDL_TYPE);
  /* members must have at least one declarator */
  assert(n->declarators);
  assert((n->declarators->node.mask & (IDL_DECL | IDL_DECLARATOR))
    == (IDL_DECL | IDL_DECLARATOR));
  return true;
}

bool idl_is_type_spec(const void *node, idl_mask_t mask)
{
  idl_node_t *n = (idl_node_t *)node;
  assert(!(mask & ~(IDL_DECL | IDL_TYPE)) ||
          (mask & IDL_CONSTR_TYPE) == IDL_CONSTR_TYPE ||
          (mask & IDL_TEMPL_TYPE) == IDL_TEMPL_TYPE ||
          (mask & IDL_BASE_TYPE) == IDL_BASE_TYPE);
  if (!n)
    return false;
  if (!(n->mask & IDL_TYPE))
    return false;
  if ((n->mask & IDL_DECL) && idl_is_module(n->parent)) {
    mask |= IDL_TYPE | IDL_DECL;
  } else {
    mask |= IDL_TYPE;
    /* type specifiers cannot have sibling nodes unless its also a declared
       type and declared in a module */
    assert(!n->previous);
    assert(!n->next);
    /* type specifiers must also have a parent node */
    assert(n->parent);
  }
  return (n->mask & mask) == mask;
}

bool idl_is_declarator(const void *node)
{
  idl_declarator_t *n = (idl_node_t *)node;
  if (!n)
    return false;
  if (!(n->node.mask & IDL_DECLARATOR))
    return false;
  assert(n->node.mask & IDL_DECL);
  /* declarators must have an identifier */
  assert(n->identifier);
  /* declarators must have a parent node */
  assert(n->node.parent);
  /* declarators can have sizes */
  if (n->const_expr) {
    assert((n->const_expr->mask & IDL_EXPR) == IDL_EXPR ||
           (n->const_expr->mask & IDL_CONST) == IDL_CONST);
  }
  return true;
}

const char *idl_identifier(const void *node)
{
  if (!idl_is_declaration(node))
    return NULL;
  if (idl_is_module(node))
    return ((const idl_module_t *)node)->identifier;
  if (idl_is_struct(node))
    return ((const idl_struct_t *)node)->identifier;
  if (idl_is_union(node))
    return ((const idl_union_t *)node)->identifier;
  if (idl_is_enum(node))
    return ((const idl_enum_t *)node)->identifier;
  if (idl_is_declarator(node))
    return ((const idl_declarator_t *)node)->identifier;
  if (idl_is_enumerator(node))
    return ((const idl_enumerator_t *)node)->identifier;
  return NULL;
}

const char *idl_label(const void *node)
{
  assert(node);

#if 0
  switch (((idl_node_t *)node)->mask) {
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
#endif

  return "unknown";
}

const idl_location_t *idl_location(const void *node)
{
  return &((const idl_node_t *)node)->location;
}

void *idl_parent(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* non-declarators must have a parent */
  assert((n->mask & IDL_DECL) || n->parent);
  return (idl_node_t *)n->parent;
}

void *idl_previous(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* declarators can have siblings */
  if ((n->mask & (IDL_DECL)))
    return n->previous;
  /* as do expressions (or constants) if specifying array sizes */
  if ((n->mask & (IDL_EXPR | IDL_CONST)) && idl_is_declarator(n->parent))
    return n->previous;
  assert(!n->previous);
  assert(!n->next);
  return NULL;
}

void *idl_next(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* declarators can have siblings */
  if ((n->mask & (IDL_DECL)))
    return n->next;
  /* as do expressions (or constants) if specifying array sizes */
  if ((n->mask & (IDL_EXPR | IDL_CONST)) && idl_is_declarator(n->parent))
    return n->next;
  assert(!n->previous);
  assert(!n->next);
  return NULL;
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
  idl_mask_t mask,
  idl_print_t printer,
  idl_delete_t destructor)
{
  idl_node_t *node;
  assert(size >= sizeof(*node));
  if ((node = calloc(1, size))) {
    node->mask = mask;
    node->printer = printer;
    node->destructor = destructor;
    node->weight = 1;
  }
  return node;
}

idl_literal_t *idl_create_integer_literal(uint64_t ullng)
{
  idl_literal_t *node;
  if ((node = make_node(sizeof(*node), IDL_INTEGER_LITERAL, 0, 0)))
    node->value.ullng = ullng;
  return node;
}

idl_literal_t *idl_create_boolean_literal(bool bln)
{
  idl_literal_t *node;
  if ((node = make_node(sizeof(*node), IDL_BOOLEAN_LITERAL, 0, 0)))
    node->value.bln = bln;
  return node;
}

idl_literal_t *idl_create_string_literal(char *str)
{
  idl_literal_t *node;
  if ((node = make_node(sizeof(*node), IDL_STRING_LITERAL, 0, 0)))
    node->value.str = str;
  return node;
}

idl_binary_expr_t *idl_create_binary_expr(idl_mask_t kind)
{
  return make_node(sizeof(idl_binary_expr_t), IDL_BINARY_EXPR | kind, 0, 0);
}

idl_unary_expr_t *idl_create_unary_expr(idl_mask_t kind)
{
  return make_node(sizeof(idl_unary_expr_t), IDL_UNARY_EXPR | kind, 0, 0);
}

idl_const_t *idl_create_const(void)
{
  return make_node(sizeof(idl_const_t), IDL_DECL | IDL_CONST, 0, 0);
}

idl_module_t *idl_create_module(void)
{
  return make_node(sizeof(idl_module_t), IDL_DECL | IDL_MODULE, 0, 0);
}

idl_base_type_t *idl_create_base_type(idl_mask_t type)
{
  return make_node(sizeof(idl_base_type_t), IDL_TYPE | type, 0, 0);
}

//idl_scoped_name_t *idl_create_scoped_name(char *name)
//{
//  idl_scoped_name_t *node = make_node(sizeof(idl_scoped_name_t), IDL_SCOPED_NAME, 0, 0);
//  if (node)
//    node->name = name;
//  return node;
//}

idl_variant_t *idl_create_const_ullng(uint64_t ullng)
{
  idl_variant_t *var;
  if ((var = make_node(sizeof(*var), IDL_CONST | IDL_ULLONG, 0, 0)))
    var->value.ullng = ullng;
  return var;
}

idl_sequence_t *idl_create_sequence(void)
{
  return make_node(sizeof(idl_sequence_t), IDL_TYPE | IDL_SEQUENCE, 0, 0);
}

idl_string_t *idl_create_string(void)
{
  return make_node(sizeof(idl_string_t), IDL_TYPE | IDL_STRING, 0, 0);
}

idl_struct_t *idl_create_struct(void)
{
  return make_node(sizeof(idl_struct_t), IDL_DECL | IDL_TYPE | IDL_STRUCT, 0, 0);
}

idl_member_t *idl_create_member(void)
{
  return make_node(sizeof(idl_member_t), IDL_DECL | IDL_MEMBER, 0, 0);
}

idl_union_t *idl_create_union(void)
{
  return make_node(sizeof(idl_union_t), IDL_DECL | IDL_TYPE | IDL_UNION, 0, 0);
}

idl_case_label_t *idl_create_case_label(void)
{
  return make_node(sizeof(idl_case_label_t), IDL_DECL | IDL_CASE_LABEL, 0, 0);
}

idl_case_t *idl_create_case(void)
{
  return make_node(sizeof(idl_case_t), IDL_DECL | IDL_CASE, 0, 0);
}

idl_forward_t *idl_create_forward(idl_mask_t mask)
{
  assert((mask & IDL_STRUCT) == IDL_STRUCT || (mask & IDL_UNION) == IDL_UNION);
  return make_node(sizeof(idl_forward_t), IDL_DECL | IDL_FORWARD | mask, 0, 0);
}

idl_enum_t *idl_create_enum(void)
{
  return make_node(sizeof(idl_enum_t), IDL_DECL | IDL_TYPE | IDL_ENUM, 0, 0);
}

idl_enumerator_t *idl_create_enumerator(void)
{
  return make_node(sizeof(idl_enumerator_t), IDL_DECL | IDL_ENUMERATOR, 0, 0);
}

idl_annotation_appl_param_t *idl_create_annotation_appl_param(void)
{
  return make_node(sizeof(idl_annotation_appl_param_t), IDL_DECL | IDL_ANNOTATION_APPL_PARAM, 0, 0);
}

idl_annotation_appl_t *idl_create_annotation_appl(void)
{
  return make_node(sizeof(idl_annotation_appl_t), IDL_DECL | IDL_ANNOTATION_APPL, 0, 0);
}

idl_typedef_t *idl_create_typedef(void)
{
  return make_node(sizeof(idl_typedef_t), IDL_DECL | IDL_TYPEDEF, 0, 0);
}

idl_declarator_t *idl_create_declarator(void)
{
  return make_node(sizeof(idl_declarator_t), IDL_DECL | IDL_DECLARATOR, 0, 0);
}
