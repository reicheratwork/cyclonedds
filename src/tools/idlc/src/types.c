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

//
// we need to offer 3 functions
//  1. generate
//  2. generator_options (optional)
//  3. generator_annotations (optional)
//

  // >> members with multiple declarators will result in multiple members
  //    with only a single declarator!
  //    >> for the opcodes though, that doesn't matter

// >> i like the way the c++ backend plays these tricks and the option
//    to write to multiple streams. so lets do that?!?!

#if 0

#define INDENT "%.*s"

struct idlc_generator {
  idl_stream_t *header_stream;
  idl_stream_t *source_stream;
  //uint32_t indent; << for c we don't need indentation... that's c++
};

static const char *
type_name(idlc_generator_t *gen, const idl_node_t *node)
{
  // generate name in buffer of generator!
  // >> we generate the node name only once and place it in a list
  //    the address of the node is the key, the name is the value
  //    the list is freed on exit of generate!
  // >> unless it's just a basic type, in that case we just return a static string
}

uint32_t array_size(const idl_const_expr_t *const_expr)
{
  // assert on wrong type!!!!!
  //   >> and too large value!
}

static idl_retcode_t
emit_field(
  const idl_pstate_t *pstate,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarators,
  void *user_data)
{
  const char *name, *sep = " ";
  const char *strpref = "", strsuf[32];
  idl_retcode_t ret;
  idl_generator_t *gen;
  const idl_declarator_t *declarator;
  const idl_constval_t *size;

  memset(strsuf, 0, sizeof(strsuf));
  //member = (const idl_member_t *)node;
  assert(member);
  if (!(type = type_name(gen, type_spec)))
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = idl_printf(gen->header, INDENT "  %3$s", gen->indent, "", type)) < 0)
    return ret;
  /* strings are special */
  if (idl_is_string(member->type_spec)) {
    uint32_t maximum = ((const idl_string_t *)type_spec)->maximum;
    /* bounded strings are fixed width arrays */
    if (maximum)
      idl_snprintf(strsuf, sizeof(strsuf), "%" PRIu32, maximum);
    /* unbounded strings are pointers */
    else
      strpref = "*";
  }
  declarator = declarators;
  do {
    name = idl_identifier(declarator);
    if ((ret = idl_printf(gen->header, "%s%s%s", sep, strpref, name, strsuf)))
      return ret;
    for (size = declarator->const_expr; size; size = idl_next(size)) {
      if ((ret = idl_printf(gen->header, "[%" PRIu32 "]", array_size(size))))
        return ret;
    }
    sep = ", ";
  } while ((declarator = idl_next(declarator)));
  if ((ret = idl_printf(gen->header, ";\n")) < 0)
    return ret;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_member(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  const idl_type_spec_t *type_spec;
  const idl_declarator_t *declarators;

  type_spec = ((const idl_member_t *)node)->type_spec;
  declarators = ((const idl_member_t *)node)->declarators;
  return print_field(pstate, type_spec, declarators, user_data);
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  const char header[] = "typedef struct %1$s %1$s;\n"
                        "struct %1$s {\n";
  const char footer[] = "};\n";
  const char topic[] = "extern const dds_topic_descriptor_t %1$s_desc;\n"
                       "\n"
                       "#define %1$s__alloc() \\\n"
                       "  ((%1$s*) dds_alloc (sizeof (%1$s)));\n"
                       "\n"
                       "#define %1$s_free(d,o) \\\n"
                       "  dds_sample_free ((d), &%1$s_desc, (o))\n"
  char *name;
  idl_retcode_t ret;
  idlc_generator_t *gen = user_data;
  idl_selector_t sel;
  const idl_member_t *sub = ((const idl_struct_t *)node)->members;

  sel = *filter;
  sel.mask = IDL_MEMBER;

  if ((name = type_name(gen, node)))
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = idl_printf(gen->header, header, name)) < 0)
    return ret;
  if ((ret = idl_visit(pstate, &sel, sub, &print_member, gen)))
    return ret;
  if ((ret = idl_printf(gen->header, footer)) < 0)
    return ret;
  if (idl_is_topic(node) && (ret = idl_printf(gen->header, topic, name)) < 0)
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_case(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  const idl_type_spec_t *type_spec;
  const idl_declarator_t *declarator;
 
  type_spec = ((idl_case_t *)node)->type_spec;
  declarator = ((idl_case_t *)node)->declarator;
  return print_field(pstate, type_spec, declarator, user_data);
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  const char header[] = "typedef struct %1$s\n"
                        "{\n"
                        "  %2$s _d;\n"
                        "  union\n"
                        "  {\n";
  const char footer[] = "  } _u;\n"
                        "} %1$s;\n"
                        "\n"
                        "#define %1$s__alloc() \\\n"
                        "((%1$s*) dds_alloc (sizeof (%1$s)))";

  const char *type, *switch_type;
  idl_filter_t filt;
  const idl_case_t *sub = ((const idl_union_t *)node)->cases;

  filt = *filter;
  filt.mask = IDL_CASE;
  filt.recurse = false;
  if ((type = type_name(gen, node)))
    return IDL_RETCODE_NO_MEMORY;
  if ((switch_type = type_name(gen, _union->switch_type_spec->type_spec)))
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = idl_printf(gen->header, header, type, switch_type)) < 0)
    return ret;
  if ((ret = idl_visit(pstate, &filt, sub, &print_case, gen)))
    return ret;
  if ((ret = idl_printf(gen->header, footer, type)) < 0)
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_enumerator(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  idl_retcode_t ret;
  const char *sep = "", fmt[] = "%3$s" INDENT "%4$s";
  const idl_enumerator_t *enumerator;
  idlc_generator_t *gen = user_data;

  enumerator = ((const idl_enum_t *)node)->enumerators;
  do {
    if ((ret = idl_printf(gen->header, fmt, gen->indent, "", sep, name)) < 0)
      return ret;
    sep = ",\n";
  } while ((enumerator = idl_next(enumerator)));
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_enum(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  const char header[] = INDENT "typedef enum %3$s\n"
                        INDENT "{\n";
  const char footer[] = INDENT "} %3$s;";
  const char *type;
  const idl_enumerator_t *sub = ((const idl_enum_t *)node)->enumerators;
  idl_filter_t filt;

  filt = *filter;
  filt.mask = IDL_ENUMERATOR;
  filt.recurse = false;
  if (!(type = type_name(gen, node)))
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = idl_printf(gen->header, header, gen->indent, "", type)) < 0)
    return ret;
  gen->indent += 2;
  if ((ret = idl_visit(pstate, &filt, &print_enumerator, gen)))
    return ret;
  gen->indent -= 2;
  if ((ret = idl_printf(gen->header, footer, gen->indent, "")) < 0)
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_constval(
  const idl_stream_t *stream,
  const idl_constval_t *constval)
{
  const char *name;

  switch (idl_type(constval)) {
    case IDL_CHAR:
      return idl_printf(stream, "'%c'", constval->value.chr);
    case IDL_BOOL:
      return idl_printf(stream, "%s", constval->value.bln ? "true" : "false");
    case IDL_INT8:
      return idl_printf(stream, "%" PRId8, constval->value.int8);
    case IDL_OCTET:
    case IDL_UINT8:
      return idl_printf(stream, "%" PRIu8, constval->value.uint8);
    case IDL_SHORT:
    case IDL_INT16:
      return idl_printf(stream, "%" PRId16, constval->value.int16);
    case IDL_USHORT:
    case IDL_UINT16:
      return idl_printf(stream, "%" PRIu16, constval->value.uint16);
    case IDL_LONG:
    case IDL_INT32:
      return idl_printf(stream, "%" PRId32, constval->value.int32);
    case IDL_ULONG:
    case IDL_UINT32:
      return idl_printf(stream, "%" PRIu32, constval->value.uint32);
    case IDL_LLONG:
    case IDL_INT64:
      return idl_printf(stream, "%" PRId64, constval->value.int64);
    case IDL_ULLONG:
    case IDL_UINT64:
      return idl_printf(stream, "%" PRIu64, constval->value.uint64);
    case IDL_FLOAT:
      return idl_printf(stream, "%.6f", constval->value.flt);
    case IDL_DOUBLE:
      return idl_printf(stream, "%f", constval->value.dbl);
    case IDL_LDOUBLE:
      return idl_printf(stream, "%lf", constval->value.ldbl);
    case IDL_STRING:
      return idl_printf(stream, "\"%s\"", constval->value.str);
    case IDL_ENUM:
      if (!(name = absolute_name(constval)))
        return IDL_RETCODE_NO_MEMORY;
      return idl_printf(stream, "%s", name);
    default:
      assert(0);
  }
}

