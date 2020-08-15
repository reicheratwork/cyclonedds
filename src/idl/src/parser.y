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

#include "idl/tree.h"
#include "idl/string.h"
#include "idl/processor.h"
#include "table.h"
#include "expression.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
#endif

static void yyerror(idl_location_t *loc, idl_processor_t *proc, const char *);
static void merge(void *parent, void *member, void *node);
static void annotate(void *node, idl_annotation_appl_t *ann);
static void locate(void *node, idl_position_t *floc, idl_position_t *lloc);
static void push(void *list, void *node);
static void *reference(void *node);

#define MAKE(lval, floc, lloc, ctor, ...) \
  do { \
    if (!(lval = (void*)ctor(__VA_ARGS__))) \
      goto yyexhaustedlab; \
    locate(lval, floc, lloc); \
  } while (0)

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
  void *node;
  /* expressions */
  idl_literal_t *literal;
  idl_binary_expr_t *binary_expr;
  idl_unary_expr_t *unary_expr;
  idl_variant_t *variant;
  /* simple specifications */
  idl_mask_t kind;
  char *scoped_name;
  char *identifier;
  char *string_literal;
  /* specifications */
  idl_sequence_type_t *sequence_type;
  idl_string_type_t *string_type;
  /* declarations */
  idl_definition_t *definition;
  idl_module_t *module_dcl;
  idl_struct_type_t *struct_dcl;
  idl_struct_forward_dcl_t *struct_forward_dcl;
  idl_member_t *member;
  idl_declarator_t *declarator;
  //idl_array_size_t *array_size;
  idl_union_type_t *union_dcl;
  idl_union_forward_dcl_t *union_forward_dcl;
  idl_case_t *_case;
  idl_case_label_t *case_label;
  idl_enum_type_t *enum_dcl;
  idl_enumerator_t *enumerator;
  idl_typedef_dcl_t *typedef_dcl;
  idl_const_dcl_t *const_dcl;
  /* annotations */
  idl_annotation_appl_t *annotation_appl;
  idl_annotation_appl_param_t *annotation_appl_param;

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

%type <node> definitions definition type_dcl type_spec simple_type_spec
             template_type_spec constr_type_dcl struct_dcl union_dcl enum_dcl
             constr_type switch_type_spec const_expr const_type primary_expr
             fixed_array_sizes fixed_array_size
%type <variant> positive_int_const
%type <binary_expr> or_expr xor_expr and_expr shift_expr add_expr mult_expr
%type <unary_expr> unary_expr
%type <kind> unary_operator base_type_spec floating_pt_type integer_type
             signed_int unsigned_int char_type wide_char_type boolean_type
             octet_type
%type <literal> literal boolean_literal
%type <identifier> identifier
%type <scoped_name> scoped_name at_scoped_name
%type <string_literal> string_literal
%type <sequence_type> sequence_type
%type <string_type> string_type
%type <module_dcl> module_dcl module_header
%type <struct_dcl> struct_def struct_header
%type <struct_forward_dcl> struct_forward_dcl
%type <member> members member
%type <union_dcl> union_def
%type <_case> switch_body case element_spec
%type <case_label> case_labels case_label
%type <union_forward_dcl> union_forward_dcl
%type <enum_dcl> enum_def
%type <enumerator> enumerators enumerator
%type <declarator> declarators declarator simple_declarator
                   complex_declarator array_declarator
%type <typedef_dcl> typedef_dcl
%type <const_dcl> const_dcl
%type <annotation_appl> annotation_appls annotation_appl
%type <annotation_appl_param> annotation_appl_params annotation_appl_param

/* comments and line comments are not evaluated by the parser, but unique
   token identifiers are required to avoid clashes */
%token IDL_TOKEN_LINE_COMMENT
%token IDL_TOKEN_COMMENT
%token <str> IDL_TOKEN_PP_NUMBER
%token <str> IDL_TOKEN_IDENTIFIER
%token <str> IDL_TOKEN_CHAR_LITERAL
%token <str> IDL_TOKEN_STRING_LITERAL
%token <ullng> IDL_TOKEN_INTEGER_LITERAL


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
      { if (!idl_add_symbol(proc, idl_scope(proc), idl_identifier($1), $1))
          goto yyexhaustedlab;
        $$ = $1; }
  | definitions definition
      { if (!idl_add_symbol(proc, idl_scope(proc), idl_identifier($1), $2))
          goto yyexhaustedlab;
        push($1, $2);
        $$ = $1;
      }
  ;

