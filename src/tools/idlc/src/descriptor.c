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
#include "idl/string.h"

#include "descriptor.h"
#include "dds/ddsc/dds_opcodes.h"

#define TYPE (16)
#define SUBTYPE (8)

static const uint16_t nop = UINT16_MAX;

/* no-op name for emitting union cases with array declarator */
static const idl_file_t nop_file =
  { NULL, __FILE__ };
static const idl_source_t nop_source =
  { NULL, NULL, NULL, NULL, true, &nop_file, &nop_file };
#define NOP_POSITION { &nop_source, &nop_file, 1, 1 }
#define NOP_LOCATION { NOP_POSITION, NOP_POSITION }
static const idl_name_t nop_name =
  { { IDL_NAME, NOP_LOCATION, 0, 0 }, "" };

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

/* scope and type together are used to create a name scope (for offsetof) on
   stack without the need for (de)allocation of heap memory */
struct type {
  struct type *next;
  const idl_name_t *name;
};

struct scope {
  struct scope *next;
  struct {
    struct type *first, *last;
  } types;
};

struct descriptor {
  struct {
    struct scope *first, *last;
  } scopes;
  char *ctype; /**< name of C type */
  char *typename;
  size_t keys; /**< number of keys in topic */
  // FIXME: add something for DDS_TOPIC_CONTAINS_UNION
  struct {
    uint16_t size; /**< available number of instructions */
    uint16_t count; /**< used number of instructions */
    struct instruction *table;
  } instructions;
  const idl_node_t *topic;
};

static void push_type(struct descriptor *descriptor, struct type *type)
{
  struct scope *scope;

  assert(descriptor->scopes.last);
  scope = descriptor->scopes.last;
  if (scope->types.last) {
    assert(scope->types.first);
    scope->types.last->next = type;
    scope->types.last = type;
  } else {
    assert(!scope->types.first);
    scope->types.first = scope->types.last = type;
  }
}

static void pop_type(struct descriptor *descriptor)
{
  struct scope *scope;

  assert(descriptor->scopes.last);
  scope = descriptor->scopes.last;
  assert(scope->types.last);
  if (scope->types.last == scope->types.first) {
    scope->types.first = scope->types.last = NULL;
  } else {
    struct type *type, *last;
    type = scope->types.first;
    last = scope->types.last;
    for (; type->next != last; type = type->next) ;
    scope->types.last = type;
    scope->types.last->next = NULL;
  }
}

static void push_scope(struct descriptor *descriptor, struct scope *scope)
{
  if (descriptor->scopes.last) {
    assert(descriptor->scopes.first);
    descriptor->scopes.last->next = scope;
    descriptor->scopes.last = scope;
  } else {
    descriptor->scopes.first = descriptor->scopes.last = scope;
  }
}

static void pop_scope(struct descriptor *descriptor)
{
  assert(descriptor->scopes.last);
  assert(!descriptor->scopes.last->types.last);
  if (descriptor->scopes.last == descriptor->scopes.first) {
    descriptor->scopes.first = descriptor->scopes.last = NULL;
  } else {
    struct scope *scope, *last;
    scope = descriptor->scopes.first;
    last = descriptor->scopes.last;
    for (; scope->next != last; scope = scope->next) ;
    descriptor->scopes.last = scope;
    descriptor->scopes.last->next = NULL;
  }
}

static idl_retcode_t
stash_instruction(
  struct descriptor *descriptor,
  uint16_t index,
  const struct instruction *instruction)
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

  descriptor->instructions.table[index] = *instruction;
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
  struct descriptor *descriptor, uint16_t index, const void *node)
{
  const char *sep;
  const struct type *type;
  struct instruction inst = { OFFSET, { .offset = { NULL, NULL } } };
  size_t off = 0, len = 0;


  if (!descriptor->scopes.last || !node || idl_name(node) == &nop_name)
    return stash_instruction(descriptor, index, &inst);

