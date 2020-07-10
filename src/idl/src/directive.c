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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/processor.h"
#include "idl/parser.h"
#include "directive.h"

static int32_t
push_line(idl_processor_t *proc, idl_line_t *dir)
{
  if (dir->file) {
    idl_file_t *file;
    for (file = proc->files; file; file = file->next) {
      if (strcmp(dir->file, file->name) == 0)
        break;
    }
    if (!file) {
      if (!(file = malloc(sizeof(*file))))
        return IDL_RETCODE_NO_MEMORY;
      file->name = dir->file;
      file->next = proc->files;
      proc->files = file;
      /* do not free filename on return */
      dir->file = NULL;
    } else {
      free(dir->file);
    }
    proc->scanner.position.file = (const char *)file->name;
  }
  proc->scanner.position.line = dir->line;
  proc->scanner.position.column = 1;
  free(dir);
  proc->directive = NULL;
  return 0;
}

static int32_t
parse_line(idl_processor_t *proc, idl_token_t *tok)
{
  idl_line_t *dir = (idl_line_t *)proc->directive;

  switch (proc->state) {
    case IDL_SCAN_LINE: {
      char *end;
      unsigned long long ullng;

      assert(!dir);

      if (tok->code != IDL_TOKEN_PP_NUMBER) {
        idl_error(proc, &tok->location,
          "no line number in #line directive");
        return IDL_RETCODE_PARSE_ERROR;
      }
      // FIXME: use strtoull_l instead
      ullng = strtoull(tok->value.str, &end, 10);
      if (end == tok->value.str || *end != '\0' || ullng > INT32_MAX) {
        idl_error(proc, &tok->location,
          "invalid line number in #line directive");
        return IDL_RETCODE_PARSE_ERROR;
      }
      if (!(dir = malloc(sizeof(*dir)))) {
        return IDL_RETCODE_NO_MEMORY;
      }
      dir->directive.type = IDL_LINE;
      dir->line = (uint32_t)ullng;
      dir->file = NULL;
      dir->extra_tokens = false;
      proc->directive = (idl_directive_t *)dir;
      proc->state = IDL_SCAN_FILENAME;
    } break;
    case IDL_SCAN_FILENAME:
      assert(dir);
      proc->state = IDL_SCAN_EXTRA_TOKEN;
      if (tok->code != '\n' && tok->code != 0) {
        if (tok->code != IDL_TOKEN_STRING_LITERAL) {
          idl_error(proc, &tok->location,
            "invalid filename in #line directive");
          return IDL_RETCODE_PARSE_ERROR;
        }
        assert(dir && !dir->file);
        dir->file = tok->value.str;
        /* do not free string on return */
        tok->value.str = NULL;
        break;
      }
      /* fall through */
    case IDL_SCAN_EXTRA_TOKEN:
      assert(dir);
      if (tok->code == '\n' || tok->code == 0) {
        proc->state = IDL_SCAN;
        return push_line(proc, dir);
      } else if (!dir->extra_tokens) {
        idl_warning(proc, &tok->location,
          "extra tokens at end of #line directive");
      }
      break;
    default:
      assert(0);
      break;
  }
  return 0;
}

static int32_t
push_keylist(idl_processor_t *proc, idl_keylist_t *dir)
{
  if (proc->tree.pragmas) {
    idl_node_t *last;
    for (last = proc->tree.pragmas; last->next; last = last->next) ;
    last->next = dir->node;
    dir->node->previous = last;
  } else {
    proc->tree.pragmas = dir->node;
  }
  //if (proc->pragma)
  //  idl_push_node(&proc->pragma, dir->node);
  free(dir);
  proc->directive = NULL;
  return 0;
}

