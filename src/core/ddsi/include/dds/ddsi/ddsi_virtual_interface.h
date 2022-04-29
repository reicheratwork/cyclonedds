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
#ifndef DDSI_VIRTUAL_INTERFACE_H
#define DDSI_VIRTUAL_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dds/ddsi/ddsi_locator.h"


typedef struct ddsi_virtual_interface_s ddsi_virtual_interface;
typedef struct dds_qos dds_qos_t;
struct dds_topic;
struct dds_writer;
struct dds_reader;
struct proxy_reader;
struct proxy_writer;
struct dds_serdata;
struct ddsi_domaingv;

enum pipe_kind {
  PIPE_KIND_UNSET,
  PIPE_KIND_SOURCE,
  PIPE_KIND_SINK
};

union local_endpoint {
  struct dds_writer * source;
  struct dds_reader * sink;
};

union remote_endpoint {
  struct proxy_writer * source;
  struct proxy_reader * sink;
};

typedef struct ddsi_virtual_interface_pipe_s ddsi_virtual_interface_pipe;
struct ddsi_virtual_interface_pipe_s {
  ddsi_virtual_interface * virtual_interface;
  enum pipe_kind kind;
  bool supports_loan;
  union local_endpoint here;
  union remote_endpoint there;
};

typedef struct ddsi_virtual_interface_pipe_list_elem_s ddsi_virtual_interface_pipe_list_elem;
typedef struct ddsi_virtual_interface_pipe_list_elem_s {
  ddsi_virtual_interface_pipe * pipe;
  ddsi_virtual_interface_pipe_list_elem * next;
};

typedef bool (*ddsi_virtual_interface_compute_locator) (
  ddsi_virtual_interface * self,
  ddsi_locator_t ** locator,
  struct ddsi_domaingv * gv
);

typedef bool (*ddsi_virtual_interface_match_locator) (
  ddsi_virtual_interface * self,
  const ddsi_locator_t * locator
);

typedef bool (*ddsi_virtual_interface_topic_and_qos_supported) (
  ddsi_virtual_interface * self,
  const struct dds_topic * topic,
  const dds_qos_t * qos
);

typedef bool (*ddsi_virtual_interface_pipe_open) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_pipe ** pipe,
  pipe_kind kind,
  void * local,
  void * remote
);

typedef bool (*ddsi_virtual_interface_pipe_close) (
  ddsi_virtual_interface_pipe * pipe
);

typedef bool (*ddsi_virtual_interface_on_data_func) (
  ddsi_virtual_interface_pipe * pipe,
  void ** data
);

typedef bool (*ddsi_virtual_interface_pipe_set_on_source_data) (
  ddsi_virtual_interface_pipe * pipe,
  ddsi_virtual_interface_on_data_func * on_data_func  /*this function is to be triggered when data is incoming on this pipe*/
);

typedef bool (*ddsi_virtual_interface_pipe_request_loan) (
  ddsi_virtual_interface_pipe * pipe,
  void ** out,
  size_t size_requested
);

typedef bool (*ddsi_virtual_interface_pipe_return_loan) (
  ddsi_virtual_interface_pipe * pipe,
  void * in
);

typedef bool (*ddsi_virtual_interface_pipe_sink_data) (
  ddsi_virtual_interface_pipe * pipe,
  struct dds_serdata * serdata
);

typedef bool (*ddsi_virtual_interface_deinit) (
  ddsi_virtual_interface * self
);

struct ddsi_virtual_interface_ops {
  ddsi_virtual_interface_compute_locator          compute_locator;
  ddsi_virtual_interface_match_locator            match_locator;
  ddsi_virtual_interface_topic_and_qos_supported  topic_and_qos_supported;
  ddsi_virtual_interface_pipe_open                pipe_open;
  ddsi_virtual_interface_pipe_close               pipe_close;
  ddsi_virtual_interface_pipe_request_loan        pipe_request_loan;
  ddsi_virtual_interface_pipe_return_loan         pipe_return_loan;
  ddsi_virtual_interface_pipe_sink_data           pipe_sink_data;
  ddsi_virtual_interface_pipe_set_on_source_data  pipe_set_on_source;
  ddsi_virtual_interface_deinit                   deinit;
};

struct ddsi_virtual_interface_s {
  uint16_t kind;
  int32_t  default_priority;
  bool     loan_supported;
  struct ddsi_virtual_interface_ops ops;
};

typedef bool (*ddsi_virtual_interface_create_fn) (ddsi_virtual_interface * interface, const char * configuration_string);
#endif // DDSI_VIRTUAL_INTERFACE_H
