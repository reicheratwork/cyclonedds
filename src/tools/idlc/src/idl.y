/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 * Copyright(c) 2019 Jeroen Koekkoek
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
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")
#endif

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsts/typetree.h"
%}

%code provides {
#include <stdarg.h>

#define YY_DECL \
  int idl_yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param, idl_parser_t *parser, yyscan_t yyscanner)

extern YY_DECL;

#define yyerror idl_yyerror
void idl_yyerror(YYLTYPE *yylloc, idl_parser_t *parser, const char *fmt, ...);
#define yywarning idl_yywarning
void idl_yywarning(YYLTYPE *yylloc, idl_parser_t *parser, const char *fmt, ...);
#define yystrtok idl_yystrtok
int idl_yystrtok(const char *str, bool nc);
}

%code requires {
#include "idl.h"
#include "tt_create.h"

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
      (Cur).first_file = YYRHSLOC(Rhs, 1).first_file; \
      (Cur).first_line = YYRHSLOC(Rhs, 1).first_line; \
      (Cur).first_column = YYRHSLOC(Rhs, 1).first_column; \
    } else { \
      (Cur).first_file = YYRHSLOC(Rhs, 0).last_file; \
      (Cur).first_line = YYRHSLOC(Rhs, 0).last_line; \
      (Cur).first_column = YYRHSLOC(Rhs, 0).last_column; \
    } \
    (Cur).last_line = YYRHSLOC(Rhs, N).last_line; \
    (Cur).last_column = YYRHSLOC(Rhs, N).last_column; \
  } while (0)

#define YYLLOC_INITIAL(Cur, File) \
  do { \
    (Cur).first_file = NULL; \
    (Cur).first_line = 0; \
    (Cur).first_column = 0; \
    (Cur).last_file = (File); \
    (Cur).last_line = 1; \
    (Cur).last_column = 1; \
  } while (0);
}

%union {
  ddsts_flags_t base_type_flags;
  ddsts_type_t *type_ptr;
  ddsts_literal_t literal;
  ddsts_identifier_t identifier;
  ddsts_scoped_name_t *scoped_name;
}

%define api.pure full
%define api.prefix {idl_yy}
%define api.push-pull push
%define parse.trace

%locations

%param { idl_parser_t *parser }
%initial-action { YYLLOC_INITIAL(@$, parser->files ? parser->files->name : NULL); }


%token-table

%start specification

%token <identifier>
  IDL_T_IDENTIFIER
  IDL_T_NOWS_IDENTIFIER

%token <literal>
  IDL_T_INTEGER_LITERAL

%type <base_type_flags>
  base_type_spec
  switch_type_spec
  floating_pt_type
  integer_type
  signed_int
  signed_tiny_int
  signed_short_int
  signed_long_int
  signed_longlong_int
  unsigned_int
  unsigned_tiny_int
  unsigned_short_int
  unsigned_long_int
  unsigned_longlong_int
  char_type
  wide_char_type
  boolean_type
  octet_type

%type <type_ptr>
  type_spec
  simple_type_spec
  template_type_spec
  sequence_type
  string_type
  wide_string_type
  fixed_pt_type
  map_type
  struct_type
  struct_def

%destructor { ddsts_free_type($$); } <type_ptr>

%type <scoped_name>
  scoped_name
  at_scoped_name

%destructor { ddsts_free_scoped_name($$); } <scoped_name>

%type <literal>
  positive_int_const
  literal
  const_expr

%destructor { ddsts_free_literal(&($$)); } <literal>

%type <identifier>
  simple_declarator
  identifier

/* standardized annotations */
%token IDL_T_AT "@"

/* scope operators, see idl.l for details */
%token IDL_T_SCOPE
%token IDL_T_SCOPE_L
%token IDL_T_SCOPE_R
%token IDL_T_SCOPE_LR

/* keywords */
%token IDL_T_MODULE "module"
%token IDL_T_CONST "const"
%token IDL_T_NATIVE "native"
%token IDL_T_STRUCT "struct"
%token IDL_T_TYPEDEF "typedef"
%token IDL_T_UNION "union"
%token IDL_T_SWITCH "switch"
%token IDL_T_CASE "case"
%token IDL_T_DEFAULT "default"
%token IDL_T_ENUM "enum"
%token IDL_T_UNSIGNED "unsigned"
%token IDL_T_FIXED "fixed"
%token IDL_T_SEQUENCE "sequence"
%token IDL_T_STRING "string"
%token IDL_T_WSTRING "wstring"