  sep = "";
  for (type = descriptor->scopes.last->types.first; type; type = type->next) {
    len += strlen(sep) + strlen(type->name->identifier);
    sep = ".";
  }

  if (!(inst.data.offset.type = malloc(len + 1)))
    goto err_type;

  sep = "";
  for (type = descriptor->scopes.last->types.first; type; type = type->next) {
    len = strlen(sep);
    memcpy(inst.data.offset.type+off, sep, len);
    off += len;
    len = strlen(type->name->identifier);
    memcpy(inst.data.offset.type+off, type->name->identifier, len);
    off += len;
    sep = ".";
  }
  inst.data.offset.type[off] = '\0';

  if (idl_is_switch_type_spec(node)) {
    if (!(inst.data.offset.member = idl_strdup("_d")))
      goto err_member;
  } else {
    const char *fmt = "%s";
    assert(idl_is_declarator(node));
    if (idl_is_case(idl_parent(node)))
      fmt = "_u.%s";
    if (idl_asprintf(&inst.data.offset.member, fmt, idl_identifier(node)) == -1)
      goto err_member;
  }

  if (stash_instruction(descriptor, index, &inst))
    goto err_emit;

  return IDL_RETCODE_OK;
err_emit:
  free(inst.data.offset.member);
err_member:
  free(inst.data.offset.type);
err_type:
  return IDL_RETCODE_OUT_OF_MEMORY;
}

static idl_retcode_t
stash_size(
  struct descriptor *descriptor, uint16_t index, const idl_type_spec_t *type_spec)
{
  struct instruction inst = { SIZE, { .size = { NULL } } };

  assert(type_spec);

  if (idl_is_sequence(type_spec)) {
    if (idl_is_array(type_spec)) {
      char buf[1], *str = NULL;
      size_t len, pos;
      ssize_t cnt;
      const idl_const_expr_t *const_expr;
      /* sequences of (multi-)dimensional arrays require a sizeof with
         dimensions included */
      assert(idl_is_declarator(type_spec));
      len = strlen(idl_identifier(type_spec));
      const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
      assert(const_expr);
      while (const_expr) {
        uint32_t dim = ((const idl_constval_t *)const_expr)->value.uint32;
        cnt = idl_snprintf(buf, sizeof(buf), "[%" PRIu32 "]", dim);
        assert(cnt > 0);
        len += (size_t)cnt;
        const_expr = idl_next(const_expr);
      }
      if (!(str = malloc(len + 1)))
        return IDL_RETCODE_OUT_OF_MEMORY;
      pos = strlen(idl_identifier(type_spec));
      memcpy(str, idl_identifier(type_spec), pos);
      const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
      while (const_expr && pos < len) {
        uint32_t dim = ((const idl_constval_t *)const_expr)->value.uint32;
        cnt = idl_snprintf(str+pos, (len+1)-pos, "[%" PRIu32 "]", dim);
        assert(cnt > 0);
        pos += (size_t)cnt;
        const_expr = idl_next(const_expr);
      }
      assert(pos == len && !const_expr);
      inst.data.size.type = str;
    } else {
      inst.data.size.type = idl_strdup("dds_sequence_t");
      // for sequences a type has been created if it did not already exist.
      // so if it's a direct sequence that type_spec points towards, we have to
      // do some magic. for now assume that the type exists!
    }
  } else {
    inst.data.size.type = idl_strdup(idl_identifier(type_spec));
  }

  if (!inst.data.size.type)
    goto err_type;
  if (stash_instruction(descriptor, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.size.type);
err_type:
  return IDL_RETCODE_OUT_OF_MEMORY;
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
    // FIXME: implement absolute_name
    *strp = idl_strdup(idl_identifier(const_expr));
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
      return 0u;
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
emit_union(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator);

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator);

static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator);

