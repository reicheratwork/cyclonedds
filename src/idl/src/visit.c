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
#include <string.h>
#include <stdlib.h>

#include "idl/processor.h"

static idl_accept_t idl_accept(const void *node)
{
  idl_mask_t mask = idl_mask(node);
  if ((mask & IDL_SEQUENCE) == IDL_SEQUENCE)
    return IDL_ACCEPT_SEQUENCE;
  if ((mask & IDL_STRING) == IDL_STRING)
    return IDL_ACCEPT_STRING;
  if (mask & IDL_INHERIT_SPEC)
    return IDL_ACCEPT_INHERIT_SPEC;
  if (mask & IDL_SWITCH_TYPE_SPEC)
    return IDL_ACCEPT_SWITCH_TYPE_SPEC;
  if (!(mask & IDL_DECLARATION))
    return IDL_ACCEPT;
  if (mask & IDL_MODULE)
    return IDL_ACCEPT_MODULE;
  if (mask & IDL_CONST)
    return IDL_ACCEPT_CONST;
  if (mask & IDL_MEMBER)
    return IDL_ACCEPT_MEMBER;
  if (mask & IDL_FORWARD)
    return IDL_ACCEPT_FORWARD;
  if (mask & IDL_CASE)
    return IDL_ACCEPT_CASE;
  if (mask & IDL_CASE_LABEL)
    return IDL_ACCEPT_CASE_LABEL;
  if (mask & IDL_ENUMERATOR)
    return IDL_ACCEPT_ENUMERATOR;
  if (mask & IDL_DECLARATOR)
    return IDL_ACCEPT_DECLARATOR;
  if (mask & IDL_ANNOTATION)
    return IDL_ACCEPT_ANNOTATION;
  if (mask & IDL_ANNOTATION_APPL)
    return IDL_ACCEPT_ANNOTATION_APPL;
  if (mask & IDL_TYPEDEF)
    return IDL_ACCEPT_TYPEDEF;
  if (mask & IDL_STRUCT)
    return IDL_ACCEPT_STRUCT;
  if (mask & IDL_UNION)
    return IDL_ACCEPT_UNION;
  if (mask & IDL_ENUM)
    return IDL_ACCEPT_ENUM;
  return IDL_ACCEPT;
}

struct frame {
  uint32_t flags;
  idl_visit_t visit;
};

struct stack {
  size_t size;
  size_t depth;
  size_t cookie;
  void *frames;
};

#define FRAME(stack, depth) \
  ((struct frame *)(((uintptr_t)(stack)->frames)+((depth) * (sizeof(struct frame) + (stack)->cookie))))

static void fixup(struct stack *stack, size_t depth)
{
  struct frame *frame;
  idl_visit_t *previous = NULL, *next = NULL;
  void *cookie = NULL;

  frame = FRAME(stack, depth);
  if (depth > 0)
    previous = &FRAME(stack, depth - 1)->visit;
  if (depth < (stack->depth - 1))
    next = &FRAME(stack, depth + 1)->visit;
  if (stack->cookie)
    cookie = (void *)((uintptr_t)frame + sizeof(*frame));
  { const struct frame template = {
      frame->flags, {
        previous, next,
        frame->visit.type,
        frame->visit.node,
        cookie
      }
    };
    memcpy(frame, &template, sizeof(template));
  }
}

static void setup(struct stack *stack, size_t depth, const void *node)
{
  struct frame *frame;
  idl_visit_t *previous = NULL;
  void *cookie = NULL;

  frame = FRAME(stack, depth);
  memset(frame, 0, sizeof(*frame) + stack->cookie);
  if (depth > 0)
    previous = &FRAME(stack, depth - 1)->visit;
  if (stack->cookie)
    cookie = (void *)((uintptr_t)frame + sizeof(*frame));
  { const struct frame template =
      { 0, { previous, NULL, IDL_ENTER, node, cookie } };
    memcpy(frame, &template, sizeof(template));
  }
}

static struct frame *peek(struct stack *stack)
{
  assert(stack);
  return stack->depth ? FRAME(stack, stack->depth - 1) : NULL;
}

static const idl_node_t *pop(struct stack *stack)
{
  struct frame *frame;

  assert(stack);
  assert(stack->depth);
  /* FIXME: implement shrinking the stack */
  stack->depth--;
  if (stack->depth)
    fixup(stack, stack->depth - 1);
  frame = FRAME(stack, stack->depth);
  return frame->visit.node;
}

static struct frame *push(struct stack *stack, const idl_node_t *node)
{
  struct frame *frame;

  /* grow stack if necessary */
  if (stack->depth == stack->size) {
    size_t size = stack->size + 10;
    struct frame *frames;
    if (!(frames = realloc(stack->frames, size*(sizeof(*frame)+stack->cookie))))
      return NULL;
    stack->size = size;
    stack->frames = frames;
    /* correct pointers */
    for (size_t i=0; i < stack->depth; i++)
      fixup(stack, i);
  }

  stack->depth++;
  if (stack->depth > 1)
    fixup(stack, stack->depth-2);
  setup(stack, stack->depth-1, node);
  return FRAME(stack, stack->depth-1);
}