%token IDL_T_FLOAT "float"
%token IDL_T_DOUBLE "double"
%token IDL_T_SHORT "short"
%token IDL_T_LONG "long"
%token IDL_T_CHAR "char"
%token IDL_T_WCHAR "wchar"
%token IDL_T_BOOLEAN "boolean"
%token IDL_T_OCTET "octet"
%token IDL_T_ANY "any"

%token IDL_T_MAP "map"
%token IDL_T_BITSET "bitset"
%token IDL_T_BITFIELD "bitfield"
%token IDL_T_BITMASK "bitmask"

%token IDL_T_INT8 "int8"
%token IDL_T_INT16 "int16"
%token IDL_T_INT32 "int32"
%token IDL_T_INT64 "int64"
%token IDL_T_UINT8 "uint8"
%token IDL_T_UINT16 "uint16"
%token IDL_T_UINT32 "uint32"
%token IDL_T_UINT64 "uint64"

%token IDL_T_DOUBLE_COLON "::"

%token IDL_T_END 0 "end of file"

%%


/* Constant Declaration */

specification:
    definitions IDL_T_END
    { ddsts_accept(parser->context); YYACCEPT; }
  ;

definitions:
    definition definitions
  | definition
  ;

definition:
    module_dcl ';'
  | type_dcl ';'
  ;

module_dcl:
    "module" identifier
      {
        if (!ddsts_module_open(parser->context, $2)) {
          YYABORT;
        }
      }
    '{' definitions '}'
      { ddsts_module_close(parser->context); };

at_scoped_name:
    identifier
      {
        if (!ddsts_new_scoped_name(parser->context, 0, false, $1, &($$))) {
          YYABORT;
        }
      }
  | IDL_T_SCOPE_R identifier
      {
        if (!ddsts_new_scoped_name(parser->context, 0, true, $2, &($$))) {
          YYABORT;
        }
      }
  | at_scoped_name IDL_T_SCOPE_LR identifier
      {
        if (!ddsts_new_scoped_name(parser->context, $1, false, $3, &($$))) {
          YYABORT;
        }
      }
  ;

scope:
    IDL_T_SCOPE
  | IDL_T_SCOPE_L
  | IDL_T_SCOPE_R
  | IDL_T_SCOPE_LR
  ;

scoped_name:
    identifier
      {
        if (!ddsts_new_scoped_name(parser->context, 0, false, $1, &($$))) {
          YYABORT;
        }
      }
  | scope identifier
      {
        if (!ddsts_new_scoped_name(parser->context, 0, true, $2, &($$))) {
          YYABORT;
        }
      }
  | scoped_name scope identifier
      {
        if (!ddsts_new_scoped_name(parser->context, $1, false, $3, &($$))) {
          YYABORT;
        }
      }
  ;

const_expr:
    literal
  | '(' const_expr ')'
      { $$ = $2; };

literal:
    IDL_T_INTEGER_LITERAL
  ;

positive_int_const:
    const_expr;

type_dcl:
    constr_type_dcl
  ;

type_spec:
    simple_type_spec
  ;

