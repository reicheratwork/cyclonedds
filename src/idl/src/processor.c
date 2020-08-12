/*
 * Copyright(c) 2020 Jeroen Koekkoek
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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "idl/parser.h"
#include "idl/processor.h"
#include "idl/string.h"
#include "directive.h"
#include "scanner.h"

int32_t idl_processor_init(idl_processor_t *proc)
{
  memset(proc, 0, sizeof(*proc));
  if (!(proc->parser.yypstate = (void *)idl_yypstate_new()))
    return IDL_RETCODE_NO_MEMORY;

  return 0;
}

void idl_processor_fini(idl_processor_t *proc)
{
  if (proc) {
    idl_file_t *file, *next;

    if (proc->parser.yypstate)
      idl_yypstate_delete((idl_yypstate *)proc->parser.yypstate);
    // FIXME: free the symbol table
    // FIXME: free the tree
#if 0
    if (proc->tree.root)
      ddsts_free_context(proc->context);
#endif
    if (proc->directive) {
      if (proc->directive->type == IDL_KEYLIST) {
        // FIXME: free memory
        //idl_keylist_t *dir = (idl_keylist_t *)proc->directive;
        //if (dir->data_type)
        //  free(dir->data_type);
        //for (char **keys = dir->keys; keys && *keys; keys++)
        //  free(*keys);
        //if (dir->keys)
        //  free(dir->keys);
      }
      free(proc->directive);
    }
    for (file = proc->files; file; file = next) {
      next = file->next;
      if (file->name)
        free(file->name);
      free(file);
    }

    if (proc->buffer.data)
      free(proc->buffer.data);
  }
}

static void
idl_log(
  idl_processor_t *proc, uint32_t prio, idl_location_t *loc, const char *fmt, va_list ap)
{
  char buf[1024];
  int cnt;
  size_t off;

  (void)proc;
  (void)prio;
  if (loc->first.file)
    cnt = snprintf(
      buf, sizeof(buf)-1, "%s:%u:%u: ", loc->first.file, loc->first.line, loc->first.column);
  else
    cnt = snprintf(
      buf, sizeof(buf)-1, "%u:%u: ", loc->first.line, loc->first.column);

  if (cnt == -1)
    return;

  off = (size_t)cnt;
  cnt = vsnprintf(buf+off, sizeof(buf)-off, fmt, ap);

  if (cnt == -1)
    return;

  fprintf(stderr, "%s\n", buf);
}

#define IDL_LC_ERROR 1
#define IDL_LC_WARNING 2

void
idl_verror(
  idl_processor_t *proc, const idl_location_t *loc, const char *fmt, va_list ap)
{
  idl_log(proc, IDL_LC_ERROR, loc, fmt, ap);
}

void
idl_error(
  idl_processor_t *proc, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(proc, IDL_LC_ERROR, loc, fmt, ap);
  va_end(ap);
}

void
idl_warning(
  idl_processor_t *proc, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(proc, IDL_LC_WARNING, loc, fmt, ap);
  va_end(ap);
}

static int32_t idl_parse_code(idl_processor_t *proc, idl_token_t *tok)
{
  YYSTYPE yylval;

  /* prepare Bison yylval */
  switch (tok->code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_CHAR_LITERAL:
    case IDL_TOKEN_STRING_LITERAL:
      yylval.str = tok->value.str;
      break;
    case IDL_TOKEN_INTEGER_LITERAL:
      yylval.ullng = tok->value.ullng;
      break;
    default:
      memset(&yylval, 0, sizeof(yylval));
      break;
  }

  switch (idl_yypush_parse(
    proc->parser.yypstate, tok->code, &yylval, &tok->location, proc))
  {
    case YYPUSH_MORE:
      return IDL_RETCODE_PUSH_MORE;
    case 1: /* parse error */
      return IDL_RETCODE_PARSE_ERROR;
    case 2: /* out of memory */
      return IDL_RETCODE_NO_MEMORY;
    default:
      break;
  }

  return IDL_RETCODE_OK;
}

const char *idl_scope(idl_processor_t *proc)
{
  assert(proc);
  return proc->scope ? (const char *)proc->scope : "::";
}

const char *idl_enter_scope(idl_processor_t *proc, const char *ident)
{
  char *scope;

  assert(proc);
  assert(ident);

  if (idl_asprintf(&scope, "%s::%s", proc->scope ? proc->scope : "", ident) == -1)
    return NULL;

  free(proc->scope);
  return proc->scope = scope;
}

void idl_exit_scope(idl_processor_t *proc, const char *ident)
{
  size_t ident_len, scope_len;

  assert(proc);
  assert(proc->scope);
  assert(ident);

  ident_len = strlen(ident);
  scope_len = strlen(proc->scope);
  if (ident_len + 2 == scope_len) {
    free(proc->scope);
    proc->scope = NULL;
  } else {
    assert((ident_len + 2) < scope_len);
    assert(strcmp(proc->scope + (scope_len - ident_len), ident) == 0);
    proc->scope[(scope_len - (ident_len + 2))] = '\0';
  }
}

int32_t idl_parse(idl_processor_t *proc)
{
  int32_t code;
  idl_token_t tok;
  memset(&tok, 0, sizeof(tok));

  do {
    if ((code = idl_scan(proc, &tok)) < 0)
      break;
    if ((unsigned)proc->state & (unsigned)IDL_SCAN_DIRECTIVE)
      code = idl_parse_directive(proc, &tok);
    else if (code != '\n')
      code = idl_parse_code(proc, &tok);
    else
      code = 0;
    /* free memory associated with token value */
    switch (tok.code) {
      case '\n':
        proc->state = IDL_SCAN;
        break;
      case IDL_TOKEN_IDENTIFIER:
      case IDL_TOKEN_CHAR_LITERAL:
      case IDL_TOKEN_STRING_LITERAL:
      case IDL_TOKEN_PP_NUMBER:
        if (tok.value.str)
          free(tok.value.str);
        break;
      default:
        break;
    }
  } while ((tok.code != '\0') &&
           (code == IDL_RETCODE_OK || code == IDL_RETCODE_PUSH_MORE));

  return code;
}

int32_t
idl_parse_string(const char *str, uint32_t flags, idl_tree_t **treeptr)
{
  int32_t ret;
  idl_tree_t *tree;
  idl_processor_t proc;

  assert(str != NULL);
  assert(treeptr != NULL);

  if ((ret = idl_processor_init(&proc)) != 0)
    return ret;

  proc.flags = flags;
  proc.buffer.data = (char *)str;
  proc.buffer.size = proc.buffer.used = strlen(str);
  proc.scanner.cursor = proc.buffer.data;
  proc.scanner.limit = proc.buffer.data + proc.buffer.used;
  proc.scanner.position.line = 1;
  proc.scanner.position.column = 1;

  if ((ret = idl_parse(&proc)) == 0) {
    if ((tree = malloc(sizeof(*tree)))) {
      tree->root = proc.tree.root;
      tree->files = NULL;
      *treeptr = tree;
      memset(&proc.tree, 0, sizeof(proc.tree));
    } else {
      ret = IDL_RETCODE_NO_MEMORY;
    }
  }

  proc.buffer.data = NULL;

  idl_processor_fini(&proc);

  return ret;
}
