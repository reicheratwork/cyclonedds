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

#include "idl/processor.h"
#include "idl/string.h"

#include "descriptor.h"

const uint16_t nop = UINT16_MAX;

#define DDS_OP_ADR (1u<<0)
#define DDS_OP_FLAG_KEY (1u<<10)
#define DDS_OP_RTS (1u<<2)

/* store each instruction separately for easy post processing and reduced
   complexity. arrays and sequences introduce a new scope and the relative
   offset to the next field is stored with the instructions for the respective
   field. this requires the generator to revert its position. using separate
   streams intruduces too much bookkeeping. the table can also be used to
   generate a key offset table after the fact */
struct instruction {
  enum {
    OPCODE,
    OFFSET,
    SIZE,
    BRANCH,
    NUMBER
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
      uint16_t next; /**< offset to instruction for next field */
      uint16_t first; /**< offset to first instruction for element or first case label */
    } branch;
    uint32_t number;
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
  struct {
    uint16_t size; /**< available number of instructions */
    uint16_t count; /**< used number of instructions */
    struct instruction *table;
  } instructions;
  const idl_node_t *topic;
};

static inline void push_type(struct descriptor *descriptor, struct type *type)
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

static inline void pop_type(struct descriptor *descriptor)
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

static inline void push_scope(
  struct descriptor *descriptor, struct scope *scope)
{
  if (descriptor->scopes.last) {
    assert(descriptor->scopes.first);
    descriptor->scopes.last->next = scope;
    descriptor->scopes.last = scope;
  } else {
    descriptor->scopes.first = descriptor->scopes.last = scope;
  }
}

