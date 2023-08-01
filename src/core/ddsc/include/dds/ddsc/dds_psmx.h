// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/**
 * @defgroup psmx (Publish Subscribe Message Exchange)
 * @ingroup dds
 */
#ifndef DDS_PSMX_H
#define DDS_PSMX_H

#include "dds/export.h"
#include "dds/dds.h"
#include "dds/ddsc/dds_loan.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_MAX_PSMX_INSTANCES 1

struct dds_psmx;
struct dds_psmx_topic;
struct dds_psmx_topic_list_elem;
struct dds_psmx_endpoint;
struct dds_psmx_endpoint_list_elem;

/**
 * @brief Type of the PSMX endpoint
 */
typedef enum dds_psmx_endpoint_type {
  DDS_PSMX_ENDPOINT_TYPE_UNSET,
  DDS_PSMX_ENDPOINT_TYPE_READER,
  DDS_PSMX_ENDPOINT_TYPE_WRITER
} dds_psmx_endpoint_type_t;

/**
 * @brief identifier used to communicate the properties of the data being communicated
 */
typedef uint64_t dds_psmx_data_type_properties_t;

/**
 * @brief identifier used to distinguish between PSMX instances on nodes
 */
typedef struct dds_psmx_node_identifier
{
  uint8_t x[16];
} dds_psmx_node_identifier_t;

/**
 * @brief Definition for function that checks data type support
 *
 * Definition for function that checks whether a type with the provided
 * data type properties is supported by the PSMX implementation.
 *
 * @param[in] data_type_props  The properties of the data type.
 * @returns true if the type is supported, false otherwise
 */
typedef bool (*dds_psmx_data_type_supported_fn) (dds_psmx_data_type_properties_t data_type_props);

/**
 * @brief Definition for function that checks QoS support
 *
 * Definition for function that checks whether the provided QoS
 * is supported by the PSMX implementation.
 *
 * @param[in] qos  The QoS.
 * @returns true if the QoS is supported, false otherwise
 */
typedef bool (*dds_psmx_qos_supported_fn) (const struct dds_qos *qos);

/**
 * @brief Definition for function to create a topic
 *
 * Definition for a function that is called to create a new topic
 * for a PSMX instance.
 *
 * @param[in] psmx_instance  The PSMX instance.
 * @param[in] topic_name  The name of the topic to create
 * @param[in] data_type_props  The data type properties for the topic's data type.
 * @returns a PSMX topic structure
 */
typedef struct dds_psmx_topic * (* dds_psmx_create_topic_fn) (
    struct dds_psmx * psmx_instance,
    const char * topic_name,
    dds_psmx_data_type_properties_t data_type_props);

/**
 * @brief Definition for function to destruct a topic
 *
 * Definition for a function that is called on topic destruction.
 *
 * @param[in] psmx_topic  The PSMX topic to destruct
 * @returns a DDS return code
 *
 */
typedef dds_return_t (*dds_psmx_delete_topic_fn) (struct dds_psmx_topic *psmx_topic);

/**
 * @brief Function definition for pubsub message exchange cleanup
 *
 * @param[in] psmx_instance  the psmx instance to de-initialize
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_deinit_fn) (struct dds_psmx *psmx_instance);

/**
 * @brief Definition for PSMX locator generation function
 *
 * Returns a locator which is unique between nodes, but identical for instances on
 * the same node
 *
 * @param[in] psmx_instance  a PSMX instance
 * @returns a unique node identifier (locator)
 */
typedef dds_psmx_node_identifier_t (* dds_psmx_get_node_identifier_fn) (const struct dds_psmx *psmx_instance);

/**
 * @brief functions which are used on a PSMX instance
 */
typedef struct dds_psmx_ops {
  dds_psmx_data_type_supported_fn  data_type_supported;
  dds_psmx_qos_supported_fn        qos_supported;
  dds_psmx_create_topic_fn         create_topic;
  dds_psmx_delete_topic_fn         delete_topic;
  dds_psmx_deinit_fn               deinit;
  dds_psmx_get_node_identifier_fn  get_node_id;
} dds_psmx_ops_t;

