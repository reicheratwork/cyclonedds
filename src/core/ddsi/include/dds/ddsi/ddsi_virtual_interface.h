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

typedef void* ddsi_virtual_interface_source_pipe;
typedef bool (*ddsi_virtual_interface_source_pipe_open) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_source_pipe ** pipe,
  struct dds_reader * reader,
  struct proxy_writer * writer
);

typedef bool (*ddsi_virtual_interface_source_pipe_close) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_source_pipe * pipe
);

typedef void* ddsi_virtual_interface_sink_pipe;
typedef bool (*ddsi_virtual_interface_sink_pipe_open) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_sink_pipe ** pipe,
  struct dds_writer * writer,
  struct proxy_reader * reader
);

typedef bool (*ddsi_virtual_interface_sink_pipe_close) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_sink_pipe * pipe
);

typedef bool (*ddsi_virtual_interface_sink_pipe_write) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_sink_pipe ** pipe,
  struct dds_serdata * serdata
);

typedef bool (*ddsi_virtual_interface_sink_pipe_chunk_loan) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_sink_pipe * pipe,
  void ** chunk,
  size_t size_requested
);

typedef bool (*ddsi_virtual_interface_sink_pipe_chunk_return) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_sink_pipe * pipe,
  void * chunk
);

typedef bool (*ddsi_virtual_interface_source_pipe_chunk_return) (
  ddsi_virtual_interface * self,
  ddsi_virtual_interface_source_pipe * pipe,
  void * chunk
);

typedef bool (*ddsi_virtual_interface_deinit) (
  ddsi_virtual_interface * self
);

struct ddsi_virtual_interface_ops {
  ddsi_virtual_interface_compute_locator          compute_locator;
  ddsi_virtual_interface_match_locator            match_locator;
  ddsi_virtual_interface_topic_and_qos_supported  topic_and_qos_supported;
  ddsi_virtual_interface_source_pipe_open         source_pipe_open;
  ddsi_virtual_interface_source_pipe_close        source_pipe_close;
  ddsi_virtual_interface_sink_pipe_open           sink_pipe_open;
  ddsi_virtual_interface_sink_pipe_close          sink_pipe_close;
  ddsi_virtual_interface_sink_pipe_write          sink_pipe_write;
  ddsi_virtual_interface_sink_pipe_chunk_loan     sink_pipe_chunk_loan;
  ddsi_virtual_interface_sink_pipe_chunk_return   sink_pipe_chunk_return;
  ddsi_virtual_interface_source_pipe_chunk_return source_pipe_chunk_return;
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
