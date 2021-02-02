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
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "idl/processor.h"

#define IDL_EXPRESSION \
  (IDL_LITERAL|IDL_UNARY_OPERATOR|IDL_BINARY_OPERATOR)

typedef enum idl_operator idl_operator_t;
enum idl_operator {
  IDL_NOP = 0,
#define IDL_UNARY_OPERATOR (1ull<<20)
  IDL_MINUS = (IDL_UNARY_OPERATOR|1u),
  IDL_PLUS,
  IDL_NOT,
#define IDL_BINARY_OPERATOR (1ull<<19)
  IDL_OR = (IDL_BINARY_OPERATOR|1u),
  IDL_XOR,
  IDL_AND,
  IDL_LSHIFT,
  IDL_RSHIFT,
  IDL_ADD,
  IDL_SUBTRACT,
  IDL_MULTIPLY,
  IDL_DIVIDE,
  IDL_MODULO
};

idl_operator_t idl_operator(const void *node);

typedef struct idl_intval idl_intval_t;
struct idl_intval {
  idl_type_t type;
  union {
    int64_t llng;
    uint64_t ullng;
  } value;
};

typedef long double idl_floatval_t;

IDL_EXPORT idl_retcode_t
idl_evaluate(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_type_t type,
  void *nodep);

//
// FIXME: this must be moved to the public section of the api!!!!
//
typedef enum idl_equality idl_equality_t;
enum idl_equality {
  IDL_INVALID = -3,
  IDL_MISMATCH = -2, /**< type mismatch */
  IDL_LESS = -1,
  IDL_EQUAL,
  IDL_GREATER,
};

IDL_EXPORT idl_equality_t
idl_compare(
  idl_pstate_t *pstate,
  const idl_const_expr_t *lhs,
  const idl_const_expr_t *rhs);

#endif /* EXPRESSION_H */
