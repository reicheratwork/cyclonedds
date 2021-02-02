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

#include "CUnit/Test.h"

static void test_bad_use(const char *str)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(pstate->root);
  idl_delete_pstate(pstate);
}

CU_Test(idl_pragma, keylist_bad_use)
{
  const char *str[] = {
    /* non-existent type */
    "#pragma keylist foo bar",
    /* non-existent member */
    "struct s { char c; };\n"
    "#pragma keylist s foo\n",
    /* duplicate keylists */
    "struct s { char c; };\n"
    "#pragma keylist s c\n"
    "#pragma keylist s c\n",
    /* duplicate keys */
    "struct s { char c; };\n"
    "#pragma keylist s c c\n",
    NULL
  };

  for (const char **ptr = str; *ptr; ptr++)
    test_bad_use(*ptr);
}

CU_Test(idl_pragma, keylist)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s;
  idl_member_t *m;
  idl_keylist_t *kl;

  const char str[] = "struct s { char c; long l; };\n"
                     "#pragma keylist s c";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  s = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  m = s->members;
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_PTR_NOT_NULL_FATAL(m->declarators);
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(m->declarators));
  CU_ASSERT_STRING_EQUAL(idl_identifier(m->declarators), "c");
  kl = s->keylist;
  CU_ASSERT_FATAL((idl_mask(kl) & IDL_KEYLIST) != 0);
  //CU_ASSERT_PTR_NOT_NULL_FATAL(k->declarator);
  //CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(k->declarator));
  //CU_ASSERT(k->declarator == m->declarators);
  CU_ASSERT_PTR_NULL(idl_next(kl));
  idl_delete_pstate(pstate);
}
