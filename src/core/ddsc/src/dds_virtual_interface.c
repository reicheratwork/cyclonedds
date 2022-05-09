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
  ddsi_virtual_interface_topic_t *toremove,
  ddsi_virtual_interface_topic_list_elem_t **removefrom)
{
  assert (toremove);

  if (!removefrom || !*removefrom)
    return false;

  ddsi_virtual_interface_topic_list_elem_t *list_entry = *removefrom;

  while (list_entry && list_entry->topic != toremove) {
    list_entry = list_entry->next;
  }

  if (!list_entry ||  //no entry in the list matching the topic
      !list_entry->topic->virtual_interface->ops.topic_destruct(list_entry->topic)) //destruct failure
    return false;

  if (list_entry->prev)
    list_entry->prev->next = list_entry->next;

  if (list_entry->next)
    list_entry->next->prev = list_entry->prev;

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
  ddsi_virtual_interface_pipe_t *toremove,
  ddsi_virtual_interface_pipe_list_elem_t **removefrom)
{
  assert (toremove);

  if (!removefrom || !*removefrom)
    return false;

  ddsi_virtual_interface_pipe_list_elem_t *list_entry = *removefrom;

  while (list_entry && list_entry->pipe != toremove) {
    list_entry = list_entry->next;
  }

  if (!list_entry ||  //no entry in the list matching the topic
      !list_entry->pipe->topic->ops.pipe_close(list_entry->pipe))   //destruct failure
    return false;

  if (list_entry->prev)
    list_entry->prev->next = list_entry->next;

  if (list_entry->next)
    list_entry->next->prev = list_entry->prev;

  dds_free(list_entry);

  return true;
}
