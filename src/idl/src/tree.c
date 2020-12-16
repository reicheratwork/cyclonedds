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
#include <string.h>
#include <math.h>

#include "expression.h"
#include "file.h" /* for ssize_t on Windows */
#include "tree.h"
#include "scope.h"
#include "symbol.h"

void *idl_push_node(void *list, void *node)
{
  idl_node_t *last;

  if (!node)
    return list;
  if (!list)
    return node;

  for (last = list; last != node && last->next; last = last->next) ;
  assert(last != node);

  last->next = node;
  ((idl_node_t *)node)->previous = last;
  return list;
}

void *idl_reference_node(void *node)
{
  if (node)
    ((idl_node_t *)node)->references++;
  return node;
}

void *idl_unreference_node(void *ptr)
{
  idl_node_t *node = ptr;
  if (!node)
    return NULL;
  node->references--;
  assert(node->references >= 0);
  if (node->references == 0) {
    idl_node_t *previous, *next;
    previous = node->previous;
    next = node->next;
    if (previous)
      previous->next = next;
    if (next)
      next->previous = previous;
    idl_delete_node((idl_node_t *)node->annotations);
    if (node->symbol.destructor)
      node->destructor(node);
    return next;
  }
  return node;
}

const idl_name_t *idl_name(const void *node)
{
  if (idl_is_masked(node, IDL_DECLARATION)) {
    if (idl_is_masked(node, IDL_MODULE))
      return ((const idl_module_t *)node)->name;
    if (idl_is_masked(node, IDL_FORWARD))
      return ((const idl_forward_t *)node)->name;
    if (idl_is_masked(node, IDL_STRUCT))
      return ((const idl_struct_t *)node)->name;
    if (idl_is_masked(node, IDL_UNION))
      return ((const idl_union_t *)node)->name;
    if (idl_is_masked(node, IDL_ENUM))
      return ((const idl_enum_t *)node)->name;
    if (idl_is_masked(node, IDL_ENUMERATOR))
      return ((const idl_enumerator_t *)node)->name;
    if (idl_is_masked(node, IDL_DECLARATOR))
      return ((const idl_declarator_t *)node)->name;
    if (idl_is_masked(node, IDL_CONST))
      return ((const idl_const_t *)node)->name;
    if (idl_is_masked(node, IDL_ANNOTATION))
      return ((const idl_annotation_t *)node)->name;
    if (idl_is_masked(node, IDL_ANNOTATION_MEMBER))
      return ((const idl_annotation_member_t *)node)->declarator->name;
  }
  return NULL;
}

const char *idl_identifier(const void *node)
{
  const idl_name_t *name = idl_name(node);
  return name ? name->identifier : NULL;
}

void *idl_unalias(const void *node)
{
  idl_node_t *n = (idl_node_t *)node;

  while (n && idl_is_masked(n, IDL_TYPEDEF)) {
    n = ((idl_typedef_t *)n)->type_spec;
  }

  return n;
}

idl_scope_t *idl_scope(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;

  if (!n) {
    return NULL;
  } else if (idl_is_masked(n, IDL_DECLARATION)) {
    /* declarations must have a scope */
    assert(n->scope);
    return (idl_scope_t *)n->scope;
  }
  return NULL;
}

void *idl_parent(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* non-declarators must have a parent */
  assert(idl_is_masked(n, IDL_DECLARATION) || n->parent);
  return ((idl_node_t *)n)->parent;
}

void *idl_previous(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* declarators can have siblings */
  if (idl_is_masked(n, IDL_DECLARATION))
    return n->previous;
  /* as do expressions (or constants) if specifying array sizes */
  if (idl_is_masked(n, IDL_CONST) && idl_is_declarator(n->parent))
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
  /* declaration can have siblings */
  if (idl_is_masked(n, IDL_DECLARATION))
    return n->next;
  /* as do expressions (or constants) if specifying array sizes */
  if (idl_is_masked(n, IDL_CONST) && idl_is_declarator(n->parent))
    return n->next;
  assert(!n->previous);
  assert(!n->next);
  return n->next;
}

size_t idl_length(const void *node)
{
  size_t len = 0;
  for (const idl_node_t *n = node; n; n = n->next)
    len++;
  return len;
}

idl_type_t idl_type(const void *node)
{
  idl_mask_t mask = idl_mask(node) & (IDL_TYPEDEF|(IDL_CONSTR_TYPE - 1));

  switch (mask) {
    case IDL_TYPEDEF:
    /* constructed types */
    case IDL_STRUCT:
    case IDL_UNION:
    case IDL_ENUM:
    /* template types */
    case IDL_SEQUENCE:
    case IDL_STRING:
    case IDL_WSTRING:
    case IDL_FIXED_PT:
    /* simple types */
    /* miscellaneous base types */
    case IDL_CHAR:
    case IDL_WCHAR:
    case IDL_BOOL:
    case IDL_OCTET:
    case IDL_ANY:
    /* integer types */
    case IDL_SHORT:
    case IDL_USHORT:
    case IDL_LONG:
    case IDL_ULONG:
    case IDL_LLONG:
    case IDL_ULLONG:
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_INT16:
    case IDL_UINT16:
    case IDL_INT32:
    case IDL_UINT32:
    case IDL_INT64:
    case IDL_UINT64:
    /* floating point types */
    case IDL_FLOAT:
    case IDL_DOUBLE:
    case IDL_LDOUBLE:
      return (idl_type_t)mask;
    default:
      break;
  }

  return IDL_NULL;
}

static void delete_node(void *node)
{
  idl_delete_node(node);
}