#undef FRAME

#define YES (0)
#define NO (1)
#define MAYBE (2)

static const idl_visit_recurse_t recurse[] = {
  IDL_VISIT_RECURSE,
  IDL_VISIT_DONT_RECURSE,
  IDL_VISIT_RECURSE|IDL_VISIT_DONT_RECURSE
};

static idl_visit_iterate_t iterate[] = {
  IDL_VISIT_ITERATE,
  IDL_VISIT_DONT_ITERATE,
  IDL_VISIT_ITERATE|IDL_VISIT_DONT_ITERATE
};

static idl_visit_revisit_t revisit[] = {
  IDL_VISIT_REVISIT,
  IDL_VISIT_DONT_REVISIT,
  IDL_VISIT_REVISIT|IDL_VISIT_DONT_REVISIT
};

/* visit iteratively to save stack space */
idl_retcode_t
idl_visit(
  const idl_pstate_t *pstate,
  const void *node,
  const idl_visitor_t *visitor,
  void *user_data)
{
  idl_retcode_t ret;
  idl_accept_t accept;
  idl_visitor_callback_t callback;
  struct stack stack = { 0, 0, 0u, NULL };
  struct frame *frame;
  uint32_t flags = 0u;
  bool walk = true;

  assert(pstate);
  assert(node);
  assert(visitor);

  flags |= recurse[ visitor->recurse == recurse[NO]  ];
  flags |= iterate[ visitor->iterate == iterate[NO]  ];
  flags |= revisit[ visitor->revisit != revisit[YES] ];

  stack.cookie = visitor->cookie;
  if (!(frame = push(&stack, node)))
    goto err_push;
  frame->flags = flags;

  while ((frame = peek(&stack))) {
    accept = idl_accept(frame->visit.node);
    if (visitor->accept[accept])
      callback = visitor->accept[accept];
    else
      callback = visitor->accept[IDL_ACCEPT];

    if (walk) {
      /* skip or visit */
      if (!callback || !(idl_mask(frame->visit.node) & visitor->visit))
        ret = IDL_RETCODE_OK;
      else if (visitor->glob && strcmp(((const idl_node_t *)frame->visit.node)->symbol.location.first.source->path->name, visitor->glob) != 0)
        ret = IDL_RETCODE_OK;
      else if ((ret = callback(pstate, &frame->visit, frame->visit.node, user_data)) < 0)
        goto err_visit;
      /* override default flags */
      if (ret & (idl_retcode_t)recurse[MAYBE]) {
        frame->flags &= ~recurse[MAYBE];
        frame->flags |=  recurse[ (ret & (idl_retcode_t)recurse[NO]) != 0 ];
      }
      if (ret & (idl_retcode_t)iterate[MAYBE]) {
        frame->flags &= ~iterate[MAYBE];
        frame->flags |=  iterate[ (ret & (idl_retcode_t)iterate[NO]) != 0 ];
      }
      if (ret & (idl_retcode_t)revisit[MAYBE]) {
        frame->flags &= ~revisit[MAYBE];
        frame->flags |=  revisit[ (ret & (idl_retcode_t)revisit[NO]) != 0 ];
      }
      if (ret & IDL_VISIT_TYPE_SPEC) {
        node = idl_type_spec(node);
        if (ret & IDL_VISIT_UNALIAS_TYPE_SPEC) {
          while (idl_is_typedef(node))
            node = idl_type_spec(node);// = idl_unalias(node);
        }
        assert(node);
        if (!(frame = push(&stack, node)))
          goto err_push;
        frame->flags = flags | IDL_VISIT_TYPE_SPEC;
        walk = true;
      } else if (frame->flags & IDL_VISIT_RECURSE) {
        node = idl_iterate(frame->visit.node, NULL);
        if (node) {
          if (!(frame = push(&stack, node)))
            goto err_push;
          frame->flags = flags;
          walk = true;
        } else {
          walk = false;
        }
      } else {
        walk = false;
      }
    } else {
      if (callback && (frame->flags & IDL_VISIT_REVISIT)) {
        /* callback must exist if revisit is true */
        frame->visit.type = IDL_EXIT;
        if ((ret = callback(pstate, &frame->visit, frame->visit.node, user_data)) < 0)
          goto err_revisit;
      }
      if (frame->flags & (IDL_VISIT_TYPE_SPEC|IDL_VISIT_DONT_ITERATE)) {
        pop(&stack);
      } else {
        node = pop(&stack);
        if ((frame = peek(&stack)))
          node = idl_iterate(frame->visit.node, node);
        else
          node = idl_next(node);
        if (node) {
          if (!(frame = push(&stack, node)))
            goto err_push;
          frame->flags = flags;
          walk = true;
        }
      }
    }
  }

  if (stack.frames)
    free(stack.frames);
  return IDL_RETCODE_OK;
err_push:
  ret = IDL_RETCODE_OUT_OF_MEMORY;
err_revisit:
err_visit:
  if (stack.frames)
    free(stack.frames);
  return ret;
}
