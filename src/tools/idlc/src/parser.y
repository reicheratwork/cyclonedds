/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
%{
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "idl.h"
#include "typetree.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
#endif

static void yyerror(idl_location_t *loc, idl_processor_t *proc, const char *);

static idl_node_t *alloc_node(idl_processor_t *proc, uint32_t flags)
{
  idl_node_t *node;
  if (!(node = malloc(sizeof(*node))))
    return NULL;
  memset(node, 0, sizeof(*node));
  node->flags = flags;
  node->weight = 1;
}

static idl_node_t *reference_node(idl_processor_t *proc, idl_node_t *node)
{
  assert(node);
  node->weight++;
}

static void free_node(idl_processor_t *proc, idl_node_t *node)
{
  // .. implement ..
  return;
}

static void push_node(idl_processor_t *proc, idl_node_t *node)
{
  assert(proc);
  assert(node);

  if (proc->tree.cursor) {
    idl_node_t *last, *cursor = proc->tree.cursor;
    /* node may actually be a list of nodes, e.g. in the case of struct
       members, locate last node first */
    for (last = node; last->next; last = last->next) ;
    assert(!last->next);
    assert(!node->previous);
    if (cursor->type.constr_type_dcl.children.last) {
      assert(cursor->type.constr_type_dcl.children.first);
      assert(!cursor->type.constr_type_dcl.children.first->previous);
      assert(!cursor->type.constr_type_dcl.children.last->next);
      cursor->type.constr_type_dcl.children.last->next = node;
      node->previous = cursor->type.constr_type_dcl.children.last;
    } else {
      assert(!cursor->type.constr_type_dcl.children.first);
      cursor->type.constr_type_dcl.children.first = node;
      node->previous = NULL;
    }
    cursor->type.constr_type_dcl.children.last = last;
  } else {
    proc->tree.root = node;
    proc->tree.cursor = node;
    assert(!node->parent);
    assert(!node->previous);
  }
}

static void pop_node(idl_processor_t *proc, idl_node_t *node)
{
  // .. implement ..
}

static void enter_scope(idl_processor_t *proc, idl_node_t *node)
{
  assert(proc);
  assert(node);
  /* node must be a module or a constructed type */
  assert(((node->flags & IDL_MODULE)) ||
         ((node->flags & IDL_CONSTR_TYPE) && !(node->flags & IDL_FLAG_FORWARD)));
  assert(node->parent == proc->tree.cursor || node == proc->tree.cursor);
  proc->tree.cursor = node;
}

static void exit_scope(idl_processor_t *proc, idl_node_t *node)
{
  assert(proc);
  assert(node);
  assert(node == proc->tree.cursor);
  if (node->parent) {
    assert(node != proc->tree.root);
    proc->tree.cursor = node->parent;
  } else {
    assert(node == proc->tree.root);
    proc->tree.cursor = NULL;
  }
}
%}

%code provides {
int idl_iskeyword(idl_processor_t *proc, const char *str, int nc);
}

%code requires {
/* convenience macro to complement YYABORT */
#define ABORT(proc, loc, ...) \
  do { idl_error(proc, loc, __VA_ARGS__); YYABORT; } while(0)
#define EXHAUSTED \
  do { goto yyexhaustedlab; } while (0)

/* Make yytoknum available */
#define YYPRINT(A,B,C) YYUSE(A)
/* Use YYLTYPE definition below */
#define IDL_YYLTYPE_IS_DECLARED

#define YYSTYPE IDL_YYSTYPE
#define YYLTYPE IDL_YYLTYPE

typedef struct idl_location YYLTYPE;

#define YYLLOC_DEFAULT(Cur, Rhs, N) \
  do { \
    if (N) { \
      (Cur).first.file = YYRHSLOC(Rhs, 1).first.file; \
      (Cur).first.line = YYRHSLOC(Rhs, 1).first.line; \
      (Cur).first.column = YYRHSLOC(Rhs, 1).first.column; \
    } else { \
      (Cur).first.file = YYRHSLOC(Rhs, 0).last.file; \
      (Cur).first.line = YYRHSLOC(Rhs, 0).last.line; \
      (Cur).first.column = YYRHSLOC(Rhs, 0).last.column; \
    } \
    (Cur).last.line = YYRHSLOC(Rhs, N).last.line; \
    (Cur).last.column = YYRHSLOC(Rhs, N).last.column; \
  } while (0)

#define YYLLOC_INITIAL(Cur, File) \
  do { \
    (Cur).first.file = NULL; \
    (Cur).first.line = 0; \
    (Cur).first.column = 0; \
    (Cur).last.file = (File); \
    (Cur).last.line = 1; \
    (Cur).last.column = 1; \
  } while (0);
}

