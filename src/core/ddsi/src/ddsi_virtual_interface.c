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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_virtual_interface.h"
#include "dds/ddsi/ddsi_virtual_interface_loader.h"


bool ddsi_virtual_interface_load(struct ddsi_domaingv *gv, struct ddsi_config_virtual_interface *config, ddsi_virtual_interface_wrapper **out_wrapper) {
    ddsi_virtual_interface_wrapper *wrapper = ddsrt_calloc(1, sizeof(*wrapper));
    ddsi_virtual_interface_create_fn *creator = NULL;
    const char *toload;
    char load_fn[100];
    bool ok = true;

    if (!wrapper) {
        GVERROR("Out of memory!\n");
        return false;
    }

    if (!config->library || config->library[0] == '\0') {
        toload = config->name;
    } else {
        toload = config->library;
    }

    if (ddsrt_dlopen(toload, true, &wrapper->handle) != DDS_RETCODE_OK) {
        GVERROR("Failed to load virtual interface library '%s'.\n", toload);
        return false;
    }

    snprintf(load_fn, 100, "%s_create_virtual_interface", config->name);

    if (ddsrt_dlsym(wrapper->handle, load_fn, (void**)&creator) != DDS_RETCODE_OK) {
        GVERROR("Failed to initialize virtual interface '%s', could not load init function '%s'.\n", config->name, load_fn);
        ok = false;
        goto err;
    }

    if (!(*creator)(&wrapper->interface, wrapper->config->config)) {
        GVERROR("Failed to initialize virtual interface '%s'.\n", config->name);
        ok = false;
        goto err;
    }

err:
    if (!ok) {
        if (wrapper->handle)
            ddsrt_dlclose(wrapper->handle);
        ddsrt_free(wrapper);
    } else {
        *out_wrapper = wrapper;
    }

    return ok;
}