/**
 * @brief Definition for function to check if serialization is required
 *
 * Definition of a function that checks whether serialization is
 * required for a data type with the provided properties.
 *
 * @param[in] data_type_props  The properties of the data type
 * @returns true if serialization is required, else otherwise
 *
 * FIXME: I wonder if we shouldn't do this check in Cyclone's core? But it is true that, e.g., OpenSplice [cw]ould also say "no need", so it isn't simply complicating things
 */
typedef bool (* dds_psmx_serialization_required_fn) (dds_psmx_data_type_properties_t data_type_props);

/**
 * @brief Definition of function to create an endpoint for a topic
 *
 * @param[in] psmx_topic  The PSMX topic to create the endpoint for
 * @param[in] endpoint_type  The type of endpoint to create (publisher or subscriber)
 * @returns A PSMX endpoint struct
 */
typedef struct dds_psmx_endpoint * (* dds_psmx_create_endpoint_fn) (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);

/**
 * @brief Definition of function to delete an PSMX endpoint
 *
 * @param[in] psmx_endpoint  The endpoint to be deleted
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_delete_endpoint_fn) (struct dds_psmx_endpoint *psmx_endpoint);

/**
 * @brief functions which are used on a PSMX topic
 */
typedef struct dds_psmx_topic_ops {
  dds_psmx_serialization_required_fn serialization_required;
  dds_psmx_create_endpoint_fn        create_endpoint;
  dds_psmx_delete_endpoint_fn        delete_endpoint;
} dds_psmx_topic_ops_t;


/**
 * @brief Definition for function to requests a loan from the PSMX
 *
 * @param[in] psmx_endpoint        the endpoint to loan from
 * @param[in] size_requested  the size of the loan requested
 * @returns a pointer to the loaned block on success
 */
typedef dds_loaned_sample_t * (* dds_psmx_endpoint_request_loan_fn) (struct dds_psmx_endpoint *psmx_endpoint, uint32_t size_requested);

/**
 * @brief Definition of function to write data on a PSMX endpoint
 *
 * @param[in] psmx_endpoint    The endpoint to publish the data on
 * @param[in] data    The data to publish
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_endpoint_write_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_loaned_sample_t *data);

/**
 * @brief Definition of function to take data from an PSMX endpoint
 *
 * Used in a poll based implementation.
 *
 * @param[in] psmx_endpoint The endpoint to take the data from
 * @returns the oldest unread received block of memory
 */
typedef dds_loaned_sample_t * (* dds_psmx_endpoint_take_fn) (struct dds_psmx_endpoint *psmx_endpoint);

/**
 * @brief Definition of function to set the a callback function on an PSMX endpoint
 *
 * @param[in] psmx_endpoint the endpoint to set the callback function on
 * @param[in] reader        the DDS reader associated with the endpoint
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_endpoint_on_data_available_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_entity_t reader);

/**
 * @brief Functions that are used on a PSMX endpoint
 */
typedef struct dds_psmx_endpoint_ops {
  dds_psmx_endpoint_request_loan_fn       request_loan;
  dds_psmx_endpoint_write_fn              write;
  dds_psmx_endpoint_take_fn               take;
  dds_psmx_endpoint_on_data_available_fn  on_data_available;
} dds_psmx_endpoint_ops_t;

/**
 * @brief the top-level entry point on the PSMX is bound to a specific implementation of a PSMX
 */
typedef struct dds_psmx {
  dds_psmx_ops_t ops; //!< associated functions
  const char *instance_name; //!< name of this PSMX instance
  int32_t priority; //!< priority of choosing this interface
  const struct ddsi_locator *locator; //!< the locator for this PSMX instance
  dds_loan_origin_type_t instance_type; //!< the type identifier of this PSMX instance
  struct dds_psmx_topic_list_elem *psmx_topics; //!< associated topics
} dds_psmx_t;

