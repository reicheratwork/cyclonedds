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

#include "idl/processor.h"

#include "CUnit/Theory.h"

// x. union with same declarators
// x. struct with same declarators
// x. struct with embedded struct
// x. struct with anonymous embedded struct
// x. forward declared union
//   x.x. forward declared union before definition
//   x.x. forward declared union after definition
//   x.x. forward declared union with no definition at all
// x. forward declared struct
//   x.x. see union
// x. constant expressions
// x. identifier that collides with a keyword

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


CU _ Test(idl_parser, enumerator)
{
  //
}
*/


#define M(name, contents) "module " name " { " contents " };"
#define S(name, contents) "struct " name " { " contents " };"
#define LL(name) "long long " name ";"
#define LD(name) "long double " name ";"

CU_Test(idl_parser, embedded)
{
  idl_retcode_t ret;
  idl_tree_t *tree;
  idl_node_t *node, *parent;
  const char str[] = M("foo", M("bar", S("baz", LL("foobar") LD("foobaz"))));

  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  node = tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(node);
  CU_ASSERT_PTR_NULL(node->parent);
  CU_ASSERT_PTR_NULL(node->previous);
  CU_ASSERT_PTR_NULL(node->next);
  CU_ASSERT_EQUAL_FATAL(node->flags, IDL_MODULE);
  CU_ASSERT_STRING_EQUAL(node->name, "foo");
  parent = node;
  node = node->children;
  CU_ASSERT_PTR_NOT_NULL_FATAL(node);
  CU_ASSERT_PTR_EQUAL(node->parent, parent);
  CU_ASSERT_PTR_NULL(node->previous);
  CU_ASSERT_PTR_NULL(node->next);
  CU_ASSERT_EQUAL_FATAL(node->flags, IDL_MODULE);
  CU_ASSERT_STRING_EQUAL(node->name, "bar");
  parent = node;
  node = node->children;
  CU_ASSERT_PTR_NOT_NULL_FATAL(node);
  CU_ASSERT_PTR_EQUAL(node->parent, parent);
  CU_ASSERT_PTR_NULL(node->previous);
  CU_ASSERT_PTR_NULL(node->next);
  CU_ASSERT_EQUAL_FATAL(node->flags, IDL_STRUCT);
  CU_ASSERT_STRING_EQUAL(node->name, "baz");
  parent = node;
  node = node->children;
  CU_ASSERT_PTR_NOT_NULL_FATAL(node);
  CU_ASSERT_PTR_EQUAL(node->parent, parent);
  CU_ASSERT_PTR_NULL(node->previous);
  CU_ASSERT_EQUAL_FATAL(node->flags, IDL_MEMBER | IDL_LLONG);
  CU_ASSERT_STRING_EQUAL(node->type.member.declarator, "foobar");
  node = node->next;
  CU_ASSERT_PTR_NOT_NULL_FATAL(node);
  CU_ASSERT_PTR_EQUAL(node->parent, parent);
  CU_ASSERT_PTR_NOT_NULL(node->previous);
  CU_ASSERT_PTR_NULL(node->next);
  CU_ASSERT_EQUAL_FATAL(node->flags, IDL_MEMBER | IDL_LDOUBLE);
  CU_ASSERT_STRING_EQUAL(node->type.member.declarator, "foobaz");
}


#define T(type) "struct s{" type " c;};"

static void
test_base_type(const char *str, uint32_t flags, int32_t retcode, uint32_t type)
{
  int32_t ret;
  idl_tree_t *tree;
  idl_node_t *node;

  ret = idl_parse_string(str, flags, &tree);
  CU_ASSERT(ret == retcode);
  if (ret != 0)
    return;
  node = tree->root;
  CU_ASSERT_PTR_NOT_NULL(node);
  if (!node)
    return;
  CU_ASSERT_EQUAL(node->flags, IDL_STRUCT);
  if (node->flags == IDL_STRUCT) {
    node = node->children;
    CU_ASSERT_PTR_NOT_NULL(node);
    if (!node)
      return;
    CU_ASSERT_EQUAL(node->flags, type);
    CU_ASSERT_PTR_NOT_NULL(node->type.member.declarator);
    if (!node->type.member.declarator)
      return;
    CU_ASSERT_STRING_EQUAL(node->type.member.declarator, "c");
  }
}

CU_TheoryDataPoints(idl_parser, base_types) = {
  CU_DataPoints(const char *,
    T("short"), T("unsigned short"),
    T("long"), T("unsigned long"),
    T("long long"), T("unsigned long long"),
    T("float"), T("double"), T("long double"),
    T("char"), T("wchar"),
    T("boolean"), T("octet")),
  CU_DataPoints(uint32_t,
    IDL_SHORT, IDL_USHORT,
    IDL_LONG, IDL_ULONG,
    IDL_LLONG, IDL_ULLONG,
    IDL_FLOAT, IDL_DOUBLE, IDL_LDOUBLE,
    IDL_CHAR, IDL_WCHAR,
    IDL_BOOL, IDL_OCTET)
};

CU_Theory((const char *s, uint32_t t), idl_parser, base_types)
{
  test_base_type(s, 0u, 0, IDL_MEMBER | t);
}

CU_TheoryDataPoints(idl_parser, extended_base_types) = {
  CU_DataPoints(const char *, T("int8"), T("uint8"),
                              T("int16"), T("uint16"),
                              T("int32"), T("uint32"),
                              T("int64"), T("uint64")),
  CU_DataPoints(uint32_t, IDL_INT8, IDL_UINT8,
                          IDL_INT16, IDL_UINT16,
                          IDL_INT32, IDL_UINT32,
                          IDL_INT64, IDL_UINT64)
};

CU_Theory((const char *s, uint32_t t), idl_parser, extended_base_types)
{
  test_base_type(s, IDL_FLAG_EXTENDED_DATA_TYPES, 0, IDL_MEMBER | t);
  test_base_type(s, 0u, IDL_RETCODE_PARSE_ERROR, 0);
}
