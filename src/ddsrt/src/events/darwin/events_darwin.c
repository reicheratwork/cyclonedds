/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
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
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/events/darwin.h"
#include "dds/ddsrt/events.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"

#define EVENTS_CONTAINER_DELTA 8

struct ddsrt_event_queue {
  ddsrt_event_t**         events;   /**< container for triggered events*/
  size_t                  nevents;  /**< number of triggered events stored*/
  size_t                  cevents;  /**< capacity of triggered events stored*/
  size_t                  ievents;  /**< current iterator for getting the next triggered event*/
  int                     kq;       /**< kevent polling instance*/
  kevent*                 kevents;  /**< array which kevent uses to write back to, has identical size as this->events*/
  ddsrt_mutex_t           lock;     /**< for add/delete */
#if !defined(LWIP_SOCKET)
  ddsrt_socket_t          interrupt[2]; /**< pipe for interrupting waits*/
#endif
};

/**
* @brief Initializes an event queue.
*
* Will set the counters to 0 and create the containers for triggers and additional necessary ones.
*
* @param[in] queue The queue to initialize.
*
* @returns DDS_RETCODE_OK if everything went OK.
*/
static dds_return_t ddsrt_event_queue_init(ddsrt_event_queue_t* queue) {
  queue->nevents = 0;
  queue->cevents = 8;
  queue->ievents = 0;
  ddsrt_mutex_init(&queue->lock);

  /*create kevent polling instance */
  queue->kq = kqueue();
  assert(queue->kq != -1);
  assert(fcntl(queue->kq, F_SETFD, fcntl(queue->kq, F_GETFD) | FD_CLOEXEC) != -1);

  queue->events = ddsrt_malloc(sizeof(ddsrt_event_t*) * queue->cevents);
  assert(queue->events);

#if !defined(LWIP_SOCKET)
  if (-1 == pipe(queue->interrupt))
    return DDS_RETCODE_ERROR;
  /*register interrupt event*/
  kevent kev;
  EV_SET(&kev, queue->interrupt[0], EVFILT_READ, EV_ADD, 0, 0, 0);
  assert(kevent(kq, &kev, 1, NULL, 0, NULL) != -1);
#endif

  /*create kevents array*/
  queue->kevents = ddsrt_malloc(sizeof(kevent) * queue->cevents);
  assert(queue->kevents);

  return DDS_RETCODE_OK;
}

/**
* @brief Finishes an event queue.
*
* Will free created containers and do any additional cleanup of things created in ddsrt_event_queue_init.
*
* @param queue The queue to finish.
*
* @returns DDS_RETCODE_OK if everything went OK.
*/
static dds_return_t ddsrt_event_queue_fini(ddsrt_event_queue_t* queue) {
  assert(queue);
  /*deregister all currently stored triggers from kqueue*/
  kevent kev;
  for (unsigned int i = 0; i < queue->nevents; i++) {
    ddsrt_event_t* evt = queue->events[i];
    if (evt->flags & DDSRT_EVENT_FLAG_READ)
      EV_SET(&kev, evt->data.socket, EVFILT_READ, EV_DELETE, 0, 0, evt);
    assert(kevent(kq, &kev, 1, NULL, 0, NULL) != -1);
  }
#if !defined(LWIP_SOCKET)
  if (evt->flags & DDSRT_EVENT_FLAG_READ)
    EV_SET(&kev, interrupt[0], EVFILT_READ, EV_DELETE, 0, 0, 0);
  close(queue->interrupt[0]);
  close(queue->interrupt[1]);
#endif
  close(queue->kq);

  ddsrt_mutex_destroy(&queue->lock);
  ddsrt_free(queue->events);
  ddsrt_free(queue->kevents);
  return DDS_RETCODE_OK;
}

ddsrt_event_queue_t* ddsrt_event_queue_create(void) {
  ddsrt_event_queue_t* returnptr = ddsrt_malloc(sizeof(ddsrt_event_queue_t));
  assert(returnptr);
  ddsrt_event_queue_init(returnptr);
  return returnptr;
}

dds_return_t ddsrt_event_queue_destroy(ddsrt_event_queue_t* queue) {
  assert(queue);
  ddsrt_event_queue_fini(queue);
  ddsrt_free(queue);
  return DDS_RETCODE_OK;
}