%union {
  uint32_t base_type;
  idl_node_t *node;
  idl_node_t *scope;
  char *identifier;
  char *scoped_name;
  char *str;
  long long llng;
  unsigned long long ullng;
}

%define api.pure true
%define api.prefix {idl_yy}
%define api.push-pull push
%define parse.trace

%locations

%param { idl_processor_t *proc }
%initial-action { YYLLOC_INITIAL(@$, proc->files ? proc->files->name : NULL); }


%token-table

%start specification

%token IDL_TOKEN_LINE_COMMENT
%token IDL_TOKEN_COMMENT

%token <str> IDL_TOKEN_PP_NUMBER
%token <str> IDL_TOKEN_IDENTIFIER
%token <str> IDL_TOKEN_CHAR_LITERAL
%token <str> IDL_TOKEN_STRING_LITERAL
%token <ullng> IDL_TOKEN_INTEGER_LITERAL

%type <base_type>
  base_type_spec
  floating_pt_type
  integer_type
  signed_int
  unsigned_int
  char_type
  wide_char_type
  boolean_type
  octet_type

%type <node>
  simple_type_spec
  type_spec
  declarators
  member
  members
  struct_def
  struct_forward_dcl
  struct_dcl

%type <scoped_name>
  scoped_name

%type <identifier>
  declarator
  simple_declarator
  identifier

%token IDL_TOKEN_AT "@"

/* scope operators, see scanner.c for details */
%token IDL_TOKEN_SCOPE
%token IDL_TOKEN_SCOPE_L
%token IDL_TOKEN_SCOPE_R
%token IDL_TOKEN_SCOPE_LR

/* keywords */
%token IDL_TOKEN_MODULE "module"
%token IDL_TOKEN_CONST "const"
%token IDL_TOKEN_NATIVE "native"
%token IDL_TOKEN_STRUCT "struct"
%token IDL_TOKEN_TYPEDEF "typedef"
%token IDL_TOKEN_UNION "union"
%token IDL_TOKEN_SWITCH "switch"
%token IDL_TOKEN_CASE "case"
%token IDL_TOKEN_DEFAULT "default"
%token IDL_TOKEN_ENUM "enum"
%token IDL_TOKEN_UNSIGNED "unsigned"
%token IDL_TOKEN_FIXED "fixed"
%token IDL_TOKEN_SEQUENCE "sequence"
%token IDL_TOKEN_STRING "string"
%token IDL_TOKEN_WSTRING "wstring"

%token IDL_TOKEN_FLOAT "float"
%token IDL_TOKEN_DOUBLE "double"
%token IDL_TOKEN_SHORT "short"
%token IDL_TOKEN_LONG "long"
%token IDL_TOKEN_CHAR "char"
%token IDL_TOKEN_WCHAR "wchar"
%token IDL_TOKEN_BOOLEAN "boolean"
%token IDL_TOKEN_OCTET "octet"
%token IDL_TOKEN_ANY "any"

%token IDL_TOKEN_MAP "map"
%token IDL_TOKEN_BITSET "bitset"
%token IDL_TOKEN_BITFIELD "bitfield"
%token IDL_TOKEN_BITMASK "bitmask"

%token IDL_TOKEN_INT8 "int8"
%token IDL_TOKEN_INT16 "int16"
%token IDL_TOKEN_INT32 "int32"
%token IDL_TOKEN_INT64 "int64"
%token IDL_TOKEN_UINT8 "uint8"
%token IDL_TOKEN_UINT16 "uint16"
%token IDL_TOKEN_UINT32 "uint32"
%token IDL_TOKEN_UINT64 "uint64"