static idl_retcode_t
emit_member(
  const idl_pstate_t *,
  struct descriptor *,
  const idl_type_spec_t *,
  const idl_declarator_t *);

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator)
{
  idl_retcode_t ret;
  const idl_member_t *member;
  struct type type = { .next = NULL, .name = NULL };

  /* introduces new type, NOT scope */
  type.name = declarator ? idl_name(declarator) : idl_name(type_spec);
  push_type(descriptor, &type);
  /* type specifier maybe a typedef of struct, unalias */
  while (idl_is_typedef(type_spec)) {
    assert(!idl_is_array(type_spec));
    type_spec = idl_type_spec(type_spec);
  }
  assert(idl_is_struct(type_spec));

  member = ((const idl_struct_t *)type_spec)->members;
  for (; member; member = idl_next(member)) {
    if ((ret = emit_member(pstate, descriptor, member->type_spec, member->declarators)))
      return ret;
  }

  pop_type(descriptor);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator)
{
  idl_retcode_t ret;
  uint32_t opcode;
  uint16_t cnt, off, next, mult = 0;
  const idl_case_t *_case;
  const idl_case_label_t *case_label;
  const idl_switch_type_spec_t *switch_type_spec;
  struct type type = { .next = NULL, .name = NULL };

  /* introduce new type, NOT scope */
  type.name = declarator ? idl_name(declarator) : idl_name(type_spec);
  push_type(descriptor, &type);

  while (idl_is_typedef(type_spec)) {
    assert(!idl_is_array(type_spec));
    type_spec = idl_type_spec(type_spec);
  }
  assert(idl_is_union(type_spec));

  opcode = DDS_OP_ADR | DDS_OP_TYPE_UNI;
  switch_type_spec = ((const idl_union_t *)type_spec)->switch_type_spec;
  opcode |= typecode(switch_type_spec->type_spec, SUBTYPE);
  if (idl_is_topic_key(pstate, descriptor->topic, switch_type_spec))
    opcode |= DDS_OP_FLAG_KEY;

  /* determine relative offset required for complex cases */
  off = descriptor->instructions.count;
  _case = ((const idl_union_t *)type_spec)->cases;
  for (; _case; _case = idl_next(_case)) {
    case_label = _case->case_labels;
    for (; case_label; case_label = idl_next(case_label), mult++) {
      if (idl_is_default_case(case_label))
        opcode |= DDS_OP_FLAG_DEF;
    }
  }

  /* generate data field opcode */
  if ((ret = stash_opcode(descriptor, nop, opcode)))
    return ret;
  /* generate data field offset */
  if ((ret = stash_offset(descriptor, nop, switch_type_spec)))
    return ret;
  /* generate date field alen */
  if ((ret = stash_single(descriptor, nop, mult)))
    return ret;

  cnt = descriptor->instructions.count;
  _case = ((const idl_union_t *)type_spec)->cases;
  for (; _case; _case = idl_next(_case)) {
    const idl_type_spec_t *alias;// = _case->type_spec;
    const idl_declarator_t *declarator = _case->declarator;
    bool simple = false;

    alias = _case->type_spec;
    //
    // FIXME: resolve type_spec
    //

    next = descriptor->instructions.count - cnt;
    /* distinguish between simple and complex cases */
    if (idl_is_array(declarator)) {
      opcode = DDS_OP_JEQ | DDS_OP_TYPE_ARR;
      // >> actually
      idl_declarator_t nop_declarator;
      nop_declarator = *declarator;
      nop_declarator.name = &nop_name;
      ret = emit_array(pstate, descriptor, alias, &nop_declarator);
    } else {
      opcode = DDS_OP_JEQ | typecode(alias, TYPE);
      if (idl_is_struct(alias)) {
        ret = emit_struct(pstate, descriptor, alias, NULL);
      } else if (idl_is_union(alias)) {
        ret = emit_union(pstate, descriptor, alias, NULL);
      } else if (idl_is_sequence(alias)) {
        ret = emit_sequence(pstate, descriptor, alias, NULL);
      } else if (idl_is_array(alias)) {
        ret = emit_array(pstate, descriptor, alias, NULL);
      } else {
        simple = true;
      }
    }

    /* complex cases are terminated with DDS_OP_RTS */
    if (ret || (!simple && (ret = stash_opcode(descriptor, nop, DDS_OP_RTS))))
      return ret;

    /* emit case label(s) */
    case_label = _case->case_labels;
    for (; case_label; case_label = idl_next(case_label), mult--) {
      if (!simple) {
        /* update offset to first instruction for complex cases */
        opcode &= ~0xffffu;
        opcode |= next + (mult*3);
      }
      /* generate union case opcode */
      if ((ret = stash_opcode(descriptor, cnt++, opcode)))
        return ret;
      /* generate union case discriminator */
      if ((ret = stash_constant(descriptor, cnt++, case_label->const_expr)))
        return ret;
      /* generate union case offset */
      if ((ret = stash_offset(descriptor, cnt++, declarator)))
        return ret;
    }
  }

  next = descriptor->instructions.count - off;
  if ((ret = stash_couple(descriptor, off+3, next, 4u)))
    return ret;

  pop_type(descriptor);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator)
{
  idl_retcode_t ret;
  uint32_t opcode = DDS_OP_ADR, size = 0;
  const idl_type_spec_t *alias = type_spec;
  uint16_t cnt, off, next;

  if (idl_is_array(declarator)) {
    size = idl_array_size(declarator);
    opcode |= typecode(declarator, TYPE);
  } else {
    while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
      type_spec = idl_type_spec(type_spec);
    size = idl_array_size(type_spec);
    opcode |= typecode(type_spec, TYPE);
    type_spec = idl_type_spec(type_spec);
  }

  /* either declarator or typedef had to declare array */
  assert(size);

  /* resolve typedef and squash multi-dimensional arrays */
  while (idl_is_typedef(type_spec)) {
    if (idl_is_array(type_spec))
      size *= idl_array_size(type_spec);
    type_spec = idl_type_spec(type_spec);
  }

  opcode |= typecode(type_spec, SUBTYPE);
//  if (idl_is_base_type(type_spec) || idl_is_enum(type_spec)) {
    if (idl_is_topic_key(pstate, descriptor->topic, declarator))
      opcode |= DDS_OP_FLAG_KEY;
//  }

  off = descriptor->instructions.count;
  /* generate data field opcode */
  if ((ret = stash_opcode(descriptor, nop, opcode)))
    return ret;
  /* generate data field offset */
  if ((ret = stash_offset(descriptor, nop, declarator)))
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
    return IDL_RETCODE_OK;
  } else if (idl_is_enum(type_spec)) {
    abort(); /* FIXME: implement */
  } else if (idl_is_base_type(type_spec)) {
    return IDL_RETCODE_OK;
  }

  struct scope scope = { .next = NULL, .types = { NULL, NULL } };
  push_scope(descriptor, &scope);

  cnt = descriptor->instructions.count;
  if (idl_is_struct(type_spec))
    ret = emit_struct(pstate, descriptor, alias, NULL);
  else if (idl_is_union(type_spec))
    ret = emit_union(pstate, descriptor, alias, NULL);
  else if (idl_is_sequence(type_spec))
    ret = emit_sequence(pstate, descriptor, alias, NULL);
  else
    abort();

  pop_scope(descriptor);

  /* generate data field [next-insn, elem-insn] */
  next = descriptor->instructions.count - off;
  next += (uint16_t)3u; /* opcodes below */
  if ((ret = stash_couple(descriptor, cnt++, next, 5u)))
    return ret;
  /* generate data field [elem-size] */
  if ((ret = stash_size(descriptor, cnt++, alias)))
    return ret;
  if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator)
{
  idl_retcode_t ret;
  const idl_type_spec_t *alias = type_spec;
  uint32_t opcode = DDS_OP_ADR;
  uint16_t cnt, off, next;

  /* resolve non-array aliases */
  while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
    type_spec = idl_type_spec(type_spec);

  assert(idl_is_sequence(type_spec));
  opcode |= DDS_OP_TYPE_SEQ; /* sequences cannot be keys */
  /* resolve sequence type specifier */
  alias = type_spec = idl_type_spec(type_spec);
  while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
    type_spec = idl_type_spec(type_spec);
  opcode |= typecode(type_spec, TYPE);

  off = descriptor->instructions.count;
  if ((ret = stash_opcode(descriptor, nop, opcode)))
    return ret;
  if ((ret = stash_offset(descriptor, nop, declarator)))
    return ret;

  /* short-circuit on simple types */
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
    uint32_t bnd = ((const idl_string_t *)type_spec)->maximum;
    if ((ret = stash_single(descriptor, nop, bnd)))
      return ret;
    return IDL_RETCODE_OK;
  } else if (idl_is_enum(type_spec)) {
    /* FIXME: will have maximum in future versions */
    return IDL_RETCODE_OK;
  } else if (idl_is_base_type(type_spec)) {
    return IDL_RETCODE_OK;
  }

  struct scope scope = { .next = NULL, .types = { NULL, NULL } };
  push_scope(descriptor, &scope);

  cnt = descriptor->instructions.count;
  if (idl_is_struct(type_spec))
    ret = emit_struct(pstate, descriptor, alias, NULL);
  else if (idl_is_union(type_spec))
    ret = emit_union(pstate, descriptor, alias, NULL);
  else if (idl_is_sequence(type_spec))
    ret = emit_sequence(pstate, descriptor, alias, NULL);
  else if (idl_is_array(type_spec))
    ret = emit_array(pstate, descriptor, alias, NULL);
  else
    abort();

  pop_scope(descriptor);

  next = descriptor->instructions.count - off;
  next += (uint16_t)3u; /* opcodes below */
  /* generate data field [elem-size] */
  if ((ret = stash_size(descriptor, cnt++, alias)))
    return ret;
  /* generate data field [next-insn, elem-insn] */
  if ((ret = stash_couple(descriptor, cnt++, next, 4u)))
    return ret;
  if ((ret = stash_opcode(descriptor, nop, DDS_OP_RTS)))
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_member(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  const idl_type_spec_t *alias = type_spec;

  /* resolve non-array aliases */
  while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
    type_spec = idl_type_spec(type_spec);

  for (; !ret && declarator; declarator = idl_next(declarator)) {
    /* special case */
    if (idl_is_array(declarator)) {
      ret = emit_array(pstate, descriptor, type_spec, declarator);
    } else if (idl_is_struct(type_spec)) {
      ret = emit_struct(pstate, descriptor, alias, declarator);
    } else if (idl_is_union(type_spec)) {
      ret = emit_union(pstate, descriptor, alias, declarator);
    } else if (idl_is_array(type_spec)) {
      ret = emit_array(pstate, descriptor, alias, declarator);
    } else if (idl_is_sequence(type_spec)) {
      ret = emit_sequence(pstate, descriptor, alias, declarator);
    } else {
      uint32_t opcode = 0;

      /* generate data field opcode */
      opcode = DDS_OP_ADR;
      opcode |= typecode(type_spec, TYPE);
      if (idl_is_topic_key(pstate, descriptor->topic, declarator))
        opcode |= DDS_OP_FLAG_KEY;
      if ((ret = stash_opcode(descriptor, nop, opcode)))
        return ret;
      /* generate data field offset */
      if ((ret = stash_offset(descriptor, nop, declarator)))
        return ret;
      /* generate data field bound */
      if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
        uint32_t max = ((const idl_string_t *)type_spec)->maximum;
        if ((ret = stash_single(descriptor, nop, max)))
          return ret;
      /* generate data field max */
      } else if (idl_is_enum(type_spec)) {
        abort(); /* FIXME: implement */
      }
    }
  }

  return ret;
}

