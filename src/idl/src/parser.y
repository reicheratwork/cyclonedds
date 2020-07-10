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
%{
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
#endif

#include "idl/processor.h"

static void yyerror(idl_location_t *loc, idl_processor_t *proc, const char *);
static idl_node_t *make_node(idl_processor_t *proc);
static idl_node_t *reference_node(idl_processor_t *proc, idl_node_t *);
static void free_node(idl_processor_t *proc, idl_node_t *node);
static void push_node(idl_processor_t *proc, idl_node_t *parent, idl_node_t *node);
static void link_node(idl_processor_t *proc, idl_node_t *previous, idl_node_t *next);
static void enter_scope(idl_processor_t *proc, idl_node_t *node);
static void exit_scope(idl_processor_t *proc, idl_node_t *node);
%}

%code provides {
#include "idl/processor.h"
int idl_iskeyword(idl_processor_t *proc, const char *str, int nc);
}

%code requires {
#include "idl/processor.h"
/* convenience macro to complement YYABORT */
#define ABORT(proc, loc, ...) \
  do { idl_error(proc, loc, __VA_ARGS__); YYABORT; } while(0)
#define EXHAUSTED \
  do { goto yyexhaustedlab; } while (0)

#define MAKE_NODE(lval, proc) \
  do { \
    if (!(lval = make_node(proc))) \
      EXHAUSTED; \
  } while (0)

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

%type <ullng>
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
  definition definitions
  module_dcl
  module_header
  const_expr
  primary_expr
  literal
  integer_literal
  positive_int_const
  type_spec
  simple_type_spec
  template_type_spec
  sequence_type
  string_type
  wide_string_type
  fixed_pt_type
  map_type
  struct_dcl
  struct_def
  struct_header
  member
  members
  struct_forward_dcl
  union_dcl
  union_def
  switch_type_spec
  switch_body
  case
  case_label
  case_labels
  element_spec
  union_forward_dcl
  enum_dcl
  enumerator enumerators
  declarators

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

%token IDL_TOKEN_TRUE "TRUE"
%token IDL_TOKEN_FALSE "FALSE"

%%

/* Constant Declaration */

specification:
    definitions
      { proc->tree.root = $1; }
  ;

definitions:
    definition
      { $$ = $1; }
  | definitions definition
      { link_node(proc, $1, $2);
        $$ = $1;
      }
  ;

definition:
    module_dcl ';'
  | type_dcl ';'
  ;

module_dcl:
    module_header '{' definitions '}'
    { push_node(proc, $1, $3);
      exit_scope(proc, $1);
      $$ = $1;
    }
  ;

module_header:
    "module" identifier
    { MAKE_NODE($$, proc);
      $$->flags = IDL_MODULE;
      $$->name = $2;
      $$->location = @1;
      enter_scope(proc, $$);
    }
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

scope:
    IDL_TOKEN_SCOPE
  | IDL_TOKEN_SCOPE_L
  | IDL_TOKEN_SCOPE_R
  | IDL_TOKEN_SCOPE_LR
  ;

const_expr:
    primary_expr
  ;

primary_expr:
    literal { $$ = $1; }
  | '(' const_expr ')' { $$ = $2; }
  ;

literal:
    integer_literal
  ;

integer_literal:
    IDL_TOKEN_INTEGER_LITERAL
      { MAKE_NODE($$, proc);
        $$->flags = IDL_LITERAL | IDL_LONG;
        $$->type.literal.integer = $1;
        $$->location = @1;
      }
  ;

positive_int_const:
    const_expr
  ;

type_dcl:
    constr_type_dcl
  ;

type_spec:
    simple_type_spec
  ;

simple_type_spec:
    base_type_spec
      { MAKE_NODE($$, proc);
        $$->flags = $1;
        $$->location = @1;
      }
  | scoped_name
      { MAKE_NODE($$, proc);
        $$->flags = IDL_SCOPED_NAME;
        $$->name = $1;
        $$->location = @1;
      }
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
    "unsigned" "short" { $$ = IDL_USHORT; }
  | "unsigned" "long" { $$ = IDL_ULONG; }
  | "unsigned" "long" "long" { $$ = IDL_ULLONG; }
  /* building block extended data-types */
  | "uint8" { $$ = IDL_UINT8; }
  | "uint16" { $$ = IDL_UINT16; }
  | "uint32" { $$ = IDL_UINT32; }
  | "uint64" { $$ = IDL_UINT64; }
  ;

char_type:
    "char" { $$ = IDL_CHAR; };

wide_char_type:
    "wchar" { $$ = IDL_WCHAR; };

boolean_type:
    "boolean" { $$ = IDL_BOOL; };

octet_type:
    "octet" { $$ = IDL_OCTET; };

template_type_spec:
    sequence_type
  | string_type
  | wide_string_type
  | fixed_pt_type
  /* building block extended data-types */
  | map_type
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_SEQUENCE_TYPE;
        $$->type.sequence.bound = $5;
        $$->location = @1;
        push_node(proc, $$, $3);
      }
  | "sequence" '<' type_spec '>'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_SEQUENCE_TYPE;
        $$->location = @1;
        push_node(proc, $$, $3);
      }
  ;