%%

/* Constant Declaration */

specification:
    definitions
  ;

definitions:
    definition
  | definitions definition
  ;

definition:
    module_dcl ';'
  | type_dcl ';'
  ;

module_dcl:
    "module" identifier '{'
      <scope>{
        if (!($$ = alloc_node(proc, IDL_MODULE)))
          EXHAUSTED;
        $$->name = $2;
        push_node(proc, $$);
        enter_scope(proc, $$);
      }
    definitions '}'
      { exit_scope(proc, $4); };

scope:
    IDL_TOKEN_SCOPE
  | IDL_TOKEN_SCOPE_L
  | IDL_TOKEN_SCOPE_R
  | IDL_TOKEN_SCOPE_LR
  ;

scoped_name:
    identifier
      { $$ = $1; }
  | scope identifier
      { if (asprintf(&$$, "::%s", $2) == -1)
          EXHAUSTED;
        free($2);
      }
  | scoped_name scope identifier
      { if (asprintf(&$$, "%s::%s", $1, $3) == -1)
          EXHAUSTED;
        free($1);
        free($3);
      }
  ;

type_dcl:
    constr_type_dcl
  ;

base_type_spec:
    integer_type
  | floating_pt_type
  | char_type
  | wide_char_type
  | boolean_type
  | octet_type
  ;

floating_pt_type:
    "float" { $$ = IDL_FLOAT; }
  | "double" { $$ = IDL_DOUBLE; }
  | "long" "double" { $$ = IDL_LDOUBLE; };

integer_type:
    signed_int
  | unsigned_int
  ;

signed_int:
    "short" { $$ = IDL_SHORT; }
  | "long" { $$ = IDL_LONG; }
  | "long" "long" { $$ = IDL_LLONG; }
  /* building block extended data-types */
  | "int8" { $$ = IDL_INT8; }
  | "int16" { $$ = IDL_INT16; }
  | "int32" { $$ = IDL_INT32; }
  | "int64" { $$ = IDL_INT64; }
  ;

unsigned_int:
    "unsigned" "short" { $$ = IDL_SHORT | IDL_FLAG_UNSIGNED; }
  | "unsigned" "long" { $$ = IDL_LONG | IDL_FLAG_UNSIGNED; }
  | "unsigned" "long" "long" { $$ = IDL_LLONG | IDL_FLAG_UNSIGNED; }
  /* building block extended data-types */
  | "uint8" { $$ = IDL_INT8 | IDL_FLAG_UNSIGNED; }
  | "uint16" { $$ = IDL_INT16 | IDL_FLAG_UNSIGNED; }
  | "uint32" { $$ = IDL_INT32 | IDL_FLAG_UNSIGNED; }
  | "uint64" { $$ = IDL_INT64 | IDL_FLAG_UNSIGNED; }
  ;

char_type:
    "char" { $$ = IDL_CHAR; };

wide_char_type:
    "wchar" { $$ = IDL_WCHAR; };

boolean_type:
    "boolean" { $$ = IDL_BOOL; };

octet_type:
    "octet" { $$ = IDL_OCTET; };

type_spec:
    simple_type_spec
  ;

simple_type_spec:
    base_type_spec
      { if (!($$ = alloc_node(proc, $1)))
          EXHAUSTED;
      }
  | scoped_name
      { idl_node_t *node;
        if (!($$ = alloc_node(proc, IDL_SCOPED_NAME)))
          EXHAUSTED;
        $$->name = $1;
        /* try to resolve scoped name. second pass may be required */
        if (!(node = idl_find_node(proc->tree.root, $$->name)))
          ABORT(proc, &@1, "scoped name '%s' cannot be resolved");
        else
          $$->type.scoped_name.reference = reference_node(proc, $$);
      }
  ;

constr_type_dcl:
    struct_dcl
  ;

struct_dcl:
    struct_def
  | struct_forward_dcl
  ;

