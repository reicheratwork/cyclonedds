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

#include "idl/visit.h"
#include "idl/processor.h"

#include "CUnit/Theory.h"

// x. test following type specifiers
// x. test depth-first ordering

static idl_retcode_t
visit_member(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  assert(idl_is_declarator(node));
  fprintf(stderr, "%s called on member %s\n", __func__, idl_identifier(node));
  return IDL_RETCODE_OK;
}

CU_Test(idl_visit, depth_first)
{
  const char str[] = "module a { struct s1 { char c; }; struct s2 { char c; }; };";
  idl_visitor_t visitor;
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret;

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_FATAL(ret == IDL_RETCODE_OK);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_FATAL(ret == IDL_RETCODE_OK);

  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_DECLARATOR;
  visitor.accept[IDL_ACCEPT] = &visit_member;
  //
  // .. implement ..
  //

  idl_visit(pstate, pstate->root, &visitor, NULL);
  idl_delete_pstate(pstate);
}