definition:
    annotation_appls module_dcl ';'
      { annotate($2, $1);
        $$ = $2;
      }
  | annotation_appls const_dcl ';'
      { annotate($2, $1);
        $$ = $2;
      }
  | annotation_appls type_dcl ';'
      { annotate($2, $1);
        $$ = $2;
      }
  ;

module_dcl:
    module_header definitions '}'
      { idl_exit_scope(proc, $1->identifier);
        locate($1, &@1.first, &@3.last);
        merge($$, &$$->definitions, $2);
      }
  ;

module_header:
    "module" identifier '{'
      { if (!idl_enter_scope(proc, $2))
          goto yyexhaustedlab;
        MAKE($$, &@1.first, &@2.last, idl_create_module);
        $$->identifier = $2;
      }
  ;

scoped_name:
    identifier
      { $$ = $1; }
  | scope identifier
      { if (idl_asprintf(&$$, "::%s", $2) == -1)
          goto yyexhaustedlab;
        free($2);
      }
  | scoped_name scope identifier
      { if (idl_asprintf(&$$, "%s::%s", $1, $3) == -1)
          goto yyexhaustedlab;
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

const_dcl:
    "const" const_type identifier '=' const_expr
      { MAKE($$, &@1.first, &@5.last, idl_create_const_dcl);
        $$->identifier = $3;
        merge($$, &$$->type_spec, $2);
        merge($$, &$$->const_expr, $5);
      }
  ;

const_type:
    integer_type
      { MAKE($$, &@1.first, &@1.last, idl_create_base_type, $1); }
  | boolean_type
      { MAKE($$, &@1.first, &@1.last, idl_create_base_type, $1); }
  ;

const_expr: or_expr { $$ = $1; } ;

or_expr:
    xor_expr
  | or_expr '|' xor_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_OR_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  ;

xor_expr:
    and_expr
  | or_expr '^' and_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_XOR_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  ;

and_expr:
    shift_expr
  | and_expr '&' shift_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_AND_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  ;

shift_expr:
    add_expr
  | shift_expr ">>" add_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_RSHIFT_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  | shift_expr "<<" add_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_LSHIFT_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  ;

add_expr:
    mult_expr
  | add_expr '+' mult_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_ADD_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  | add_expr '-' mult_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_SUB_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  ;

mult_expr:
    unary_expr
      { $$ = (void *)$1; }
  | mult_expr '*' unary_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_MULT_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  | mult_expr '/' unary_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_DIV_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  | mult_expr '%' unary_expr
      { MAKE($$, &@2.first, &@2.last, idl_create_binary_expr, IDL_MOD_EXPR);
        merge($$, &$$->left, $1);
        merge($$, &$$->right, $3);
      }
  ;

unary_expr:
    unary_operator primary_expr
      { MAKE($$, &@1.first, &@1.last, idl_create_unary_expr, $1);
        merge($$, &$$->right, $2);
      }
  | primary_expr
      { $$ = $1; }
  ;

unary_operator:
    '-' { $$ = IDL_MINUS_EXPR; }
  | '+' { $$ = IDL_PLUS_EXPR; }
  | '~' { $$ = IDL_NOT_EXPR; }
  ;

primary_expr:
    scoped_name
      { const idl_symbol_t *sym;
        if (!(sym = idl_find_symbol(proc, NULL, $1, NULL)))
          ABORT(proc, &@1, "scoped name %s cannot be resolved", $1);
        if (sym->node->mask != (IDL_DECL | IDL_CONST) &&
            sym->node->mask != (IDL_DECL | IDL_ENUMERATOR))
          ABORT(proc, &@1, "scoped name %s does not resolve to a constant", $1);
        $$ = reference((void *)sym->node);
        free($1);
      }
  | literal
      { $$ = $1; }
  | '(' const_expr ')'
      { $$ = $2; }
  ;

literal:
    IDL_TOKEN_INTEGER_LITERAL
      { MAKE($$, &@1.first, &@1.last, idl_create_integer_literal, $1); }
  | boolean_literal
  | string_literal
      { MAKE($$, &@1.first, &@1.last, idl_create_string_literal, $1); }
  ;

boolean_literal:
    IDL_TOKEN_TRUE
      { MAKE($$, &@1.first, &@1.last, idl_create_boolean_literal, true); }
  | IDL_TOKEN_FALSE
      { MAKE($$, &@1.first, &@1.last, idl_create_boolean_literal, false); }
  ;

string_literal:
    IDL_TOKEN_STRING_LITERAL
      { if (!($$ = strdup($1)))
          goto yyexhaustedlab;
      }
  ;

positive_int_const:
    const_expr
      { idl_intval_t intval;
        if (idl_eval_int_expr(proc, &intval, $1, IDL_LLONG) != 0)
          YYABORT;
        if (intval.negative)
          ABORT(proc, idl_location($1), "size must be greater than zero");
        MAKE($$, &@1.first, &@1.last, idl_create_const_ullng, intval.value.ullng);
        idl_delete($1);
      }
  ;

type_dcl:
    constr_type_dcl { $$ = $1; }
  | typedef_dcl { $$ = $1; }
  ;

type_spec:
    simple_type_spec
  /* building block anonymous types */
  | template_type_spec
  /* embedded-struct-def extension */
  | constr_type
  ;

simple_type_spec:
    base_type_spec
      { MAKE($$, &@1.first, &@1.last, idl_create_base_type, $1); }
  | scoped_name
      { const idl_symbol_t *sym;
        if (!(sym = idl_find_symbol(proc, idl_scope(proc), $1, NULL)))
          ABORT(proc, &@1, "scoped name %s cannot be resolved", $1);
        if (!(sym->node->mask & IDL_TYPE_SPEC))
          ABORT(proc, &@1, "scoped name %s does not resolve to a type", $1);
        $$ = reference((void *)sym->node);
        free($1);
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
  | "long" "double" { $$ = IDL_LDOUBLE; }
  ;

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
    sequence_type { $$ = $1; }
  | string_type { $$ = $1; }
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      { MAKE($$, &@1.first, &@6.last, idl_create_sequence_type);
        merge($$, &$$->type_spec, $3);
        merge($$, &$$->const_expr, $5);
      }
  | "sequence" '<' type_spec '>'
      { MAKE($$, &@1.first, &@4.last, idl_create_sequence_type);
        merge($$, &$$->type_spec, $3);
      }
  ;

string_type:
    "string" '<' positive_int_const '>'
      { MAKE($$, &@1.first, &@4.last, idl_create_string_type);
        merge($$, &$$->const_expr, $3);
      }
  | "string"
      { MAKE($$, &@1.first, &@1.last, idl_create_string_type); }
  ;

//
// this is not default in IDL 4.0!!!!
//
constr_type:
    struct_def { $$ = $1; }
  | union_def { $$ = $1; }
  | enum_def { $$ = $1; }
  ;

constr_type_dcl:
    struct_dcl
  | union_dcl
  | enum_dcl
  ;

struct_dcl:
    struct_def { $$ = $1; }
  | struct_forward_dcl { $$ = $1; }
  ;

struct_def:
    struct_header members '}'
      { idl_exit_scope(proc, $1->identifier);
        locate($1, &@1.first, &@3.last);
        merge($$, &$$->members, $2);
      }
  ;

struct_header:
    "struct" identifier '{'
      { if (!idl_enter_scope(proc, $2))
          goto yyexhaustedlab;
        MAKE($$, &@1.first, &@1.last, idl_create_struct);
        $$->identifier = $2;
      }
  ;

members:
    member
      { $$ = $1; }
  | members member
      { push($1, $2);
        $$ = $1;
      }
  ;

member:
    annotation_appls type_spec declarators ';'
      { MAKE($$, &@2.first, &@4.last, idl_create_member);
        annotate($$, $1);
        merge($$, &$$->type_spec, $2);
        merge($$, &$$->declarators, $3);
      }
  ;

///* embedded-struct-def extension */
//  | annotation_appls struct_def declarators ';'
//      { if (!(proc->flags & IDL_FLAG_EMBEDDED_STRUCT_DEF))
//          ABORT(proc, &@1, "embedded struct definitions are not allowed");
//        /* FIXME: check no array declarators are used, unless embedded array
//                  definitions or building block anonymous types are enabled */
//        MAKE_NODE($$, &@2, idl_create_member);
//        PUSH_NODE($$, &$$->node.annotations, $1);
//        PUSH_NODE($$, &$$->type_spec, $2);
//        PUSH_NODE($$, &$$->declarators, $3);
//      }
//  ;

struct_forward_dcl:
    "struct" identifier
      { MAKE($$, &@1.first, &@2.last, idl_create_struct_forward_dcl);
        $$->identifier = $2;
      }
  ;

union_dcl:
    union_def { $$ = $1; }
  | union_forward_dcl { $$ = $1; }
  ;

union_def:
    "union" identifier "switch" '(' annotation_appls switch_type_spec ')' '{' switch_body '}'
      { MAKE($$, &@1.first, &@10.last, idl_create_union);
        annotate($6, $5);
        $$->identifier = $2;
        merge($$, &$$->switch_type_spec, $6);
        merge($$, &$$->cases, $9);
      }
  ;

switch_type_spec:
    integer_type
      { MAKE($$, &@1.first, &@1.last, idl_create_base_type, $1); }
  | char_type
      { MAKE($$, &@1.first, &@1.last, idl_create_base_type, $1); }
  | boolean_type
      { MAKE($$, &@1.first, &@1.last, idl_create_base_type, $1); }
  | scoped_name
      { const idl_symbol_t *sym;
        if (!(sym = idl_find_symbol(proc, NULL, $1, NULL)))
          ABORT(proc, &@1, "scoped name %s cannot be resolved", $1);
        if ((sym->node->mask & IDL_BASE_TYPE) != IDL_BASE_TYPE)
          ABORT(proc, &@1, "scoped name %s does not resolve to a base type", $1);
        $$ = reference((void *)sym->node);
        free($1);
      }
  ;

switch_body:
    case
      { $$ = $1; }
  | switch_body case
      { push($1, $2);
        $$ = $1;
      }
  ;

case:
    case_labels element_spec ';'
      { merge($2, &$2->case_labels, $1);
        // FIXME: warn for and ignore duplicate labels
        // FIXME: warn for and ignore for labels combined with default
        $$ = $2;
      }
  ;

case_labels:
    case_label
      { $$ = $1; }
  | case_labels case_label
      { push($1, $2);
        $$ = $1;
      }
  ;

case_label:
    "case" const_expr ':'
      { MAKE($$, &@1.first, &@2.last, idl_create_case_label);
        merge($$, &$$->const_expr, $2);
      }
  | "default" ':'
      { MAKE($$, &@1.first, &@1.last, idl_create_case_label); }
  ;

element_spec:
    type_spec declarator
      { MAKE($$, &@1.first, &@2.last, idl_create_case);
        merge($$, &$$->type_spec, $1);
        merge($$, &$$->declarator, $2);
      }
  ;

union_forward_dcl:
    "union" identifier
      { MAKE($$, &@1.first, &@2.last, idl_create_union_forward_dcl);
        $$->identifier = $2;
      }
  ;

enum_dcl: enum_def { $$ = $1; } ;

enum_def:
    "enum" identifier '{' enumerators '}'
      { MAKE($$, &@1.first, &@5.last, idl_create_enum);
        for (idl_node_t *n = (idl_node_t *)$4; n; n = n->next) {
          const char *scope = idl_scope(proc);
          const char *identifier = ((idl_enumerator_t *)n)->identifier;
          assert(identifier);
          if (idl_add_symbol(proc, scope, identifier, n))
            continue;
          free($$);
          goto yyexhaustedlab;
        }
        $$->identifier = $2;
        merge($$, &$$->enumerators, $4);
      }
  ;

enumerators:
    enumerator
      { $$ = $1; }
  | enumerators ',' enumerator
      { push($1, $3);
        $$ = $1;
      }
  ;

enumerator:
    annotation_appl identifier
      { MAKE($$, &@2.first, &@2.last, idl_create_enumerator);
        annotate($$, $1);
        $$->identifier = $2;
      }
  ;

array_declarator:
    identifier fixed_array_sizes
      { MAKE($$, &@1.first, &@2.last, idl_create_declarator);
        $$->identifier = $1;
        merge($$, &$$->array_sizes, $2);
      }
  ;

fixed_array_sizes:
    fixed_array_size
      { $$ = $1; }
  | fixed_array_sizes fixed_array_size
      { push($1, $2);
        $$ = $1;
      }
  ;

fixed_array_size:
    '[' positive_int_const ']'
      { $$ = $2; }
  ;

simple_declarator:
    identifier
      { MAKE($$, &@1.first, &@1.last, idl_create_declarator);
        $$->identifier = $1;
      }
  ;

complex_declarator: array_declarator ;

typedef_dcl:
    "typedef" type_spec declarators
      { MAKE($$, &@1.first, &@3.last, idl_create_typedef_dcl);
        for (idl_node_t *n = (idl_node_t *)$3; n; n = n->next) {
          const char *scope = idl_scope(proc);
          const char *identifier = ((idl_declarator_t *)n)->identifier;
          assert(identifier);
          if (idl_add_symbol(proc, scope, identifier, n))
            continue;
          free($$);
          goto yyexhaustedlab;
        }
        merge($$, &$$->type_spec, $2);
        merge($$, &$$->declarators, $3);
      }
  ;

declarators:
    declarator
      { $$ = $1; }
  | declarators ',' declarator
      { push($1, $3);
        $$ = $1;
      }
  ;

declarator:
    simple_declarator
  | complex_declarator
  ;

identifier:
    IDL_TOKEN_IDENTIFIER
      {
        size_t off = 0;
        if ($1[0] == '_')
          off = 1;
        else if (idl_iskeyword(proc, $1, 1))
          ABORT(proc, &@1, "identifier '%s' collides with a keyword", $1);
        if (!($$ = strdup(&$1[off])))
          goto yyexhaustedlab;
      }
  ;

annotation_appls:
      { $$ = NULL; }
  | annotation_appl
      { $$ = $1; }
  | annotation_appls annotation_appl
      { push($1, $2);
        $$ = $1;
      }

annotation_appl:
    "@" at_scoped_name '(' const_expr ')'
      { MAKE($$, &@1.first, &@4.last, idl_create_annotation_appl);
        $$->scoped_name = $2;
        merge($$, &$$->parameters, $4);
      }
  | "@" at_scoped_name '(' annotation_appl_params ')'
      { MAKE($$, &@1.first, &@5.last, idl_create_annotation_appl);
        $$->scoped_name = $2;
        merge($$, &$$->parameters, $4);
      }
  | "@" at_scoped_name
      { MAKE($$, &@1.first, &@2.last, idl_create_annotation_appl);
        $$->scoped_name = $2;
      }
  ;

at_scoped_name:
    identifier
      { $$ = $1; }
  | IDL_TOKEN_SCOPE_R identifier
      { if (idl_asprintf(&$$, "::%s", $2) == -1)
          goto yyexhaustedlab;
        free($2);
      }
  | at_scoped_name IDL_TOKEN_SCOPE_LR identifier
      { if (idl_asprintf(&$$, "%s::%s", $1, $3) == -1)
          goto yyexhaustedlab;
        free($1);
        free($3);
      }
  ;

annotation_appl_params:
    annotation_appl_param
      { $$ = $1; }
  | annotation_appl_params ',' annotation_appl_param
      { push($1, $3);
        $$ = $1;
      }
  ;

annotation_appl_param:
    identifier '=' const_expr
      { MAKE($$, &@1.first, &@3.last, idl_create_annotation_appl_param);
        $$->identifier = $1;
        merge($$, &$$->const_expr, $3);
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

static void merge(void *parent, void *member, void *node)
{
  idl_node_t *next;

  assert(parent);
  assert(member);
  assert(node);

  next = (idl_node_t *)node;
  *(idl_node_t **)member = next;
  for (; next; next = next->next)
    next->parent = (idl_node_t *)parent;
}

static void annotate(void *node, idl_annotation_appl_t *annot)
{
  assert(node);
  if (!annot)
    return;
  ((idl_node_t *)node)->annotations = annot;
}

static void locate(void *node, idl_position_t *floc, idl_position_t *lloc)
{
  assert(node);
  assert(floc);
  assert(lloc);
  ((idl_node_t *)node)->location.first = *floc;
  ((idl_node_t *)node)->location.last = *lloc;
}

static void push(void *list, void *node)
{
  idl_node_t *last;

  assert(list);
  assert(node);

  for (last = (idl_node_t *)list; last->next; last = last->next) ;
  last->next = (idl_node_t *)node;
  ((idl_node_t *)node)->previous = last;
}

static void *reference(void *node)
{
  ((idl_node_t *)node)->weight++;
  return node;
}

int32_t idl_iskeyword(idl_processor_t *proc, const char *str, int nc)
{
  int toknum = 0;
  int(*cmp)(const char *s1, const char *s2, size_t n);

  assert(str != NULL);

  cmp = (nc ? &idl_strncasecmp : strncmp);

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
