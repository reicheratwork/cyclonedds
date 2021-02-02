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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "idl/processor.h"
#include "idl/stream.h"
#include "idl/string.h"

#include "generator.h"
#include "descriptor.h"
#include "dds/ddsc/dds_opcodes.h"

#define TYPE (16)
#define SUBTYPE (8)

#define MAX_SIZE (16)

extern char *typename(const void *node);
extern char *absolute_name(const void *node, const char *separator);

static const uint16_t nop = UINT16_MAX;

/* store each instruction separately for easy post processing and reduced
   complexity. arrays and sequences introduce a new scope and the relative
   offset to the next field is stored with the instructions for the respective
   field. this requires the generator to revert its position. using separate
   streams intruduces too much complexity. the table is also used to generate
   a key offset table after the fact */
struct instruction {
  enum {
    OPCODE,
    OFFSET,
    SIZE,
    CONSTANT,
    COUPLE,
    SINGLE,
  } type;
  union {
    uint32_t opcode;
    struct {
      char *type;
      char *member;
    } offset; /**< name of type and member to generate offsetof */
    struct {
      char *type;
    } size; /**< name of type to generate sizeof */
    struct {
      char *value;
    } constant;
    struct {
      uint16_t high;
      uint16_t low;
    } couple;
    uint32_t single;
  } data;
};

struct field {
  struct field *previous;
  const void *node;
};

struct type {
  struct type *previous;
  struct field *fields;
  const void *node;
  uint32_t offset;
  uint32_t label, labels;
};

struct alignment {
  int value;
  int ordering;
  const char *rendering;
};

struct descriptor {
  const idl_node_t *topic;
  const struct alignment *alignment; /**< alignment of topic type */
  uint32_t keys; /**< number of keys in topic */
  uint32_t opcodes; /**< number of opcodes in descriptor */
  uint32_t flags; /**< topic descriptor flag values */
  struct type *types;
  struct {
    uint32_t size; /**< available number of instructions */
    uint32_t count; /**< used number of instructions */
    struct instruction *table;
  } instructions;
};

static const struct alignment alignments[] = {
#define ALIGNMENT_1BY (&alignments[0])
  { 1, 0, "1u" },
#define ALIGNMENT_2BY (&alignments[1])
  { 2, 2, "2u" },
#define ALIGNMENT_4BY (&alignments[2])
  { 4, 4, "4u" },
#define ALIGNMENT_PTR (&alignments[3])
  { 0, 6, "sizeof (char *)" },
#define ALIGNMENT_8BY (&alignments[4])
  { 8, 8, "8u" }
};

static const struct alignment *
max_alignment(const struct alignment *a, const struct alignment *b)
{
  if (!a)
    return b;
  if (!b)
    return a;
  return b->ordering > a->ordering ? b : a;
}

static idl_retcode_t push_field(
  struct descriptor *descriptor, const void *node, struct field **fieldp)
{
  struct type *type;
  struct field *field;
  assert(descriptor);
  assert(idl_is_declarator(node) ||
         idl_is_switch_type_spec(node) ||
         idl_is_case(node));
  type = descriptor->types;
  assert(type);
  if (!(field = calloc(1, sizeof(*field))))
    goto err_field;
  field->previous = type->fields;
  field->node = node;
  type->fields = field;
  if (fieldp)
    *fieldp = field;
  for (; type; type = type->previous)
    if (!idl_is_struct(type->node))
      return IDL_RETCODE_OK;
  return IDL_RETCODE_OK;
err_field:
  return IDL_RETCODE_NO_MEMORY;
}

static void pop_field(struct descriptor *descriptor)
{
  struct field *field;
  struct type *type;
  assert(descriptor);
  type = descriptor->types;
  assert(type);
  field = type->fields;
  type->fields = field->previous;
  free(field);
}

static idl_retcode_t push_type(
  struct descriptor *descriptor, const void *node, struct type **typep)
{
  struct type *type;
  assert(descriptor);
  assert(idl_is_struct(node) ||
         idl_is_union(node) ||
         idl_is_sequence(node) ||
         idl_is_array(node));
  if (!(type = calloc(1, sizeof(*type))))
    return IDL_RETCODE_NO_MEMORY;
  type->previous = descriptor->types;
  type->node = node;
  descriptor->types = type;
  /* can access fields in embedded structs and unions */
  if (type->previous && idl_is_constr_type(type->previous->node))
    type->fields = type->previous->fields;
  if (typep)
    *typep = type;
  return IDL_RETCODE_OK;
}

static void pop_type(struct descriptor *descriptor)
{
  struct type *type;
  assert(descriptor);
  assert(descriptor->types);
  type = descriptor->types;
  descriptor->types = type->previous;
  free(type);
}

static idl_retcode_t
stash_instruction(
  struct descriptor *descriptor, uint32_t index, const struct instruction *inst)
{
  /* make more slots available as necessary */
  if (descriptor->instructions.count == descriptor->instructions.size) {
    uint32_t size = descriptor->instructions.size + 100;
    struct instruction *table = descriptor->instructions.table;
    if (!(table = realloc(table, size * sizeof(*table))))
      return IDL_RETCODE_NO_MEMORY;
    descriptor->instructions.size = size;
    descriptor->instructions.table = table;
  }

