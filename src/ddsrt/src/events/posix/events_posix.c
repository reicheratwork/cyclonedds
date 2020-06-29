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

#include "dds/ddsrt/events/posix.h"
#include "dds/ddsrt/events.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"

#ifdef __VXWORKS__
#include <pipeDrv.h>
#include <ioLib.h>
#include <string.h>
#include <selectLib.h>
#define OSPL_PIPENAMESIZE 26
#endif

#define EVENTS_CONTAINER_DELTA 8

/**
* @brief Posix implementation of ddsrt_event_queue.
*
* This implementation uses the ddsrt_select function to set the rfds set of file descriptors 
* for sockets which have data available for reading. If this not implemented under the light-
* weight IP stack, then interuption of a wait using a trigger on a self socket.
*/
struct ddsrt_event_queue {
  ddsrt_event_t**         events;  /**< container for administered events*/
  size_t                  nevents;  /**< number of administered events stored*/
  size_t                  cevents;  /**< capacity of administered events stored*/
  size_t                  ievents;  /**< current iterator for getting the next triggered event*/
  fd_set                  rfds;     /**< set of fds for reading data*/
  ddsrt_mutex_t           lock;     /**< for add/delete */
#if !defined(LWIP_SOCKET)
  ddsrt_socket_t          interrupt[2]; /**< pipe for interrupting waits*/
#endif /* !LWIP_SOCKET */
};

/**
* @brief Initializes an event queue.
*
* Will set the counters to 0 and create the containers for triggers and additional necessary ones.
*
* @param[out] queue The queue to initialize.
*
* @returns DDS_RETCODE_OK if everything went OK.
*/
static dds_return_t ddsrt_event_queue_init(ddsrt_event_queue_t* queue) {
  queue->nevents = 0;
  queue->cevents = 8;
  queue->ievents = 0;
  queue->events = ddsrt_malloc(sizeof(ddsrt_event_t*) * queue->cevents);
  assert(queue->events);
  ddsrt_mutex_init(&queue->lock);
#if defined(_WIN32)
  /*windows type sockets*/
  struct sockaddr_in addr;
  socklen_t asize = sizeof(addr);
  ddsrt_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
  queue->interrupt[0] = socket(AF_INET, SOCK_STREAM, 0);
  queue->interrupt[1] = DDSRT_INVALID_SOCKET;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
    getsockname(listener, (struct sockaddr*)&addr, &asize) == -1 ||
    listen(listener, 1) == -1 ||
    connect(queue->interrupt[0], (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
    (queue->interrupt[1] = accept(listener, 0, 0)) == -1) {
    closesocket(queue->interrupt[0]);
    closesocket(queue->interrupt[1]);
    return DDS_RETCODE_ERROR;
  }
  SetHandleInformation((HANDLE)queue->interrupt[0], HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE)queue->interrupt[1], HANDLE_FLAG_INHERIT, 0);
#elif defined(__VXWORKS__)  
  /*vxworks type pipe*/
  char pipename[OSPL_PIPENAMESIZE];
  int pipecount = 0;
  int pipe_result = 0;
  do {
    snprintf((char*)&pipename, sizeof(pipename), "/pipe/ospl%d", pipecount++);
  } while ((pipe_result = pipeDevCreate((char*)&pipename, 1, 1)) == -1 &&
    os_getErrno() == EINVAL);
  if (pipe_result != -1)
    return DDS_RETCODE_ERROR;
  queue->interrupt[0] = open((char*)&pipename, O_RDWR, 0644);
  queue->interrupt[1] = open((char*)&pipename, O_RDWR, 0644);
  /*the pipe was succesfully created, but one of the sockets on either end was not*/
  if (-1 == queue->interrupt[0] || -1 == p[1]) {
    pipeDevDelete(pipename, 0);
    if (-1 != queue->interrupt[0])
      close(queue->interrupt[0]);
    if (-1 != queue->interrupt[1])
      close(queue->interrupt[1]);
    return DDS_RETCODE_ERROR;
  }
#elif !defined(LWIP_SOCKET)
  /*simple linux type pipe*/
  if (pipe(queue->interrupt) == -1) return DDS_RETCODE_ERROR;
#endif /* _WIN32, __VXWORKS__, !LWIP_SOCKET*/
  return DDS_RETCODE_OK;
}

/**
* @brief Finishes an event queue.
*
* Will free created containers and do any additional cleanup of things created in ddsrt_event_queue_init.
*
* @param[out] queue The queue to finish.
*
* @returns DDS_RETCODE_OK if everything went OK.
*/
static dds_return_t ddsrt_event_queue_fini(ddsrt_event_queue_t* queue) {
  assert(queue);
  ddsrt_mutex_destroy(&queue->lock); 
#if !defined(LWIP_SOCKET)

#if defined(__VXWORKS__) && defined(__RTP__)
  char nameBuf[OSPL_PIPENAMESIZE];
  ioctl(queue->interrupt[0], FIOGETNAME, &nameBuf);
#endif /*__VXWORKS__ && __RTP__*/

#if defined(_WIN32)
  closesocket(queue->interrupt[0]);
  closesocket(queue->interrupt[1]);
#else
  close(queue->interrupt[0]);
  close(queue->interrupt[1]);
#endif  /* _WIN32 */

#if defined(__VXWORKS__) && defined(__RTP__)
  pipeDevDelete((char*)&nameBuf, 0);
#endif /* __VXWORKS__ && __RTP__ */

#endif /* !LWIP_SOCKET */
  ddsrt_free(queue->events);
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
  return queue->nevents;
}