#define idl_fprintf(...) fprintf(__VA_ARGS__)

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
                          type == DDS_OP_TYPE_UNI))
        || (!subtype && !(type == DDS_OP_TYPE_SEQ ||
                          type == DDS_OP_TYPE_ARR ||
                          type == DDS_OP_TYPE_UNI)));
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

static int print_operations(FILE *fp, const struct descriptor *desc)
{
  const struct instruction *inst;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode_primary optype;
  const char *seps[] = { ", ", ",\n  " };
  const char *sep = "  ";

  if (idl_fprintf(fp, "static const uint32_t %s_ops[] =\n{\n", desc->ctype) < 0)
    return -1;
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
        else if (optype == DDS_OP_TYPE_ARR)
          brk = op+3;
        else if (optype == DDS_OP_TYPE_UNI)
          brk = op+4;
        else
          brk = op+2;
        if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
          return -1;
        break;
      case OFFSET:
        if (fputs(sep, fp) < 0 || print_offset(fp, inst) < 0)
          return -1;
        break;
      case SIZE:
        if (fputs(sep, fp) < 0 || print_size(fp, inst) < 0)
          return -1;
        break;
      case CONSTANT:
        if (fputs(sep, fp) < 0 || print_constant(fp, inst) < 0)
          return -1;
        break;
      case COUPLE:
        if (fputs(sep, fp) < 0 || print_couple(fp, inst) < 0)
          return -1;
        break;
      case SINGLE:
        if (fputs(sep, fp) < 0 || print_single(fp, inst) < 0)
          return -1;
        break;
    }
  }
  if (fputs("\n};\n\n", fp) < 0)
    return -1;
  return 0;
}

