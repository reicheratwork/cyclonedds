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
#include "dds/ddsc/dds_basic_types.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_keyhash.h"
#include "dds/ddsrt/time.h"

/*state of the data contained in a memory block*/
typedef enum loaned_sample_state {
  LOANED_SAMPLE_STATE_UNITIALIZED,
  LOANED_SAMPLE_STATE_RAW,
  LOANED_SAMPLE_STATE_SERIALIZED_KEY,
  LOANED_SAMPLE_STATE_SERIALIZED_DATA
} loaned_sample_state_t;

/*forward declarations of used data types*/
struct dds_qos;
struct ddsi_locator;  //is private header
struct ddsi_domaingv; //is private header
struct ddsi_sertype;

/*forward declarations of virtual interfaces data types*/
typedef struct ddsi_virtual_interface ddsi_virtual_interface_t;
typedef struct ddsi_virtual_interface_topic ddsi_virtual_interface_topic_t;
typedef struct ddsi_virtual_interface_pipe ddsi_virtual_interface_pipe_t;
typedef struct dds_loaned_sample dds_loaned_sample_t;

#define MAX_VIRTUAL_INTERFACES 8

/* linked list describing a number of topics
*/
typedef struct ddsi_virtual_interface_topic_list_elem ddsi_virtual_interface_topic_list_elem_t;
struct ddsi_virtual_interface_topic_list_elem {
  ddsi_virtual_interface_topic_t * topic; /*the current element in the list*/
  ddsi_virtual_interface_topic_list_elem_t * prev; /*the previous element in the list*/
  ddsi_virtual_interface_topic_list_elem_t * next; /*the next element in the list*/
};

/* adds a topic to the list, will create the first list entry if it does not yet exist
*/
DDS_EXPORT bool add_topic_to_list (
  ddsi_virtual_interface_topic_t * toadd,
  ddsi_virtual_interface_topic_list_elem_t ** addto);

/* removes a topic from the list, will set the pointer to the list to null if the last entry is removed
*/
DDS_EXPORT bool remove_topic_from_list (
  ddsi_virtual_interface_topic_t * to_remove,
  ddsi_virtual_interface_topic_list_elem_t ** remove_from);

/* linked list describing a number of pipes
*/
typedef struct ddsi_virtual_interface_pipe_list_elem ddsi_virtual_interface_pipe_list_elem_t;
struct ddsi_virtual_interface_pipe_list_elem {
  ddsi_virtual_interface_pipe_t * pipe; /*the current element in the list*/
  ddsi_virtual_interface_pipe_list_elem_t * prev; /*the previous element in the list*/
  ddsi_virtual_interface_pipe_list_elem_t * next; /*the next element in the list*/
};

/* adds a pipe to the list, will create the first list entry if it does not yet exist
*/
DDS_EXPORT bool add_pipe_to_list (
  ddsi_virtual_interface_pipe_t * toadd,
  ddsi_virtual_interface_pipe_list_elem_t ** addto);

/* removes a pipe from the list, will set the pointer to the list to null if the last entry is removed
*/
DDS_EXPORT bool remove_pipe_from_list (
  ddsi_virtual_interface_pipe_t * to_remove,
  ddsi_virtual_interface_pipe_list_elem_t ** remove_from);

/*identifier used to distinguish between raw data types (C/C++/Python/...)*/
typedef uint32_t virtual_interface_data_type_t;

/*identifier used to uniquely identify a topic across different processes*/
typedef uint32_t virtual_interface_topic_identifier_t;

/*identifier used to distinguish between local and remote virtual interfaces*/
typedef uint32_t virtual_interface_identifier_t;

/*identifier used to communicate the properties of the data being communicated*/
typedef uint64_t virtual_interface_data_type_properties_t;