struct_def:
    "struct" identifier '{'
      <scope>{
        if (!($$ = alloc_node(proc, IDL_STRUCT)))
          EXHAUSTED;
        $$->flags = IDL_STRUCT;
        $$->name = $2;
        push_node(proc, $$);
        enter_scope(proc, $$);
      }
    members '}'
      { $$ = $4;
        assert($$->type.constr_type_dcl.children.first == $5);
        exit_scope(proc, $$);
      }
  ;

members:
    member
  | members member
  ;

member:
    type_spec declarators ';'
      { for (idl_node_t *node = $2; node; node = node->next) {
          node->flags |= $1->flags;
          if (node->flags & IDL_SCOPED_NAME)
            node->type.member_dcl.reference = reference_node(proc, $1);
        }
        //free_node(proc, $1); pop_node();
        push_node(proc, $2);
        $$ = $2;
      }
  /* embedded-struct-def extension */
  | struct_def declarators ';'
      { if (!(proc->flags & IDL_FLAG_EMBEDDED_STRUCT_DEF))
          ABORT(proc, &@1, "embedded struct definitions are not allowed");
        /* do not clone and push struct node for given declarators. instead,
           generate struct members that reference it on-the-fly. notice that
           the original struct node does not have the member flag set */
        for (idl_node_t *node = $2; node; node = node->next) {
          node->flags |= IDL_STRUCT;
          node->type.member_dcl.reference = reference_node(proc, $1);
        }
        push_node(proc, $2);
        $$ = $2;
      }
  ;

struct_forward_dcl:
    "struct" identifier
      { if (!($$ = alloc_node(proc, IDL_STRUCT | IDL_FLAG_FORWARD)))
          EXHAUSTED;
      }
  ;

simple_declarator: identifier ;

declarators:
    declarator
      { assert((proc->tree.cursor->flags & IDL_STRUCT) == IDL_STRUCT);
        if (!($$ = alloc_node(proc, IDL_FLAG_MEMBER)))
          EXHAUSTED;
        $$->type.member_dcl.name = $1;
      }
  | declarators ',' declarator
      { idl_node_t *last;
        for (last = $1; last->next; last = last->next) ;
        if (!(last->next = alloc_node(proc, IDL_FLAG_MEMBER)))
          EXHAUSTED;
        last = last->next;
        last->type.member_dcl.name = $3;
        $$ = $1;
      }
  ;

declarator: simple_declarator ;

identifier:
    IDL_TOKEN_IDENTIFIER
      {
        size_t off = 0;
        if ($1[0] == '_')
          off = 1;
        else if (idl_iskeyword(proc, $1, 1))
          ABORT(proc, &@1, "identifier '%s' collides with a keyword", $1);
        if (!($$ = strdup(&$1[off])))
          EXHAUSTED;
      }
  ;

%%

#if defined(__GNUC__)
_Pragma("GCC diagnostic pop")
_Pragma("GCC diagnostic pop")
#endif

static void
yyerror(idl_location_t *loc, idl_processor_t *proc, const char *str)
{
  idl_error(proc, loc, str);
}

int32_t idl_iskeyword(idl_processor_t *proc, const char *str, int nc)
{
  int toknum = 0;
  int(*cmp)(const char *s1, const char *s2, size_t n);

  assert(str != NULL);

  cmp = (nc ? &strncasecmp : strncmp);

  for (size_t i = 0, n = strlen(str); i < YYNTOKENS && !toknum; i++) {
    if (yytname[i] != 0
        && yytname[i][    0] == '"'
        && cmp(yytname[i] + 1, str, n) == 0
        && yytname[i][n + 1] == '"'
        && yytname[i][n + 2] == '\0') {
      toknum = yytoknum[i];
    }
  }

  switch (toknum) {
    case IDL_TOKEN_INT8:
    case IDL_TOKEN_INT16:
    case IDL_TOKEN_INT32:
    case IDL_TOKEN_INT64:
    case IDL_TOKEN_UINT8:
    case IDL_TOKEN_UINT16:
    case IDL_TOKEN_UINT32:
    case IDL_TOKEN_UINT64:
      /* intX and uintX are considered keywords if and only if building block
         extended data-types is enabled */
      if (!(proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES))
        return 0;
      break;
    default:
      break;
  };

  return toknum;
}
