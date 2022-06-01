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

#ifndef DDS__VIRTUAL_INTERFACE_H
#define DDS__VIRTUAL_INTERFACE_H

#include "dds/ddsc/dds_virtual_interface.h"

struct ddsi_domaingv;
struct ddsi_sertype;
struct dds_ktopic;

/*function used to calculate the raw data type*/
virtual_interface_data_type_t calculate_data_type(const struct ddsi_sertype * s_type);

/*function used to calculate the topic identifier*/
virtual_interface_topic_identifier_t calculate_topic_identifier(const struct dds_ktopic * ktopic);

/*function used to calculate the interface identifier*/
virtual_interface_identifier_t calculate_interface_identifier(const struct ddsi_domaingv * cyclone_domain);

#endif // DDS__VIRTUAL_INTERFACE_H