simple_type_spec:
    base_type_spec
      {
        if (!ddsts_new_base_type(parser->context, $1, &($$))) {
          YYABORT;
        }
      }
  | scoped_name
      {
        if (!ddsts_get_type_from_scoped_name(parser->context, $1, &($$))) {
          YYABORT;
        }
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

/* Basic Types */
floating_pt_type:
    "float" { $$ = DDSTS_FLOAT; }
  | "double" { $$ = DDSTS_DOUBLE; }
  | "long" "double" { $$ = DDSTS_LONGDOUBLE; };

integer_type:
    signed_int
  | unsigned_int
  ;

signed_int:
    "short" { $$ = DDSTS_INT16; }
  | "long" { $$ = DDSTS_INT32; }
  | "long" "long" { $$ = DDSTS_INT64; }
  ;

unsigned_int:
    "unsigned" "short" { $$ = DDSTS_INT16 | DDSTS_UNSIGNED; }
  | "unsigned" "long" { $$ = DDSTS_INT32 | DDSTS_UNSIGNED; }
  | "unsigned" "long" "long" { $$ = DDSTS_INT64 | DDSTS_UNSIGNED; }
  ;

char_type:
    "char" { $$ = DDSTS_CHAR; };

wide_char_type:
    "wchar" { $$ = DDSTS_CHAR | DDSTS_WIDE; };

boolean_type:
    "boolean" { $$ = DDSTS_BOOLEAN; };

octet_type:
    "octet" { $$ = DDSTS_OCTET; };

template_type_spec:
    sequence_type
  | string_type
  | wide_string_type
  | fixed_pt_type
  | struct_type
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      {
        if (!ddsts_new_sequence(parser->context, $3, &($5), &($$))) {
          YYABORT;
        }
      }
  | "sequence" '<' type_spec '>'
      {
        if (!ddsts_new_sequence_unbound(parser->context, $3, &($$))) {
          YYABORT;
        }
      }
  ;

string_type:
    "string" '<' positive_int_const '>'
      {
        if (!ddsts_new_string(parser->context, &($3), &($$))) {
          YYABORT;
        }
      }
  | "string"
      {
        if (!ddsts_new_string_unbound(parser->context, &($$))) {
          YYABORT;
        }
      }
  ;

wide_string_type:
    "wstring" '<' positive_int_const '>'
      {
        if (!ddsts_new_wide_string(parser->context, &($3), &($$))) {
          YYABORT;
        }
      }
  | "wstring"
      {
        if (!ddsts_new_wide_string_unbound(parser->context, &($$))) {
          YYABORT;
        }
      }
  ;

fixed_pt_type:
    "fixed" '<' positive_int_const ',' positive_int_const '>'
      {
        if (!ddsts_new_fixed_pt(parser->context, &($3), &($5), &($$))) {
          YYABORT;
        }
      }
  ;

/* Annonimous struct extension: */
struct_type:
    "struct" '{'
      {
        if (!ddsts_add_struct_open(parser->context, NULL)) {
          YYABORT;
        }
      }
    members '}'
      { ddsts_struct_close(parser->context, &($$)); }
  ;

constr_type_dcl:
    struct_dcl
  | union_dcl
  ;

struct_dcl:
    struct_def
  | struct_forward_dcl
  ;

struct_def:
    "struct" identifier '{'
      {
        if (!ddsts_add_struct_open(parser->context, $2)) {
          YYABORT;
        }
      }
    members '}'
      { ddsts_struct_close(parser->context, &($$)); }
  ;
members:
    member members
  | member
  ;

member:
    annotation_appls type_spec
      {
        if (!ddsts_add_struct_member(parser->context, &($2))) {
          YYABORT;
        }
      }
    declarators ';'
      { ddsts_struct_member_close(parser->context); }
  | type_spec
      {
        if (!ddsts_add_struct_member(parser->context, &($1))) {
          YYABORT;
        }
      }
    declarators ';'
      { ddsts_struct_member_close(parser->context); }
/* Embedded struct extension: */
  | struct_def { ddsts_add_struct_member(parser->context, &($1)); }
    declarators ';'
  ;

struct_forward_dcl:
    "struct" identifier
      {
        if (!ddsts_add_struct_forward(parser->context, $2)) {
          YYABORT;
        }
      };

union_dcl:
    union_def
  | union_forward_dcl
  ;

union_def:
    "union" identifier
       {
         if (!ddsts_add_union_open(parser->context, $2)) {
           YYABORT ;
         }
       }
    "switch" '(' switch_type_spec ')'
       {
         if (!ddsts_union_set_switch_type(parser->context, $6)) {
           YYABORT ;
         }
       }
    '{' switch_body '}'
       { ddsts_union_close(parser->context); }
  ;
switch_type_spec:
    integer_type
  | char_type
  | boolean_type
  | scoped_name
      {
        if (!ddsts_get_base_type_from_scoped_name(parser->context, $1, &($$))) {
          YYABORT;
        }
      }
  ;

switch_body: cases ;
cases:
    case cases
  | case
  ;

case:
    case_labels element_spec ';'
  ;

case_labels:
    case_label case_labels
  | case_label
  ;

case_label:
    "case" const_expr ':'
      {
        if (!ddsts_union_add_case_label(parser->context, &($2))) {
          YYABORT;
        }
      }
  | "default" ':'
      {
        if (!ddsts_union_add_case_default(parser->context)) {
          YYABORT;
        }
      }
  ;

element_spec:
    type_spec
      {
        if (!ddsts_union_add_element(parser->context, &($1))) {
          YYABORT;
        }
      }
    declarator
  ;

union_forward_dcl:
    "union" identifier
      {
        if (!ddsts_add_union_forward(parser->context, $2)) {
          YYABORT;
        }
      }
  ;

array_declarator:
    identifier fixed_array_sizes
      {
        if (!ddsts_add_declarator(parser->context, $1)) {
          YYABORT;
        }
      }
  ;

fixed_array_sizes:
    fixed_array_size fixed_array_sizes
  | fixed_array_size
  ;

fixed_array_size:
    '[' positive_int_const ']'
      {
        if (!ddsts_add_array_size(parser->context, &($2))) {
          YYABORT;
        }
      }
  ;

simple_declarator: identifier ;

declarators:
    declarator ',' declarators
  | declarator
  ;

declarator:
    simple_declarator
      {
        if (!ddsts_add_declarator(parser->context, $1)) {
          YYABORT;
        }
      };


/* From Building Block Extended Data-Types: */
struct_def:
    "struct" identifier ':' scoped_name '{'
      {
        if (!ddsts_add_struct_extension_open(parser->context, $2, $4)) {
          YYABORT;
        }
      }
    members '}'
      { ddsts_struct_close(parser->context, &($$)); }
  | "struct" identifier '{'
      {
        if (!ddsts_add_struct_open(parser->context, $2)) {
          YYABORT;
        }
      }
    '}'
      { ddsts_struct_empty_close(parser->context, &($$)); }
  ;

template_type_spec:
     map_type
  ;

map_type:
    "map" '<' type_spec ',' type_spec ',' positive_int_const '>'
      {
        if (!ddsts_new_map(parser->context, $3, $5, &($7), &($$))) {
          YYABORT;
        }
      }
  | "map" '<' type_spec ',' type_spec '>'
      {
        if (!ddsts_new_map_unbound(parser->context, $3, $5, &($$))) {
          YYABORT;
        }
      }
  ;

signed_int:
    signed_tiny_int
  | signed_short_int
  | signed_long_int
  | signed_longlong_int
  ;

unsigned_int:
    unsigned_tiny_int
  | unsigned_short_int
  | unsigned_long_int
  | unsigned_longlong_int
  ;

signed_tiny_int: "int8" { $$ = DDSTS_INT8; };
unsigned_tiny_int: "uint8" { $$ = DDSTS_INT8 | DDSTS_UNSIGNED; };
signed_short_int: "int16" { $$ = DDSTS_INT16; };
signed_long_int: "int32" { $$ = DDSTS_INT32; };
signed_longlong_int: "int64" { $$ = DDSTS_INT64; };
unsigned_short_int: "uint16" { $$ = DDSTS_INT16 | DDSTS_UNSIGNED; };
unsigned_long_int: "uint32" { $$ = DDSTS_INT32 | DDSTS_UNSIGNED; };
unsigned_longlong_int: "uint64" { $$ = DDSTS_INT64 | DDSTS_UNSIGNED; };

/* From Building Block Anonymous Types: */
type_spec: template_type_spec ;
declarator: array_declarator ;


/* From Building Block Annotations (minimal for support of @key): */

annotation_appls:
    annotation_appl annotation_appls
  | annotation_appl
  ;

annotation_appl:
    "@" at_scoped_name
    {
      if (!ddsts_add_annotation(parser->context, $2)) {
        YYABORT;
      }
    }
  ;

identifier:
    IDL_T_IDENTIFIER
      {
        size_t off = 0;
        if ($1[0] == '_') {
          off = 1;
        } else if (yystrtok($1, 1) != -1) {
          yyerror(&yylloc, parser, "identifier '%s' collides with a keyword", $1);
          YYABORT;
        }
        if (!ddsts_context_copy_identifier(parser->context, $1 + off, &($$))) {
          YYABORT;
        }
      };

%%

void idl_yyerror(
  YYLTYPE *yylloc, idl_parser_t *parser, const char *fmt, ...)
{
  dds_return_t rc;
  va_list ap;

  DDSRT_UNUSED_ARG(parser);
  rc = ddsts_context_get_retcode(parser->context);
  if (rc == DDS_RETCODE_OK) {
    if (strcmp(fmt, "memory exhausted") == 0) {
      ddsts_context_set_retcode(parser->context, DDS_RETCODE_OUT_OF_RESOURCES);
    } else {
      ddsts_context_set_retcode(parser->context, DDS_RETCODE_BAD_SYNTAX);
    }
  }

  fprintf(stderr, "%s at %d.%d: ", yylloc->first_file, yylloc->first_line, yylloc->first_column);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
}

void idl_yywarning(
  YYLTYPE *yylloc, idl_parser_t *parser, const char *fmt, ...)
{
  va_list ap;

  assert(yylloc != NULL);
  (void)parser;
  assert(fmt != NULL);

  fprintf(stderr, "%s at %d.%d: ", yylloc->first_file, yylloc->first_line, yylloc->first_column);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
}

int idl_yystrtok(const char *str, bool nc)
{
  size_t i, n;
  int(*cmp)(const char *s1, const char *s2, size_t n);

  assert(str != NULL);

  cmp = (nc ? &ddsrt_strncasecmp : strncmp);
  for (i = 0, n = strlen(str); i < YYNTOKENS; i++) {
    if (yytname[i] != 0
        && yytname[i][    0] == '"'
        && cmp(yytname[i] + 1, str, n) == 0
        && yytname[i][n + 1] == '"'
        && yytname[i][n + 2] == '\0') {
      return yytoknum[i];
    }
  }

  return -1;
}