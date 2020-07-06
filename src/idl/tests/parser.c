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
    node = node->type.constr_type_dcl.children.first;
    CU_ASSERT_PTR_NOT_NULL(node);
    if (!node)
      return;
    CU_ASSERT_EQUAL(node->flags, type);
    CU_ASSERT_PTR_NOT_NULL(node->type.member_dcl.name);
    if (!node->type.member_dcl.name)
      return;
    CU_ASSERT_STRING_EQUAL(node->type.member_dcl.name, "c");
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
    IDL_SHORT, IDL_SHORT | IDL_FLAG_UNSIGNED,
    IDL_LONG, IDL_LONG | IDL_FLAG_UNSIGNED,
    IDL_LLONG, IDL_LLONG | IDL_FLAG_UNSIGNED,
    IDL_FLOAT, IDL_DOUBLE, IDL_LDOUBLE,
    IDL_CHAR, IDL_WCHAR,
    IDL_BOOL, IDL_OCTET)
};

CU_Theory((const char *s, uint32_t t), idl_parser, base_types)
{
  test_base_type(s, 0u, 0, IDL_FLAG_MEMBER | t);
}

CU_TheoryDataPoints(idl_parser, extended_base_types) = {
  CU_DataPoints(const char *, T("int8"), T("uint8"),
                              T("int16"), T("uint16"),
                              T("int32"), T("uint32"),
                              T("int64"), T("uint64")),
  CU_DataPoints(uint32_t, IDL_INT8, IDL_INT8 | IDL_FLAG_UNSIGNED,
                          IDL_INT16, IDL_INT16 | IDL_FLAG_UNSIGNED,
                          IDL_INT32, IDL_INT32 | IDL_FLAG_UNSIGNED,
                          IDL_INT64, IDL_INT64 | IDL_FLAG_UNSIGNED)
};

CU_Theory((const char *s, uint32_t t), idl_parser, extended_base_types)
{
  test_base_type(s, IDL_FLAG_EXTENDED_DATA_TYPES, 0, IDL_FLAG_MEMBER | t);
  test_base_type(s, 0u, IDL_RETCODE_PARSE_ERROR, 0);
}
