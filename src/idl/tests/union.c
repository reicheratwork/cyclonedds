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
#include <stdbool.h>

#include "idl/tree.h"

#include "CUnit/Theory.h"

/* a union must have at least one case */
CU_Test(idl_union, no_case)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;

  const char str[] = "union u switch(char) { };";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SYNTAX_ERROR);
  CU_ASSERT_PTR_NULL(tree);
  idl_delete_tree(tree);
}

CU_Test(idl_union, single_case)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_union_t *u;
  idl_case_t *c;

  const char str[] = "union u switch(char) { case 1: char c; };";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  u = (idl_union_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_union(u));
  CU_ASSERT(idl_is_type_spec(u->switch_type_spec, IDL_CHAR));
  c = (idl_case_t *)u->cases;
  CU_ASSERT_FATAL(idl_is_case(c));
  CU_ASSERT_PTR_EQUAL(idl_parent(c), u);
  CU_ASSERT(idl_is_case_label(c->case_labels));
  CU_ASSERT(idl_is_type_spec(c->type_spec, IDL_CHAR));
  CU_ASSERT_FATAL(idl_is_declarator(c->declarator));
  CU_ASSERT_STRING_EQUAL(c->declarator->identifier, "c");
  c = idl_next(c);
  CU_ASSERT_PTR_NULL(c);
  idl_delete_tree(tree);
}

CU_Test(idl_union, single_default_case)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_union_t *u;
  idl_case_t *c;

  const char str[] = "union u switch(char) { default: char c; };";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  u = (idl_union_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_union(u));
  CU_ASSERT(idl_is_type_spec(u->switch_type_spec, IDL_CHAR));
  c = (idl_case_t *)u->cases;
  CU_ASSERT_FATAL(idl_is_case(c));
  CU_ASSERT_PTR_EQUAL(idl_parent(c), u);
  CU_ASSERT(idl_is_default_case(c));
  CU_ASSERT(idl_is_type_spec(c->type_spec, IDL_CHAR));
  CU_ASSERT_FATAL(idl_is_declarator(c->declarator));
  CU_ASSERT_STRING_EQUAL(c->declarator->identifier, "c");
  c = idl_next(c);
  CU_ASSERT_PTR_NULL(c);
  idl_delete_tree(tree);
}

// x. union with same declarators
// x. forward declared union
//   x.x. forward declared union before definition
//   x.x. forward declared union after definition
//   x.x. forward declared union with no definition at all
// x. forward declared struct
//   x.x. see union
// x. constant expressions
// x. identifier that collides with a keyword
// x. union with default
// x. union with two default branches
// x. union with multile labels for branch
// x. union with enumeration A and an enumerator from enumeration B

/* the type for the union discriminator must be an integer, char, boolean,
   enumeration, or a reference to one of these */
#define M(name, definitions) "module " name " { " definitions " };"
#define S(name) "struct " name " { char c; };"
#define T(type, name) "typedef " type " " name ";"
#define U(type) "union u switch (" type ") { default: char c; };"

CU_Test(idl_union, typedef_switch_types)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_module_t *m;
  idl_typedef_t *t;
  idl_union_t *u;
  const char *str;

  str = T("char", "baz") U("baz");
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  t = (idl_typedef_t *)tree->root;
  CU_ASSERT(idl_is_typedef(t));
  u = idl_next(t);
  CU_ASSERT_FATAL(idl_is_union(u));
  CU_ASSERT_PTR_EQUAL(t, u->switch_type_spec);
  idl_delete_tree(tree);

  str = M("foo", T("char", "baz") U("baz"));
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  m = (idl_module_t *)tree->root;
  CU_ASSERT(idl_is_module(m));
  t = (idl_typedef_t *)m->definitions;
  CU_ASSERT(idl_is_typedef(t));
  u = idl_next(t);
  CU_ASSERT(idl_is_union(u));
  CU_ASSERT_PTR_EQUAL(t, u->switch_type_spec);
  idl_delete_tree(tree);

  str = M("foo", T("char", "baz")) M("bar", U("foo::baz"));
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  m = (idl_module_t *)tree->root;
  CU_ASSERT(idl_is_module(m));
  t = (idl_typedef_t *)m->definitions;
  CU_ASSERT(idl_is_typedef(t));
  m = idl_next(m);
  CU_ASSERT(idl_is_module(m));
  u = (idl_union_t *)m->definitions;
  CU_ASSERT(idl_is_union(u));
  CU_ASSERT_PTR_EQUAL(t, u->switch_type_spec);
  idl_delete_tree(tree);
}

