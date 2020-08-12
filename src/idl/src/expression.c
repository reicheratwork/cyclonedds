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
#include <stdint.h>

#include "idl/processor.h"
#include "idl/tree.h"
#include "table.h"
#include "expression.h"

static idl_retcode_t
eval_expr(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_const_expr_t *expr,
  idl_kind_t kind);

static idl_retcode_t
eval_or_expr(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_binary_expr_t *expr,
  idl_kind_t kind)
{
  idl_retcode_t ret;
  idl_variant_t lrval, rrval;
  uint64_t max;

  if ((ret = eval_expr(proc, &lrval, expr->left, kind)) != 0)
    return ret;
  if ((ret = eval_expr(proc, &rrval, expr->right, kind)) != 0)
    return ret;

  if ((kind & IDL_INTEGER_TYPE) != IDL_INTEGER_TYPE) {
    idl_error(proc, &expr->node.location,
      "cannot apply | (bitwise or) expression to %s", "unkown");//idl_type(kind));
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  max = ((kind & IDL_INT32) == IDL_INT32) ? INT32_MAX : INT64_MAX;
  switch (((lrval.kind & IDL_UNSIGNED) ? 0 : 1) +
          ((rrval.kind & IDL_UNSIGNED) ? 0 : 2))
  {
    case 0:
      *val = lrval;
      val->value.unsigned_int |= rrval.value.unsigned_int;
      break;
    case 1:
      assert(lrval.value.signed_int < 0);
      if (rrval.value.unsigned_int > max) {
        *val = rrval;
        val->value.unsigned_int |= lrval.value.unsigned_int;
      } else {
        *val = lrval;
        val->value.signed_int |= rrval.value.signed_int;
      }
      break;
    case 2:
      assert(rrval.value.signed_int < 0);
      if (lrval.value.unsigned_int > max) {
        *val = lrval;
        val->value.unsigned_int |= rrval.value.unsigned_int;
      } else {
        *val = rrval;
        val->value.signed_int |= lrval.value.signed_int;
      }
      break;
    case 3:
      assert(lrval.value.signed_int < 0);
      assert(rrval.value.signed_int < 0);
      *val = lrval;
      val->value.signed_int |= rrval.value.signed_int;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_xor_expr(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_binary_expr_t *expr,
  idl_kind_t kind)
{
  idl_retcode_t ret;
  idl_variant_t lrval, rrval;
  uint64_t max;

  if ((ret = eval_expr(proc, &lrval, expr->left, kind)) != 0)
    return ret;
  if ((ret = eval_expr(proc, &rrval, expr->right, kind)) != 0)
    return ret;

  if ((kind & IDL_INTEGER_TYPE) != IDL_INTEGER_TYPE) {
    idl_error(proc, &expr->node.location,
      "cannot apply ^ (xor) expression to %s", idl_type(expr->left));
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  max = ((kind & IDL_INT32) == IDL_INT32) ? INT32_MAX : INT64_MAX;
  switch (((lrval.kind & IDL_UNSIGNED) ? 0 : 1) +
          ((rrval.kind & IDL_UNSIGNED) ? 0 : 2))
  {
    case 0:
      *val = lrval;
      val->value.unsigned_int |= rrval.value.unsigned_int;
      break;
    case 1:
      assert(lrval.value.signed_int < 0);
      if (rrval.value.unsigned_int > max) {
        *val = rrval;
        val->value.unsigned_int |= lrval.value.unsigned_int;
      } else {
        *val = lrval;
        val->value.signed_int |= rrval.value.signed_int;
      }
      break;
    case 2:
      assert(rrval.value.signed_int < 0);
      if (lrval.value.unsigned_int > max) {
        *val = lrval;
        val->value.unsigned_int |= rrval.value.unsigned_int;
      } else {
        *val = rrval;
        val->value.signed_int |= lrval.value.signed_int;
      }
      break;
    case 3:
      assert(lrval.value.signed_int < 0);
      assert(rrval.value.signed_int < 0);
      *val = lrval;
      val->value.signed_int |= rrval.value.signed_int;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_minus_expr(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_unary_expr_t *expr,
  idl_kind_t kind)
{
  idl_retcode_t ret;
  idl_variant_t rval;

  if ((ret = eval_expr(proc, &rval, expr->right, kind)) != IDL_RETCODE_OK)
    return ret;

  if ((rval.kind & IDL_INTEGER_TYPE) != IDL_INTEGER_TYPE &&
      (rval.kind & IDL_FLOATING_PT_TYPE) != IDL_FLOATING_PT_TYPE)
  {
    idl_error(proc, &expr->node.location,
      "cannot apply - (minus) expression to %s", idl_type(expr->right));
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (kind & IDL_INTEGER_TYPE) {
    if (rval.kind & IDL_UNSIGNED) {
      const char *type;
      uint64_t max_int;

      if ((kind & IDL_INT32) == IDL_INT32) {
        max_int = INT32_MAX;
        type = "long";
      } else {
        max_int = INT64_MAX;
        type = "long long";
      }
      if (rval.value.unsigned_int > max_int) {
        idl_error(proc, &expr->node.location,
          "value exceeds maximum for %s", type);
        return IDL_RETCODE_OUT_OF_RANGE;
      }

      val->kind = rval.kind & ~IDL_UNSIGNED;
      val->value.signed_int = -(int64_t)rval.value.unsigned_int;
    } else {
      assert(rval.value.signed_int < 0);
      val->kind = rval.kind | IDL_UNSIGNED;
      val->value.unsigned_int = (uint64_t)-rval.value.signed_int;
    }
  } else {
    assert((kind & IDL_FLOATING_PT_TYPE) == IDL_FLOATING_PT_TYPE);
    // FIXME: check stuff here
    val->kind = rval.kind;
    val->value.floating_pt = -rval.value.floating_pt;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_plus_expr(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_unary_expr_t *expr,
  idl_kind_t kind)
{
  idl_retcode_t ret;
  idl_variant_t rval;

  if ((ret = eval_expr(proc, &rval, expr->right, kind)) != IDL_RETCODE_OK)
    return ret;

  if (!idl_is_integer(expr->right) && !idl_is_floating_pt(expr->right)) {
    idl_error(proc, &expr->node.location,
      "cannot apply + (plus) expression to %s", idl_type(expr->right));
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  *val = rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_not_expr(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_unary_expr_t *expr,
  idl_kind_t kind)
{
  idl_retcode_t ret;
  idl_variant_t rval;

  if ((ret = eval_expr(proc, &rval, expr->right, kind)) != IDL_RETCODE_OK)
    return ret;

  if ((rval.kind & IDL_INTEGER_TYPE) != IDL_INTEGER_TYPE) {
    idl_error(proc, &expr->node.location,
      "cannot apply ~ (NOT) expression to %s", idl_type(expr->right));
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (rval.kind & IDL_UNSIGNED) {
    uint64_t uint_max;
    if ((rval.kind & IDL_INT64) == IDL_INT64)
      uint_max = UINT64_MAX;
    else
      uint_max = UINT32_MAX;
    val->kind = rval.kind;
    val->value.unsigned_int = uint_max & ~rval.value.unsigned_int;
  } else {
    assert(rval.value.signed_int < 0);
    val->kind = rval.kind | IDL_UNSIGNED;
    val->value.signed_int = ~rval.value.signed_int;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_expr(
  idl_processor_t *proc,
  idl_variant_t *var,
  const idl_const_expr_t *expr,
  idl_kind_t kind)
{
  assert(proc);
  assert(var);
  assert(expr);

  switch (expr->kind) {
    case IDL_OR_EXPR:
      return eval_or_expr(proc, var, (idl_binary_expr_t *)expr, kind);
    case IDL_XOR_EXPR:
      return eval_xor_expr(proc, var, (idl_binary_expr_t *)expr, kind);
    case IDL_MINUS_EXPR:
      return eval_minus_expr(proc, var, (idl_unary_expr_t *)expr, kind);
    case IDL_PLUS_EXPR:
      return eval_plus_expr(proc, var, (idl_unary_expr_t *)expr, kind);
    case IDL_NOT_EXPR:
      return eval_not_expr(proc, var, (idl_unary_expr_t *)expr, kind);
  }

  assert(idl_is_literal(expr));
  if ((kind & IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE) {
    uint64_t uint_max;
    const char *type;

    if ((kind & IDL_INT32) == IDL_INT32) {
      uint_max = UINT32_MAX;
      type = "unsigned long";
    } else {
      uint_max = UINT64_MAX;
      type = "unsigned long long";
    }

    if (!idl_is_integer(expr)) {
      idl_error(proc, &expr->node.location,
        "cannot express %s as %s", "<type>", type);
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    } else if (((idl_literal_t *)expr)->variant.value.unsigned_int > uint_max) {
      idl_error(proc, &expr->node.location,
        "value exceeds maximum (%lu) for %s", type);
      return IDL_RETCODE_OUT_OF_RANGE;
    }
    *var = ((idl_literal_t *)expr)->variant;
    var->kind = (kind | IDL_UNSIGNED);
  } else {
    assert(0);
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_evaluate(
  idl_processor_t *proc,
  idl_variant_t *val,
  const idl_const_expr_t *expr,
  idl_kind_t kind)
{
  idl_retcode_t ret;

  assert(proc);
  assert(val);
  assert(expr);

  if ((kind & IDL_LDOUBLE) == IDL_LDOUBLE)
    ret = eval_expr(proc, val, expr, IDL_LDOUBLE);
  else if ((kind & IDL_DOUBLE) == IDL_DOUBLE)
    ret = eval_expr(proc, val, expr, IDL_DOUBLE);
  else if ((kind & IDL_LLONG) == IDL_LLONG)
    ret = eval_expr(proc, val, expr, IDL_UINT64);
  else
    ret = eval_expr(proc, val, expr, IDL_UINT32);

  return ret;
}