string_type:
    "string" '<' positive_int_const '>'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_STRING;
        $$->type.string.bound = $3;
        $$->location = @1;
      }
  | "string"
      { MAKE_NODE($$, proc);
        $$->flags = IDL_STRING; 
        $$->location = @1;
      }
  ;

wide_string_type:
    "wstring" '<' positive_int_const '>'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_WSTRING_TYPE;
        $$->type.wstring.bound = $3;
        $$->location = @1;
      }
  | "wstring"
      { MAKE_NODE($$, proc);
        $$->flags = IDL_WSTRING_TYPE;
        $$->location = @1;
      }
  ;

fixed_pt_type:
    "fixed" '<' positive_int_const ',' positive_int_const '>'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_FIXED_PT_TYPE;
        $$->location = @1;
        $$->type.fixed_pt.digits = digits;
        $$->type.fixed_pt.scale = scale;
      }
  ;

map_type:
    "map" '<' type_spec ',' type_spec ',' positive_int_const '>'
      { assert(proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES);
        MAKE_NODE($$, proc);
        $$->flags = IDL_MAP_TYPE;
        $$->type.map.bound = bound;
        $$->location = @1;
        push_node(proc, $$, $3);
        push_node(proc, $$, $5);
      }
  | "map" '<' type_spec ',' type_spec '>'
      { assert(proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES);
        MAKE_NODE($$, proc);
        $$->flags = IDL_MAP_TYPE;
        $$->location = @1;
        push_node(proc, $$, $3);
        push_node(proc, $$, $5);
      }
  ;

constr_type_dcl:
    struct_dcl
  | union_dcl
  | enum_dcl
  ;

struct_dcl:
    struct_def
  | struct_forward_dcl
  ;

struct_def:
    struct_header '{' members '}'
      { push_node(proc, $1, $3);
        exit_scope(proc, $1);
        $$ = $1;
      }
  ;

struct_header:
    "struct" identifier
      { MAKE_NODE($$, proc);
        $$->flags = IDL_STRUCT;
        $$->name = $2;
        $$->location = @1;
        enter_scope(proc, $$);
      }
  ;

members:
    member
      { $$ = $1; }
  | members member
      { link_node(proc, $1, $2);
        $$ = $1;
      }
  ;

member:
    type_spec declarators ';'
      { for (idl_node_t *n = $2; n; n = n->next) {
          n->flags |= $1->flags;
          n->location = $1->location;
          if ($1->flags == IDL_SCOPED_NAME) {
            assert($1->name);
            if (!(n->name = strdup($1->name)))
              EXHAUSTED;
          }
        }
        free_node(proc, $1);
        $$ = $2;
      }
  /* embedded-struct-def extension */
  | struct_def declarators ';'
      { if (!(proc->flags & IDL_FLAG_EMBEDDED_STRUCT_DEF))
          ABORT(proc, &@1, "embedded struct definitions are not allowed");
        /* do not clone and push struct node for given declarators. instead,
           generate struct members that reference it on-the-fly. notice that
           the original struct node does not have the member flag set */
        for (idl_node_t *n = $2; n; n = n->next) {
          n->flags |= IDL_STRUCT;
          n->children = reference_node(proc, $1);
        }
        link_node(proc, $1, $2);
        $$ = $1;
      }
  ;

struct_forward_dcl:
    "struct" identifier
      { MAKE_NODE($$, proc);
        $$->flags = IDL_FORWARD_DCL | IDL_STRUCT;
        $$->name = $2;
        $$->location = @1;
      }
  ;

union_dcl:
    union_def
  | union_forward_dcl
  ;

union_def:
    "union" identifier "switch" '(' switch_type_spec ')' '{' switch_body '}'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_UNION;
        $$->name = $2;
        $$->location = @1;
        push_node(proc, $$, $5);
        push_node(proc, $$, $8);
      }
  ;

switch_type_spec:
    integer_type
      { MAKE_NODE($$, proc);
        $$->flags = $1 | IDL_SWITCH_TYPE_SPEC;
        $$->location = @1;
      }
  | char_type
      { MAKE_NODE($$, proc);
        $$->flags = $1 | IDL_SWITCH_TYPE_SPEC;
        $$->location = @1;
      }
  | boolean_type
      { MAKE_NODE($$, proc);
        $$->flags = $1 | IDL_SWITCH_TYPE_SPEC;
        $$->location = @1;
      }
  | scoped_name
      { MAKE_NODE($$, proc);
        $$->flags = IDL_SCOPED_NAME | IDL_SWITCH_TYPE_SPEC;
        $$->name = $1;
        $$->location = @1;
      }
  ;