dds_return_t ddsrt_event_queue_wait(ddsrt_event_queue_t* queue, dds_duration_t reltime) {
  assert(queue);
  
  /*reset triggered status*/
  ddsrt_mutex_lock(&queue->lock);
  queue->ievents = 0;
  for (size_t i = 0; i < queue->nevents; i++)
    ddsrt_atomic_st32(&queue->events[i]->triggered, DDSRT_EVENT_FLAG_UNSET);

  /*zero all fds*/
  fd_set* rfds = &queue->rfds;
  FD_ZERO(rfds);
#if !defined(LWIP_SOCKET)
  FD_SET(queue->interrupt[0],rfds);
#endif /* !LWIP_SOCKET */

  /*add events to queue->rfds*/
  ddsrt_socket_t maxfd = 0;
  for (size_t i = 0; i < queue->nevents; i++) {
    ddsrt_event_t *evt = queue->events[i];
    if (evt->type != ddsrt_event_type_socket)
      continue;

    ddsrt_socket_t s = evt->data.socket.sock;
    if (evt->flags & DDSRT_EVENT_FLAG_READ) {
      FD_SET(s, rfds);
      if (s > maxfd)
        maxfd = s;
    }
  }
  ddsrt_mutex_unlock(&queue->lock);

  /*start wait*/
  int ready = -1;
  dds_return_t retval = ddsrt_select(maxfd + 1, rfds, NULL, NULL, reltime, &ready);

  if (DDS_RETCODE_OK == retval ||
    DDS_RETCODE_TIMEOUT == retval) {

#if !defined(LWIP_SOCKET)
    /*read the data from the interrupt socket (if any)*/
    if (FD_ISSET(queue->interrupt[0], rfds)) {
      char buf = 0x0;
#if defined(_WIN32)
      if (1 != recv(queue->interrupt[0], &buf, 1, 0))
        return DDS_RETCODE_ERROR;
#else
      if (1 != read(queue->interrupt[0], &buf, 1))
        return DDS_RETCODE_ERROR;
#endif  /* _WIN32 */
    }
#endif  /* !LWIP_SOCKET */

    /*check queue->rfds for set items*/
    ddsrt_mutex_lock(&queue->lock);
    for (size_t i = 0; i < queue->nevents; i++) {
      ddsrt_event_t *evt = queue->events[i];
      if (evt->type != ddsrt_event_type_socket)
        continue;

      ddsrt_socket_t s = evt->data.socket.sock;
      if (FD_ISSET(s, rfds))
        ddsrt_atomic_st32(&evt->triggered, DDSRT_EVENT_FLAG_READ);
    }
    ddsrt_mutex_unlock(&queue->lock);
  }
  else {
    /*something else happened*/
    return DDS_RETCODE_ERROR;
  }

  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_event_queue_signal(ddsrt_event_queue_t* queue) {
#if !defined(LWIP_SOCKET)
  char buf = 0;
#if defined(_WIN32)
  if (1 != send(queue->interrupt[1], &buf, 1, 0))
    return DDS_RETCODE_ERROR;
#else
  if (1 != write(queue->interrupt[1], &buf, 1))
    return DDS_RETCODE_ERROR;
#endif /* _WIN32 */

#endif /* !LWIP_SOCKET */
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_event_queue_add(ddsrt_event_queue_t* queue, ddsrt_event_t* evt) {
  assert(queue);
  assert(evt);

  ddsrt_mutex_lock(&queue->lock);
  if (queue->nevents == queue->cevents) {
    queue->cevents += EVENTS_CONTAINER_DELTA;
    ddsrt_realloc(queue->events, sizeof(ddsrt_event_t*) * queue->cevents);
  }
  
  queue->events[queue->nevents++] = evt;
  ddsrt_mutex_unlock(&queue->lock);
  
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_event_queue_remove(ddsrt_event_queue_t* queue, ddsrt_event_t* evt) {
  ddsrt_mutex_lock(&queue->lock);
  for (size_t i = 0; i < queue->nevents; i++) {
    if (queue->events[i] == evt) {
      memmove(queue->events + i, queue->events + i + 1, (queue->nevents - i - 1)*sizeof(ddsrt_event_t*));
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
    if (DDSRT_EVENT_FLAG_UNSET != ddsrt_atomic_ld32(&evt->triggered))
      ptr = evt;
  }
  ddsrt_mutex_unlock(&queue->lock);
  return ptr;
}