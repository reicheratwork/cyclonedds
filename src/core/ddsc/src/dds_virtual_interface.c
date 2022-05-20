/*
 * Copyright(c) 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/ddsc/dds_virtual_interface.h"

#include <assert.h>

#include "dds__alloc.h"

bool add_topic_to_list (
  ddsi_virtual_interface_topic_t *toadd,
  ddsi_virtual_interface_topic_list_elem_t **addto)
{
  ddsi_virtual_interface_topic_list_elem_t *ptr = dds_alloc(sizeof(ddsi_virtual_interface_topic_list_elem_t));

  if (!ptr)
    return false;

  ptr->topic = toadd;
  ptr->next = NULL;

  if (!*addto) {
    //there is no list yet
    ptr->prev = NULL;
    *addto = ptr;
  } else {
    //add to the end of the list
    ddsi_virtual_interface_topic_list_elem_t *ptr2 = *addto;
    while (ptr2->next) {
      ptr2 = ptr2->next;
    }
    ptr2->next = ptr;
    ptr->prev = ptr2;
  }

  return true;
}

bool remove_topic_from_list (
  ddsi_virtual_interface_topic_t *to_remove,
  ddsi_virtual_interface_topic_list_elem_t **remove_from)
{
  assert (to_remove);

  if (!remove_from || !*remove_from)
    return false;

  ddsi_virtual_interface_topic_list_elem_t *list_entry = *remove_from;

  while (list_entry && list_entry->topic != to_remove) {
    list_entry = list_entry->next;
  }

  if (!list_entry ||  //no entry in the list matching the topic
      !list_entry->topic->virtual_interface->ops.topic_destruct(list_entry->topic)) //destruct failure
    return false;

  if (list_entry->prev)
    list_entry->prev->next = list_entry->next;

  if (list_entry->next)
    list_entry->next->prev = list_entry->prev;

  if (list_entry == *remove_from)
    *remove_from = list_entry->next;

  dds_free(list_entry);

  return true;
}

bool add_pipe_to_list (
  ddsi_virtual_interface_pipe_t *toadd,
  ddsi_virtual_interface_pipe_list_elem_t **addto)
{
  ddsi_virtual_interface_pipe_list_elem_t *ptr = dds_alloc(sizeof(ddsi_virtual_interface_pipe_list_elem_t));

  if (!ptr)
    return false;

  ptr->pipe = toadd;
  ptr->next = NULL;

  if (!*addto) {
    //there is no list yet
    ptr->prev = NULL;
    *addto = ptr;
  } else {
    //add to the end of the list
    ddsi_virtual_interface_pipe_list_elem_t *ptr2 = *addto;
    while (ptr2->next) {
      ptr2 = ptr2->next;
    }
    ptr2->next = ptr;
    ptr->prev = ptr2;
  }

  return true;
}

bool remove_pipe_from_list (
  ddsi_virtual_interface_pipe_t *to_remove,
  ddsi_virtual_interface_pipe_list_elem_t **remove_from)
{
  assert (to_remove);

  if (!remove_from || !*remove_from)
    return false;

  ddsi_virtual_interface_pipe_list_elem_t *list_entry = *remove_from;

  while (list_entry && list_entry->pipe != to_remove) {
    list_entry = list_entry->next;
  }

  if (!list_entry ||  //no entry in the list matching the topic
      !list_entry->pipe->topic->ops.pipe_close(list_entry->pipe))   //destruct failure
    return false;

  if (list_entry->prev)
    list_entry->prev->next = list_entry->next;

  if (list_entry->next)
    list_entry->next->prev = list_entry->prev;

  if (list_entry == *remove_from)
    *remove_from = list_entry->next;

  dds_free(list_entry);

  return true;
}

virtual_interface_data_type_t calculate_data_type(struct ddsi_sertype * type)
{
  (void) type;

  return (virtual_interface_data_type_t)-1; /*type->serdata_basehash; ???  maybe use a better calculation method?*/
}

virtual_interface_topic_identifier_t calculate_topic_identifier(struct dds_topic * topic)
{
  (void) topic;

 return (virtual_interface_topic_identifier_t)-1; /*hash of topic->m_name?*/
}

virtual_interface_identifier_t calculate_interface_identifier(struct ddsi_domaingv * cyclone_domain)
{
  (void) cyclone_domain;

  return (virtual_interface_identifier_t)-1; /*!!!TODO!!!: implement!*/
}


bool memory_block_cleanup(memory_block_t *block)
{
  if (NULL == block)
    return true;

  if (block->block_origin) {
    block->block_origin->ops.unref_block(block->block_origin, block);
  } else {
    dds_free(block->block_ptr);
    dds_free(block);
  }

  return true;
}

memory_block_t * memory_block_create(ddsi_virtual_interface_pipe_t *pipe, size_t size)
{
  memory_block_t * ptr = NULL;

  if (pipe) {
    ptr = pipe->ops.request_loan(pipe, size);
    if (!ptr)
      goto fail;
  } else {
    ptr = dds_alloc(sizeof(memory_block_t));
    if (!ptr)
      goto fail;
    memset(ptr, 0x0, sizeof(memory_block_t));
    ptr->block_ptr = dds_alloc(size);
    if (!ptr->block_ptr)
      goto fail;
  }

  return ptr;

fail:
  if (ptr)
    memory_block_cleanup(ptr);
  return NULL;
}