static idl_retcode_t
create_node(
  idl_pstate_t *pstate,
  size_t size,
  idl_mask_t mask,
  const idl_location_t *location,
  idl_delete_t destructor,
  void *nodep)
{
  idl_node_t *node;

  (void)pstate;
  assert(size >= sizeof(*node));
  if (!(node = calloc(1, size)))
    return IDL_RETCODE_OUT_OF_MEMORY;
  if (mask & IDL_DECLARATION)
    node->scope = pstate->scope;
  node->symbol.mask = mask;
  node->symbol.location = *location;
  node->symbol.destructor = &delete_node;
  node->destructor = destructor;
  node->references = 1;

  *((idl_node_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

static void *delete_type_spec(void *node, idl_type_spec_t *type_spec)
{
  void *next;
  if (!type_spec)
    return NULL;
  if (((idl_node_t *)type_spec)->parent != node) {
    next = idl_unreference_node(type_spec);
    assert(next == type_spec);
  } else {
    next = idl_delete_node(type_spec);
    assert(next == NULL);
  }
  return next;
}

static void *delete_const_expr(void *node, idl_const_expr_t *const_expr)
{
  void *next;
  if (!const_expr)
    return NULL;
  if (((idl_node_t *)const_expr)->parent != node) {
    next = idl_unreference_node(const_expr);
    assert(next == const_expr);
  } else {
    next = idl_delete_node(const_expr);
    assert(next == NULL);
  }
  return next;
}

void *idl_delete_node(void *ptr)
{
  idl_node_t *next, *node = ptr;
  if (!node)
    return NULL;
  if ((next = node->next))
    next = idl_delete_node(node->next);
  node->references--;
  assert(node->references >= 0);
  if (node->references == 0) {
    idl_node_t *previous = node->previous;
    if (previous)
      previous->next = next;
    if (next)
      next->previous = next;
    idl_delete_node((idl_node_t *)node->annotations);
    /* self-destruct */
    if (node->destructor)
      node->destructor(node);
    return next;
  }
  return node;
}

bool idl_is_declaration(const void *node)
{
  return idl_is_masked(node, IDL_DECLARATION);
}

bool idl_is_module(const void *node)
{
  const idl_module_t *n = (const idl_module_t *)node;
  if (!idl_is_masked(n, IDL_MODULE))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_MODULE));
  /* modules must have an identifier */
  assert(n->name->identifier);
  /* modules must have at least on definition */
  assert(idl_is_masked(n->definitions, IDL_DECLARATION));
  return true;
}

static void delete_module(void *ptr)
{
  idl_module_t *node = (idl_module_t *)ptr;
  idl_delete_node(node->definitions);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_finalize_module(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_module_t *node,
  idl_definition_t *definitions)
{
  idl_exit_scope(state);
  node->node.symbol.location.last = location->last;
  node->definitions = definitions;
  for (idl_node_t *n = (idl_node_t *)definitions; n; n = n->next) {
    assert(!n->parent);
    n->parent = (idl_node_t *)node;
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_module(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_module_t *node;
  const idl_module_t *previous = NULL;
  idl_scope_t *scope;
  idl_declaration_t *decl;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_MODULE;
  static const enum idl_declaration_kind kind = IDL_MODULE_DECLARATION;

  /* an identifier declaring a module is considered to be defined by its
     first occurence in a scope. subsequent occurrences of a module
     declaration with the same identifier within the same scope reopens the
     module and hence its scope, allowing additional definitions to be added
     to it */
  decl = idl_find(pstate, NULL, name, 0u);
  if (decl && idl_is_masked(decl->node, IDL_MODULE))
    previous = (const idl_module_t *)decl->node;

  if ((ret = idl_create_scope(pstate, IDL_MODULE_SCOPE, name, &scope)))
    goto err_scope;
  if ((ret = create_node(pstate, size, mask, location, &delete_module, &node)))
    goto err_node;
  node->name = name;
  if ((ret = idl_declare(pstate, kind, name, node, scope, NULL)))
    goto err_declare;
  node->previous = previous;

  idl_enter_scope(pstate, scope);
  *((idl_module_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  idl_delete_scope(scope);
err_scope:
  return ret;
}

bool idl_is_const(const void *node)
{
  const idl_const_t *n = (const idl_const_t *)node;
  if (!idl_is_masked(n, IDL_CONST|IDL_DECLARATION))
    return false;
  assert(idl_is_base_type(n->type_spec) ||
         idl_is_string(n->type_spec) ||
         idl_is_enumerator(n->type_spec));
  assert(n->const_expr);
  return true;
}

static void delete_const(void *ptr)
{
  idl_const_t *node = (idl_const_t *)ptr;
  delete_type_spec(node, node->type_spec);
  delete_const_expr(node, node->const_expr);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_const(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_name_t *name,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_const_t *node;
  idl_const_expr_t *constval = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_CONST|IDL_DECLARATION;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  assert(idl_is_masked(type_spec, IDL_BASE_TYPE) ||
         idl_is_masked(type_spec, IDL_ENUM) ||
         idl_is_masked(type_spec, IDL_STRING));

  if ((ret = create_node(pstate, size, mask, location, &delete_const, &node)))
    goto err_node;
  node->name = name;
  /* evaluate constant expression */
  assert(idl_is_masked(type_spec, IDL_TYPE));
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t*)node;
  if ((ret = idl_evaluate(pstate, const_expr, idl_type(type_spec), &constval)))
    goto err_evaluate;
  node->const_expr = constval;
  if (!idl_scope(constval))
    ((idl_node_t *)constval)->parent = (idl_node_t*)node;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_const_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
err_evaluate:
  free(node);
err_node:
  return ret;
}

bool idl_is_templ_type(const void *node)
{
  if (!idl_is_masked(node, IDL_TEMPL_TYPE))
    return false;
  assert(idl_is_masked(node, IDL_STRING|IDL_TYPE) ||
         idl_is_masked(node, IDL_SEQUENCE|IDL_TYPE));
  return true;
}

bool idl_is_sequence(const void *node)
{
  if (!idl_is_masked(node, IDL_SEQUENCE))
    return false;
  assert(idl_is_masked(node, IDL_TYPE));
  return true;
}

static void delete_sequence(void *ptr)
{
  idl_sequence_t *node = (idl_sequence_t *)ptr;
  delete_type_spec(node, node->type_spec);
  free(node);
}

idl_retcode_t
idl_create_sequence(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_constval_t *constval,
  void *nodep)
{
  idl_retcode_t ret;
  idl_sequence_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_SEQUENCE|IDL_TYPE;
  static const idl_delete_t dtor = &delete_sequence;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    goto err_node;
  assert(idl_is_masked(type_spec, IDL_TYPE));
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t*)node;
  if (constval) {
    assert(idl_type(constval) == IDL_ULONG);
    node->maximum = constval->value.uint32;
    idl_delete_node(constval);
  } else {
    node->maximum = 0u;
  }
  *((idl_sequence_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

bool idl_is_string(const void *node)
{
  return idl_is_masked(node, IDL_TYPE|IDL_STRING);
}

static void delete_string(void *ptr)
{
  free(ptr);
}

idl_retcode_t
idl_create_string(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_constval_t *constval,
  void *nodep)
{
  idl_retcode_t ret;
  idl_string_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_TYPE|IDL_STRING;

  if ((ret = create_node(state, size, mask, location, &delete_string, &node)))
    goto err_node;

  if (!constval) {
    node->maximum = 0u;
  } else {
    assert(!constval->node.parent);
    assert(idl_type(constval) == IDL_ULONG);
    node->maximum = constval->value.uint32;
    idl_delete_node(constval);
  }
  *((idl_string_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

static void delete_inherit_spec(void *ptr)
{
  idl_inherit_spec_t *node = ptr;
  idl_unreference_node(node->base);
  free(node);
}

idl_retcode_t
idl_create_inherit_spec(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *base,
  void *nodep)
{
  idl_retcode_t ret;
  idl_inherit_spec_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_INHERIT_SPEC;
  static const idl_delete_t dtor = &delete_inherit_spec;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  node->base = base;
  *((idl_inherit_spec_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_struct(const void *node)
{
  idl_struct_t *n = (idl_struct_t *)node;
  if (!idl_is_masked(n, IDL_STRUCT))
    return false;
  if (idl_is_masked(n, IDL_FORWARD))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_TYPE|IDL_STRUCT));
  /* structs must have an identifier */
  assert(n->name && n->name->identifier);
  /* structs must have at least one member */
  /* FIXME: unless building block extended data types is enabled */
  //assert(idl_is_masked(n->members, IDL_DECL|IDL_MEMBER));
  return true;
}

static void delete_struct(void *ptr)
{
  idl_struct_t *node = ptr;
  idl_delete_node(node->inherit_spec);
  idl_delete_node(node->keys);
  idl_delete_node(node->members);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_finalize_struct(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_struct_t *node,
  idl_member_t *members)
{
  idl_exit_scope(state);
  node->node.symbol.location.last = location->last;
  if (members) {
    node->members = members;
    for (idl_node_t *n = (idl_node_t *)members; n; n = n->next) {
      assert(!n->parent);
      n->parent = (idl_node_t *)node;
    }
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_struct(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_inherit_spec_t *inherit_spec,
  void *nodep)
{
  idl_retcode_t ret;
  idl_struct_t *node;
  idl_scope_t *scope;
  idl_declaration_t *decl;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_TYPE|IDL_CONSTR_TYPE|IDL_STRUCT;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = idl_create_scope(pstate, IDL_STRUCT_SCOPE, name, &scope)))
    goto err_scope;
  if ((ret = create_node(pstate, size, mask, location, &delete_struct, &node)))
    goto err_node;
  if ((ret = idl_declare(pstate, kind, name, node, scope, &decl)))
    goto err_declare;

  if (inherit_spec) {
    idl_struct_t *base = inherit_spec->base;

    assert(!((idl_struct_t *)node)->inherit_spec);
    if (!idl_is_masked(inherit_spec->base, IDL_STRUCT)) {
      idl_error(pstate, idl_location(inherit_spec->base),
        "Struct types cannot inherit from '%s'", "<foobar>");
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (inherit_spec->node.next) {
      idl_error(pstate, idl_location(inherit_spec->node.next),
        "Struct types are limited to single inheritance");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    /* find scope */
    scope = decl->scope;
    /* find imported scope */
    decl = idl_find(pstate, idl_scope(base), idl_name(base), 0u);
    assert(decl && decl->scope);
    if ((ret = idl_import(pstate, scope, decl->scope)))
      return ret;
    ((idl_struct_t *)node)->inherit_spec = inherit_spec;
  }

  node->name = name;
  idl_enter_scope(pstate, scope);
  *((idl_struct_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  idl_delete_scope(scope);
err_scope:
  return ret;
}

static void delete_key(void *ptr)
{
  idl_key_t *node = ptr;
  idl_unreference_node(node->declarator);
  free(node);
}

idl_retcode_t
idl_create_key(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *nodep)
{
  idl_key_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_KEY;

  if (create_node(pstate, size, mask, location, &delete_key, &node))
    return IDL_RETCODE_OUT_OF_MEMORY;
  *((idl_key_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_member(const void *node)
{
  const idl_member_t *n = (const idl_member_t *)node;
  if (!idl_is_masked(n, IDL_MEMBER))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_MEMBER));
  /* members must have a parent node which is a struct */
  assert(idl_is_masked(n->node.parent, IDL_DECLARATION|IDL_TYPE|IDL_STRUCT));
  /* members must have a type specifier */
  assert(idl_is_masked(n->type_spec, IDL_TYPE));
  /* members must have at least one declarator */
  assert(idl_is_masked(n->declarators, IDL_DECLARATION|IDL_DECLARATOR));
  return true;
}

static void delete_member(void *ptr)
{
  idl_member_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_delete_node(node->declarators);
  free(node);
}

idl_retcode_t
idl_create_member(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators,
  void *nodep)
{
  idl_retcode_t ret;
  idl_member_t *node;
  static const size_t size = sizeof(*node);
  static const idl_delete_t dtor = &delete_member;
  static const idl_mask_t mask = IDL_DECLARATION|IDL_MEMBER;
  static const enum idl_declaration_kind kind = IDL_INSTANCE_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    goto err_node;
  assert(idl_is_masked(type_spec, IDL_TYPE));
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t*)node;
  assert(declarators);
  node->declarators = declarators;
  for (idl_node_t *n = (idl_node_t *)declarators; n; n = n->next) {
    const idl_name_t *name = ((const idl_declarator_t *)n)->name;
    assert(!n->parent);
    assert(idl_is_masked(n, IDL_DECLARATION));
    n->parent = (idl_node_t *)node;
    // FIXME: embedded structs have a scope, fix when implementing IDL3.5
    if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
      goto err_declare;
  }

  *((idl_member_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_forward(const void *node)
{
  idl_forward_t *n = (idl_forward_t *)node;
  if (!idl_is_masked(n, IDL_FORWARD))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_TYPE|IDL_STRUCT) ||
         idl_is_masked(n, IDL_DECLARATION|IDL_TYPE|IDL_UNION));
  return true;
}

static void delete_forward(void *ptr)
{
  idl_forward_t *node = ptr;
  //idl_delete_node(n->type_spec);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_forward(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_forward_t *node;
  static const size_t size = sizeof(*node);
  static const idl_delete_t destructor = &delete_forward;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  assert(mask == IDL_STRUCT || mask == IDL_UNION);
  mask |= IDL_DECLARATION|IDL_TYPE|IDL_FORWARD;

  if ((ret = create_node(pstate, size, mask, location, destructor, &node)))
    goto err_node;
  node->name = name;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_forward_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

static void delete_switch_type_spec(void *ptr)
{
  idl_switch_type_spec_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  free(node);
}

static bool
is_switch_type_spec(
  const idl_pstate_t *pstate,
  const void *node)
{
  const idl_type_spec_t *type_spec;
  const idl_switch_type_spec_t *switch_type_spec = node;
  if (!idl_is_masked(switch_type_spec, IDL_SWITCH_TYPE_SPEC))
    return false;
  type_spec = idl_unalias(switch_type_spec->type_spec);
  switch (idl_type(type_spec)) {
    case IDL_ENUM:
      return true;
    case IDL_WCHAR:
    case IDL_OCTET:
      return (pstate->flags & IDL_FLAG_EXTENDED_DATA_TYPES) != 0;
    default:
      return idl_is_masked(type_spec, IDL_BASE_TYPE);
  }
}

idl_retcode_t
idl_create_switch_type_spec(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  void *nodep)
{
  idl_retcode_t ret;
  idl_switch_type_spec_t *node = NULL;
  idl_type_t type;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_SWITCH_TYPE_SPEC;
  static const idl_delete_t dtor = &delete_switch_type_spec;
  bool ext = (pstate->flags & IDL_FLAG_EXTENDED_DATA_TYPES) != 0;

  assert(type_spec);
  type = idl_type(idl_unalias(type_spec));
  if (!(type == IDL_ENUM) &&
      !(type == IDL_BOOL) &&
      !(type == IDL_CHAR) &&
      !(((unsigned)type & (unsigned)IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE) &&
      !(ext && type == IDL_OCTET) &&
      !(ext && type == IDL_WCHAR))
  {
    idl_error(pstate, idl_location(type_spec),
      "Invalid switch type '%s'", "<foobar>");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t *)node;
  *((idl_switch_type_spec_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_case_label(const void *node)
{
  idl_case_label_t *n = (idl_case_label_t *)node;
  if (!idl_is_masked(n, IDL_CASE_LABEL))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_CASE_LABEL));
  /* case labels must have a parent which is a case */
  assert(idl_is_masked(n->node.parent, IDL_DECLARATION|IDL_CASE));
  /* case labels may have an expression (default case does not) */
  if (n->const_expr) {
    assert(idl_is_masked(n->const_expr, IDL_CONST) ||
           idl_is_masked(n->const_expr, IDL_ENUMERATOR));
  }
  return true;
}

static void delete_case_label(void *ptr)
{
  idl_case_label_t *node = ptr;
  idl_delete_node(node->const_expr);
  free(node);
}

idl_retcode_t
idl_create_case_label(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_case_label_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_CASE_LABEL;
  static const idl_delete_t dtor = &delete_case_label;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  node->const_expr = const_expr;
  if (const_expr && !idl_scope(const_expr))
    ((idl_node_t *)const_expr)->parent = (idl_node_t *)node;

  *((idl_case_label_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_case(const void *node)
{
  idl_case_t *n = (idl_case_t *)node;
  if (!idl_is_masked(n, IDL_CASE))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_CASE));
  /* cases must have a parent which is a union */
  assert(idl_is_masked(n->node.parent, IDL_DECLARATION|IDL_TYPE|IDL_UNION));
  /* cases must have at least one case label */
  assert(idl_is_masked(n->case_labels, IDL_DECLARATION|IDL_CASE_LABEL));
  /* cases must have exactly one declarator */
  assert(idl_is_masked(n->declarator, IDL_DECLARATION|IDL_DECLARATOR));
  return true;
}

bool idl_is_default_case(const void *node)
{
  const idl_case_label_t *n;
  if (!idl_is_case(node))
    return false;
  n = ((const idl_case_t *)node)->case_labels;
  for (; n; n = (const idl_case_label_t *)n->node.next) {
    if (!n->const_expr)
      return true;
  }
  return false;
}

static void delete_case(void *ptr)
{
  idl_case_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_delete_node(node->case_labels);
  idl_delete_node(node->declarator);
  free(node);
}

idl_retcode_t
idl_finalize_case(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_case_t *node,
  idl_case_label_t *case_labels)
{
  (void)state;
  node->node.symbol.location.last = location->last;
  node->case_labels = case_labels;
  for (idl_node_t *n = (idl_node_t*)case_labels; n; n = n->next)
    n->parent = (idl_node_t*)node;
  return IDL_RETCODE_OK;
  // FIXME: warn for and ignore duplicate labels
  // FIXME: warn for and ignore for labels combined with default
}

idl_retcode_t
idl_create_case(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator,
  void *nodep)
{
  idl_retcode_t ret;
  idl_case_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_CASE;
  static const idl_delete_t dtor = &delete_case;
  static const enum idl_declaration_kind kind = IDL_INSTANCE_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    goto err_node;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t*)type_spec)->parent = (idl_node_t *)node;
  node->declarator = declarator;
  assert(!declarator->node.parent);
  declarator->node.parent = (idl_node_t *)node;
  assert(!declarator->node.next);
  if ((ret = idl_declare(pstate, kind, declarator->name, node, NULL, NULL)))
    goto err_declare;

  *((idl_case_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_union(const void *node)
{
  const idl_union_t *n = (const idl_union_t *)node;
  if (!idl_is_masked(n, IDL_UNION))
    return false;
  if (idl_is_masked(n, IDL_FORWARD))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_TYPE|IDL_UNION));
  /* unions must have an identifier */
  assert(n->name && n->name->identifier);
  /* unions must have a type specifier */
  assert(idl_is_masked(n->switch_type_spec, IDL_SWITCH_TYPE_SPEC));
  /* unions must have at least one case */
  assert(idl_is_masked(n->cases, IDL_DECLARATION|IDL_CASE));
  return true;
}

static void delete_union(void *ptr)
{
  idl_union_t *node = ptr;
  idl_delete_node(node->switch_type_spec);
  idl_delete_node(node->cases);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_finalize_union(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_union_t *node,
  idl_case_t *cases)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_case_t *default_case = NULL;
  idl_switch_type_spec_t *switch_type_spec;
  idl_type_t type;

  switch_type_spec = node->switch_type_spec;
  assert(switch_type_spec);
  assert(idl_is_masked(switch_type_spec, IDL_SWITCH_TYPE_SPEC));
  type = idl_type(switch_type_spec->type_spec);

  assert(cases);
  for (idl_case_t *c = cases; c; c = idl_next(c)) {
    /* iterate case labels and evaluate constant expressions */
    idl_case_label_t *null_label = NULL;
    /* determine if null-label is present */
    for (idl_case_label_t *cl = c->case_labels; cl; cl = idl_next(cl)) {
      if (!cl->const_expr) {
        null_label = cl;
        if (default_case) {
          idl_error(state, idl_location(cl),
            "error about default case!");
          return IDL_RETCODE_SEMANTIC_ERROR;
        } else {
          default_case = c;
        }
        break;
      }
    }
    /* evaluate constant expressions */
    for (idl_case_label_t *cl = c->case_labels; cl; cl = idl_next(cl)) {
      if (cl->const_expr) {
        if (null_label) {
          idl_warning(state, idl_location(cl),
            "Label in combination with null-label is not useful");
        }
        idl_constval_t *constval = NULL;
        ret = idl_evaluate(state, cl->const_expr, type, &constval);
        if (ret != IDL_RETCODE_OK)
          return ret;
        if (type == IDL_ENUMERATOR) {
          if ((uintptr_t)switch_type_spec != (uintptr_t)constval->node.parent) {
            idl_error(state, idl_location(cl),
              "Enumerator of different enum type");
            return IDL_RETCODE_SEMANTIC_ERROR;
          }
        }
        cl->const_expr = (idl_node_t*)constval;
        if (!idl_scope(constval))
          constval->node.parent = (idl_node_t*)cl;
      }
    }
    assert(!c->node.parent);
    c->node.parent = (idl_node_t *)node;
  }

  node->node.symbol.location.last = location->last;
  node->cases = cases;

  // FIXME: for C++ the lowest value must be known. if beneficial for other
  //        language bindings we may consider marking that value or perhaps
  //        offer convenience functions to do so

  idl_exit_scope(state);
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_union(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_switch_type_spec_t *switch_type_spec,
  void *nodep)
{
  idl_retcode_t ret;
  idl_scope_t *scope;
  idl_union_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_TYPE|IDL_CONSTR_TYPE|IDL_UNION;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if (!is_switch_type_spec(pstate, switch_type_spec)) {
    static const char *fmt =
      "Type '%s' is not a valid switch type specifier";
    idl_error(pstate, idl_location(switch_type_spec), fmt, "<foobar>");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if ((ret = idl_create_scope(pstate, IDL_UNION_SCOPE, name, &scope)))
    goto err_scope;
  if ((ret = create_node(pstate, size, mask, location, &delete_union, &node)))
    goto err_node;
  node->name = name;
  node->switch_type_spec = switch_type_spec;
  assert(!idl_scope(switch_type_spec));
  ((idl_node_t *)switch_type_spec)->parent = (idl_node_t *)node;
  if ((ret = idl_declare(pstate, kind, name, node, scope, NULL)))
    goto err_declare;

  idl_enter_scope(pstate, scope);
  *((idl_union_t **)nodep) = node;
  return ret;
err_declare:
  free(node);
err_node:
  idl_delete_scope(scope);
err_scope:
  return ret;
}

bool idl_is_enumerator(const void *node)
{
  const idl_enumerator_t *n = (const idl_enumerator_t *)node;
  return idl_is_masked(n, IDL_DECLARATION|IDL_ENUMERATOR);
}

static void delete_enumerator(void *ptr)
{
  idl_enumerator_t *node = ptr;
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_enumerator(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_enumerator_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_ENUMERATOR;
  static const idl_delete_t dtor = &delete_enumerator;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    goto err_alloc;
  node->name = name;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_enumerator_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_alloc:
  return ret;
}

bool idl_is_enum(const void *node)
{
  const idl_enum_t *n = (const idl_enum_t *)node;
  return idl_is_masked(n, IDL_DECLARATION|IDL_ENUM);
}

static void delete_enum(void *ptr)
{
  idl_enum_t *node = ptr;
  idl_delete_node(node->enumerators);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_enum(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_enumerator_t *enumerators,
  void *nodep)
{
  idl_retcode_t ret;
  idl_enum_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_TYPE|IDL_CONSTR_TYPE|IDL_ENUM;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;
  uint32_t value = 0;

  if ((ret = create_node(pstate, size, mask, location, &delete_enum, &node)))
    goto err_alloc;
  node->name = name;
  node->enumerators = enumerators;
  assert(enumerators);

  for (idl_enumerator_t *p = enumerators; p; p = idl_next(p), value++) {
    p->node.parent = (idl_node_t*)node;
    if (p->value)
      value = p->value;
    else
      p->value = value;
    for (idl_enumerator_t *q = enumerators; q && q != p; q = idl_next(q)) {
      if (p->value != q->value)
        continue;
      idl_error(pstate, idl_location(p),
        "Value of enumerator '%s' clashes with the value of enumerator '%s'",
        p->name->identifier, q->name->identifier);
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_clash;
    }
  }

  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_enum_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
err_clash:
  free(node);
err_alloc:
  return ret;
}

bool idl_is_typedef(const void *node)
{
  const idl_typedef_t *n = (const idl_typedef_t *)node;
  if (!idl_is_masked(n, IDL_TYPEDEF))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_TYPEDEF));
  return true;
}

static void delete_typedef(void *ptr)
{
  idl_typedef_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_delete_node(node->declarators);
  free(node);
}

idl_retcode_t
idl_create_typedef(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators,
  void *nodep)
{
  idl_retcode_t ret;
  idl_typedef_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_TYPE|IDL_TYPEDEF;
  static const idl_delete_t dtor = &delete_typedef;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t*)type_spec)->parent = (idl_node_t*)node;
  node->declarators = declarators;
  for (idl_declarator_t *p = declarators; p; p = idl_next(p)) {
    p->node.parent = (idl_node_t*)node;
    if ((ret = idl_declare(pstate, kind, p->name, node, NULL, NULL)))
      goto err_declare;
  }

  *((idl_typedef_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
  return ret;
}

bool idl_is_declarator(const void *node)
{
  const idl_declarator_t *n = (const idl_declarator_t *)node;
  if (!idl_is_masked(n, IDL_DECLARATOR))
    return false;
  assert(idl_is_masked(n, IDL_DECLARATION|IDL_DECLARATOR));
  /* declarators must have an identifier */
  assert(n->name->identifier);
  /* declarators must have a parent node */
  assert(n->node.parent);
  /* declarators can have sizes */
  if (n->const_expr)
    assert(idl_is_masked(n->const_expr, IDL_CONST));
  return true;
}

static void delete_declarator(void *ptr)
{
  idl_declarator_t *node = ptr;
  delete_const_expr(node, node->const_expr);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_declarator(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_declarator_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_DECLARATOR;

  if ((ret = create_node(pstate, size, mask, location, &delete_declarator, &node)))
    return ret;
  node->name = name;
  if (const_expr) {
    node->const_expr = const_expr;
    for (idl_node_t *n = (idl_node_t*)const_expr; n; n = n->next) {
      assert(!n->parent);
      assert(idl_is_masked(n, IDL_CONST|IDL_INTEGER_TYPE));
      n->parent = (idl_node_t*)node;
    }
  }
  *((idl_declarator_t **)nodep) = node;
  return ret;
}

static void idl_delete_annotation_member(void *ptr)
{
  idl_annotation_member_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  delete_const_expr(node, node->const_expr);
  idl_delete_node(node->declarator);
  free(node);
}

bool idl_is_annotation_member(const void *ptr)
{
  const idl_annotation_member_t *node = ptr;
  if (!idl_is_masked(node, IDL_ANNOTATION_MEMBER))
    return false;
  assert(idl_is_masked(node, IDL_DECLARATION));
  assert(node->type_spec);
  assert(node->declarator);
  return true;
}

idl_retcode_t
idl_create_annotation_member(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_member_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_ANNOTATION_MEMBER;
  static const idl_delete_t dtor = &idl_delete_annotation_member;
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    goto err_alloc;
  if ((ret = idl_declare(pstate, kind, declarator->name, node, NULL, NULL)))
    goto err_declare;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t *)node;
  node->declarator = declarator;
  ((idl_node_t *)declarator)->parent = (idl_node_t *)node;
  if (idl_is_enumerator(const_expr)) {
    assert(((idl_node_t *)const_expr)->references > 1);
    if (((idl_node_t *)const_expr)->parent != (idl_node_t *)type_spec) {
      idl_error(pstate, idl_location(const_expr),
        "not a valid enumerator for '%s'", "<foobar>");
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_evaluate;
    }
    node->const_expr = const_expr;
  } else if (const_expr) {
    idl_type_t type = idl_type(type_spec);
    idl_constval_t *constval = NULL;
    assert(idl_is_masked(const_expr, IDL_CONST) ||
           idl_is_masked(const_expr, IDL_LITERAL));
    if ((ret = idl_evaluate(pstate, const_expr, type, &constval)))
      goto err_evaluate;
    assert(constval);
    node->const_expr = constval;
    ((idl_node_t *)constval)->parent = (idl_node_t *)node;
  }

  *((idl_annotation_member_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_evaluate:
err_declare:
  free(node);
err_alloc:
  return ret;
}

static bool
type_is_consistent(
  idl_pstate_t *pstate,
  const idl_type_spec_t *lhs,
  const idl_type_spec_t *rhs)
{
  const idl_scope_t *lscp, *rscp;
  const idl_name_t *lname, *rname;

  (void)pstate;
  lscp = idl_scope(lhs);
  rscp = idl_scope(rhs);
  if (!lscp != !rscp)
    return false;
  if (!lscp)
    return idl_type(lhs) == idl_type(rhs);
  if (lscp->kind != rscp->kind)
    return false;
  if (lscp->kind != IDL_ANNOTATION_SCOPE)
    return lhs == rhs;
  if (idl_type(lhs) != idl_type(rhs))
    return false;
  if (idl_is_typedef(lhs)) {
    assert(idl_is_typedef(rhs));
    lname = idl_name(((idl_typedef_t *)lhs)->declarators);
    rname = idl_name(((idl_typedef_t *)rhs)->declarators);
  } else {
    lname = idl_name(lhs);
    rname = idl_name(rhs);
  }
  return strcmp(lname->identifier, rname->identifier) == 0;
}

static idl_retcode_t
enum_is_consistent(
  idl_pstate_t *pstate,
  const idl_enum_t *lhs,
  const idl_enum_t *rhs)
{
  const idl_enumerator_t *a, *b;
  size_t n = 0;

  (void)pstate;
  for (a = lhs->enumerators; a; a = idl_next(a), n++) {
    for (b = rhs->enumerators; b; b = idl_next(b))
      if (strcmp(idl_identifier(a), idl_identifier(b)) == 0)
        break;
    if (!n || a->value != b->value)
      return false;
  }

  return n == idl_length(rhs->enumerators);
}

static bool
const_is_consistent(
  idl_pstate_t *pstate,
  const idl_const_t *lhs,
  const idl_const_t *rhs)
{
  if (strcmp(idl_identifier(lhs), idl_identifier(rhs)) != 0)
    return false;
  if (!type_is_consistent(pstate, lhs->type_spec, rhs->type_spec))
    return false;
  return idl_compare(pstate, lhs->const_expr, rhs->const_expr) == IDL_EQUAL;
}

static bool
typedef_is_consistent(
  idl_pstate_t *pstate,
  const idl_typedef_t *lhs,
  const idl_typedef_t *rhs)
{
  const idl_declarator_t *a, *b;
  size_t n = 0;

  /* typedefs may have multiple associated declarators */
  for (a = lhs->declarators; a; a = idl_next(a), n++) {
    for (b = rhs->declarators; b; b = idl_next(b))
      if (strcmp(idl_identifier(a), idl_identifier(b)) == 0)
        break;
    if (!b)
      return false;
  }

  if (n != idl_length(rhs->declarators))
    return false;
  return type_is_consistent(pstate, lhs->type_spec, rhs->type_spec);
}

static bool
member_is_consistent(
  idl_pstate_t *pstate,
  const idl_annotation_member_t *lhs,
  const idl_annotation_member_t *rhs)
{
  if (strcmp(idl_identifier(lhs), idl_identifier(rhs)) != 0)
    return false;
  if (!type_is_consistent(pstate, lhs->type_spec, rhs->type_spec))
    return false;
  if (!lhs->const_expr != !rhs->const_expr)
    return false;
  if (!lhs->const_expr)
    return true;
  return idl_compare(pstate, lhs->const_expr, rhs->const_expr) == IDL_EQUAL;
}

static bool
is_consistent(idl_pstate_t *pstate, const void *lhs, const void *rhs)
{
  if (idl_mask(lhs) != idl_mask(rhs))
    return false;
  else if (idl_mask(lhs) & IDL_ENUM)
    return enum_is_consistent(pstate, lhs, rhs);
  else if (idl_mask(lhs) & IDL_CONST)
    return const_is_consistent(pstate, lhs, rhs);
  else if (idl_mask(lhs) & IDL_TYPEDEF)
    return typedef_is_consistent(pstate, lhs, rhs);
  assert(idl_is_annotation_member(lhs));
  return member_is_consistent(pstate, lhs, rhs);
}

idl_retcode_t
idl_finalize_annotation(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_t *node,
  idl_definition_t *definitions)
{
  bool discard = false;
  const idl_scope_t *scope;

  node->node.symbol.location.last = location->last;
  scope = pstate->scope;
  idl_exit_scope(pstate);

  if (pstate->parser.state == IDL_PARSE_EXISTING_ANNOTATION_BODY) {
    const idl_name_t *name;
    const idl_declaration_t *decl;
    idl_definition_t *d;
    ssize_t n;

    decl = idl_find(pstate, NULL, node->name, IDL_FIND_ANNOTATION);
    /* earlier declaration must exist given the current state */
    assert(decl);
    assert(decl->node && decl->node != (void *)node);
    assert(decl->scope && decl->scope == scope);
    n = (ssize_t)idl_length(((const idl_annotation_t *)decl->node)->definitions);
    for (d = definitions; n >= 0 && d; d = idl_next(d), n--) {
      if (idl_is_typedef(d))
        name = idl_name(((idl_typedef_t *)d)->declarators);
      else
        name = idl_name(d);
      decl = idl_find(pstate, scope, name, 0u);
      if (!decl || !is_consistent(pstate, d, decl->node))
        goto err_incompat;
    }
    if (n != 0)
      goto err_incompat;
    /* declarations are compatible, discard redefinition */
    discard = true;
  }

  node->definitions = definitions;
  for (idl_node_t *n = (idl_node_t *)definitions; n; n = n->next)
    n->parent = (idl_node_t *)node;
  if (discard)
    idl_unreference_node(node);
  pstate->parser.state = IDL_PARSE;
  return IDL_RETCODE_OK;
err_incompat:
  idl_error(pstate, idl_location(node),
    "Incompatible redefinition of '@%s'", idl_identifier(node));
  return IDL_RETCODE_SEMANTIC_ERROR;
}

static void delete_annotation(void *ptr)
{
  idl_annotation_t *node = ptr;
  idl_delete_node(node->definitions);
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_annotation(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_t *node = NULL;
  idl_scope_t *scope = NULL;
  idl_declaration_t *decl;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ANNOTATION|IDL_DECLARATION;
  static const idl_delete_t dtor = &delete_annotation;
  static const enum idl_declaration_kind kind = IDL_ANNOTATION_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    goto err_node;
  decl = idl_find(pstate, NULL, name, IDL_FIND_ANNOTATION);
  if (decl) {
    /* annotations should not cause more compile errors than strictly needed.
       therefore, in case of multiple definitions of the same annotation in
       one IDL specification, the compiler should accept them, provided that
       they are consistent */
    assert(decl->kind == IDL_ANNOTATION_DECLARATION);
    node->name = name;
    idl_enter_scope(pstate, decl->scope);
    *((idl_annotation_t **)nodep) = node;
    pstate->parser.state = IDL_PARSE_EXISTING_ANNOTATION_BODY;
    return IDL_RETCODE_OK;
  }
  if ((ret = idl_create_scope(pstate, IDL_ANNOTATION_SCOPE, name, &scope)))
    goto err_scope;
  if ((ret = idl_declare(pstate, kind, name, node, scope, NULL)))
    goto err_declare;
  node->name = name;
  idl_enter_scope(pstate, scope);
  *((idl_annotation_t **)nodep) = node;
  pstate->parser.state = IDL_PARSE_ANNOTATION_BODY;
  return IDL_RETCODE_OK;
err_declare:
  idl_delete_scope(scope);
err_scope:
  idl_delete_node(node);
err_node:
  return ret;
}

static void delete_annotation_appl_param(void *ptr)
{
  idl_annotation_appl_param_t *node = ptr;
  delete_const_expr(node, node->const_expr);
  idl_unreference_node(node->member);
  free(node);
}

idl_retcode_t
idl_create_annotation_appl_param(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_annotation_member_t *member,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_appl_param_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_ANNOTATION_APPL_PARAM;
  static const idl_delete_t dtor = &delete_annotation_appl_param;

  if ((ret = create_node(state, size, mask, location, dtor, &node)))
    return ret;
  node->member = member;
  assert(idl_is_masked(const_expr, IDL_EXPRESSION) ||
         idl_is_masked(const_expr, IDL_CONST) ||
         idl_is_masked(const_expr, IDL_ENUMERATOR));
  node->const_expr = const_expr;
  *((idl_annotation_appl_param_t **)nodep) = node;
  return ret;
}

static void delete_annotation_appl(void *ptr)
{
  idl_annotation_appl_t *node = ptr;
  idl_unreference_node(node->annotation);
  idl_delete_node(node->parameters);
  free(node);
}

idl_retcode_t
idl_finalize_annotation_appl(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_appl_t *node,
  idl_annotation_appl_param_t *parameters)
{
  assert(node);
  assert(node->annotation);
  node->node.symbol.location.last = location->last;

  /* constant expressions cannot be evaluated until annotations are applied
     as values for members of type any must match with the element under
     annotation */
  if (idl_is_masked(parameters, IDL_EXPRESSION)) {
    idl_definition_t *definition = node->annotation->definitions;
    idl_annotation_member_t *member = NULL;
    while (definition && !member) {
      if (idl_is_annotation_member(definition))
        member = definition;
    }
    idl_annotation_appl_param_t *parameter = NULL;
    static const size_t size = sizeof(*parameter);
    static const idl_mask_t mask = IDL_DECLARATION|IDL_ANNOTATION_APPL_PARAM;
    static const idl_delete_t dtor = &delete_annotation_appl_param;
    if (!member) {
      idl_error(pstate, idl_location(parameters),
        "@%s does not take any parameters", idl_identifier(node->annotation));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    if (create_node(pstate, size, mask, location, dtor, &parameter)) {
      return IDL_RETCODE_OUT_OF_MEMORY;
    }
    node->parameters = parameter;
    ((idl_node_t *)parameter)->parent = (idl_node_t *)node;
    parameter->member = idl_reference_node(member);
    parameter->const_expr = parameters;
    ((idl_node_t *)parameters)->parent = (idl_node_t *)parameter;
  } else if (idl_is_masked(parameters, IDL_ANNOTATION_APPL_PARAM)) {
    node->parameters = parameters;
    for (idl_annotation_appl_param_t *ap = parameters; ap; ap = idl_next(ap))
      ((idl_node_t *)parameters)->parent = (idl_node_t *)node;
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_annotation_appl(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_annotation_t *annotation,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_appl_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATION|IDL_ANNOTATION_APPL;
  static const idl_delete_t destructor = &delete_annotation_appl;

  if ((ret = create_node(state, size, mask, location, destructor, &node)))
    return ret;
  // now we need to find the annotation scope

  node->annotation = annotation;
  *((idl_annotation_appl_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_type(const void *node, idl_type_t type)
{
  if (!idl_is_masked(node, IDL_TYPE))
    return false;
  assert(idl_type(node));
  return idl_type(node) == type;
}

bool idl_is_base_type(const void *node)
{
  if (!idl_is_masked(node, IDL_BASE_TYPE) ||
       idl_is_masked(node, IDL_CONST))
    return false;
#ifndef NDEBUG
  assert(idl_is_masked(node, IDL_TYPE));
  idl_mask_t mask = ((idl_symbol_t *)node)->mask & ~IDL_TYPE;
  assert(mask == IDL_CHAR ||
         mask == IDL_WCHAR ||
         mask == IDL_BOOL ||
         mask == IDL_OCTET ||
         /* integer types */
         mask == IDL_INT8 || mask == IDL_UINT8 ||
         mask == IDL_INT16 || mask == IDL_UINT16 ||
         mask == IDL_INT32 || mask == IDL_UINT32 ||
         mask == IDL_INT64 || mask == IDL_UINT64 ||
         /*other integer types*/
         mask == IDL_SHORT || mask == IDL_USHORT ||
         mask == IDL_LONG || mask == IDL_ULONG ||
         mask == IDL_LLONG || mask == IDL_ULLONG ||
         /* floating point types*/
         mask == IDL_FLOAT ||
         mask == IDL_DOUBLE ||
         mask == IDL_LDOUBLE);
#endif
  return true;
}

static void delete_base_type(void *ptr)
{
  free(ptr);
}

idl_retcode_t
idl_create_base_type(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_mask_t mask,
  void *nodep)
{
  static const size_t size = sizeof(idl_base_type_t);

  mask |= IDL_TYPE;
  return create_node(state, size, mask, location, &delete_base_type, nodep);
}

bool idl_is_constval(const void *node)
{
  if (!idl_is_masked(node, IDL_CONST) ||
       idl_is_masked(node, IDL_DECLARATION))
    return false;
  assert(idl_is_masked(node, IDL_BASE_TYPE) ||
         idl_is_masked(node, IDL_STRING) ||
         idl_is_enumerator(node));
  return true;
}

static void delete_constval(void *ptr)
{
  idl_constval_t *node = ptr;
  if (idl_type(node) == IDL_STRING && node->value.str)
    free(node->value.str);
  free(node);
}

idl_retcode_t
idl_create_constval(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_mask_t mask,
  void *nodep)
{
  static const size_t size = sizeof(idl_constval_t);

  mask |= IDL_CONST;
  return create_node(state, size, mask, location, &delete_constval, nodep);
}


static void delete_binary_expr(void *ptr)
{
  idl_binary_expr_t *node = ptr;
  assert(idl_is_masked(node, IDL_BINARY_OPERATOR));
  delete_const_expr(node, node->left);
  delete_const_expr(node, node->right);
  free(node);
}

idl_retcode_t
idl_create_binary_expr(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *left,
  idl_primary_expr_t *right,
  void *nodep)
{
  idl_retcode_t ret;
  idl_binary_expr_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_delete_t dtor = &delete_binary_expr;

  mask |= IDL_BINARY_OPERATOR|IDL_EXPRESSION;
  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  node->left = left;
  if (!idl_scope(left))
    ((idl_node_t *)left)->parent = (idl_node_t *)node;
  node->right = right;
  if (!idl_scope(right))
    ((idl_node_t *)right)->parent = (idl_node_t *)node;
  *((idl_binary_expr_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

static void delete_unary_expr(void *ptr)
{
  idl_unary_expr_t *node = ptr;
  assert(idl_is_masked(node, IDL_UNARY_OPERATOR));
  delete_const_expr(node, node->right);
  free(node);
}

idl_retcode_t
idl_create_unary_expr(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *right,
  void *nodep)
{
  idl_retcode_t ret;
  idl_unary_expr_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_delete_t dtor = &delete_unary_expr;

  mask |= IDL_UNARY_OPERATOR|IDL_EXPRESSION;
  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  node->right = right;
  if (!idl_scope(right))
    ((idl_node_t *)right)->parent = (idl_node_t *)node;
  *((idl_unary_expr_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_literal(const void *node)
{
  if (idl_is_masked(node, IDL_LITERAL))
    return true;
  return false;
}

static void delete_literal(void *ptr)
{
  idl_literal_t *node = ptr;
  assert(idl_is_masked(node, IDL_LITERAL));
  if (idl_type(node) == IDL_STRING && node->value.str)
    free(node->value.str);
  free(node);
}

idl_retcode_t
idl_create_literal(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  void *value,
  void *nodep)
{
  idl_retcode_t ret;
  idl_literal_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_delete_t dtor = &delete_literal;

  mask |= IDL_LITERAL|IDL_EXPRESSION;
  if ((ret = create_node(pstate, size, mask, location, dtor, &node)))
    return ret;
  if ((mask & IDL_ULLONG) == IDL_ULLONG) {
    unsigned long long ullng = *((unsigned long long *)value);
    if (ullng > UINT64_MAX) {
      ((idl_symbol_t *)node)->mask = IDL_ULLONG|IDL_LITERAL|IDL_EXPRESSION;
      node->value.ullng = (uint64_t)ullng;
    } else {
      ((idl_symbol_t *)node)->mask = IDL_ULONG|IDL_LITERAL|IDL_EXPRESSION;
      node->value.ulng = (uint32_t)ullng;
    }
  } else if ((mask & IDL_LDOUBLE) == IDL_LDOUBLE) {
    double dbl = (double)*((long double *)value);
    if (isnan(dbl) || isinf(dbl)) {
      ((idl_symbol_t *)node)->mask = IDL_LDOUBLE|IDL_LITERAL|IDL_EXPRESSION;
      node->value.ldbl = *((long double *)value);
    } else {
      ((idl_symbol_t *)node)->mask = IDL_DOUBLE|IDL_LITERAL|IDL_EXPRESSION;
      node->value.dbl = *((double *)value);
    }
  } else if ((mask & IDL_CHAR) == IDL_CHAR) {
    node->value.chr = *((char *)value);
  } else if ((mask & IDL_BOOL) == IDL_BOOL) {
    node->value.bln = *((bool *)value);
  } else if ((mask & IDL_STRING) == IDL_STRING) {
    node->value.str = (char *)value;
  } else {
    assert(0);
  }

  *((idl_literal_t **)nodep) = node;
  return IDL_RETCODE_OK;
}
