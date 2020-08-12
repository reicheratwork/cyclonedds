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
#ifndef IDL_EXPRESSION_H
#define IDL_EXPRESSION_H

idl_retcode_t
idl_evaluate(
  idl_processor_t *proc,
  idl_variant_t *var,
  const idl_const_expr_t *expr,
  idl_kind_t kind);

#endif /* IDL_EXPRESSION_H */
