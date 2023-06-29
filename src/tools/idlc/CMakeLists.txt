#
# Copyright(c) 2018 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
configure_file(src/config.h.in config.h)

set(headers
  include/idl_defs.h
  src/plugin.h
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set(sources
  src/idlc.c
  src/plugin.c
  src/options.c)

add_executable(idlc ${sources} ${headers})

if(MSVC)
  # ignore warnings C6255 and 6263 about _alloca
  target_compile_options(idlc PRIVATE /wd6255 /wd6263)
endif()
target_link_libraries(idlc PRIVATE compat idl idlpp idl_meta ddsc ${CMAKE_DL_LIBS})

if(ENABLE_TYPE_DISCOVERY)
target_link_libraries(idlc PRIVATE idl_meta)
endif()

target_include_directories(
  idlc PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>)

add_executable(${PROJECT_NAME}::idlc ALIAS idlc)

set(
  libidlc_hdrs
  include/libidlc/generator.h
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(
  libidlc_srcs
  src/types.h
  src/descriptor.h
  src/generator.h
  src/descriptor.c
  src/generator.c
  src/types.c)

if(ENABLE_TYPE_DISCOVERY)

set(idl_meta_hdrs include/idl_meta/descriptor_type_meta.h)
set(libidl_meta_srcs src/descriptor_type_meta.c)

add_library(
  idl_meta SHARED ${libidl_meta_srcs} ${libidl_meta_hdrs})

set_target_properties(idl_meta PROPERTIES
   OUTPUT_NAME "idl_meta"
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
   C_STANDARD 99
   C_VISIBILITY_PRESET hidden)

target_link_libraries(idl_meta PRIVATE libidlc)

add_library(${PROJECT_NAME}::idl_meta ALIAS idl_meta)

install(
  TARGETS idl_meta
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

endif()

add_library(
  libidlc SHARED ${libidlc_srcs} ${libidlc_hdrs})

set_target_properties(libidlc PROPERTIES
   OUTPUT_NAME "cycloneddsidlc"
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
   C_STANDARD 99
   C_VISIBILITY_PRESET hidden)

target_include_directories(
  libidlc PUBLIC include)

target_link_libraries(libidlc PUBLIC idl ddsc)

add_library(${PROJECT_NAME}::libidlc ALIAS libidlc)

install(
  TARGETS libidlc
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  TARGETS idlc
  EXPORT "${CMAKE_PROJECT_NAME}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT dev)

if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:idlc>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()

install(
  FILES "${CycloneDDS_SOURCE_DIR}/cmake/Modules/Generate.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/idlc"
  COMPONENT dev)

include("${CycloneDDS_SOURCE_DIR}/cmake/Modules/Generate.cmake")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BUILD_IDLC_XTESTS)
  add_subdirectory(xtests)
endif()