static int32_t
parse_keylist(idl_processor_t *proc, idl_token_t *tok)
{
  idl_keylist_t *dir = (idl_keylist_t *)proc->directive;

  /* #pragma keylist does not support scoped names */

  switch (proc->state) {
    case IDL_SCAN_KEYLIST:
      if (tok->code == '\n' || tok->code == '\0') {
        idl_error(proc, &tok->location,
          "no data-type in #pragma keylist directive");
        return IDL_RETCODE_PARSE_ERROR;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid data-type in #pragma keylist directive");
        return IDL_RETCODE_PARSE_ERROR;
      }
      assert(!dir);
      // FIXME: this leaks memory still!
      if (!(dir = malloc(sizeof(*dir))) ||
          !(dir->node = malloc(sizeof(*dir->node))))
        return IDL_RETCODE_NO_MEMORY;
      dir->directive.type = IDL_DIRECTIVE_KEYLIST;
      memset(dir->node, 0, sizeof(*dir->node));
      dir->node->flags = IDL_KEYLIST;
      dir->node->location = tok->location;
      dir->node->type.keylist.data_type = tok->value.str;
      proc->directive = (idl_directive_t *)dir;
      /* do not free identifier on return */
      tok->value.str = NULL;
      proc->state = IDL_SCAN_KEY;
      break;
    case IDL_SCAN_DATA_TYPE:
    case IDL_SCAN_KEY: {
      idl_node_t *node, *last;

      if (tok->code == '\n' || tok->code == '\0') {
        proc->state = IDL_SCAN;
        return push_keylist(proc, dir);
      } else if (tok->code == ',' && dir->node->children) {
        /* #pragma keylist takes space or comma separated list of keys */
        break;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid key in #pragma keylist directive");
        return IDL_RETCODE_PARSE_ERROR;
      } else if (idl_iskeyword(proc, tok->value.str, 1)) {
        idl_error(proc, &tok->location,
          "invalid key %s in #pragma keylist directive", tok->value.str);
        return IDL_RETCODE_PARSE_ERROR;
      }

      if (!(node = malloc(sizeof(*node))))
        return IDL_RETCODE_NO_MEMORY;
      memset(node, 0, sizeof(*node));
      node->flags = IDL_KEY;
      node->location = tok->location;
      node->type.key.identifier = tok->value.str;
      if (dir->node->children) {
        for (last = dir->node->children; last->next; last = last->next) ;
        last->next = node;
        node->previous = last;
      } else {
        dir->node->children = node;
      }
      //idl_push_node(proc, dir->node, node);
        
      /* do not free identifier on return */
      tok->value.str = NULL;
    } break;
    default:
      assert(0);
      break;
  }
  return 0;
}

idl_retcode_t idl_parse_directive(idl_processor_t *proc, idl_token_t *tok)
{
  /* order is important here */
  if ((proc->state & IDL_SCAN_LINE) == IDL_SCAN_LINE) {
    return parse_line(proc, tok);
  } else if ((proc->state & IDL_SCAN_KEYLIST) == IDL_SCAN_KEYLIST) {
    return parse_keylist(proc, tok);
  } else if (proc->state == IDL_SCAN_PRAGMA) {
    /* expect keylist */
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      if (strcmp(tok->value.str, "keylist") == 0) {
        proc->state = IDL_SCAN_KEYLIST;
        return 0;
      }
      idl_error(proc, &tok->location,
        "unsupported #pragma directive %s", tok->value.str);
      return IDL_RETCODE_PARSE_ERROR;
    }
  } else if (proc->state == IDL_SCAN_DIRECTIVE_NAME) {
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      /* expect line or pragma */
      if (strcmp(tok->value.str, "line") == 0) {
        proc->state = IDL_SCAN_LINE;
        return 0;
      } else if (strcmp(tok->value.str, "pragma") == 0) {
        /* support #pragma keylist for backwards compatibility */
        proc->state = IDL_SCAN_PRAGMA;
        return 0;
      }
    } else if (tok->code == '\n' || tok->code == '\0') {
      proc->state = IDL_SCAN;
      return 0;
    }
  } else if (proc->state == IDL_SCAN_DIRECTIVE) {
    /* expect # */
    if (tok->code == '#') {
      proc->state = IDL_SCAN_DIRECTIVE_NAME;
      return 0;
    }
  }

  idl_error(proc, &tok->location, "invalid compiler directive");
  return IDL_RETCODE_PARSE_ERROR;
}
