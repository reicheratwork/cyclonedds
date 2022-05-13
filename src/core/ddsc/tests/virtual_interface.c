/*
 * Copyright(c) 2022 ZettaScale Technology, Incorporated
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>

#include "dds/dds.h"
#include "config_env.h"

#include "dds/version.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"

#include "test_common.h"

#include "VIDataModels.h"

#define FORCE_ENV

static void config__check_env (const char *env_variable, const char *expected_value)
{
  const char *env_uri = NULL;
  ddsrt_getenv (env_variable, &env_uri);
#ifdef FORCE_ENV
  {
    bool env_ok;

    if (env_uri == NULL)
      env_ok = false;
    else if (strncmp (env_uri, expected_value, strlen (expected_value)) != 0)
      env_ok = false;
    else
      env_ok = true;

    if (!env_ok)
    {
      dds_return_t r = ddsrt_setenv (env_variable, expected_value);
      CU_ASSERT_EQUAL_FATAL (r, DDS_RETCODE_OK);
    }
  }
#else
  CU_ASSERT_PTR_NOT_NULL_FATAL (env_uri);
  CU_ASSERT_STRING_EQUAL_FATAL (env_uri, expected_value);
#endif /* FORCE_ENV */
}

CU_Test (ddsc_virtual_interface, create, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t participant, topic, writer;
  config__check_env ("CYCLONEDDS_URI", CONFIG_ENV_VIRTUAL_INTERFACE);

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);

  dds_domainid_t did;
  dds_get_domainid(participant, &did);

  fprintf(stderr, "did: %u\n", did);

  CU_ASSERT_FATAL (participant > 0);

  /* Create a Topic. */
  topic = dds_create_topic (
    participant, &SC_Model_desc, "SC_Model", NULL, NULL);

  CU_ASSERT_FATAL (topic > 0);

  /* Create a Writer. */
  writer = dds_create_writer (participant, topic, NULL, NULL);

  CU_ASSERT_FATAL (writer > 0);

  dds_delete (participant);
}