  if (index >= descriptor->instructions.count) {
    index = descriptor->instructions.count;
  } else {
    size_t size = descriptor->instructions.count - index;
    struct instruction *table = descriptor->instructions.table;
    memmove(&table[index+1], &table[index], size * sizeof(*table));
  }

  descriptor->instructions.table[index] = *inst;
  descriptor->instructions.count++;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
stash_opcode(
  struct descriptor *descriptor, uint32_t index, uint32_t opcode)
{
  uint32_t type = 0;
  struct instruction inst = { OPCODE, { .opcode = opcode } };
  const struct alignment *alignment = NULL;

  descriptor->opcodes++;
  switch ((opcode & (0xffu<<24))) {
    case DDS_OP_ADR:
      if (opcode & DDS_OP_FLAG_KEY)
        descriptor->keys++;
      /* fall through */
    case DDS_OP_JEQ:
      type = (opcode >> 16) & 0xffu;
      if (type == DDS_OP_VAL_ARR)
        type = (opcode >> 8) & 0xffu;
      break;
    default:
      return stash_instruction(descriptor, index, &inst);
  }

  switch (type) {
    case DDS_OP_VAL_STR:
    case DDS_OP_VAL_SEQ:
      alignment = ALIGNMENT_PTR;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_BST:
      alignment = ALIGNMENT_1BY;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_8BY:
      alignment = ALIGNMENT_8BY;
      break;
    case DDS_OP_VAL_4BY:
      alignment = ALIGNMENT_4BY;
      break;
    case DDS_OP_VAL_2BY:
      alignment = ALIGNMENT_2BY;
      break;
    case DDS_OP_VAL_1BY:
      alignment = ALIGNMENT_1BY;
      break;
    case DDS_OP_VAL_UNI:
      /* strictly speaking a topic with a union can be optimized if all
         members have the same size, and if the non-basetype members are all
         optimizable themselves, and the alignment of the discriminant is not
         less than the alignment of the members */
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE | DDS_TOPIC_CONTAINS_UNION;
      break;
    default:
      break;
  }