switch_body:
    case
  | switch_body case
  ;

case:
    case_labels element_spec ';'
      { push_node(proc, $2, $1);
        $$ = $2;
        // FIXME: warn for and ignore duplicate labels
        // FIXME: warn for and ignore for labels combined with default
      }
  ;

case_labels:
    case_label
      { $$ = $1; }
  | case_labels case_label
      { link_node(proc, $1, $2);
        $$ = $1;
      }
  ;

case_label:
    "case" const_expr ':'
      { $2->flags |= IDL_CASE_LABEL;
        $2->location = @1;
        $$ = $2;
      }
  | "default" ':'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_CASE_LABEL;
        $$->location = @1;
      }
  ;

element_spec:
    type_spec declarator
      { $1->flags |= IDL_CASE;
        $$->type._case.declarator = $2;
        $$ = $1;
      }
  ;

union_forward_dcl:
    "union" identifier
      { MAKE_NODE($$, proc);
        $$->flags = IDL_FORWARD_DCL | IDL_UNION;
        $$->name = $2;
        $$->location = @1;
      }
  ;

enum_dcl:
    "enum" identifier '{' enumerators '}'
      { MAKE_NODE($$, proc);
        $$->flags = IDL_ENUM;
        $$->name = $2;
        $$->location = @1;
        push_node(proc, $$, $4);
      }
  ;

enumerators:
    enumerator
      { $$ = $1; }
  | enumerators ',' enumerator
      { link_node(proc, $1, $3);
        $$ = $1;
      }
  ;

enumerator:
    identifier
      { MAKE_NODE($$, proc);
        $$->flags = IDL_ENUMERATOR;
        $$->name = $1;
        $$->location = @1;
      }
  ;

simple_declarator: identifier ;

declarators:
    declarator
      { MAKE_NODE($$, proc);
        $$->flags = IDL_MEMBER;
        $$->type.member.declarator = $1;
        $$->location = @1;
      }
  | declarators ',' declarator
      { MAKE_NODE($$, proc);
        $$->flags = IDL_MEMBER;
        $$->type.member.declarator = $3;
        $$->location = @3;
        link_node(proc, $1, $$);
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

static void push_node(idl_processor_t *proc, idl_node_t *parent, idl_node_t *node)
{
  idl_node_t *last;

  assert(proc);
  assert(parent);
  assert(node);

  (void)proc;
  /* node may actually be a list of nodes, e.g. in the case of struct members,
     and may not point to the first node in the list, e.g. a struct with
     preprended pragmas */
  for (; node->previous; node = node->previous) ;

  if (parent->children) {
    for (last = parent->children; last->next; last = last->next) ;
    last->next = node;
    node->previous = last;
  } else {
    parent->children = node;
  }

  for (; node; node = node->next) {
    assert(!node->parent);
    assert(!node->next || node == node->next->previous);
    node->parent = parent;
  }
}

static idl_node_t *reference_node(idl_processor_t *proc, idl_node_t *node)
{
  (void)proc;
  assert(node);
  node->weight++;
  return node;
}

static void free_node(idl_processor_t *proc, idl_node_t *node)
{
  (void)proc;
  (void)node;
  // .. implement ..
  return;
}

static void link_node(idl_processor_t *proc, idl_node_t *list, idl_node_t *node)
{
  (void)proc;
  idl_node_t *last;
  for (last = list; last->next; last = last->next) ;
  last->next = node;
  node->previous = last;
}

static idl_node_t *make_node(idl_processor_t *proc)
{
  idl_node_t *node;
  (void)proc;
  if (!(node = malloc(sizeof(*node))))
    return NULL;
  memset(node, 0, sizeof(*node));
  return node;
}

static void enter_scope(idl_processor_t *proc, idl_node_t *node)
{
  assert(proc);
  assert(node);
  /* node must be a module or a constructed type */
  assert(((node->flags & IDL_MODULE)) ||
         ((node->flags & IDL_CONSTR_TYPE) && !(node->flags & IDL_FORWARD_DCL)));
  /* check if any pragma nodes are ready to merge */
  if (proc->tree.pragmas) {
    idl_node_t *last;
    for (last = proc->tree.pragmas; last->next; last = last->next) ;
    last->next = node;
    node->previous = last;
    proc->tree.pragmas = NULL;
  }
}

static void exit_scope(idl_processor_t *proc, idl_node_t *node)
{
  assert(proc);
  assert(node);

  if (proc->tree.pragmas) {
    push_node(proc, node, proc->tree.pragmas);
    proc->tree.pragmas = NULL;
  }
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
    case IDL_TOKEN_MAP:
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