static idl_retcode_t
emit_const(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  const char *type;
  const char *lparen = "", *rparen = "";
  const idl_const_expr_t *const_expr = ((const idl_const_t *)node)->const_expr;

  if (!(type = type_name(gen, node)))
    return IDL_RETCODE_NO_MEMORY;
  switch (idl_type(const_expr)) {
    case IDL_CHAR:
    case IDL_STRING:
      lparen = "(";
      rparen = ")";
      break;
    default:
      break;
  }
  if ((ret = idl_printf(gen->header, "#define %s %s", type, lparen)) < 0)
    return ret;
  if ((ret = emit_value(pstate, filter, const_expr, gen)))
    return ret;
  if ((ret = idl_printf(gen->header, "%s\n", rparen)) < 0)
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
generate_header(const idl_pstate_t *pstate, const idlc_generator_t *gen)
{
  //
  // .. implement ..
  //
}

static idl_retcode_t
emit_opcodes_base_type()
{
  // .. implement ..
}

static idl_retcode_t
emit_opcodes_struct(
  )
{
  idl_retcode_t ret;
  size_t total_size = 0u;

  /* embedded struct */
  if (idl_is_struct(node))
    return idl_visit(
      pstate, filter, ((const idl_struct_t *)node)->members, user_data);
  //
  // we're going to have issues with unions here!!!!
  //

  if (idl_is_member(node)) {
    //
  }
  // well... i guess we'll have a member here?!?!


  //
  // .. implement ..
  //
}


//
// FIXME: support generation of implicit sequences!!!!
//


static idl_retcode_t
emit_src_struct(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  void *user_data)
{
  //
  // .. implement ..
  // >> first we need to figure out which opcodes exist and for what...
  //
}

static idl_retcode_t
generate_source(const idl_pstate_t *pstate, const idlc_generator_t *gen)
{
  //
  // .. implement ..
  //
}
#endif

#include "types.h"
#include "descriptor.h"

int idlc_generate(const idl_pstate_t *pstate)
{
  idl_retcode_t ret;
  idl_node_t *node;

  fprintf(stderr, "arrived in %s\n", __func__);
  // quick test
  for (node = pstate->root; node; node = idl_next(node)) {
    if ((ret = emit_topic_descriptor(pstate, node, NULL)))
      return ret;
    //fprintf(stderr, "%p == %p? ret: %d\n", node, pstate->root, ret);
  }

  //
  // x. we must be able to generate to a memory buffer too for basic
  //    testing! >> nah, not really
  //    >> idl_stream comes into play here!
  //      >> nope... won't be necessary!!!!
  //
  // x. we'll probably have our own context here somewhere?!?!
  // x. >> and we'll definitely need to figure out the filenames here and place
  //       them in the context!
  // x. first of, we need to figure out the output file name.
  //    >> the user may want to specify the base name himself!
  //
  return ret;
}