CU_TheoryDataPoints(idl_union, bad_switch_types) = {
  CU_DataPoints(const char *,
    U("octet"),
    T("octet", "baz") U("baz"),
    S("baz") U("baz"),
    U("baz"),
    M("foo", T("octet", "baz")) M("bar", U("foo::baz"))),
  CU_DataPoints(idl_retcode_t,
    IDL_RETCODE_SYNTAX_ERROR,
    IDL_RETCODE_SEMANTIC_ERROR,
    IDL_RETCODE_SEMANTIC_ERROR,
    IDL_RETCODE_SEMANTIC_ERROR,
    IDL_RETCODE_SEMANTIC_ERROR)
};

CU_Theory((const char *str, idl_retcode_t expret), idl_union, bad_switch_types)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;

  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL(ret, expret);
  CU_ASSERT_PTR_NULL(tree);
  idl_delete_tree(tree);
}

/*
#define U(type) \
  "union u switch (" type ") { " \
  "  case 0: " \
  "    " type " a; " \
  "  default: " \
  "    " type " b; " \
  "};"

CU _ TheoryDataPoints(idl_parser, union_w_base_types) = {
  CU _ DataPoints(const char *,
    U("short"), U("unsigned short")
//    U("long"), U("unsigned long"),
//    U("long long"), U("unsigned long long"),
//    U("float"), U("double"), U("long double"),
//    U("char"), U("wchar"),
//    U("boolean"), U("octet")
   ),
  CU _ DataPoints(uint32_t,
    IDL_SHORT, IDL_USHORT
//    IDL_SHORT | IDL_FLAG_UNSIGNED,
//    IDL_LONG, IDL_LONG | IDL_FLAG_UNSIGNED,
//    IDL_LLONG, IDL_LLONG | IDL_FLAG_UNSIGNED,
//    IDL_FLOAT, IDL_DOUBLE, IDL_LDOUBLE,
//    IDL_CHAR, IDL_WCHAR,
//    IDL_BOOL, IDL_OCTET
   )
};

static void test_union_w_base_type(
  const char *s, uint32_t f, uint32_t t, idl_retcode_t r)
{
  idl_retcode_t ret;
  idl_tree_t *tree;
  idl_node_t *node, *label, *type;

  ret = idl_parse_string(s, f, &tree);
  CU_ASSERT_EQUAL(ret, r);
  if (ret != IDL_RETCODE_OK)
    return;
  node = tree->root;
  CU_ASSERT_PTR_NOT_NULL(node);
  CU_ASSERT_EQUAL(node->flags, IDL_UNION);
  CU_ASSERT_PTR_NOT_NULL(node->type.union_dcl.type);
  if (node->type.union_dcl.type) {
    CU_ASSERT_EQUAL(node->type.union_dcl.type->flags, t);
  }
  if (node->flags == IDL_UNION) {
    CU_ASSERT_PTR_NOT_NULL(node->children.first);
    CU_ASSERT_PTR_NOT_NULL(node->children.last);
    if (!node->children.first || !node->children.last)
      return;
    CU_ASSERT_PTR_EQUAL(node->children.first->next, node->children.last);
    node = node->children.first;
    CU_ASSERT_EQUAL(node->flags, (t | IDL_FLAG_CASE));
    CU_ASSERT_PTR_NOT_NULL(node->type.case_dcl.labels);
    label = node->type.case_dcl.labels;
    CU_ASSERT(label->flags & (IDL_FLAG_LITERAL | IDL_FLAG_CASE_LABEL));
    node = node->next;
    CU_ASSERT_EQUAL(node->flags, (t | IDL_FLAG_CASE));
    CU_ASSERT_PTR_NOT_NULL(node->type.case_dcl.labels);
    label = node->type.case_dcl.labels;
    CU_ASSERT(label->flags & (IDL_FLAG_LITERAL | IDL_FLAG_CASE_LABEL));
  }
}

CU _ Theory((const char *s, uint32_t t), idl_parser, union_w_base_types)
{
  test_union_w_base_type(s, 0u, t, IDL_RETCODE_OK);
}
*/