#define DATA_TYPE_FINAL_MODIFIER        0x1u << 0
#define DATA_TYPE_APPENDABLE_MODIFIER   0x1u << 1
#define DATA_TYPE_MUTABLE_MODIFIER      0x1u << 2
#define DATA_TYPE_CONTAINS_UNION        0x1u << 0
#define DATA_TYPE_CONTAINS_BITMASK      0x1u << 3
#define DATA_TYPE_CONTAINS_ENUM         0x1u << 6
#define DATA_TYPE_CONTAINS_STRUCT       0x1u << 9
#define DATA_TYPE_CONTAINS_STRING       0x1u << 12
#define DATA_TYPE_CONTAINS_BSTRING      DATA_TYPE_CONTAINS_STRING << 1
#define DATA_TYPE_CONTAINS_WSTRING      DATA_TYPE_CONTAINS_BSTRING << 1
#define DATA_TYPE_CONTAINS_SEQUENCE     DATA_TYPE_CONTAINS_WSTRING << 1
#define DATA_TYPE_CONTAINS_BSEQUENCE    DATA_TYPE_CONTAINS_SEQUENCE << 1
#define DATA_TYPE_CONTAINS_ARRAY        DATA_TYPE_CONTAINS_BSEQUENCE << 1
#define DATA_TYPE_CONTAINS_OPTIONAL     DATA_TYPE_CONTAINS_ARRAY << 1
#define DATA_TYPE_CONTAINS_EXTERNAL     DATA_TYPE_CONTAINS_OPTIONAL << 1
#define DATA_TYPE_CONTAINS_INDIRECTIONS 0x1u << 62
#define DATA_TYPE_IS_FIXED_SIZE         0x1u << 63

/*the type of a pipe*/
typedef enum virtual_interface_pipe_type {
  VIRTUAL_INTERFACE_PIPE_TYPE_UNSET,
  VIRTUAL_INTERFACE_PIPE_TYPE_SOURCE,
  VIRTUAL_INTERFACE_PIPE_TYPE_SINK
} virtual_interface_pipe_type_t;

/*describes the data which is transferred in addition to just the sample*/
typedef struct dds_virtual_interface_metadata {
  struct ddsi_guid guid;
  dds_time_t timestamp;
  uint32_t statusinfo;
  uint32_t hash;
  uint32_t encoding_version;
  ddsi_keyhash_t keyhash;
  loaned_sample_state_t sample_state;
  uint32_t sample_size;
} dds_virtual_interface_metadata_t;

/*the main class resulting from exchanges in the virtual interface*/
typedef struct ddsi_virtual_interface_exchange_unit {
  dds_virtual_interface_metadata_t *metadata;
  dds_loaned_sample_t *loan;
} ddsi_virtual_interface_exchange_unit_t;

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

/* returns true when a data type is supported 
*/
typedef bool (*ddsi_virtual_interface_data_type_supported) (
  virtual_interface_data_type_properties_t data_type_props
);

/* returns true when a qos is supported
*/
typedef bool (*ddsi_virtual_interface_qos_supported) (
  const struct dds_qos * qos
);

/* creates a virtual interface topic
*/
typedef ddsi_virtual_interface_topic_t* (*ddsi_virtual_interface_topic_create) (
  ddsi_virtual_interface_t * vi,
  virtual_interface_topic_identifier_t topic_identifier,
  virtual_interface_data_type_t data_type
);


/* destructs a virtual interface topic
*/
typedef bool (*ddsi_virtual_interface_topic_destruct) (
  ddsi_virtual_interface_topic_t * vi_topic
);

/* checks whether serialization is required on this 
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_serialization_required) (
  virtual_interface_data_type_properties_t data_type  /*the data type to check whether serialization is required*/
);

/* opens a pipe on a virtual interface
* returns true on success
*/
typedef ddsi_virtual_interface_pipe_t* (*ddsi_virtual_interface_pipe_open) (
  ddsi_virtual_interface_topic_t * topic,  /*the topic to create the pipe on*/
  virtual_interface_pipe_type_t pipe_type /*type type of pipe to open*/
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
typedef dds_loaned_sample_t* (*ddsi_virtual_interface_pipe_request_loan) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to loan from*/
  size_t size_requested /*the size of the loan requested*/
);

/* increases the refcount of the block in the virtual interface
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_ref_block) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe who will acquire a reference of the block*/
  dds_loaned_sample_t * block /*the loaned block to return*/
);

/* decreses the refcount of the block in the virtual interface
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_unref_block) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to return the loan to*/
  dds_loaned_sample_t * block /*the loaned block to return*/
);

/* sinks data on a pipe
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_sink_data) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to sink the data on*/
  ddsi_virtual_interface_exchange_unit_t * data  /*the data to sink*/
);

/* sources data on a pipe
* used in a poll based implementation
* returns the oldest unsourced received block of memory
*/
typedef ddsi_virtual_interface_exchange_unit_t (*ddsi_virtual_interface_pipe_source_data) (
  ddsi_virtual_interface_pipe_t * pipe /*the pipe to source the data from*/
);