/**
 * @brief the topic-level PSMX
 *
 * this will exchange data for readers and writers which are matched through discovery
 * will only exchange a single type of data
 */
typedef struct dds_psmx_topic {
  dds_psmx_topic_ops_t ops; //!< associated functions
  struct dds_psmx *psmx_instance; //!< the PSMX instance which created this topic
  char * topic_name; //!< the topic name
  dds_loan_data_type_t data_type; //!< the unique identifier associated with the data type of this topic
  struct dds_psmx_endpoint_list_elem *psmx_endpoints; //!< associated endpoints
  dds_psmx_data_type_properties_t data_type_props; //!< the properties of the datatype associated with this topic
} dds_psmx_topic_t;

/**
 * @brief the definition of one instance of a dds reader/writer using a PSMX instance
 */
typedef struct dds_psmx_endpoint {
  dds_psmx_endpoint_ops_t ops; //!< associated functions
  struct dds_psmx_topic * psmx_topic; //!< the topic this endpoint belongs to
  dds_psmx_endpoint_type_t endpoint_type; //!< type type of endpoint
} dds_psmx_endpoint_t;


/**
 * @brief adds a topic to the list
 *
 * will create the first list entry if it does not yet exist
 *
 * @param[in] psmx_topic     the topic to add
 * @param[in,out] list  list to add the topic to
 * @return DDS_RETCODE_OK on success
 */
DDS_EXPORT dds_return_t dds_add_psmx_topic_to_list (struct dds_psmx_topic *psmx_topic, struct dds_psmx_topic_list_elem **list);

/**
 * @brief removes a topic from the list
 *
 * will set the pointer to the list to null if the last entry is removed
 *
 * @param[in] psmx_topic     the topic to remove
 * @param[in,out] list  list to remove the topic from
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_remove_psmx_topic_from_list (struct dds_psmx_topic *psmx_topic, struct dds_psmx_topic_list_elem **list);

/**
 * @brief adds an endpoint to the list
 *
 * will create the first list entry if it does not yet exist
 *
 * @param[in] psmx_endpoint   the endpoint to add
 * @param[in,out] list   list to add the endpoint to
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_add_psmx_endpoint_to_list (struct dds_psmx_endpoint *psmx_endpoint, struct dds_psmx_endpoint_list_elem **list);

/**
 * @brief removes an endpoint from the list
 *
 * will set the pointer to the list to null if the last entry is removed
 *
 * @param[in] psmx_endpoint  the endpoint to remove
 * @param[in,out] list  list to remove the endpoint from
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_remove_psmx_endpoint_from_list (struct dds_psmx_endpoint *psmx_endpoint, struct dds_psmx_endpoint_list_elem **list);

/**
 * @brief initialization function for PSMX instance
 *
 * Should be called from all constructors of class which inherit from dds_psmx_t
 *
 * @param[in] psmx  the PSMX instance to initialize
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_init_generic (struct dds_psmx *psmx);

/**
 * @brief cleanup function for a PSMX instance
 *
 * Should be called from all destructors of classes which inherit from dds_psmx_t
 *
 * @param[in] psmx  the PSMX instance to cleanup
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_cleanup_generic (struct dds_psmx *psmx);

/**
 * @brief init function for topic
 *
 * Should be called from all constructors of classes which inherit from struct dds_psmx_topic
 *
 * @param[in] topic  the topic to initialize
 * @param[in] psmx  the PSMX instance
 * @param[in] topic_name  the topic name
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_topic_init_generic (struct dds_psmx_topic *topic, const struct dds_psmx *psmx, const char *topic_name);

/**
 * @brief cleanup function for a topic
 *
 * Should be called from all destructors of classes which inherit from struct dds_psmx_topic
 *
 * @param[in] psmx_topic   the topic to de-initialize
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_topic_cleanup_generic(struct dds_psmx_topic *psmx_topic);


#if defined (__cplusplus)
}
#endif

#endif /* DDS_PSMX_H */
