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
#ifndef DIRECTIVE_H
#define DIRECTIVE_H

#include <stdbool.h>

#include "idl/processor.h"
#include "tree.h"
#include "scanner.h"

#define IDL_DIRECTIVE (1llu<<40)
#define IDL_LINE (1llu<<39)
#define IDL_PRAGMA (1llu<<38)
#define   IDL_KEYLIST (IDL_PRAGMA | 1llu)
#define   IDL_DATA_TYPE (IDL_PRAGMA | 2llu)

typedef struct idl_line idl_line_t;
struct idl_line {
  idl_symbol_t symbol;
  idl_literal_t *line;
  idl_literal_t *file;
  unsigned flags;
};

typedef struct idl_keylist idl_keylist_t;
struct idl_keylist {
  idl_symbol_t symbol;
  idl_name_t *data_type;
  idl_scoped_name_t **keys;
};

IDL_EXPORT idl_retcode_t
idl_parse_directive(idl_pstate_t *pstate, idl_token_t *tok);

#endif /* DIRECTIVE_H */
