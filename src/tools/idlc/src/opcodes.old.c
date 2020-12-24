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

/* logic for generating (de)serialization opcodes */

static const char *type_opcodes[][3] = {
  { /* IDL_BOOL */
    NULL, NULL, NULL },
  { /* IDL_INT8 / IDL_CHAR */
    "DDS_OP_TYPE_1BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY | DDS_OP_FLAG_SGN" },
  { /* IDL_UINT8 / IDL_OCTET */
    "DDS_OP_TYPE_1BY",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY" },
  { /* IDL_INT16 / IDL_SHORT */
    "DDS_OP_TYPE_2BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_2BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_2BY | DDS_OP_FLAG_SGN" },
  { /* IDL_UINT16 / IDL_USHORT */
    "DDS_OP_TYPE_2BY",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_2BY",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_2BY" },
  { /* IDL_INT32 / IDL_LONG */
    "DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN" },
  { /* IDL_UINT32 / IDL_ULONG */
    "DDS_OP_TYPE_4BY",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY" },
  { /* IDL_INT64 / IDL_LLONG */
    "DDS_OP_TYPE_8BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_8BY | DDS_OP_FLAG_SGN",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_8BY | DDS_OP_FLAG_SGN" },
  { /* IDL_UINT64 / IDL_ULLONG */
    "DDS_OP_TYPE_8BY",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_8BY",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_8BY" },
  { /* IDL_FLOAT */
    "DDS_OP_TYPE_4BY | DDS_OP_FLAG_FP",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_FP",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_FP" },
  { /* IDL_DOUBLE */
    "DDS_OP_TYPE_8BY | DDS_OP_FLAG_FP",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_8BY | DDS_OP_FLAG_FP",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_8BY | DDS_OP_FLAG_FP" },
  { /* IDL_LDOUBLE */
    NULL, NULL, NULL },
  { /* IDL_STRING (unbounded) */
    "DDS_OP_TYPE_STR",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STR",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STR" },
  { /* IDL_STRING (bounded) */
    "DDS_OP_TYPE_BST",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_BST",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_BST" },
  { /* IDL_SEQUENCE (unbounded) */
    NULL, /* invalid */
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_SEQ",
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_ARR" /* typedef of array */ },
  { /* IDL_SEQUENCE (bounded) */
    NULL, NULL, NULL },
  { /* IDL_ENUM */
    NULL, NULL, NULL },
  { /* IDL_UNION */
    NULL, NULL, NULL },
  { /* IDL_STRUCT */
    NULL, /* invalid */
    "DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU",
    "DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STU" }
};

static const char *type_opcode(
  const idl_type_spec_t *type_spec, const idl_declarator_t *declarator)
{
  unsigned int subtype = 0;
  const idl_type_spec_t *subtype_spec;

  subtype_spec = type_spec = idl_unalias(type_spec);
  /* array takes precedence */
  if (declarator->const_expr) {
    subtype = 2;
  } else if ((i = (idl_is_sequence(type_spec)))) {
    /* sequences of array can be defined via a typedef */
    subtype_spec = idl_unalias(((const idl_sequence_t *)type_spec)->type_spec);
    if (idl_is_declarator(subtype_spec)) {
      subtype = 2;
      goto sequence;
    }
  }

  switch (idl_type(subtype_spec)) {
    case IDL_BOOL:
      return type_opcodes[0][subtype];
    case IDL_CHAR:
    case IDL_INT8:
      return type_opcodes[1][subtype];
    case IDL_OCTET:
    case IDL_UINT8:
      return type_opcodes[2][subtype];
    case IDL_SHORT:
    case IDL_INT16:
      return type_opcodes[3][subtype];
    case IDL_USHORT:
    case IDL_UINT16:
      return type_opcodes[4][subtype];
    case IDL_LONG:
    case IDL_INT32:
      return type_opcodes[5][subtype];
    case IDL_ULONG:
    case IDL_UINT32:
      return type_opcodes[6][subtype];
    case IDL_LLONG:
    case IDL_INT64:
      return type_opcodes[7][subtype];
    case IDL_ULLONG:
    case IDL_UINT64:
      return type_opcodes[8][subtype];
    case IDL_FLOAT:
      return type_opcodes[9][subtype];
    case IDL_DOUBLE:
      return type_opcodes[10][subtype];
    case IDL_LDOUBLE:
      return type_opcodes[11][subtype];
    case IDL_STRING:
      if (((const idl_string_t *)subtype_spec)->maximum)
        return type_opcodes[13][subtype];
      else
        return type_opcodes[12][subtype];
    case IDL_SEQUENCE:
sequence:
      assert(idl_is_sequence(type_spec));
      if (((const idl_sequence_t *)subtype_spec)->maximum)
        return type_opcodes[15][subtype];
      else
        return type_opcodes[14][subtype];
    case IDL_ENUM:
      abort();
    case IDL_UNION:
      abort();
    case IDL_STRUCT:
      assert(subtype);
      return type_opcodes[OPCODE_STRUCT][subtype];
    default:
      abort();
  }
}

// we could simply opt to build the list on stack!
//   >> could make it somewhat easier and make cleanup automatic...
// >> so, the offset of the key, is the offset in the ops array, not
//    in the eventual type!!!!
//    >> that makes things considerably easier!

//
// martijns aproach for keys by using two streams works pretty well
//   >> i could opt to do a list or use the streams directly as well
//   >> streams have their advantage, it's the simplest to manage
//

typedef struct topic_namespace topic_namespace_t;
struct topic_namespace { // << list that's kept on stack
  struct topic_namespace *next; // first has previous null
  const idl_name_t *name;
};

typedef struct topic_descriptor topic_descriptor_t;
struct topic_descriptor {
  struct {
    /* list of identifiers (e.g. for enclosing member) built-up on the stack */
    topic_namespace_t *first, *last;
  } namespaces;
  struct {
    size_t count;
    idl_stream_t *stream;
  } keys;
  struct {
    size_t count;
    idl_stream_t *stream;
  } opcodes;
  const idl_node_t *topic;
};

// >> this might just as well be an inline function
#define push_scope(ops, something) \
  do { /* something clever */ } while (0)

// >> and of course we should offer the opposite of push... pop

static idl_retcode_t
emit_topic_simple_type(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  const idl_member_t *member;
  const idl_type_spec_t *type_spec;
  const idl_declarator_t *declarator;
  topic_descriptor_t *desc;

  desc = user_data;
  member = (const idl_member_t *)node;
  type_spec = member->type_spec;
  declarator = member->declarators;
  do {
    size_t len = 0, cnt;
    bool key = false;
    const char *sep, *str;
    const char *ops[10]; /* use wide margin */
    const idl_topic_namespace_t *ns;
    ops[len++] = "DDS_OP_ADR";
    ops[len++] = op_type(type_spec, declarator);
    if ((key = idl_is_key(pstate, desc->topic, member, declarator)))
      ops[len++] = "DDS_OP_FLAG_KEY";
    assert((ops/ops[0]) > len);
    /* generate opcodes for member */
    sep = "  ";
    for (size_t pos=0; pos < len; pos++, sep = " | ") {
      assert(ops[pos]);
      if ((ret = idl_printf(desc->opcodes.stream, "%s%s", sep, ops[pos])))
        return ret;
    }
    cnt = desc->opcodes.count++;
    /* generate offsetof for member. e.g. offsetof(foo, bar.baz) */
    ns = desc->namespaces.first;
    assert(ns);
    str = ns->name->identifier;
    if ((ret = idl_printf(desc->opcodes.stream, ", offsetof(%s, ", str)) < 0)
      return ret;
    if (key && (ret = idl_printf(desc->keys.stream, "  { \"")) < 0)
      return ret;
    sep = "";
    for (ns = ns->next; ns; ns = ns->next) {
      str = ns->name->identifier;
      if ((ret = idl_printf(desc->opcodes.stream, "%s%s", sep, str)) < 0)
        return ret;
      if (key && (ret = idl_printf(desc->keys.stream, "%s%s", sep, str)) < 0)
        return ret;
      sep = ".";
    }
    str = idl_identifier(declarator);
    if ((ret = idl_printf(desc->opcodes.stream, "%s%s),", sep, str)) < 0)
      return ret;
    //
    // FIXME: for arrays
    //
    if (key && (ret = idl_printf(desc->keys.stream, "%s%s\", %zu", sep, str, cnt)) < 0)
      return ret;
    desc->opcodes.count++;
  } while ((declarator = idl_next(declarator)));

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_desc_string(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  const idl_string_t *type_spec;
  const idl_declarator_t *declarator;
  topic_descriptor_t *desc = user_data;

  if (idl_is_member(node)) {
    type_spec = ((const idl_member_t *)node)->type_spec;
    declarator = ((const idl_member_t *)node)->declarators;
  } else if (idl_is_case(node)) {
    type_spec = ((const idl_case_t *)node)->type_spec;
    declarator = ((const idl_case_t *)node)->declarator;
    assert(!idl_next(declarator));
  }

  declarator = member->declarators;
  do {
    bool key;
    size_t arr, len, max;
    const char *sep;
    const char *ops[4];
    const topic_namespace_t *spc;

    max = type_spec->maximum;
    arr = array_size(declarator->const_expr);
    ops[len++] = "DDS_OP_ADR";
    ops[len++] = type_opcode(type_spec, declarator);
    if ((key = idl_is_key(pstate, topic, node, declarator)))
      ops[len++] = "DDS_OP_FLAG_KEY";
    /* generate type opcode */
    sep = "  ";
    for (size_t cnt=0; cnt < len; cnt++) {
      if ((ret = idl_putf(desc->opcodes.stream, "%s%s", sep, ops[cnt])) < 0)
        return ret;
      sep = " | ";
    }
    cnt = desc->opcodes.count++;
    /* generate offsetof opcode and key */
    spc = desc->namespaces.first;
    str = spc->name->identifier;
    if ((ret = idl_putf(desc->opcodes.stream, ", offsetof(%s, ", str)) < 0)
      return ret;
    if (key && (ret = idl_puts(desc->keys.stream, "  { \"")) < 0)
      return ret;
    sep = "";
    for (spc = spc->next; spc; spc = spc->next) {
      str = spc->name->identifier;
      if ((ret = idl_putf(desc->opcodes.stream, "%s%s", sep, str)) < 0)
        return ret;
      if (key && (ret = idl_putf(desc->keys.stream, "%s%s", sep, str)) < 0)
        return ret;
      sep = ".";
    }
    str = idl_identifier(declarator);
    if ((ret = idl_putf(desc->opcodes.stream, "%s%s)", sep, str)) < 0)
      return ret;
    if ((ret = idl_putf(desc->keys.stream, "%s%s, %zu }\n", sep, str, cnt)) < 0)
      return ret;
    desc->opcodes.count++;
    desc->keys.count++;
    /* generate array size opcodes if applicable */
    if (arr && (ret = idl_putf(desc->opcodes.stream, ", %u, 0", arr)) < 0)
      return ret;
    desc->opcodes.count++;
    /* generate bound opcode */
    if ((ret = idl_putf(desc->opcodes.stream, ", %"PRIu32",\n", max+1)) < 0)
      return ret;
    desc->opcodes.count++;
  } while ((declarator = idl_next(declarator)));

  return IDL_RETCODE_OK;
}

//
// in case of a non-declared type... kind of...
//
// -----
// typedef char chars[2];
// struct s {
//   sequence<chars> c;
// }
// -----
//
static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  // .. implement ..
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  // .. implement ..
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  // .. implement ..
}

//
// I guess the extra functions are required because there's a type indirection
// which causes the normal offsetof trick to not work!
//
// yes, it's about indirections entirely!!!!
//   >> for each we generate the "normal" opcode.
//   >> then we see if the type it points towards needs to be further resolved or not!:w
//

static idl_retcode_t
emit_desc_sequence(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  //const idl_member_t *member; >> whether it's a member or not is of no importance!
  // well... assume it's a field in this case (normal case)
  const idl_sequence_t *sequence;
  const idl_type_spec_t *type_spec;
  const idl_declarator_t *declarator;

  const char *opcode = type_opcode(type_spec);

  /////////
  //
  // >> if the sequence is anonymous, it is generated ad-hoc where applicable!
  // >> we do have to figure out the name though...
  //
  /////////
  //
  // >> keep in mind sequence of array is possible via typedef!!!!
  // >> I believe the operation is the same as for sequence of structs?!?!
  // >> test with sequence to array of struct!
  //
  /////////

  // whether or not we need to actually generate complex opcodes depends on the
  //   subtype and the declarator!
  // >> if the declarator is an array, things become way more difficult
  // >> same is true if the type_spec of the sequence is special!

  //
  // >> we apparently don't have to support bounded sequences here...
  //
  // .. implement ..
  //
}

//
// this is a proxy that calls the more specialized versions. it's required
// because all members etc must be processed in order!!!!
//
static idl_retcode_t
emit_topic_declaration(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  struct opcodes *ops = user_data;

  assert(ops);
  //
  // we should have a separate function for the first time a struct is
  //   entered... much easier. we then just create a stacked name!
  //
  // >> and for inherited structs... we just pretend they're the first
  //    member out there!
  //
  // first we get the type!
  // we walk, but only as a list, not recursive!
  //   >> for the offsetof generation, we must keep the dotted-name
  //      >> so that's something that needs to go in the context?
  //        >> let's do a specialized type like frans did
  //
  //   >> ...
  //
  if (idl_is_struct(node)) {
    const idl_member_t *members = ((const idl_struct_t *)node)->members;
    topic_namespace_t namespace = { .next = NULL, .name = idl_name(node) };
    push_namespace(descriptor, namespace);
    ret = idl_visit(pstate, filter, members, &emit_descriptor, descriptor);
    pop_namespace(descriptor);
    return ret;
  } else if (idl_is_member(node)) {
    const idl_member_t *member = (const idl_member_t *)node;
    if (idl_is_base_type(member->type_spec))
      return idl_topic_simple_type(pstate, node, user_data);
    //else if (idl_is_string(member->type_spec))
    //  return
    return emit_opcodes_member(pstate, node, user_data);
  }
  //
  //
}


//
// the emit_topic_descriptor thingy should be available in a library I
// think!
//



