/* checks whether a sample is loaned from a pipe
* returns the memory block of the sample if it originates from the pipe
*/
typedef dds_loaned_sample_t* (*ddsi_virtual_interface_sample_to_loan) (
  const ddsi_virtual_interface_t *vi, /*the virtual interface to check*/
  const void * sample /*the sample to check*/
);

/* callback function setter
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_pipe_enable_on_source_data) (
  ddsi_virtual_interface_pipe_t * pipe, /*the pipe to set the callback function on*/
  dds_entity_t reader /*the reader associated with the pipe*/
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
  ddsi_virtual_interface_data_type_supported      data_type_supported;
  ddsi_virtual_interface_qos_supported            qos_supported;
  ddsi_virtual_interface_topic_create             topic_create;
  ddsi_virtual_interface_topic_destruct           topic_destruct;
  ddsi_virtual_interface_deinit                   deinit;
  ddsi_virtual_interface_sample_to_loan           sample_to_loan;
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
  ddsi_virtual_interface_pipe_ref_block           ref_block;
  ddsi_virtual_interface_pipe_unref_block         unref_block;
  ddsi_virtual_interface_pipe_sink_data           sink_data;
  ddsi_virtual_interface_pipe_source_data         source_data;
  /*if the set_on_source is not set, then there is no event based functionality, you will need to poll for new data*/
  ddsi_virtual_interface_pipe_enable_on_source_data set_on_source;
} ddsi_virtual_interface_pipe_ops_t;

/* the top-level entry point on the virtual interface
* is bound to a specific implementation of a virtual interface
*/
struct ddsi_virtual_interface {
  ddsi_virtual_interface_ops_t ops; /*associated functions*/
  const char * interface_name; /*type of interface being used*/
  int32_t default_priority; /*priority of choosing this interface*/
  virtual_interface_identifier_t interface_id; /*the unique id of this interface*/
  ddsi_virtual_interface_topic_list_elem_t * topics; /*associated topics*/
};

/**
 * cleanup function for the C-level administration, should be called from all
 * destructors of classes which inherit from ddsi_virtual_interface_t
 */
bool ddsi_virtual_interface_cleanup_generic(ddsi_virtual_interface_t *to_cleanup);

/* the topic-level virtual interface
* this will exchange data for readers and writers which are matched through discovery
* will only exchange a single type of data!
*/
struct ddsi_virtual_interface_topic {
  ddsi_virtual_interface_topic_ops_t ops; /*associated functions*/
  ddsi_virtual_interface_t * virtual_interface; /*the virtual interface which created this pipe*/
  virtual_interface_topic_identifier_t topic_id; /*unique identifier of topic representation*/
  ddsi_virtual_interface_pipe_list_elem_t * pipes; /*associated pipes*/
  virtual_interface_data_type_t data_type; /*the data type of the raw samples read/written*/
  bool supports_loan; /*whether the topic supports loan semantics*/
};

/**
 * cleanup function for the C-level administration, should be called from all
 * destructors of classes which inherit from ddsi_virtual_interface_topic_t
 */
bool ddsi_virtual_interface_topic_cleanup_generic(ddsi_virtual_interface_topic_t *to_cleanup);

/* the definition of one instance of a dds
* reader/writer using a virtual interface
*/
struct ddsi_virtual_interface_pipe {
  ddsi_virtual_interface_pipe_ops_t ops; /*associated functions*/
  ddsi_virtual_interface_topic_t * topic; /*the topic this pipe belongs to*/
  virtual_interface_pipe_type_t pipe_type; /*type type of pipe*/
};

/**
 * cleanup function for the C-level administration, should be called from all
 * destructors of classes which inherit from ddsi_virtual_interface_pipe_t
 */
bool ddsi_virtual_interface_pipe_cleanup_generic(ddsi_virtual_interface_pipe_t *to_cleanup);

/* this is the only function exported from the virtual interface library
* returns true on success
*/
typedef bool (*ddsi_virtual_interface_create_fn) (
  ddsi_virtual_interface_t **virtual_interface, /*output for the virtual interface to be created*/
  virtual_interface_identifier_t identifier, /*the domain associated with this interface*/
  const char * configuration_string /*optional configuration data*/
);
#endif // DDS_VIRTUAL_INTERFACE_H