  descriptor->alignment = max_alignment(descriptor->alignment, alignment);
  return stash_instruction(descriptor, index, &inst);
}

static idl_retcode_t
stash_offset(
  struct descriptor *descriptor,
  uint32_t index,
  const struct type *type,
  const struct field *field)
{
  size_t cnt, pos, len;
  const char *ident;
  const struct field *fld;
  struct instruction inst = { OFFSET, { .offset = { NULL, NULL } } };

  assert(type);

  if (!idl_is_struct(type->node) && !idl_is_union(type->node))
    return stash_instruction(descriptor, index, &inst);

  assert(field);
  if (!(inst.data.offset.type = typename(type->node)))
    goto err_type;

  len = 0;
  for (fld=field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else
      ident = idl_identifier(fld->node);
    len += strlen(ident);
    if (!fld->previous)
      break;
    len += strlen(".");
  }

  pos = len;
  if (!(inst.data.offset.member = malloc(len + 1)))
    goto err_member;

  inst.data.offset.member[pos] = '\0';
  for (fld=field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else
      ident = idl_identifier(fld->node);
    cnt = strlen(ident);
    assert(pos >= cnt);
    pos -= cnt;
    memcpy(inst.data.offset.member+pos, ident, cnt);
    if (!fld->previous)
      break;
    assert(pos > 1);
    pos -= 1;
    inst.data.offset.member[pos] = '.';
  }
  assert(pos == 0);

  if (stash_instruction(descriptor, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.offset.member);
err_member:
  free(inst.data.offset.type);
err_type:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_size(
  struct descriptor *descriptor, uint32_t index, const struct field *field)
{
  const void *node;
  const idl_type_spec_t *type_spec;
  struct instruction inst = { SIZE, { .size = { NULL } } };

  node = field->node;
  type_spec = idl_unalias(idl_type_spec(field->node), 0u);

  if (idl_is_sequence(type_spec) && !idl_is_array(node)) {
    type_spec = idl_unalias(idl_type_spec(type_spec), 0u);

    /* sequence of array */
    if (idl_is_array(type_spec)) {
      char buf[1], *str = NULL;
      const char *fmt = "[%" PRIu32 "]", *name;
      size_t len = 0, pos;
      ssize_t cnt;
      const idl_type_spec_t *node;
      const idl_literal_t *literal;
      /* sequence of (multi-)dimensional array requires sizes in a sizeof */
      for (node = type_spec; node; node = idl_type_spec(node)) {
        if (!idl_is_declarator(node))
          break;
        literal = ((const idl_declarator_t *)node)->const_expr;
        for (; literal; literal = idl_next(literal)) {
          assert(idl_type(literal) == IDL_ULONG);
          cnt = idl_snprintf(buf, sizeof(buf), fmt, literal->value.uint32);
          assert(cnt > 0);
          len += (size_t)cnt;
        }
      }
      name = typename(node);
      len += strlen(name);
      if (!(inst.data.size.type = malloc(len + 1)))
        goto err_type;
      pos = strlen(name);
      memcpy(str, name, pos);
      for (node = type_spec; node; node = idl_type_spec(node)) {
        if (!idl_is_declarator(node))
          break;
        literal = ((const idl_declarator_t *)node)->const_expr;
        for(; literal && pos < len; literal = idl_next(literal)) {
          cnt = idl_snprintf(str+pos, (len+1)-pos, fmt, literal->value.uint32);
          assert(cnt > 0);
          pos += (size_t)cnt;
        }
      }
      assert(pos == len && !literal);
    } else if (!(inst.data.size.type = typename(type_spec))) {
      return IDL_RETCODE_NO_MEMORY;
    }
  } else {
    assert(idl_is_array(node) || idl_is_array(type_spec));
    type_spec = idl_unalias(type_spec, IDL_UNALIAS_IGNORE_ARRAY);
    if (!(inst.data.size.type = typename(type_spec)))
      goto err_type;
  }

  if (stash_instruction(descriptor, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.size.type);
err_type:
  return IDL_RETCODE_NO_MEMORY;
}

/* used to stash case labels. no need to take into account strings etc */
static idl_retcode_t
stash_constant(
  struct descriptor *descriptor, uint32_t index, const idl_const_expr_t *const_expr)
{
  int cnt;
  struct instruction inst = { CONSTANT, { .constant = { NULL } } };
  char **strp = &inst.data.constant.value;

  if (idl_is_enumerator(const_expr)) {
    *strp = typename(const_expr);
  } else {
    const idl_literal_t *literal = const_expr;

    switch (idl_type(const_expr)) {
      case IDL_CHAR:
        cnt = idl_asprintf(strp, "'%c'", literal->value.chr);
        break;
      case IDL_BOOL:
        cnt = idl_asprintf(strp, "%s", literal->value.bln ? "true" : "false");
        break;
      case IDL_INT8:
        cnt = idl_asprintf(strp, "%" PRId8, literal->value.int8);
        break;
      case IDL_OCTET:
      case IDL_UINT8:
        cnt = idl_asprintf(strp, "%" PRIu8, literal->value.uint8);
        break;
      case IDL_SHORT:
      case IDL_INT16:
        cnt = idl_asprintf(strp, "%" PRId16, literal->value.int16);
        break;
      case IDL_USHORT:
      case IDL_UINT16:
        cnt = idl_asprintf(strp, "%" PRIu16, literal->value.uint16);
        break;
      case IDL_LONG:
      case IDL_INT32:
        cnt = idl_asprintf(strp, "%" PRId32, literal->value.int32);
        break;
      case IDL_ULONG:
      case IDL_UINT32:
        cnt = idl_asprintf(strp, "%" PRIu32, literal->value.uint32);
        break;
      case IDL_LLONG:
      case IDL_INT64:
        cnt = idl_asprintf(strp, "%" PRId64, literal->value.int64);
        break;
      case IDL_ULLONG:
      case IDL_UINT64:
        cnt = idl_asprintf(strp, "%" PRIu64, literal->value.uint64);
        break;
      default:
        break;
    }
  }

  if (!strp || cnt < 0)
    goto err_value;
  if (stash_instruction(descriptor, index, &inst))
    goto err_stash;
  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.constant.value);
err_value:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_couple(
  struct descriptor *desc, uint32_t index, uint16_t high, uint16_t low)
{
  struct instruction inst = { COUPLE, { .couple = { high, low } } };
  return stash_instruction(desc, index, &inst);
}

static idl_retcode_t
stash_single(
  struct descriptor *desc, uint32_t index, uint32_t single)
{
  struct instruction inst = { SINGLE, { .single = single } };
  return stash_instruction(desc, index, &inst);
}

static uint32_t typecode(const idl_type_spec_t *type_spec, unsigned int shift)
{
  assert(shift == 8 || shift == 16);
  if (idl_is_array(type_spec))
    return (DDS_OP_VAL_ARR << shift);
  type_spec = idl_unalias(type_spec, 0u);
  assert(!idl_is_typedef(type_spec));
  switch (idl_type(type_spec)) {
    case IDL_CHAR:
      return (DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_BOOL:
      return (DDS_OP_VAL_1BY << shift);
    case IDL_INT8:
      return (DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_OCTET:
    case IDL_UINT8:
      return (DDS_OP_VAL_1BY << shift);
    case IDL_SHORT:
    case IDL_INT16:
      return (DDS_OP_VAL_2BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_USHORT:
    case IDL_UINT16:
      return (DDS_OP_VAL_2BY << shift);
    case IDL_LONG:
    case IDL_INT32:
      return (DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_ULONG:
    case IDL_UINT32:
      return (DDS_OP_VAL_4BY << shift);
    case IDL_LLONG:
    case IDL_INT64:
      return (DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_ULLONG:
    case IDL_UINT64:
      return (DDS_OP_VAL_8BY << shift);
    case IDL_FLOAT:
      return (DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
    case IDL_DOUBLE:
      return (DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
    case IDL_LDOUBLE:
      /* long doubles are not supported yet */
      return 0u;
    case IDL_STRING:
      if (idl_is_bounded(type_spec))
        return (DDS_OP_VAL_BST << shift);
      return (DDS_OP_VAL_STR << shift);
    case IDL_SEQUENCE:
      /* bounded sequences are not supported */
      if (idl_is_bounded(type_spec))
        abort();
      return (DDS_OP_VAL_SEQ << shift);
    case IDL_ENUM:
      return (DDS_OP_VAL_4BY << shift);
    case IDL_UNION:
      return (DDS_OP_VAL_UNI << shift);
    case IDL_STRUCT:
      return (DDS_OP_VAL_STU << shift);
    default:
      break;
  }
  return 0u;
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;

  (void)pstate;
  (void)path;
  if (revisit) {
    pop_type(descriptor);
    return IDL_RETCODE_OK;
  } else {
    if ((ret = push_type(descriptor, node, NULL)))
      return ret;
    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_case(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;

  (void)pstate;
  (void)path;
  if (revisit) {
    if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
      return ret;
    pop_field(descriptor);
  } else {
    bool simple = true;
    uint32_t off, cnt;
    uint32_t opcode = DDS_OP_JEQ;
    const idl_case_t *_case = node;
    const idl_case_label_t *case_label;
    const idl_type_spec_t *type_spec;
    struct type *type = descriptor->types;

    type_spec = idl_unalias(idl_type_spec(node), 0u);

    /* simple elements are embedded, complex elements are not */
    if (idl_is_array(_case->declarator)) {
      opcode |= DDS_OP_SUBTYPE_ARR;
      simple = false;
    } else {
      opcode |= typecode(type_spec, SUBTYPE);
      if (idl_is_array(type_spec) || !idl_is_base_type(type_spec))
        simple = false;
    }

    if ((ret = push_field(descriptor, _case, NULL)))
      return ret;
    if ((ret = push_field(descriptor, _case->declarator, NULL)))
      return ret;

    cnt = descriptor->instructions.count + (type->labels - type->label) * 3;
    case_label = _case->case_labels;
    for (; case_label; case_label = idl_next(case_label)) {
      off = type->offset + 2 + (type->label * 3);
      /* update offset to first instruction for complex cases */
      if (!simple)
        opcode = (opcode & ~0xffffu) | (cnt - off);
      /* generate union case opcode */
      if ((ret = stash_opcode(descriptor, off++, opcode)))
        return ret;
      /* generate union case discriminator */
      if ((ret = stash_constant(descriptor, off++, case_label->const_expr)))
        return ret;
      /* generate union case offset */
      if ((ret = stash_offset(descriptor, off++, type, type->fields)))
        return ret;
      type->label++;
    }

    pop_field(descriptor); /* field readded by declarator for complex types */

    return IDL_VISIT_REVISIT | (simple ? IDL_VISIT_DONT_RECURSE : 0);
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_switch_type_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  uint32_t opcode, version = IDL4;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;
  struct field *field = NULL;
  struct type *type = descriptor->types;

  (void)revisit;

  type_spec = idl_unalias(idl_type_spec(node), 0u);
  assert(!idl_is_typedef(type_spec) && !idl_is_array(type_spec));

  if ((ret = push_field(descriptor, node, &field)))
    return ret;

  opcode = DDS_OP_ADR | DDS_OP_TYPE_UNI | typecode(type_spec, SUBTYPE);
  version = (pstate->flags & IDL35) ? IDL35 : IDL4;
  if (idl_is_topic_key(descriptor->topic, version, path))
    opcode |= DDS_OP_FLAG_KEY;
  if ((ret = stash_opcode(descriptor, nop, opcode)))
    return ret;
  if ((ret = stash_offset(descriptor, nop, type, field)))
    return ret;
  pop_field(descriptor);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct type *type;

  (void)pstate;
  (void)path;
  if (revisit) {
    uint32_t cnt;
    type = descriptor->types;
    assert(type->label == type->labels);
    cnt = (descriptor->instructions.count - type->offset) + 2;
    if ((ret = stash_single(descriptor, type->offset+2, type->labels)))
      return ret;
    if ((ret = stash_couple(descriptor, type->offset+3, (uint16_t)cnt, 4u)))
      return ret;
    pop_type(descriptor);
    return IDL_RETCODE_OK;
  } else {
    const idl_case_t *_case;
    const idl_case_label_t *case_label;

    if ((ret = push_type(descriptor, node, &type)))
      return ret;
    type->offset = descriptor->instructions.count;
    type->labels = type->label = 0;

    /* determine total number of case labels as opcodes for complex elements
       are stored after case label opcodes */
    _case = ((const idl_union_t *)node)->cases;
    for (; _case; _case = idl_next(_case)) {
      case_label = _case->case_labels;
      for (; case_label; case_label = idl_next(case_label))
        type->labels++;
    }

    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;

  (void)pstate;
  (void)path;
  if (revisit) {
    uint32_t off, cnt;
    off = descriptor->types->offset;
    cnt = descriptor->instructions.count;
    /* generate data field [elem-size] */
    if ((ret = stash_size(descriptor, off+2, descriptor->types->fields)))
      return ret;
    /* generate data field [next-insn, elem-insn] */
    if ((ret = stash_couple(descriptor, off+3, (uint16_t)(cnt-off)+3, 4u)))
      return ret;
    /* generate return from subroutine */
    if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
      return ret;
    pop_type(descriptor);
  } else {
    uint32_t opcode = DDS_OP_ADR | DDS_OP_TYPE_SEQ;
    uint32_t off;
    const idl_type_spec_t *type_spec;
    struct field *field = descriptor->types->fields;
    struct type *type = descriptor->types;

    /* resolve non-array aliases */
    type_spec = idl_unalias(idl_type_spec(node), 0u);
    opcode |= typecode(type_spec, SUBTYPE);

    off = descriptor->instructions.count;
    if ((ret = stash_opcode(descriptor, nop, opcode)))
      return ret;
    if ((ret = stash_offset(descriptor, nop, type, field)))
      return ret;

    /* short-circuit on simple types */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t bnd = ((const idl_string_t *)type_spec)->maximum;
      if ((ret = stash_single(descriptor, nop, bnd)))
        return ret;
      return IDL_RETCODE_OK;
    } else if (idl_is_base_type(type_spec)) {
      return IDL_RETCODE_OK;
    }

    if ((ret = push_type(descriptor, node, &type)))
      return ret;
    type->offset = off;
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct type *type = descriptor->types;
  const idl_type_spec_t *type_spec;
  bool simple = false;
  uint32_t size = 0;

  if (idl_is_array(node)) {
    size = idl_array_size(node);
    type_spec = idl_type_spec(node);
  } else {
    type_spec = idl_unalias(idl_type_spec(node), 0u);
    assert(idl_is_array(type_spec));
    size = idl_array_size(type_spec);
    type_spec = idl_type_spec(type_spec);
  }

  /* resolve aliases, squash multi-dimensional arrays */
  for (; idl_is_alias(type_spec); type_spec = idl_type_spec(type_spec))
    if (idl_is_array(type_spec))
      size *= idl_array_size(type_spec);

  simple = (idl_mask(type_spec) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM)) != 0;

  if (revisit) {
    if (!simple) {
      uint32_t off, cnt;

      off = type->offset;
      cnt = descriptor->instructions.count;
      /* generate data field [next-insn, elem-insn] */
      if ((ret = stash_couple(descriptor, off+3, (uint16_t)(cnt-off)+3, 5u)))
        return ret;
      /* generate data field [elem-size] */
      if ((ret = stash_size(descriptor, off+4, type->fields)))
       return ret;
      /* generate return from subroutine */
      if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
        return ret;

      pop_type(descriptor);
    }
    pop_field(descriptor);
  } else {
    uint32_t opcode, version;
    uint32_t off = 0;
    struct field *field = NULL;

    if ((ret = push_field(descriptor, node, &field)))
      return ret;

    version = (pstate->flags & IDL35) ? IDL35 : IDL4;
    opcode = DDS_OP_ADR | DDS_OP_TYPE_ARR | typecode(type_spec, SUBTYPE);
    if (idl_is_topic_key(descriptor->topic, version, path))
      opcode |= DDS_OP_FLAG_KEY;

    off = descriptor->instructions.count;
    /* generate data field opcode */
    if ((ret = stash_opcode(descriptor, nop, opcode)))
      return ret;
    /* generate data field offset */
    if ((ret = stash_offset(descriptor, nop, type, field)))
      return ret;
    /* generate data field alen */
    if ((ret = stash_single(descriptor, nop, size)))
      return ret;

    /* short-circuit on simple types */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t max = ((const idl_string_t *)type_spec)->maximum;
      /* generate data field noop [next-insn, elem-insn] */
      if ((ret = stash_single(descriptor, nop, 0)))
        return ret;
      /* generate data field bound */
      if ((ret = stash_single(descriptor, nop, max)))
        return ret;
      return IDL_VISIT_REVISIT;
    } else if (simple) {
      return IDL_VISIT_REVISIT;
    }

    if ((ret = push_type(descriptor, node, &type)))
      return ret;
    type->offset = off;
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_declarator(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;

  type_spec = idl_unalias(idl_type_spec(node), 0u);
  /* delegate array type specifiers or declarators */
  if (idl_is_array(node) || idl_is_array(type_spec))
    return emit_array(pstate, revisit, path, node, user_data);

  if (revisit) {
    pop_field(descriptor);
  } else {
    uint32_t opcode, version;
    struct type *type = descriptor->types;
    struct field *field = NULL;

    if ((ret = push_field(descriptor, node, &field)))
      return ret;

    if (idl_is_sequence(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    else if (idl_is_union(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    else if (idl_is_struct(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;

    version = (pstate->flags & IDL35) ? IDL35 : IDL4;
    opcode = DDS_OP_ADR | typecode(type_spec, TYPE);
    if (idl_is_topic_key(descriptor->topic, version, path))
      opcode |= DDS_OP_FLAG_KEY;

    /* generate data field opcode */
    if ((ret = stash_opcode(descriptor, nop, opcode)))
      return ret;
    /* generate data field offset */
    if ((ret = stash_offset(descriptor, nop, type, field)))
      return ret;
    /* generate data field bound */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t max = ((const idl_string_t *)type_spec)->maximum;
      if ((ret = stash_single(descriptor, nop, max)))
        return ret;
    }

    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static int print_opcode(FILE *fp, const struct instruction *inst)
{
  char buf[16];
  const char *vec[10];
  size_t len = 0;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode_primary type;
  enum dds_stream_typecode_subtype subtype;

  assert(inst->type == OPCODE);

  opcode = inst->data.opcode & (0xffu << 24);

  switch (opcode) {
    case DDS_OP_RTS:
      vec[len++] = "DDS_OP_RTS";
      goto print;
    case DDS_OP_JEQ:
      vec[len++] = "DDS_OP_JEQ";
      break;
    default:
      assert(opcode == DDS_OP_ADR);
      vec[len++] = "DDS_OP_ADR";
      break;
  }

  type = inst->data.opcode & (0xffu << 16);
  assert(type);
  switch (type) {
    case DDS_OP_TYPE_1BY: vec[len++] = " | DDS_OP_TYPE_1BY"; break;
    case DDS_OP_TYPE_2BY: vec[len++] = " | DDS_OP_TYPE_2BY"; break;
    case DDS_OP_TYPE_4BY: vec[len++] = " | DDS_OP_TYPE_4BY"; break;
    case DDS_OP_TYPE_8BY: vec[len++] = " | DDS_OP_TYPE_8BY"; break;
    case DDS_OP_TYPE_STR: vec[len++] = " | DDS_OP_TYPE_STR"; break;
    case DDS_OP_TYPE_BST: vec[len++] = " | DDS_OP_TYPE_BST"; break;
    case DDS_OP_TYPE_SEQ: vec[len++] = " | DDS_OP_TYPE_SEQ"; break;
    case DDS_OP_TYPE_ARR: vec[len++] = " | DDS_OP_TYPE_ARR"; break;
    case DDS_OP_TYPE_UNI: vec[len++] = " | DDS_OP_TYPE_UNI"; break;
    case DDS_OP_TYPE_STU: vec[len++] = " | DDS_OP_TYPE_STU"; break;
  }

  if (opcode == DDS_OP_JEQ && (type == DDS_OP_TYPE_SEQ ||
                               type == DDS_OP_TYPE_ARR ||
                               type == DDS_OP_TYPE_UNI ||
                               type == DDS_OP_TYPE_STU))
  {
    /* lower 16 bits contain offset to next instruction */
    idl_snprintf(buf, sizeof(buf), " | %u", inst->data.opcode & 0xffff);
    vec[len++] = buf;
  } else {
    subtype = inst->data.opcode & (0xffu << 8);
    assert(( subtype &&  (type == DDS_OP_TYPE_SEQ ||
                          type == DDS_OP_TYPE_ARR ||
                          type == DDS_OP_TYPE_UNI ||
                          type == DDS_OP_TYPE_STU))
        || (!subtype && !(type == DDS_OP_TYPE_SEQ ||
                          type == DDS_OP_TYPE_ARR ||
                          type == DDS_OP_TYPE_UNI ||
                          type == DDS_OP_TYPE_STU)));
    switch (subtype) {
      case DDS_OP_SUBTYPE_1BY: vec[len++] = " | DDS_OP_SUBTYPE_1BY"; break;
      case DDS_OP_SUBTYPE_2BY: vec[len++] = " | DDS_OP_SUBTYPE_2BY"; break;
      case DDS_OP_SUBTYPE_4BY: vec[len++] = " | DDS_OP_SUBTYPE_4BY"; break;
      case DDS_OP_SUBTYPE_8BY: vec[len++] = " | DDS_OP_SUBTYPE_8BY"; break;
      case DDS_OP_SUBTYPE_STR: vec[len++] = " | DDS_OP_SUBTYPE_STR"; break;
      case DDS_OP_SUBTYPE_BST: vec[len++] = " | DDS_OP_SUBTYPE_BST"; break;
      case DDS_OP_SUBTYPE_SEQ: vec[len++] = " | DDS_OP_SUBTYPE_SEQ"; break;
      case DDS_OP_SUBTYPE_ARR: vec[len++] = " | DDS_OP_SUBTYPE_ARR"; break;
      case DDS_OP_SUBTYPE_UNI: vec[len++] = " | DDS_OP_SUBTYPE_UNI"; break;
      case DDS_OP_SUBTYPE_STU: vec[len++] = " | DDS_OP_SUBTYPE_STU"; break;
    }

    if (type == DDS_OP_TYPE_UNI && (inst->data.opcode & DDS_OP_FLAG_DEF))
      vec[len++] = " | DDS_OP_FLAG_DEF";
    else if (inst->data.opcode & DDS_OP_FLAG_FP)
      vec[len++] = " | DDS_OP_FLAG_FP";
    if (inst->data.opcode & DDS_OP_FLAG_SGN)
      vec[len++] = " | DDS_OP_FLAG_SGN";
    if (inst->data.opcode & DDS_OP_FLAG_KEY)
      vec[len++] = " | DDS_OP_FLAG_KEY";
  }

print:
  for (size_t cnt=0; cnt < len; cnt++) {
    if (fputs(vec[cnt], fp) < 0)
      return -1;
  }
  return 0;
}

static int print_offset(FILE *fp, const struct instruction *inst)
{
  int ret;
  const char *type, *member;
  assert(inst->type == OFFSET);
  type = inst->data.offset.type;
  member = inst->data.offset.member;
  assert((!type && !member) || (type && member));
  if (!type)
    ret = fputs("0u", fp);
  else
    ret = idl_fprintf(fp, "offsetof (%s, %s)", type, member);
  return ret < 0 ? -1 : 0;
}

static int print_size(FILE *fp, const struct instruction *inst)
{
  int ret;
  const char *type;
  assert(inst->type == SIZE);
  type = inst->data.offset.type;
  ret = idl_fprintf(fp, "sizeof (%s)", type);
  return ret < 0 ? -1 : 0;
}

static int print_constant(FILE *fp, const struct instruction *inst)
{
  int ret;
  if (inst->data.constant.value)
    ret = idl_fprintf(fp, "%s", inst->data.constant.value);
  else
    ret = fputs("0", fp);
  return ret < 0 ? -1 : 0;
}

static int print_couple(FILE *fp, const struct instruction *inst)
{
  int ret;
  uint16_t high, low;
  assert(inst->type == COUPLE);
  high = inst->data.couple.high;
  low = inst->data.couple.low;
  ret = idl_fprintf(fp, "(%"PRIu16"u << 16) + %"PRIu16"u", high, low);
  return ret < 0 ? -1 : 0;
}

static int print_single(FILE *fp, const struct instruction *inst)
{
  int ret;
  assert(inst->type == SINGLE);
  ret = idl_fprintf(fp, "%"PRIu32, inst->data.single);
  return ret < 0 ? -1 : 0;
}

static idl_retcode_t print_opcodes(FILE *fp, const struct descriptor *desc)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  const struct instruction *inst;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode_primary optype;
  const char *seps[] = { ", ", ",\n  " };
  const char *sep = "  ";
  char *type = NULL;

  if (!(type = typename(desc->topic)))
    goto bail;
  if (idl_fprintf(fp, "static const uint32_t %s_ops[] =\n{\n", type) < 0)
    goto bail;
  for (size_t op=0, brk=0; op < desc->instructions.count; op++) {
    inst = &desc->instructions.table[op];
    sep = seps[op==brk];
    switch (inst->type) {
      case OPCODE:
        sep = op ? seps[1] : "  "; /* indent, always */
        /* determine when to break line */
        opcode = inst->data.opcode & (0xffu << 24);
        optype = inst->data.opcode & (0xffu << 16);
        if (opcode == DDS_OP_RTS)
          brk = op+1;
        else if (opcode == DDS_OP_JEQ)
          brk = op+3;
        else if (optype == DDS_OP_TYPE_ARR || optype == DDS_OP_TYPE_BST)
          brk = op+3;
        else if (optype == DDS_OP_TYPE_UNI)
          brk = op+4;
        else
          brk = op+2;
        if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
          goto bail;
        break;
      case OFFSET:
        if (fputs(sep, fp) < 0 || print_offset(fp, inst) < 0)
          goto bail;
        break;
      case SIZE:
        if (fputs(sep, fp) < 0 || print_size(fp, inst) < 0)
          goto bail;
        break;
      case CONSTANT:
        if (fputs(sep, fp) < 0 || print_constant(fp, inst) < 0)
          goto bail;
        break;
      case COUPLE:
        if (fputs(sep, fp) < 0 || print_couple(fp, inst) < 0)
          goto bail;
        break;
      case SINGLE:
        if (fputs(sep, fp) < 0 || print_single(fp, inst) < 0)
          goto bail;
        break;
    }
  }
  if (fputs("\n};\n\n", fp) < 0)
    goto bail;
  ret = IDL_RETCODE_OK;
bail:
  if (type) free(type);
  return ret;
}

static int print_keys(FILE *fp, struct descriptor *desc)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  char *type = NULL;
  const char *fmt, *sep="";
  uint32_t fixed = 0, cnt, key = 0;

  if (desc->keys == 0)
    return 0;

  if (!(type = typename(desc->topic)))
    goto bail;
  fmt = "static const dds_key_descriptor_t %s_keys[%"PRIu32"] =\n{\n";
  if (idl_fprintf(fp, fmt, type, desc->keys) < 0)
    goto bail;
  sep = "";
  fmt = "%s  { \"%s\", %"PRIu32" }";
  for (cnt=0; cnt < desc->instructions.count && key < desc->keys; cnt++) {
    enum dds_stream_opcode opcode;
    const struct instruction *inst = &desc->instructions.table[cnt];
    uint32_t size = 0, dims = 1;
    if (inst->type != OPCODE)
      continue;
    opcode = inst->data.opcode;
    if ((opcode & (0xffu<<24)) != DDS_OP_ADR || !(opcode & DDS_OP_FLAG_KEY))
      continue;
    if (opcode & DDS_OP_TYPE_ARR) {
      /* dimensions stored two instructions to the right */
      assert(cnt+2 < desc->instructions.count);
      assert(desc->instructions.table[cnt+2].type == SINGLE);
      dims = desc->instructions.table[cnt+2].data.single;
      opcode >>= 8;
    } else {
      opcode >>= 16;
    }

    switch (opcode & 0xffu) {
      case DDS_OP_VAL_1BY: size = 1; break;
      case DDS_OP_VAL_2BY: size = 2; break;
      case DDS_OP_VAL_4BY: size = 4; break;
      case DDS_OP_VAL_8BY: size = 8; break;
      // FIXME: probably need to handle bounded strings by size too?
      default:
        fixed = MAX_SIZE+1;
        break;
    }

    if (size > MAX_SIZE || dims > MAX_SIZE || (size*dims)+fixed > MAX_SIZE)
      fixed = MAX_SIZE+1;
    else
      fixed += size*dims;

    inst = &desc->instructions.table[cnt+1];
    assert(inst->type == OFFSET);
    assert(inst->data.offset.type);
    assert(inst->data.offset.member);
    if (idl_fprintf(fp, fmt, sep, inst->data.offset.member, cnt) < 0)
      goto bail;
    key++;
    sep=",\n";
  }
  if (fputs("\n};\n\n", fp) < 0)
    goto bail;
  if (fixed && fixed <= MAX_SIZE)
    desc->flags |= DDS_TOPIC_FIXED_KEY;
  ret = IDL_RETCODE_OK;
bail:
  if (type) free(type);
  return ret;
}

static int print_flags(FILE *fp, struct descriptor *desc)
{
  const char *fmt;
  const char *vec[3] = { NULL, NULL, NULL };
  size_t cnt, len = 0;

  if (desc->flags & DDS_TOPIC_CONTAINS_UNION)
    vec[len++] = "DDS_TOPIC_CONTAINS_UNION";
  if (desc->flags & DDS_TOPIC_NO_OPTIMIZE)
    vec[len++] = "DDS_TOPIC_NO_OPTIMIZE";
  if (desc->flags & DDS_TOPIC_FIXED_KEY)
    vec[len++] = "DDS_TOPIC_FIXED_KEY";

  if (!len)
    vec[len++] = "0u";

  for (cnt=0, fmt="%s"; cnt < len; cnt++, fmt=" | %s") {
    if (idl_fprintf(fp, fmt, vec[cnt]) < 0)
      return -1;
  }

  return fputs(",\n", fp) < 0 ? -1 : 0;
}

static int print_descriptor(FILE *fp, struct descriptor *desc)
{
  int ret = -1;
  const char *fmt;
  char *name = NULL, *type = NULL;

  if (!(name = absolute_name(desc->topic, "::")))
    goto bail;
  if (!(type = typename(desc->topic)))
    goto bail;
  fmt = "const dds_topic_descriptor_t %1$s_desc =\n{\n"
        "  sizeof (%1$s),\n" /* size of type */
        "  %2$s,\n  "; /* alignment */
  if (idl_fprintf(fp, fmt, type, desc->alignment->rendering) < 0)
    goto bail;
  if (print_flags(fp, desc) < 0)
    goto bail;
  fmt = "  %1$"PRIu32"u,\n" /* number of keys */
        "  \"%2$s\",\n"; /* fully qualified name in IDL */
  if (!desc->keys) {
    fmt = "  %1$"PRIu32"u,\n" /* number of keys */
          "  \"%2$s\",\n" /* fully qualified name in IDL */
          "  NULL,\n" /* key array */
          "  %4$"PRIu32",\n" /* number of ops */
          "  %3$s_ops,\n" /* ops array */
          "  \"\"\n" /* OpenSplice metadata */
          "};\n";
  } else {
    fmt = "  %1$"PRIu32"u,\n" /* number of keys */
          "  \"%2$s\",\n" /* fully qualified name in IDL */
          "  %3$s_keys,\n" /* key array */
          "  %4$"PRIu32",\n" /* number of ops */
          "  %3$s_ops,\n" /* ops array */
          "  \"\"\n" /* OpenSplice metadata */
          "};\n";
  }

  if (idl_fprintf(fp, fmt, desc->keys, name, type, desc->opcodes) < 0)
    goto bail;

  ret = 0;
bail:
  if (name) free(name);
  if (type) free(type);
  return ret;
}

idl_retcode_t generate_descriptor(const idl_pstate_t *pstate, struct generator *generator, const idl_node_t *node);

idl_retcode_t
generate_descriptor(
  const idl_pstate_t *pstate,
  struct generator *generator,
  const idl_node_t *node)
{
  idl_retcode_t ret;
  struct descriptor descriptor;
  idl_visitor_t visitor;

  memset(&descriptor, 0, sizeof(descriptor));
  memset(&visitor, 0, sizeof(visitor));

  visitor.visit = IDL_DECLARATOR | IDL_SEQUENCE | IDL_STRUCT | IDL_UNION | IDL_SWITCH_TYPE_SPEC | IDL_CASE;
  visitor.accept[IDL_ACCEPT_SEQUENCE] = &emit_sequence;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_CASE] = &emit_case;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;

  /* must be invoked for topics only, so structs (and unions?) only */
  assert(idl_is_struct(node));

  descriptor.topic = node;

  if ((ret = push_type(&descriptor, node, NULL)))
    return ret;

  if ((ret = idl_visit(pstate, ((const idl_struct_t *)node)->members, &visitor, &descriptor)))
    goto err_emit;
  pop_type(&descriptor);
  if ((ret = stash_opcode(&descriptor, nop, DDS_OP_RTS)))
    goto err_emit;

  print_keys(generator->source.handle, &descriptor);
  print_opcodes(generator->source.handle, &descriptor);
  if (print_descriptor(generator->source.handle, &descriptor) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_emit; }

err_emit:
  for (size_t i=0; i < descriptor.instructions.count; i++) {
    struct instruction *inst = &descriptor.instructions.table[i];
    switch (inst->type) {
      case OFFSET:
        if (inst->data.offset.member)
          free(inst->data.offset.member);
        if (inst->data.offset.type)
          free(inst->data.offset.type);
        break;
      case SIZE:
        if (inst->data.size.type)
          free(inst->data.size.type);
        break;
      case CONSTANT:
        if (inst->data.constant.value)
          free(inst->data.constant.value);
        break;
      default:
        break;
    }
  }
  if (descriptor.instructions.table)
    free(descriptor.instructions.table);
  return ret;
}