static inline void pop_scope(
  struct descriptor *descriptor)
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
emit_instruction(
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
emit_opcode(
  struct descriptor *descriptor,
  uint16_t index,
  uint32_t opcode)
{
  struct instruction inst = { OPCODE, { .opcode = opcode } };
  return emit_instruction(descriptor, index, &inst);
}

static idl_retcode_t
emit_offset(
  struct descriptor *descriptor,
  uint16_t index,
  const idl_declarator_t *declarator)
{
  const char *sep;
  const struct type *type;
  struct instruction inst = { OFFSET, { .offset = { NULL, NULL } } };
  size_t off = 0, len = 0;

  if (!descriptor->scopes.last || !declarator)
    return emit_instruction(descriptor, index, &inst);

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

  if (!(inst.data.offset.member = idl_strdup(declarator->name->identifier)))
    goto err_member;
  if (emit_instruction(descriptor, index, &inst))
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
emit_size(
  struct descriptor *descriptor,
  uint16_t index,
  const idl_declarator_t *type_spec)
{
  struct instruction inst = { SIZE, { .size = { NULL } } };

  assert(type_spec);

  if (idl_is_sequence(type_spec)) {
    // for sequences a type has been created if it did not already exist.
    // so if it's a direct sequence that type_spec points towards, we have to
    // do some magic. for now assume that the type exists!
    abort();
  } else {
    inst.data.size.type = idl_strdup(idl_identifier(type_spec));
  }

  if (!inst.data.size.type)
    goto err_type;
  if (emit_instruction(descriptor, index, &inst))
    goto err_emit;

  return IDL_RETCODE_OK;
err_emit:
  free(inst.data.size.type);
err_type:
  return IDL_RETCODE_OUT_OF_MEMORY;
}

static idl_retcode_t
emit_branch(
  struct descriptor *descriptor,
  uint16_t index,
  uint16_t next,
  uint16_t first)
{
  const struct instruction inst = { BRANCH, { .branch = { next, first } } };
  return emit_instruction(descriptor, index, &inst);
}

static idl_retcode_t
emit_number(
  struct descriptor *descriptor,
  uint16_t index,
  uint32_t number)
{
  struct instruction inst = { NUMBER, { .number = number } };
  return emit_instruction(descriptor, index, &inst);
}

static uint32_t typecode(const idl_type_spec_t *type_spec, int shift)
{
  // do not concern with array here...
  (void)type_spec;
  (void)shift;
  return 1u;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec);

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec,
  const idl_declarator_t *declarator);

static idl_retcode_t
emit_instance(
  const idl_pstate_t *,
  struct descriptor *,
  const idl_type_spec_t *,
  const idl_declarator_t *);

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec)
{
  idl_retcode_t ret;
  const idl_member_t *member;
  /* introduces new type, NOT scope */
  struct type type = { .next = NULL, .name = idl_name(type_spec) };
  push_type(descriptor, &type);
fprintf(stderr, "struct: %s\n", type.name->identifier);
  member = ((const idl_struct_t *)type_spec)->members;
  for (; member; member = idl_next(member)) {
    if ((ret = emit_instance(pstate, descriptor, member->type_spec, member->declarators)))
      return ret;
  }

  pop_type(descriptor);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  const idl_type_spec_t *type_spec)
{
  (void)pstate;
  (void)descriptor;
  (void)type_spec;
  if (1)
    abort();
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
    opcode |= typecode(declarator, 8);
  } else {
    while (idl_is_typedef(type_spec) && !idl_is_array(type_spec))
      type_spec = idl_type_spec(type_spec);
    size = idl_array_size(type_spec);
    opcode |= typecode(type_spec, 8);
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

  opcode |= typecode(type_spec, 16);
  if (idl_is_base_type(type_spec) || idl_is_enum(type_spec)) {
    if (idl_is_topic_key(pstate, descriptor->topic, declarator))
      opcode |= DDS_OP_FLAG_KEY;
  }

  off = descriptor->instructions.count;
  /* generate data field opcode */
  if ((ret = emit_opcode(descriptor, nop, opcode)))
    return ret;
  /* generate data field offset */
  if ((ret = emit_offset(descriptor, nop, declarator)))
    return ret;
  /* generate data field alen */
  if ((ret = emit_number(descriptor, nop, size)))
    return ret;

  /* short-circuit on simple types */
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
    uint32_t max = ((const idl_string_t *)type_spec)->maximum;
    /* generate data field noop [next-insn, elem-insn] */
    if ((ret = emit_branch(descriptor, nop, 0, 0)) < 0)
      return ret;
    /* generate data field bound */
    if ((ret = emit_number(descriptor, nop, max)))
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
    ret = emit_struct(pstate, descriptor, alias);
  else if (idl_is_union(type_spec))
    ret = emit_union(pstate, descriptor, alias);
  else if (idl_is_sequence(type_spec))
    ret = emit_sequence(pstate, descriptor, alias, NULL);
  else
    abort();

  pop_scope(descriptor);

  /* generate data field [next-insn, elem-insn] */
  next = descriptor->instructions.count - off;
  if ((ret = emit_branch(descriptor, cnt++, next, 4u)))
    return ret;
  /* generate data field [elem-size] */
  if ((ret = emit_size(descriptor, cnt++, alias)))
    return ret;
  if ((ret = emit_opcode(descriptor, nop, DDS_OP_RTS)))
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
  /* (possibly) introduces new opcode frame, NOT type frame */
  //
  // e.g. sequence of struct
  //      >> sequence of sequence, that sort of thing.
  // must be a sequence of array or struct
  // introduces a new opcode scope!
  // does not introduce a new type frame!
  //
  (void)pstate;
  (void)descriptor;
  (void)type_spec;
  (void)declarator;
  if (1)
    abort();
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_instance(
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
      ret = emit_struct(pstate, descriptor, alias);
    } else if (idl_is_union(type_spec)) {
      ret = emit_union(pstate, descriptor, alias);
    } else if (idl_is_array(type_spec)) {
      ret = emit_array(pstate, descriptor, alias, declarator);
    } else if (idl_is_sequence(type_spec)) {
      ret = emit_sequence(pstate, descriptor, alias, declarator);
    } else {
      uint32_t opcode = 0;

      /* generate data field opcode */
      opcode = DDS_OP_ADR;
      opcode |= typecode(type_spec, 8);
      if (idl_is_topic_key(pstate, descriptor->topic, declarator))
        opcode |= DDS_OP_FLAG_KEY;
      if ((ret = emit_opcode(descriptor, nop, opcode)) < 0)
        return ret;
      /* generate data field offset */
      if ((ret = emit_offset(descriptor, nop, declarator)) < 0)
        return ret;
      /* generate data field bound */
      if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
        uint32_t max = ((const idl_string_t *)type_spec)->maximum;
        if ((ret = emit_number(descriptor, nop, max)) < 0)
          return ret;
      /* generate data field max */
      } else if (idl_is_enum(type_spec)) {
        abort(); /* FIXME: implement */
      }
    }
  }

  return ret;
}

