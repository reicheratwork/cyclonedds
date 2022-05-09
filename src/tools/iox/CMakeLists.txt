#
# Copyright(c) 2022 ZettaScale Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(iox_interface SHARED
    "${source_dir}/src/iox_interface.c"
)

set_target_properties(iox_interface PROPERTIES VERSION ${PROJECT_VERSION})

set_target_properties(iox_interface PROPERTIES PUBLIC_HEADER "${source_dir}/inc/iox_interface.h")

target_link_libraries(iox_interface CycloneDDS::ddsc)

install(TARGETS iox_interface
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
