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
#include "idl/string.h"

#include "symbol.h"
#include "tree.h"
#include "scope.h"
#include "directive.h"
#include "parser.h"

static idl_retcode_t
push_file(idl_pstate_t *pstate, const char *inc)
{
  idl_file_t *file = pstate->files;
  for (; file && strcmp(file->name, inc); file = file->next) ;
  if (!file) {
    if (!(file = calloc(1, sizeof(*file))))
      return IDL_RETCODE_OUT_OF_MEMORY;
    file->next = pstate->files;
    pstate->files = file;
    if (!(file->name = idl_strdup(inc)))
      return IDL_RETCODE_OUT_OF_MEMORY;
  }
  pstate->scanner.position.file = file;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
push_source(idl_pstate_t *pstate, const char *inc, const char *abs, bool sys)
{
  idl_file_t *file = pstate->files, *path = pstate->paths;
  idl_source_t *src, *last;
  for (; path && strcmp(path->name, abs); path = path->next) ;
  if (!path) {
    if (!(path = calloc(1, sizeof(*path))))
      return IDL_RETCODE_OUT_OF_MEMORY;
    path->next = pstate->paths;
    pstate->paths = path;
    if (!(path->name = idl_strdup(abs)))
      return IDL_RETCODE_OUT_OF_MEMORY;
  }
  if (push_file(pstate, inc))
    return IDL_RETCODE_OUT_OF_MEMORY;
  if (!(src = calloc(1, sizeof(*src))))
    return IDL_RETCODE_OUT_OF_MEMORY;
  src->file = file;
  src->path = path;
  src->system = sys;
  if (!pstate->sources) {
    pstate->sources = src;
  } else if (pstate->scanner.position.source->includes) {
    last = ((idl_source_t *)pstate->scanner.position.source)->includes;
    for (; last->next; last = last->next) ;
    last->next = src;
    src->previous = last;
  } else {
    ((idl_source_t *)pstate->scanner.position.source)->includes = src;
  }
  pstate->scanner.position.source = src;
  return IDL_RETCODE_OK;
}

#define START_OF_FILE (1u<<0)
#define RETURN_TO_FILE (1u<<1)
#define SYSTEM_FILE (1u<<2)
#define EXTRA_TOKENS (1u<<3)

static idl_retcode_t
push_line(idl_pstate_t *pstate, idl_line_t *dir)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

  assert(dir);
  if (dir->flags & (START_OF_FILE|RETURN_TO_FILE)) {
    bool sys = (dir->flags & SYSTEM_FILE) != 0;
    char *norm = NULL, *abs, *inc;
    abs = inc = dir->file->value.str;
    /* convert to normalized file name */
    if (!idl_isabsolute(abs)) {
      /* include paths are relative to the current file. so, strip file name,
         postfix with "/relative/path/to/file" and normalize */
      const char *cwd = pstate->scanner.position.source->path->name;
      const char *sep = cwd;
      assert(idl_isabsolute(cwd));
      for (size_t i=0; cwd[i]; i++) {
        if (idl_isseparator(cwd[i]))
          sep = cwd + i;
      }
      if (idl_asprintf(&abs, "%.*s/%s", (sep-cwd), cwd, inc) < 0)
        return IDL_RETCODE_OUT_OF_MEMORY;
    }
    idl_normalize_path(abs, &norm);
    if (abs != dir->file->value.str)
      free(abs);
    if (!norm)
      return IDL_RETCODE_OUT_OF_MEMORY;

    if (dir->flags & START_OF_FILE) {
      ret = push_source(pstate, inc, norm, sys);
    } else {
      assert(pstate->scanner.position.source);
      const idl_source_t *src = pstate->scanner.position.source;
      while (src && strcmp(src->path->name, norm))
        src = src->parent;
      if (src) {
        pstate->scanner.position.source = src;
        pstate->scanner.position.file = src->file;
      } else {
        idl_error(pstate, idl_location(dir),
          "Invalid #line directive, file '%s' not on include stack", inc);
        ret = IDL_RETCODE_SEMANTIC_ERROR;
      }
    }

    free(norm);
  } else {
    ret = push_file(pstate, dir->file->value.str);
  }

  if (ret)
    return ret;
  pstate->scanner.position.line = (uint32_t)dir->line->value.ullng;
  pstate->scanner.position.column = 1;
  idl_delete_symbol(dir);
  pstate->directive = NULL;
  return IDL_RETCODE_OK;
}

