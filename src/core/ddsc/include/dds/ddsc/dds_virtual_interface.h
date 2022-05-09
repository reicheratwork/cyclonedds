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
#ifndef DDS_VIRTUAL_INTERFACE_H
#define DDS_VIRTUAL_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dds/export.h"

/*forward declarations of used data types*/
typedef struct dds_qos dds_qos_t;
struct ddsi_locator;
struct dds_topic;
struct dds_writer;
struct dds_reader;
struct proxy_reader;
struct proxy_writer;
struct ddsi_serdata;
struct ddsi_domaingv;

typedef struct ddsi_virtual_interface ddsi_virtual_interface_t;
typedef struct ddsi_virtual_interface_topic ddsi_virtual_interface_topic_t;
typedef struct ddsi_virtual_interface_pipe ddsi_virtual_interface_pipe_t;

#define MAX_VIRTUAL_INTERFACES 8

/* linked list describing a number of topics
*/
typedef struct ddsi_virtual_interface_topic_list_elem ddsi_virtual_interface_topic_list_elem_t;
struct ddsi_virtual_interface_topic_list_elem {
  ddsi_virtual_interface_topic_t * topic; /*the current element in the list*/
  ddsi_virtual_interface_topic_list_elem_t * prev; /*the next element in the list*/
  ddsi_virtual_interface_topic_list_elem_t * next; /*the next element in the list*/
};

DDS_EXPORT bool add_topic_to_list (
  ddsi_virtual_interface_topic_t *toadd,
  ddsi_virtual_interface_topic_list_elem_t **addto);

DDS_EXPORT bool remove_topic_from_list (
  ddsi_virtual_interface_topic_t *toremove,
  ddsi_virtual_interface_topic_list_elem_t **removefrom);

/* linked list describing a number of pipes
*/
typedef struct ddsi_virtual_interface_pipe_list_elem ddsi_virtual_interface_pipe_list_elem_t;
struct ddsi_virtual_interface_pipe_list_elem {
  ddsi_virtual_interface_pipe_t * pipe; /*the current element in the list*/
  ddsi_virtual_interface_pipe_list_elem_t * prev; /*the next element in the list*/
  ddsi_virtual_interface_pipe_list_elem_t * next; /*the next element in the list*/
};

DDS_EXPORT bool add_pipe_to_list (
  ddsi_virtual_interface_pipe_t *toadd,
  ddsi_virtual_interface_pipe_list_elem_t **addto);

DDS_EXPORT bool remove_pipe_from_list (
  ddsi_virtual_interface_pipe_t *toremove,
  ddsi_virtual_interface_pipe_list_elem_t **removefrom);

/* the definition of a block of memory originating
* from a virtual interface
*/
typedef struct memory_block {
  ddsi_virtual_interface_pipe_t *origin;  /*the local pipe this block originates from*/
  void *block;  /*pointer to the block*/
  size_t size;  /*size of the block*/
  bool is_serialized; /*whether the block's data is serialized*/
  /*serdata basehash, unique id of data type*/
} memory_block_t;

/*
*/
typedef bool (*ddsi_virtual_interface_compute_locator) (
  ddsi_virtual_interface_t * vi,
  struct ddsi_locator ** locator,
  struct ddsi_domaingv * gv
);

/*
*/
typedef bool (*ddsi_virtual_interface_match_locator) (
  ddsi_virtual_interface_t * vi,
  const struct ddsi_locator * locator
);

/*
*/
typedef bool (*ddsi_virtual_interface_topic_and_qos_supported) (
  const struct dds_topic * topic,
  const dds_qos_t * qos
);

/* creates a virtual interface topic
*/
typedef ddsi_virtual_interface_topic_t* (*ddsi_virtual_interface_topic_create) (
  ddsi_virtual_interface_t * vi,
  struct dds_topic * cyclone_topic
);


/* destructs a virtual interface topic
*/
typedef bool (*ddsi_virtual_interface_topic_destruct) (
  ddsi_virtual_interface_topic_t *vi_topic
);

/* checks whether serialization is required on this 
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_serialization_required) (
  ddsi_virtual_interface_topic_t * topic  /*the topic to check whether serialization is required*/
);

/* opens a pipe on a virtual interface
* returns true on success
*/
typedef ddsi_virtual_interface_pipe_t* (*ddsi_virtual_interface_pipe_open) (
  ddsi_virtual_interface_topic_t * topic,  /*the topic to create the pipe on*/
  void * cdds_counterpart /*the CDDS counterpart of the pipe*/
);

/* closes a pipe
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_close) (
  ddsi_virtual_interface_pipe_t * pipe  /*the pipe to close*/
);

/* requests a loan from the virtual interface
* returns a pointer to the loaned block on success
*/
typedef memory_block_t* (*ddsi_virtual_interface_pipe_request_loan) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to loan from*/
  size_t size_requested /*the size of the loan requested*/
);

/* returns the requested loan to the virtual interface
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_return_block) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to return the loan to*/
  memory_block_t * block /*the loaned block to return*/
);