size_t ddsrt_event_queue_nevents(ddsrt_event_queue_t* queue) {
  size_t ret;
  ddsrt_mutex_lock(&queue->lock);
  ret = queue->nevents;
  ddsrt_mutex_unlock(&queue->lock);
  return ret;
}

dds_return_t ddsrt_event_queue_wait(ddsrt_event_queue_t* queue, dds_duration_t reltime) {
  assert(queue);

  /*reset triggered status*/
  ddsrt_mutex_lock(&queue->lock);
  queue->ievents = 0;
  for (unsigned int i = 0; i < queue->nevents; queue++)
    ddsrt_atomic_st32(&queue->events[i]->triggered, DDSRT_EVENT_FLAG_UNSET);
  ddsrt_mutex_unlock(&queue->lock);

  struct timespec tmout = { milliseconds / 1000,     /* waittime (seconds) */
                           milliseconds * 1000000 }; /* waittime (nanoseconds) */
  int nevs = kevent(queue->kq, NULL, 0, queue->kevents, queue->nevents, &tmout);
  ddsrt_mutex_lock(&queue->lock);
  for (int i = 0; i < nevs; i++) {
    ddsrt_event_t* evt = queue->kevents[i].udata;
    if (NULL != evt) {
      ddsrt_atomic_st32(evt->triggered, DDSRT_EVENT_FLAG_READ);
    }
    else {
#if !defined(LWIP_SOCKET)
      char buf;
      read(queue->interrupt[0], &buf, 0);
#endif
    }
  }
  ddsrt_mutex_unlock(&queue->lock);

  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_event_queue_add(ddsrt_event_queue_t* queue, ddsrt_event_t* evt) {
  assert(queue);
  assert(evt);

  ddsrt_mutex_lock(&queue->lock);
  if (queue->nevents == queue->cevents) {
    queue->cevents += EVENTS_CONTAINER_DELTA;
    ddsrt_realloc(queue->events, queue->events, sizeof(ddsrt_event_t*) * queue->cevents);
    ddsrt_realloc(queue->kevents, queue->kevents, sizeof(kevent) * queue->cevents);
  }
  queue->events[queue->nevents++] = evt;
  ddsrt_mutex_unlock(&queue->lock);

  /*register to queue->kq*/
  kevent kev;
  EV_SET(&kev, evt->data.socket.sock, EVFILT_READ, EV_ADD, 0, 0, evt);
  assert(kevent(kq, &kev, 1, NULL, 0, NULL) != -1);

  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_event_queue_signal(ddsrt_event_queue_t* queue) {
#if !defined(LWIP_SOCKET)
  char buf = 0;
  if (-1 == write(queue->interrupt[1], &buf, 1))
    return DDS_RETCODE_ERROR;
#endif
  return DDS_RETURN_OK;
}

dds_return_t ddsrt_event_queue_remove(ddsrt_event_queue_t* queue, ddsrt_event_t* evt) {
  ddsrt_mutex_lock(&queue->lock);
  for (unsigned int i = 0; i < queue->nevents; i++) {
    if (queue->events[i] == evt) {
      /*deregister from queue->kq*/
      kevent kev;
      EV_SET(&kev, evt->data.socket.sock, EVFILT_READ, EV_DELETE, 0, 0, evt);
      assert(kevent(kq, &kev, 1, NULL, 0, NULL) != -1);

      memmove(queue->events + i, queue->events + i + 1, (queue->nevents - i - 1) * sizeof(ddsrt_event_t*));
      if (queue->ievents > i)
        queue->ievents--;
      queue->nevents--;
      break;
    }
  }
  ddsrt_mutex_unlock(&queue->lock);

  return DDS_RETCODE_OK;
}

ddsrt_event_t* ddsrt_event_queue_next(ddsrt_event_queue_t* queue) {
  ddsrt_event_t* ptr = NULL;
  ddsrt_mutex_lock(&queue->lock);
  while (queue->ievents < queue->nevents) {
    ddsrt_event_t* evt = queue->events[queue->ievents++];
    if (DDSRT_EVENT_FLAG_UNSET != evt->triggered)
      ptr = evt;
  }
  ddsrt_mutex_unlock(&queue->lock);
  return ptr;
}