//static idl_retcode_t
//emit_instance(
//  const idl_pstate_t *pstate,
//  const idl_node_t *node,
//  struct descriptor *descriptor)
//{
//  const idl_declarator_t *declarator;
//
//  declarator = idl_declarator(node);
//  assert(declarator);
//  do {
//    const idl_type_spec_t *type_spec;
//    //, *subtype_spec;
//    size_t array_size = 0;
//    const char *ops[4];
//    size_t len = 0;
//    bool key;
//    size_t arr, len, max;
//    const char *sep;
//    const char *ops[4];
//    const topic_namespace_t *spc;
//
//    type_spec = idl_type_spec(node);
//
//    if (idl_is_array(declarator)) {
//      if ((ret = emit_array(pstate, type_spec, declarator, descriptor)))
//        return ret;
//      //array_size = idl_array_size(declarator);
//      //subtype_spec = type_spec;
//    } else {
//      /* resolve non-array aliases */
//      while (idl_is_typedef(type_spec)) {
//        if (idl_is_array(type_spec))
//          break;
//        type_spec = idl_type_spec(type_spec);
//      }
//      if (idl_is_array(type_spec)) {
//        array_size = idl_array_size(type_spec);
//        subtype_spec = idl_type_spec(type_spec);
//        assert(sybtype_spec);
//      }
//    }
//
//    ops[len++] = "DDS_OP_ADR";
//    /* array takes precedence */
//    if (arr) {
//      ops[len++] = "DDS_OP_TYPE_ARR";
//      /* resolve aliases, squash multi-dimensional arrays */
//      while (idl_is_typedef(subtype_spec)) {
//        if (idl_is_array(subtype_spec)) // FIXME: add overflow detection!
//          array_size *= idl_array_size(subtype_spec);
//        subtype_spec = idl_type_spec(subtype_spec);
//      }
//      ops[len++] = typecode(subtype_spec, 16);
//    /* sequence takes precendence after array */
//    } else if (idl_is_sequence(type_spec)) {
//      ops[len++] = typecode(type_spec, 8);
//      subtype_spec = idl_type_spec(type_spec);
//      /* resolve non-array aliases */
//      while (idl_is_typedef(subtype_spec) && !idl_is_array(subtype_spec))
//        subtype_spec = idl_type_spec(subtype_spec);
//      if (idl_is_array(subtype_spec))
//        ops[len++] = "DDS_OP_SUBTYPE_ARR";
//      else
//        ops[len++] = typecode(subtype_spec, 16);
//    } else {
//      ops[len++] = typecode(type_spec, 8);
//    }
//
//    /* generate data field opcode */
//    if ((key = idl_is_key(pstate, topic, node, declarator)))
//      ops[len++] = "DDS_OP_FLAG_KEY";
//    cnt = desc->opcodes.count++;
//    /* generate data field offset and key */
//    scope = desc->namespaces.first;
//    name = scope->name->identifier;
//    if ((ret = idl_putf(desc->opcodes.stream, ", offsetof(%s, ", name)) < 0)
//      return ret;
//    if (key && (ret = idl_puts(desc->keys.stream, "  { \"")) < 0)
//      return ret;
//    sep = "";
//    for (scope = scope->next; scope; scope = scope->next) {
//      str = scope->name->identifier;
//      if ((ret = idl_putf(desc->opcodes.stream, "%s%s", sep, str)) < 0)
//        return ret;
//      if (key && (ret = idl_putf(desc->keys.stream, "%s%s", sep, str)) < 0)
//        return ret;
//    }
//    str = idl_identifier(declarator);
//    if ((ret = idl_putf(desc->opcodes.stream, "%s%s)", sep, str)) < 0)
//      return ret;
//    if ((ret = idl_putf(desc->keys.stream, "%s%s, %zu }\n", sep, str, cnt)) < 0)
//      return ret;
//    desc->opcodes.count++;
//    desc->keys.count++;
//
//    if (subtype_spec) {
//      //
//      // here comes the tricky stuff!
//      // >> at least, if it's a complex type!
//      // >> we must open a new opcode stream so that we can count!
//      //
//    }
//    /* generate array size opcodes if applicable */
//    //if (arr && (ret = idl_putf(desc->opcodes.stream, ", %u, 0", arr)) < 0)
//    //  return ret;
//    //desc->opcodes.count++;
//    /* generate bound opcode */
//    // use max so we can keep it the same for enums too!
//    //if ((ret = idl_putf(desc->opcodes.stream, ", %"PRIu32",\n", max+1)) < 0)
//    //  return ret;
//    //desc->opcodes.count++;
//  } while ((declarator = idl_next(declarator)));
//
//  return IDL_RETCODE_OK;
//}



idl_retcode_t
emit_topic_descriptor(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct scope scope = { NULL, { NULL, NULL } };
  struct descriptor descriptor = { { NULL, NULL }, { 0, 0, NULL }, NULL };

  (void)user_data;
  //   >> this gets called for topics only. so structs and unions, nothing else
  assert(idl_is_struct(node) || idl_is_union(node));

  push_scope(&descriptor, &scope);
  descriptor.topic = node;

  ret = emit_struct(pstate, &descriptor, node);
  emit_opcode(&descriptor, nop, DDS_OP_RTS);

  for (size_t i=0; i < descriptor.instructions.count; i++) {
    struct instruction *inst = &descriptor.instructions.table[i];
    switch (inst->type) {
      case OPCODE:
        fprintf(stderr, "OPCODE: %u\n", inst->data.opcode);
        break;
      case OFFSET:
        fprintf(stderr, "OFFSET: %s, %s\n", inst->data.offset.type, inst->data.offset.member);
        break;
      case SIZE:
        fprintf(stderr, "SIZE: %s\n", inst->data.size.type);
        break;
      case BRANCH:
        fprintf(stderr, "BRANCH: (%u << 16) | %u\n", inst->data.branch.next, inst->data.branch.first);
        break;
      case NUMBER:
        fprintf(stderr, "NUMBER: %u\n", inst->data.number);
        break;
      default:
        break;
    }
  }

  pop_scope(&descriptor);

  return ret;
}