static int
stash_line(idl_pstate_t *pstate, idl_location_t *loc, unsigned long long ullng)
{
  idl_line_t *dir = (idl_line_t *)pstate->directive;
  if (idl_create_literal(pstate, loc, IDL_ULLONG, &ullng, &dir->line))
    return -1;
  dir->line->value.ullng = ullng;
  return 0;
}

static int
stash_filename(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  idl_line_t *dir = (idl_line_t *)pstate->directive;
  if (idl_create_literal(pstate, loc, IDL_STRING, str, &dir->file))
    return -1;
  dir->file->value.str = str;
  return 0;
}

static void delete_line(void *ptr);

/* for proper handling of includes by parsing line controls, GCCs linemarkers
   are required. they are enabled in mcpp by defining the compiler to be GNUC
   instead of INDEPENDANT.
   See: https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html */
static int32_t
parse_line(idl_pstate_t *pstate, idl_token_t *tok)
{
  idl_line_t *dir = (idl_line_t *)pstate->directive;
  unsigned long long ullng;

  switch (pstate->scanner.state) {
    case IDL_SCAN_LINE:
      if (tok->code != IDL_TOKEN_PP_NUMBER) {
        idl_error(pstate, &tok->location,
          "No line number in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      ullng = idl_strtoull(tok->value.str, NULL, 10);
      if (ullng == 0 || ullng > INT32_MAX) {
        idl_error(pstate, &tok->location,
          "Invalid line number in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else if (stash_line(pstate, &tok->location, ullng)) {
        return IDL_RETCODE_OUT_OF_MEMORY;
      }
      pstate->scanner.state = IDL_SCAN_FILENAME;
      break;
    case IDL_SCAN_FILENAME:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      } else if (tok->code != IDL_TOKEN_STRING_LITERAL) {
        idl_error(pstate, &tok->location,
          "Invalid filename in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else if (stash_filename(pstate, &tok->location, tok->value.str)) {
        return IDL_RETCODE_OUT_OF_MEMORY;
      }
      tok->value.str = NULL; /* do not free */
      pstate->scanner.state = IDL_SCAN_FLAGS;
      break;
    case IDL_SCAN_FLAGS:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      } else if (tok->code == IDL_TOKEN_PP_NUMBER) {
        if (strcmp(tok->value.str, "1") == 0) {
          if (dir->flags & (START_OF_FILE|RETURN_TO_FILE))
            goto extra_tokens;
          dir->flags |= START_OF_FILE;
        } else if (strcmp(tok->value.str, "2") == 0) {
          if (dir->flags & (START_OF_FILE|RETURN_TO_FILE))
            goto extra_tokens;
          dir->flags |= RETURN_TO_FILE;
        } else if (strcmp(tok->value.str, "3") == 0) {
          if (dir->flags & (SYSTEM_FILE))
            goto extra_tokens;
          dir->flags |= SYSTEM_FILE;
        } else {
          goto extra_tokens;
        }
      } else {
extra_tokens:
        idl_warning(pstate, &tok->location,
          "Extra tokens at end of #line directive");
        pstate->scanner.state = IDL_SCAN_EXTRA_TOKENS;
      }
      break;
    default:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      }
      break;
  }

  return IDL_RETCODE_OK;
}

static void delete_keylist(void *dir);

static int32_t
push_keylist(idl_pstate_t *pstate, idl_keylist_t *dir)
{
  idl_name_t *data_type;
  idl_declaration_t *declaration;
  idl_struct_t *node;
  idl_scope_t *scope;
  static const uint32_t flags = IDL_FIND_IGNORE_CASE;

  data_type = dir->data_type;
  if (!(declaration = idl_find(pstate, NULL, data_type, flags))) {
    idl_error(pstate, idl_location(data_type),
      "Unknown data-type '%s' in keylist directive", data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (strcmp(data_type->identifier, declaration->name->identifier)) {
    idl_error(pstate, idl_location(data_type),
      "data-type '%s' in keylist directive differs in case", data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  node = (idl_struct_t *)declaration->node;
  if (!idl_is_masked(node, IDL_STRUCT) ||
       idl_is_masked(node, IDL_FORWARD))
  {
    idl_error(pstate, idl_location(data_type),
      "Invalid data-type '%s' in keylist directive", data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (node->keys) {
    idl_error(pstate, idl_location(data_type),
      "Redefinition of keylist for data-type '%s'", data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  node = (idl_struct_t *)declaration->node;
  scope = declaration->scope;
  for (size_t i=0; dir->keys && dir->keys[i]; i++) {
    idl_key_t *key = NULL;
    idl_member_t *member;
    idl_declarator_t *declarator;
    idl_scoped_name_t *scoped_name = dir->keys[i];
    declaration = idl_find_scoped_name(pstate, scope, scoped_name, 0u);
    if (!declaration) {
      idl_error(pstate, idl_location(scoped_name),
        "Unknown field '%s' in keylist directive", "<foobar>");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    member = (idl_member_t *)declaration->node;
    if (!member || !idl_is_masked(member, IDL_MEMBER)) {
      idl_error(pstate, idl_location(scoped_name),
        "Invalid key '%s' in keylist, not a field", "<foobar>");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    /* find exact declarator in case member has multiple */
    declarator = member->declarators;
    while (declarator) {
      const char *s1, *s2;
      s1 = declarator->name->identifier;
      s2 = scoped_name->path.names[scoped_name->path.length - 1]->identifier;
      if (strcmp(s1, s2) == 0)
        break;
      declarator = (idl_declarator_t *)declarator->node.next;
    }
    assert(declarator);
    /* detect duplicate keys */
    for (idl_key_t *k=node->keys; k; k=idl_next(k)) {
      if (declarator == k->declarator) {
        idl_error(pstate, idl_location(scoped_name),
          "Duplicate key '%s' in keylist directive", "<foobar>");
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
    if (idl_create_key(pstate, idl_location(scoped_name), &key))
      return IDL_RETCODE_OUT_OF_MEMORY;
    key->node.parent = (idl_node_t *)node;
    key->declarator = idl_reference_node(declarator);
    node->keys = idl_push_node(node->keys, key);
  }

  delete_keylist(dir);
  pstate->directive = NULL;
  return IDL_RETCODE_OK;
}

static int stash_data_type(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  idl_keylist_t *dir = (idl_keylist_t *)pstate->directive;
  idl_name_t *name = NULL;

  if (idl_create_name(pstate, loc, str, &name))
    return -1;
  dir->data_type = name;
  return 0;
}

static int stash_field(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  idl_keylist_t *dir = (idl_keylist_t *)pstate->directive;
  idl_name_t *name = NULL;
  size_t n;

  assert(dir->keys);
  if (idl_create_name(pstate, loc, str, &name))
    goto err_alloc;
  for (n=0; dir->keys[n]; n++) ;
  assert(n);
  if (idl_append_to_scoped_name(pstate, dir->keys[n], name))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    free(name);
  return -1;
}

static int stash_key(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  idl_keylist_t *dir = (idl_keylist_t *)pstate->directive;
  idl_name_t *name = NULL;
  idl_scoped_name_t **keys;
  size_t n;

  for (n=0; dir->keys && dir->keys[n]; n++) ;
  if (!(keys = realloc(dir->keys, (n + 2) * sizeof(*keys))))
    goto err_alloc;
  dir->keys = keys;
  keys[n+0] = NULL;
  if (idl_create_name(pstate, loc, str, &name))
    goto err_alloc;
  if (idl_create_scoped_name(pstate, loc, name, false, &keys[n+0]))
    goto err_alloc;
  keys[n+1] = NULL;
  return 0;
err_alloc:
  if (name)
    free(name);
  return -1;
}

static int32_t
parse_keylist(idl_pstate_t *pstate, idl_token_t *tok)
{
  idl_keylist_t *dir = (idl_keylist_t *)pstate->directive;
  assert(dir);

  /* #pragma keylist does not support scoped names for data-type */
  switch (pstate->scanner.state) {
    case IDL_SCAN_KEYLIST:
      assert(!dir->data_type);
      if (tok->code == '\n' || tok->code == '\0') {
        idl_error(pstate, &tok->location,
          "No data-type in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid data-type in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      }

      if (stash_data_type(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_OUT_OF_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_KEY;
      break;
    case IDL_SCAN_FIELD:
      assert(dir->keys);
      if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid keylist directive, identifier expected");
        return IDL_RETCODE_SEMANTIC_ERROR;
      } else if (idl_iskeyword(pstate, tok->value.str, 1)) {
        idl_error(pstate, &tok->location,
          "Invalid key '%s' in keylist directive");
        return IDL_RETCODE_SEMANTIC_ERROR;
      }

      if (stash_field(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_OUT_OF_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_SCOPE;
      break;
    case IDL_SCAN_SCOPE:
      if (tok->code == '.') {
        pstate->scanner.state = IDL_SCAN_FIELD;
        break;
      }
      pstate->scanner.state = IDL_SCAN_KEY;
      /* fall through */
    case IDL_SCAN_KEY:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_keylist(pstate, dir);
      } else if (tok->code == ',' && dir->keys) {
        /* #pragma keylist takes space or comma separated list of keys */
        break;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid token in #pragma keylist directive");
        return IDL_RETCODE_SEMANTIC_ERROR;
      } else if (idl_iskeyword(pstate, tok->value.str, 1)) {
        idl_error(pstate, &tok->location,
          "Invalid key '%s' in #pragma keylist directive", tok->value.str);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }

      if (stash_key(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_OUT_OF_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_SCOPE;
      break;
    default:
      assert(0);
      break;
  }

  return IDL_RETCODE_OK;
}

static void delete_line(void *ptr)
{
  idl_line_t *dir = (idl_line_t *)ptr;
  assert(dir);
  idl_delete_symbol(dir->line);
  idl_delete_symbol(dir->file);
  free(dir);
}

static void delete_keylist(void *sym)
{
  idl_keylist_t *dir = (idl_keylist_t *)sym;
  assert(dir);
  idl_delete_name(dir->data_type);
  if (dir->keys) {
    for (size_t i=0; dir->keys[i]; i++)
      idl_delete_scoped_name(dir->keys[i]);
    free(dir->keys);
  }
  free(dir);
}

idl_retcode_t idl_parse_directive(idl_pstate_t *pstate, idl_token_t *tok)
{
  /* order is important here */
  if ((pstate->scanner.state & IDL_SCAN_LINE) == IDL_SCAN_LINE) {
    return parse_line(pstate, tok);
  } else if ((pstate->scanner.state & IDL_SCAN_KEYLIST) == IDL_SCAN_KEYLIST) {
    return parse_keylist(pstate, tok);
  } else if (pstate->scanner.state == IDL_SCAN_PRAGMA) {
    /* expect keylist */
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      if (strcmp(tok->value.str, "keylist") == 0) {
        idl_keylist_t *dir;
        if (!(dir = malloc(sizeof(*dir))))
          return IDL_RETCODE_NO_MEMORY;
        dir->symbol.mask = IDL_DIRECTIVE|IDL_KEYLIST;
        dir->symbol.location = tok->location;
        dir->symbol.destructor = &delete_keylist;
        dir->data_type = NULL;
        dir->keys = NULL;
        pstate->directive = (idl_symbol_t *)dir;
        pstate->scanner.state = IDL_SCAN_KEYLIST;
        return IDL_RETCODE_OK;
      }
      idl_error(pstate, &tok->location,
        "unsupported #pragma directive %s", tok->value.str);
      return IDL_RETCODE_SYNTAX_ERROR;
    }
  } else if (pstate->scanner.state == IDL_SCAN_DIRECTIVE_NAME) {
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      /* expect line or pragma */
      if (strcmp(tok->value.str, "line") == 0) {
        idl_line_t *dir;
        if (!(dir = malloc(sizeof(*dir))))
          return IDL_RETCODE_NO_MEMORY;
        dir->symbol.mask = IDL_DIRECTIVE|IDL_LINE;
        dir->symbol.location = tok->location;
        dir->symbol.destructor = &delete_line;
        dir->line = NULL;
        dir->file = NULL;
        dir->flags = 0;
        pstate->directive = (idl_symbol_t *)dir;
        pstate->scanner.state = IDL_SCAN_LINE;
        return 0;
      } else if (strcmp(tok->value.str, "pragma") == 0) {
        /* support #pragma keylist for backwards compatibility */
        pstate->scanner.state = IDL_SCAN_PRAGMA;
        return 0;
      }
    } else if (tok->code == '\n' || tok->code == '\0') {
      pstate->scanner.state = IDL_SCAN;
      return 0;
    }
  } else if (pstate->scanner.state == IDL_SCAN_DIRECTIVE) {
    /* expect # */
    if (tok->code == '#') {
      pstate->scanner.state = IDL_SCAN_DIRECTIVE_NAME;
      return 0;
    }
  }

  idl_error(pstate, &tok->location, "Invalid compiler directive");
  return IDL_RETCODE_SYNTAX_ERROR;
}
