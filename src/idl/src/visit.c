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

// stuff!


//    idl_backend_ctx ctx,
//    const idl_node_t *target_node,
//    idl_walkAction action,
//    idl_mask_t mask)

typedef idl_retcode_t(*idl_visit_t)(
  const idl_pstate_t *pstate,
  const void *node,
  void *user_data);

//
// << you get stuff!
//

// >> maybe we should call this a selector?!?!
typedef struct idl_filter idl_filter_t;
struct idl_filter {
  bool iterate;
  bool recurse;
  const char *file;
};

static bool matches_filter(const idl_node_t *node, const idl_filter_t *filter)
{
  //  if (strcmp(node->symbol.location.source->path, filter.
  // >> check the mask as well!
  //uint32_t flags, // << whether to recurse or not...
  //                // << whether to only pick this single node or not...
  return true;
}

static inline idl_retcode_t
visit_module(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_module_t *node,
  idl_visit_t action,
  void *user_data)
{
  idl_retcode_t ret;
  if ((ret = action(pstate, node, user_data)))
    return ret;
  if ((ret = idl_visit(pstate, filter, node->definitions, action, user_data)))
    return ret;
  return IDL_RETCODE_OK;
}

static inline idl_retcode_t
visit_struct(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_struct_t *node,
  idl_visit_t action,
  void *user_data)
{
  idl_retcode_t ret;
  if ((ret = action(pstate, node, user_data)))
    return ret;
  if ((ret = idl_visit(pstate, filter, node->members, action, user_data)))
    return ret;
  return IDL_RETCODE_OK;
}

static inline idl_retcode_t
visit_union(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_union_t *node,
  idl_visit_t action,
  void *user_data)
{
  idl_retcode_t ret;
  if ((ret = action(pstate, node, user_data)))
    return ret;
  if ((ret = idl_visit(pstate, filter, node->cases, action, user_data)))
    return ret;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_visit(
  const idl_pstate_t *pstate,
  const idl_filter_t *filter,
  const idl_node_t *node,
  idl_visit_t action,
  void *user_data)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

// >> remember to make this here depth first?!?!

  for (; ret == IDL_RETCODE_OK && filter.iterate; node = node->next) {
    if (!matches_filter(node, filter))
      continue;
    if (idl_is_module(node))
      ret = visit_module(pstate, filter, node, action, user_data);
    else if (idl_is_struct(node))
      ret = visit_struct(pstate, filter, node, action, user_data);
    else if (idl_is_union(node))
      ret = visit_union(pstate, filter, node, action, user_data);
    else if (idl_is_enum(node))
      ret = visit_enum(pstate, filter, node, action, user_data);
    else if (idl_is_typedef(node))
      ret = visit_typedef(pstate, filter, node, action, user_data);
    else if (idl_is_const(node))
      ret = visit_const(pstate, filter, node, action, user_data);
    else if (idl_is_member(node))
      ret = visit_member(pstate, filter, node, action, user_data);
    else if (idl_is_case(node))
      ret = visit_case(pstate, filter, node, action, user_data);
    else
      assert(0);
  }

  return ret;
}


























