static int print_keys(FILE *fp, const struct descriptor *desc)
{
  int cnt;
  const struct instruction *inst;
  enum dds_stream_opcode opcode;
  size_t key = 0, len, off;
  const char headfmt[] = "static const dds_key_descriptor_t %s_keys[%zu] =\n"
                         "{\n";
  const char tailfmt[] = "};\n\n";

  len = strlen(desc->ctype);
  if (idl_fprintf(fp, headfmt, desc->ctype, desc->keys) < 0)
    return -1;
  for (size_t op=0; op < desc->instructions.count && key < desc->keys; op++) {
    inst = &desc->instructions.table[op];
    if (inst->type != OPCODE)
      continue;
    opcode = inst->data.opcode & (0xffu << 24);
    if (opcode != DDS_OP_ADR)
      continue;
    assert(op+1 < desc->instructions.count);
    inst = &desc->instructions.table[op+1];
    assert(inst->type == OFFSET);
    assert(inst->data.offset.type);
    assert(inst->data.offset.member);
    off = strncmp(desc->ctype, inst->data.offset.type, len) == 0 ? len : 0;
    for (; inst->data.offset.type[off] == '.'; off++) ;
    if (strlen(inst->data.offset.type+off))
      cnt = idl_fprintf(fp, "  { \"%s.%s\", %zu }\n", inst->data.offset.type+off, inst->data.offset.member, op);
    else
      cnt = idl_fprintf(fp, "  { \"%s\", %zu }\n", inst->data.offset.member, op);
    if (cnt == -1)
      return -1;
    op++;
    key++;
  }
  if (idl_fprintf(fp, tailfmt) == -1)
    return -1;

  return 0;
}

idl_retcode_t
emit_topic_descriptor(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct scope scope = { NULL, { NULL, NULL } };
  struct descriptor descriptor = { { NULL, NULL }, NULL, NULL, 0, { 0, 0, NULL }, NULL };

  (void)user_data;
  /* must be invoked for topics only, so structs (and unions?) only */
  assert(idl_is_struct(node) || idl_is_union(node));

  push_scope(&descriptor, &scope);
  descriptor.ctype = "foo";
  descriptor.typename = "foo::bar";
  descriptor.topic = node;

  ret = emit_struct(pstate, &descriptor, node, NULL);
  if ((ret = stash_opcode(&descriptor, nop, DDS_OP_RTS)))
    goto err_emit;
  pop_scope(&descriptor);

  print_keys(stderr, &descriptor);
  print_operations(stderr, &descriptor);

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