/* returns the requested loan to the virtual interface
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_return_loan) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to return the loan to*/
  void * loan /*the loane to return*/
);

/* sinks data on a pipe
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_sink_data) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to sink the data on*/
  struct ddsi_serdata * serdata  /*the data to sink*/
);

/* sources data on a pipe
* used in a poll based implementation
* returns the oldest unsourced received block of memory
*/
typedef memory_block_t* (*ddsi_virtual_interface_pipe_source_data) (
  ddsi_virtual_interface_pipe_t * pipe /*the pipe to source the data from*/
);

/* checks whether a sample is loaned from a pipe
* returns the memory block of the sample if it originates from the pipe
*/
typedef memory_block_t* (*ddsi_virtual_interface_pipe_loan_origin) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to check*/
  const void * sample /*the sample to check*/
);

/* definition of the callback function which is to be triggered
* on reception of data on this pipe
* used in an event based implementation
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_on_data_func) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe which is triggered*/
  memory_block_t *block  /*incoming data on the pipe*/
);

/* callback function setter
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_set_on_source_data) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to set the callback function on*/
  ddsi_virtual_interface_on_data_func * on_data_func  /*this function is to be triggered when data is incoming on this pipe*/
);

/* virtual interface cleanup function
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_deinit) (
  ddsi_virtual_interface_t * vi
);

/* container for all functions which are used on a virtual interface
*/
typedef struct ddsi_virtual_interface_ops {
  ddsi_virtual_interface_compute_locator          compute_locator;
  ddsi_virtual_interface_match_locator            match_locator;
  ddsi_virtual_interface_topic_and_qos_supported  topic_and_qos_supported;
  ddsi_virtual_interface_topic_create             topic_create;
  ddsi_virtual_interface_topic_destruct           topic_destruct;
  ddsi_virtual_interface_deinit                   deinit;
} ddsi_virtual_interface_ops_t;

/* container for all functions which are used on a virtual interface topic
*/
typedef struct ddsi_virtual_interface_topic_ops {
  ddsi_virtual_interface_serialization_required   serialization_required;
  ddsi_virtual_interface_pipe_open                pipe_open;
  ddsi_virtual_interface_pipe_close               pipe_close;
} ddsi_virtual_interface_topic_ops_t;

/* container for all functions which are used on a virtual interface pipe
*/
typedef struct ddsi_virtual_interface_pipe_ops {
  ddsi_virtual_interface_pipe_request_loan        request_loan;
  ddsi_virtual_interface_pipe_return_loan         return_loan;
  ddsi_virtual_interface_pipe_return_block        return_block;
  ddsi_virtual_interface_pipe_loan_origin         originates_loan;
  ddsi_virtual_interface_pipe_sink_data           sink_data;
  ddsi_virtual_interface_pipe_source_data         source_data;
  /*if the set_on_source is not set, then there is no event based functionality, you will need to poll for new data*/
  ddsi_virtual_interface_pipe_set_on_source_data  set_on_source;
} ddsi_virtual_interface_pipe_ops_t;

/* the top-level entry point on the virtual interface
* is bound to a specific implementation of a virtual interface
*/
struct ddsi_virtual_interface {
  ddsi_virtual_interface_ops_t ops; /*associated functions*/
  const char *interface_name; /*type of interface being used*/
  int32_t default_priority;  /*priority of choosing this interface*/
  struct ddsi_domaingv *cyclone_domain; /*the associated cyclone domain*/
  ddsi_virtual_interface_topic_list_elem_t * topics; /*associated topics*/
};

/* the topic-level virtual interface
* this will exchange data for readers and writers which are matched through discovery
* will only exchange a single type of data!
*/
struct ddsi_virtual_interface_topic {
  ddsi_virtual_interface_topic_ops_t ops; /*associated functions*/
  ddsi_virtual_interface_t * virtual_interface; /*the virtual interface which created this pipe*/
  struct dds_topic * cyclone_topic; /*the associated cyclone topic*/
  /*unique identifier of topic (name?) (entity_id?) (GUID?)*/
  ddsi_virtual_interface_pipe_list_elem_t * pipes;/*associated pipes*/
  bool supports_loan; /*whether the topic supports loan semantics*/
};

/* the definition of one instance of a dds
* reader/writer using a virtual interface
*/
struct ddsi_virtual_interface_pipe {
  ddsi_virtual_interface_pipe_ops_t ops; /*associated functions*/
  ddsi_virtual_interface_topic_t * topic; /*the topic this pipe belongs to*/
  void *cdds_counterpart;
};

/* this is the only function exported from the virtual interface library
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_create_fn) (
  ddsi_virtual_interface_t **virtual_interface, /*output for the virtual interface to be created*/
  struct ddsi_domaingv *cyclone_domain, /*the domain associated with this interface*/
  const char * configuration_string /*optional configuration data*/
);
#endif // DDS_VIRTUAL_INTERFACE_H
