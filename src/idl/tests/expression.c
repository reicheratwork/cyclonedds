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
#include "idl/retcode.h"
#include "idl/tree.h"
#include "expression.h"

#include "CUnit/Test.h"

#define NODE(_kind) \
  (idl_node_t){ _kind, { {NULL, 0, 0}, {NULL, 0, 0} }, NULL, NULL, NULL, NULL, 0, 0 }

#define MINUS() (idl_unary_expr_t){ NODE(IDL_MINUS_EXPR) }
#define PLUS() (idl_unary_expr_t){ NODE(IDL_PLUS_EXPR) }
#define NOT() (idl_unary_expr_t){ NODE(IDL_NOT_EXPR) }

#define INT32(_int) \
  (idl_literal_t){ NODE((IDL_LITERAL | IDL_INT32)), { IDL_INT32, { .signed_int = _int } } }
#define UINT32(_uint) \
  (idl_literal_t){ NODE((IDL_LITERAL | IDL_UINT32)), { IDL_UINT32, { .unsigned_int = _uint } } }

static void
test_unary_expr(
  idl_unary_expr_t *expr, idl_retcode_t ret, idl_variant_t *var)
{
  idl_retcode_t _ret;
  idl_variant_t _var;

  memset(&_var, 0, sizeof(_var));
  _ret = idl_evaluate((idl_processor_t *)1, &_var, (idl_const_expr_t *)expr, var->kind);
  CU_ASSERT_EQUAL_FATAL(_ret, ret);
  CU_ASSERT_EQUAL(_var.kind, var->kind);
  if (_var.kind == var->kind) {
    fprintf(stderr, "retcode: %d\n", _ret);
    fprintf(stderr, "kind: %u, value: ", _var.kind);
    if ((_var.kind & IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE) {
      if ((_var.kind & IDL_UNSIGNED) == IDL_UNSIGNED)
        fprintf(stderr, "%lu\n", _var.value.unsigned_int);
      else
        fprintf(stderr, "%ld\n", _var.value.signed_int);
    }
  }
}

CU_Test(idl_expression, unary_plus)
{
  idl_variant_t var = { .kind = IDL_UINT32, .value = { .unsigned_int = 1 } };
  idl_unary_expr_t expr = PLUS();
  idl_literal_t lit = UINT32(1);

  expr.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_minus)
{
  idl_variant_t var = { .kind = IDL_INT32, .value = { .signed_int = -1 } };
  idl_unary_expr_t expr = MINUS();
  idl_literal_t lit = UINT32(1);

  expr.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_minus_minus)
{
  idl_variant_t var = { .kind = IDL_UINT32, .value = { .unsigned_int = 1 } };
  idl_unary_expr_t expr1 = MINUS();
  idl_unary_expr_t expr2 = MINUS();
  idl_literal_t lit = UINT32(1);

  expr1.right = (idl_const_expr_t *)&expr2;
  expr2.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr1, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_not)
{
  idl_variant_t var = { .kind = IDL_UINT32, .value = { .unsigned_int = UINT32_MAX - 1 } };
  idl_unary_expr_t expr = NOT();
  idl_literal_t lit = UINT32(1);

  expr.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_not_minus)
{
  idl_variant_t var = { .kind = IDL_UINT32, .value = { .unsigned_int = 0 } };
  idl_unary_expr_t expr1 = NOT();
  idl_unary_expr_t expr2 = MINUS();
  idl_literal_t lit = UINT32(1);

  expr1.right = (idl_const_expr_t *)&expr2;
  expr2.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr1, IDL_RETCODE_OK, &var);
}
