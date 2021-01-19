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
    struct offset {
      char *type;
      char *member;
    } offset; /**< name of type and member to generate offsetof */
    struct size {
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

struct alignment {
  int value;
  int ordering;
  const char *rendering;
};

static const struct alignment alignments[] = {
#define ALIGNMENT_ONE (&alignments[0])
  { 1, 0, "1u" },
#define ALIGNMENT_BOOL (&alignments[1])
  { 0, 0, "sizeof(bool)" },
#define ALIGNMENT_ONE_OR_BOOL (&alignments[2])
  { 0, 1, "(sizeof(bool)>1u)?sizeof(bool):1u" },
#define ALIGNMENT_TWO (&alignments[3])
  { 2, 2, "2u" },
#define ALIGNMENT_TWO_OR_BOOL (&alignments[4])
  { 0, 3, "(sizeof(bool)>2u)?sizeof(bool):2u" },
#define ALIGNMENT_FOUR (&alignments[5])
  { 4, 4, "4u" },
#define ALIGNMENT_PTR (&alignments[6])
  { 0, 6, "sizeof (char *)" },
#define ALIGNMENT_EIGHT (&alignments[7])
  { 8, 8, "8u" }
};

struct descriptor {
  const idl_node_t *topic;
  const struct alignment *align; /**< alignment of topic type */
  uint32_t keys; /**< number of keys in topic */
  uint32_t flags; /**< topic descriptor flag values */
  struct {
    uint16_t size; /**< available number of instructions */
    uint16_t count; /**< used number of instructions */
    struct instruction *table;
  } instructions;
};

struct cookie {
  uint16_t offset;
  uint16_t label;
  uint16_t labels;
};

static const struct alignment *
max_alignment(const struct alignment *a, const struct alignment *b)
{
  if (!a)
    return b;
  if (!b)
    return a;
  if ((a == ALIGNMENT_BOOL && b == ALIGNMENT_ONE) ||
      (b == ALIGNMENT_BOOL && a == ALIGNMENT_ONE))
    return ALIGNMENT_ONE_OR_BOOL;
  if ((a == ALIGNMENT_BOOL && b == ALIGNMENT_TWO) ||
      (b == ALIGNMENT_BOOL && a == ALIGNMENT_TWO))
    return ALIGNMENT_TWO_OR_BOOL;
  return b->ordering > a->ordering ? b : a;
}

static void
correct_alignment(
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec)
{
  const struct alignment *align = NULL;

  while (idl_is_typedef(type_spec))
    type_spec = idl_type_spec(type_spec);

  switch (idl_type(type_spec)) {
    case IDL_BOOL:     align = ALIGNMENT_BOOL;   break;
    case IDL_CHAR:     align = ALIGNMENT_ONE;    break;
    case IDL_OCTET:    align = ALIGNMENT_ONE;    break;
    case IDL_INT8:     align = ALIGNMENT_ONE;    break;
    case IDL_UINT8:    align = ALIGNMENT_ONE;    break;
    case IDL_SHORT:
    case IDL_INT16:    align = ALIGNMENT_TWO;    break;
    case IDL_USHORT:
    case IDL_UINT16:   align = ALIGNMENT_TWO;    break;
    case IDL_LONG:
    case IDL_INT32:    align = ALIGNMENT_FOUR;   break;
    case IDL_ULONG:
    case IDL_UINT32:   align = ALIGNMENT_FOUR;   break;
    case IDL_LLONG:
    case IDL_INT64:    align = ALIGNMENT_EIGHT;  break;
    case IDL_ULLONG:
    case IDL_UINT64:   align = ALIGNMENT_EIGHT;  break;

    case IDL_FLOAT:    align = ALIGNMENT_FOUR;   break;
    case IDL_DOUBLE:   align = ALIGNMENT_EIGHT;  break;
    case IDL_STRING:
      align = idl_is_bounded(type_spec) ? ALIGNMENT_ONE : ALIGNMENT_PTR;
      break;
    case IDL_SEQUENCE: align = ALIGNMENT_PTR;    break;
    default:
      //assert("correct_alignment must not be called for constructed types", 0);
      return;
  }

  if (align == ALIGNMENT_PTR)
    descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;

  descriptor->align = max_alignment(descriptor->align, align);
}

static idl_retcode_t
stash_instruction(
  struct descriptor *descriptor, uint16_t index, const struct instruction *inst)
{
  /* make more slots available as necessary */
  if (descriptor->instructions.count == descriptor->instructions.size) {
    uint16_t size = descriptor->instructions.size + 100;
    struct instruction *table = descriptor->instructions.table;
    if (!(table = realloc(table, size * sizeof(*table))))
      return IDL_RETCODE_OUT_OF_MEMORY;
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
  struct descriptor *descriptor, uint16_t index, uint32_t opcode)
{
  struct instruction inst = { OPCODE, { .opcode = opcode } };
  /* update key count in descriptor */
  if ((opcode & (0xffu << 24)) == DDS_OP_ADR && (opcode & DDS_OP_FLAG_KEY))
    descriptor->keys++;
  return stash_instruction(descriptor, index, &inst);
}

static idl_retcode_t
stash_offset(
  struct descriptor *descriptor, uint16_t index, const struct offset *offset)
{
  struct instruction inst = { OFFSET, { .offset = *offset } };
  return stash_instruction(descriptor, index, &inst);
}

static idl_retcode_t
stash_size(
  struct descriptor *descriptor, uint16_t index, const struct size *size)
{
  struct instruction inst = { SIZE, { .size = *size } };
  return stash_instruction(descriptor, index, &inst);
}

/* used to stash case labels. no need to take into account strings etc */
static idl_retcode_t
stash_constant(
  struct descriptor *descriptor, uint16_t index, const idl_const_expr_t *const_expr)
{
  int cnt;
  struct instruction inst = { CONSTANT, { .constant = { NULL } } };
  char **strp = &inst.data.constant.value;

  if (idl_is_enumerator(const_expr)) {
    *strp = typename(const_expr);
  } else {
    const idl_constval_t *constval = (const idl_constval_t *)const_expr;

    switch (idl_type(const_expr)) {
      case IDL_CHAR:
        cnt = idl_asprintf(strp, "'%c'", constval->value.chr);
        break;
      case IDL_BOOL:
        cnt = idl_asprintf(strp, "%s", constval->value.bln ? "true" : "false");
        break;
      case IDL_INT8:
        cnt = idl_asprintf(strp, "%" PRId8, constval->value.int8);
        break;
      case IDL_OCTET:
      case IDL_UINT8:
        cnt = idl_asprintf(strp, "%" PRIu8, constval->value.uint8);
        break;
      case IDL_SHORT:
      case IDL_INT16:
        cnt = idl_asprintf(strp, "%" PRId16, constval->value.int16);
        break;
      case IDL_USHORT:
      case IDL_UINT16:
        cnt = idl_asprintf(strp, "%" PRIu16, constval->value.uint16);
        break;
      case IDL_LONG:
      case IDL_INT32:
        cnt = idl_asprintf(strp, "%" PRId32, constval->value.int32);
        break;
      case IDL_ULONG:
      case IDL_UINT32:
        cnt = idl_asprintf(strp, "%" PRIu32, constval->value.uint32);
        break;
      case IDL_LLONG:
      case IDL_INT64:
        cnt = idl_asprintf(strp, "%" PRId64, constval->value.int64);
        break;
      case IDL_ULLONG:
      case IDL_UINT64:
        cnt = idl_asprintf(strp, "%" PRIu64, constval->value.uint64);
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
  return IDL_RETCODE_OUT_OF_MEMORY;
}

static idl_retcode_t
stash_couple(
  struct descriptor *desc, uint16_t index, uint16_t high, uint16_t low)
{
  struct instruction inst = { COUPLE, { .couple = { high, low } } };
  return stash_instruction(desc, index, &inst);
}

static idl_retcode_t
stash_single(
  struct descriptor *desc, uint16_t index, uint32_t single)
{
  struct instruction inst = { SINGLE, { .single = single } };
  return stash_instruction(desc, index, &inst);
}

static uint32_t typecode(const idl_type_spec_t *type_spec, unsigned int shift)
{
  assert(shift == 8 || shift == 16);
  if (idl_is_array(type_spec))
    return (DDS_OP_VAL_ARR << shift);
  type_spec = idl_unalias(type_spec);
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

static const idl_visit_t *backstep(const idl_visit_t *visit, size_t steps)
{
  for (size_t cnt=0; visit && cnt < steps; cnt++)
    visit = visit->previous;
  return visit;
}

static idl_retcode_t
figure_offset(const idl_visit_t *visit, struct offset *offsetp)
{
  const char *sep, *vec[2];
  const idl_visit_t *member = NULL;
  const idl_type_spec_t *type_spec = NULL;
  size_t len, off;

  if (idl_is_switch_type_spec(visit->node) || idl_is_case(visit->node)) {
    member = visit;
    type_spec = idl_parent(member->node);
    visit = backstep(visit, 2);
  }

  /* backtrace visits to determine type and member for offsetof */
  while (visit) {
    if (!idl_is_declarator(visit->node))
      break;
    if (!idl_is_member(idl_parent(visit->node)) && member)
      break;
    member = visit;
    type_spec = idl_parent(member->node);
    type_spec = idl_parent(type_spec);
    visit = backstep(visit, 3);
  }

  if (!member || !type_spec)
    return IDL_RETCODE_OK; /* emits 0u instead of offsetof(type, member) */

  len = 0;
  for (visit=member, sep=""; visit; visit=visit->next) {
    int i, n = 0;
    if (idl_is_declarator(visit->node)) {
      vec[n++] = idl_identifier(visit->node);
    } else if (idl_is_switch_type_spec(visit->node)) {
      vec[n++] = "_d";
    } else if (idl_is_case(visit->node)) {
      const void *node = ((const idl_case_t *)visit->node)->declarator;
      vec[n++] = "_u";
      if (idl_is_declarator(node))
        vec[n++] = idl_identifier(node);
    }
    for (i=0; i < n; i++) {
      len += strlen(sep) + strlen(vec[i]);
      sep = ".";
    }
  }

  if (!(offsetp->type = typename(type_spec)))
    return IDL_RETCODE_OUT_OF_MEMORY;
  if (!(offsetp->member = malloc(len+1)))
    return IDL_RETCODE_OUT_OF_MEMORY;

  off = 0;
  for (visit=member, sep=""; visit; visit=visit->next) {
    int i, n = 0;
    if (idl_is_declarator(visit->node)) {
      vec[n++] = idl_identifier(visit->node);
    } else if (idl_is_switch_type_spec(visit->node)) {
      vec[n++] = "_d";
    } else if (idl_is_case(visit->node)) {
      const void *node = ((const idl_case_t *)visit->node)->declarator;
      vec[n++] = "_u";
      if (idl_is_declarator(node))
        vec[n++] = idl_identifier(node);
    }
    for (i=0; i < n; i++) {
      len = strlen(sep);
      memcpy(offsetp->member+off, sep, len);
      off += len;
      len = strlen(vec[i]);
      memcpy(offsetp->member+off, vec[i], len);
      off += len;
      sep = ".";
    }
  }
  offsetp->member[off] = '\0';

  return IDL_RETCODE_OK;
}

static void clear_offset(struct offset *offsetp)
{
  if (offsetp->type)
    free(offsetp->type);
  if (offsetp->member)
    free(offsetp->member);
}

static bool
idl_is_constr_type(const void *node)
{
  return idl_is_struct(node) || idl_is_union(node);
}

static idl_retcode_t
figure_size(const idl_visit_t *visit, struct size *sizep)
{
  const idl_type_spec_t *type_spec;

  /* invoked for arrays or sequences of complex types */
  if (idl_is_sequence(visit->node)) {
    type_spec = visit->node;
  } else {
    assert(idl_is_declarator(visit->node));
    type_spec = idl_type_spec(visit->node);
    while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
      type_spec = idl_type_spec(type_spec);
  }

  if (idl_is_sequence(type_spec) && !idl_is_array(visit->node)) {
    type_spec = idl_type_spec(type_spec);

    /* sequence of array */
    if (idl_is_array(type_spec)) {
      char buf[1], *str = NULL;
      const char *fmt = "[%" PRIu32 "]", *name;
      size_t len = 0, pos;
      ssize_t cnt;
      const idl_type_spec_t *node;
      const idl_constval_t *constval;
      /* sequence of (multi-)dimensional array requires sizes in a sizeof */
      for (node = type_spec; node; node = idl_type_spec(node)) {
        if (!idl_is_declarator(node))
          break;
        constval = ((const idl_declarator_t *)node)->const_expr;
        for (; constval; constval = idl_next(constval)) {
          assert(idl_type(constval) == IDL_ULONG);
          cnt = idl_snprintf(buf, sizeof(buf), fmt, constval->value.uint32);
          assert(cnt > 0);
          len += (size_t)cnt;
        }
      }
      // >> now we must figure out the typename!
      name = typename(node);
      len += strlen(name);
      if (!(str = malloc(len + 1)))
        return IDL_RETCODE_OUT_OF_MEMORY;
      pos = strlen(name);
      memcpy(str, name, pos);
      for (node = type_spec; node; node = idl_type_spec(node)) {
        if (!idl_is_declarator(node))
          break;
        constval = ((const idl_declarator_t *)node)->const_expr;
        for(; constval && pos < len; constval = idl_next(constval)) {
          cnt = idl_snprintf(str+pos, (len+1)-pos, fmt, constval->value.uint32);
          assert(cnt > 0);
          pos += (size_t)cnt;
        }
      }
      assert(pos == len && !constval);
      sizep->type = str;
    } else if (!(sizep->type = typename(type_spec))) {
      return IDL_RETCODE_OUT_OF_MEMORY;
    }
  } else {
    uint32_t size = 0;

    if (idl_is_array(visit->node))
      size = idl_array_size(visit->node);
    type_spec = idl_type_spec(visit->node);
    while (idl_is_typedef(type_spec)) {
      if (idl_is_array(type_spec))
        size = size ? size * idl_array_size(type_spec) : idl_array_size(type_spec);
      type_spec = idl_type_spec(type_spec);
    }

    assert(size);
    if (!(sizep->type = typename(type_spec)))
      return IDL_RETCODE_OUT_OF_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static void clear_size(struct size *sizep)
{
  if (sizep->type)
    free(sizep->type);
}

static idl_retcode_t
emit_case(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct cookie *cookie = visit->previous->cookie;

  assert(cookie->labels);
  if (visit->type == IDL_EXIT) {
    if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
      return ret;
    return IDL_RETCODE_OK;
  } else {
    uint16_t off, cnt;
    uint32_t opcode = DDS_OP_JEQ;
    bool simple = true;
    const idl_case_t *_case = node;
    const idl_case_label_t *case_label;
    const idl_type_spec_t *type_spec;
    struct offset offset = { NULL, NULL };

    type_spec = _case->type_spec;
    while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
      type_spec = idl_type_spec(type_spec);

    /* simple elements are embedded, complex elements are not */
    if (idl_is_array(_case->declarator)) {
      opcode |= DDS_OP_SUBTYPE_ARR;
      simple = false;
    } else {
      opcode |= typecode(type_spec, SUBTYPE);
      if (idl_is_array(type_spec)) {
        simple = false;
      } else if (idl_is_string(type_spec)) {
        correct_alignment(descriptor, type_spec);
        descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      } else if (idl_is_base_type(type_spec)) {
        correct_alignment(descriptor, type_spec);
      } else {
        simple = false;
      }
    }

    cnt = descriptor->instructions.count + (cookie->labels - cookie->label) * 3;
    case_label = _case->case_labels;
    for (; case_label; case_label = idl_next(case_label)) {
      off = cookie->offset + 2 + (cookie->label * 3);
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
      if ((ret = figure_offset(visit, &offset)))
        { clear_offset(&offset); return ret; }
      if ((ret = stash_offset(descriptor, off++, &offset)))
        { clear_offset(&offset); return ret; }
      cookie->label++;
    }

    return simple ? IDL_VISIT_DONT_RECURSE : IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_switch_type_spec(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  uint32_t opcode;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;
  struct offset offset = { NULL, NULL };

  assert(visit->type == IDL_ENTER);

  type_spec = idl_type_spec(node);
  while (idl_is_typedef(type_spec)) {
    assert(!idl_is_array(type_spec));
    type_spec = idl_type_spec(node);
  }

  opcode = DDS_OP_ADR | DDS_OP_TYPE_UNI | typecode(type_spec, SUBTYPE);
  // FIXME: possible with #pragma keylist?!?!
  //if (idl_is_topic_key(pstate, descriptor->topic, switch_type_spec))
  //  opcode |= DDS_OP_FLAG_KEY;
  if ((ret = stash_opcode(descriptor, nop, opcode)))
    goto err_emit;
  if ((ret = figure_offset(visit, &offset)))
    goto err_offset;
  if ((ret = stash_offset(descriptor, nop, &offset)))
    goto err_offset;

  correct_alignment(descriptor, type_spec);

  return IDL_RETCODE_OK;
err_offset:
  clear_offset(&offset);
err_emit:
  return ret;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct cookie *cookie = visit->cookie;

  if (visit->type == IDL_EXIT) {
    uint16_t off, cnt;
    assert(cookie->label == cookie->labels);
    off = cookie->offset;
    cnt = (descriptor->instructions.count - off) + 2;
    if ((ret = stash_single(descriptor, off+2, cookie->labels)))
      return ret;
    if ((ret = stash_couple(descriptor, off+3, cnt, 4u)))
      return ret;
    return IDL_RETCODE_OK;
  } else {
    const idl_case_t *_case;
    const idl_case_label_t *case_label;

    cookie->label = 0;
    cookie->offset = descriptor->instructions.count;
    /* strictly speaking a topic with a union can be optimized if all members
       have the same size, and if the non-basetype members are all optimizable
       themselves, and the alignment of the discriminant is not less than the
       alignment of the members */
    descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE | DDS_TOPIC_CONTAINS_UNION;

    /* determine total number of case labels as opcodes for complex elements
       are stored after case label opcodes */
    _case = ((const idl_union_t *)node)->cases;
    for (; _case; _case = idl_next(_case)) {
      case_label = _case->case_labels;
      for (; case_label; case_label = idl_next(case_label))
        cookie->labels++;
    }

    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct cookie *cookie = visit->cookie;

  if (visit->type == IDL_EXIT) {
    struct size size = { NULL };
    uint16_t off, cnt;
    off = cookie->offset;
    cnt = descriptor->instructions.count;
    /* generate data field [next-insn, elem-insn] */
    if ((ret = stash_couple(descriptor, off+3, (cnt-off)+3, 5u)))
      goto err_emit;
    /* generate data field [elem-size] */
    if ((ret = figure_size(visit, &size)))
      goto err_size;
    if ((ret = stash_size(descriptor, off+4, &size)))
      goto err_size;
    if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
      goto err_emit;
    return IDL_RETCODE_OK;
err_size:
    clear_size(&size);
  } else { /* visit */
    struct offset offset = { NULL, NULL };
    uint16_t off = 0;
    uint32_t opcode = DDS_OP_ADR, size = 0;
    const idl_type_spec_t *type_spec;

    if (idl_is_array(node)) {
      size = idl_array_size(node);
      opcode |= typecode(node, TYPE);
      type_spec = idl_type_spec(node);
    } else {
      type_spec = idl_type_spec(node);
      while (idl_is_typedef(type_spec) && idl_is_array(type_spec))
        type_spec = idl_type_spec(type_spec);
      assert(idl_is_array(type_spec));
      size = idl_array_size(type_spec);
      opcode |= typecode(type_spec, TYPE);
      type_spec = idl_type_spec(type_spec);
    }

    /* resolve aliases, squash multi-dimensional arrays */
    for (; idl_is_typedef(type_spec); type_spec = idl_type_spec(type_spec))
      if (idl_is_array(type_spec))
        size *= idl_array_size(type_spec);

    opcode |= typecode(type_spec, SUBTYPE);
    if (idl_is_topic_key(pstate, descriptor->topic, node))
      opcode |= DDS_OP_FLAG_KEY;

    off = descriptor->instructions.count;
    /* generate data field opcode */
    if ((ret = stash_opcode(descriptor, nop, opcode)))
      goto err_emit;
    /* generate data field offset */
    if ((ret = figure_offset(visit, &offset)))
      goto err_offset;
    if ((ret = stash_offset(descriptor, nop, &offset)))
      goto err_offset;
    /* generate data field alen */
    if ((ret = stash_single(descriptor, nop, size)))
      goto err_emit;

    /* short-circuit on simple types */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t max = ((const idl_string_t *)type_spec)->maximum;
      /* generate data field noop [next-insn, elem-insn] */
      if ((ret = stash_single(descriptor, nop, 0)))
        goto err_emit;
      /* generate data field bound */
      if ((ret = stash_single(descriptor, nop, max)))
        goto err_emit;
      correct_alignment(descriptor, type_spec);
      return IDL_RETCODE_OK;
    } else if (idl_is_enum(type_spec) || idl_is_base_type(type_spec)) {
      correct_alignment(descriptor, type_spec);
      return IDL_RETCODE_OK;
    } else {
      cookie->offset = off;
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;
    }
err_offset:
    clear_offset(&offset);
  }

err_emit:
  return ret;
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct cookie *cookie = visit->cookie;

  correct_alignment(descriptor, node);

  /* skip sequence type specifier for declarator */
  if (visit->previous &&
      idl_is_declarator(visit->previous->node) &&
     !idl_is_array(visit->previous->node))
      return IDL_VISIT_TYPE_SPEC;

  if (visit->type == IDL_EXIT) {
    struct size size = { NULL };
    uint16_t off, cnt;
    off = cookie->offset;
    cnt = descriptor->instructions.count;
    /* generate data field [elem-size] */
    if ((ret = figure_size(visit, &size)))
      { clear_size(&size); return ret; }
    if ((ret = stash_size(descriptor, off+2, &size)))
      { clear_size(&size); return ret; }
    /* generate data field [next-insn, elem-insn] */
    if ((ret = stash_couple(descriptor, off+3, (cnt-off)+3, 4u)))
      return ret;
    if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
      return ret;
    return IDL_RETCODE_OK;
  } else {
    struct offset offset = { NULL, NULL };
    uint32_t opcode = DDS_OP_ADR | DDS_OP_TYPE_SEQ;
    uint16_t off;
    const idl_type_spec_t *type_spec;

    /* resolve non-array aliases */
    type_spec = idl_type_spec(node);
    while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
      type_spec = idl_type_spec(type_spec);
    opcode |= typecode(type_spec, SUBTYPE);

    off = descriptor->instructions.count;
    if ((ret = stash_opcode(descriptor, nop, opcode)))
      goto err_emit;
    if ((ret = figure_offset(visit, &offset)))
      goto err_offset;
    if ((ret = stash_offset(descriptor, nop, &offset)))
      goto err_offset;

    /* short-circuit on simple types */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t bnd = ((const idl_string_t *)type_spec)->maximum;
      if ((ret = stash_single(descriptor, nop, bnd)))
        return ret;
      return IDL_RETCODE_OK;
    } else if (idl_is_base_type(type_spec)) {
      return IDL_RETCODE_OK;
    } else {
      cookie->offset = off;
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    }
err_offset:
    clear_offset(&offset);
  }

err_emit:
  return ret;
}

emit_declarator(
  const idl_pstate_t *pstate,
  idl_visit_t *visit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct offset offset = { NULL, NULL };
  uint32_t opcode;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;

  type_spec = idl_type_spec(node);
  while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
    type_spec = idl_type_spec(type_spec);

  if (idl_is_array(node) || idl_is_array(type_spec))
    return emit_array(pstate, visit, node, user_data);
  else if (idl_is_sequence(type_spec))
    return emit_sequence(pstate, visit, node, user_data);
  else if (idl_is_union(type_spec))
    return IDL_VISIT_TYPE_SPEC;
  else if (idl_is_struct(type_spec))
    return IDL_VISIT_TYPE_SPEC;

  /* generate data field opcode */
  opcode = DDS_OP_ADR;
  opcode |= typecode(type_spec, TYPE);
  if (idl_is_topic_key(pstate, descriptor->topic, node))
    opcode |= DDS_OP_FLAG_KEY;

  if ((ret = stash_opcode(descriptor, nop, opcode)))
    goto err_emit;
  /* generate data field offset */
  if ((ret = figure_offset(visit, &offset)))
    goto err_offset;
  if ((ret = stash_offset(descriptor, nop, &offset)))
    goto err_offset;
  /* generate data field bound */
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
    uint32_t max = ((const idl_string_t *)type_spec)->maximum;
    if ((ret = stash_single(descriptor, nop, max)))
      goto err_emit;
  }

  correct_alignment(descriptor, type_spec);
  return IDL_RETCODE_OK;
err_offset:
  clear_offset(&offset);
err_emit:
  return ret;
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
  idl_retcode_t ret = IDL_RETCODE_OUT_OF_MEMORY;
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
  idl_retcode_t ret = IDL_RETCODE_OUT_OF_MEMORY;
  char *type = NULL;
  const char *fmt, *sep="";
  uint32_t fixed = 0, cnt, key = 0;

  if (!(type = typename(desc->topic)))
    goto bail;
  fmt = "static const dds_key_descriptor_t %s_keys[%"PRIu32"] =\n{\n";
  if (idl_fprintf(fp, fmt, type, desc->keys) < 0)
    goto bail;
  sep = "";
  fmt = "%s  { \"%s\", %"PRIu32" }\n";
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

    inst = &desc->instructions.table[++cnt];
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
  if (idl_fprintf(fp, fmt, type, desc->align->rendering) < 0)
    goto bail;
  if (print_flags(fp, desc) < 0)
    goto bail;
  fmt = "  %1$"PRIu32"u,\n" /* number of keys */
        "  \"%2$s\",\n" /* fully qualified name in IDL */
        "  %3$s_keys,\n" /* key array */
        "  %4$"PRIu32"u,\n" /* number of ops */
        "  %3$s_ops,\n" /* ops array */
        "  \"\",\n" /* OpenSplice metadata */
        "};\n";
  if (idl_fprintf(fp, fmt, desc->keys, name, type, desc->instructions.count) < 0)
    goto bail;

  ret = 0;
bail:
  if (name) free(name);
  if (type) free(type);
  return ret;
}

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

  visitor.visit = IDL_DECLARATOR | IDL_SEQUENCE | IDL_UNION | IDL_SWITCH_TYPE_SPEC | IDL_CASE;
  visitor.cookie = sizeof(struct cookie);
  visitor.accept[IDL_ACCEPT_SEQUENCE] = &emit_sequence;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_CASE] = &emit_case;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;

  /* must be invoked for topics only, so structs (and unions?) only */
  assert(idl_is_struct(node));

  descriptor.topic = node;

  if ((ret = idl_visit(pstate, ((const idl_struct_t *)node)->members, &visitor, &descriptor)))
    goto err_emit;
  if ((ret = stash_opcode(&descriptor, nop, DDS_OP_RTS)))
    goto err_emit;

  print_keys(generator->source.handle, &descriptor);
  print_opcodes(generator->source.handle, &descriptor);
  if (print_descriptor(generator->source.handle, &descriptor) < 0)
    { ret = IDL_RETCODE_OUT_OF_MEMORY; goto err_emit; }

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
