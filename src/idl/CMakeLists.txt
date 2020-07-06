#
# Copyright(c) 2020 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.7)
project(cyclonedds-idl LANGUAGES C)

include(GenerateExportHeader)

find_package(BISON REQUIRED)

bison_target(
  parser src/parser.y "${CMAKE_CURRENT_BINARY_DIR}/parser.c"
  DEFINES_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/parser.h")

add_library(
  idl SHARED
    src/scanner.c
    src/directive.c
    src/processor.c
    src/typetree.c
    ${BISON_parser_OUTPUT_SOURCE})

generate_export_header(idl EXPORT_FILE_NAME include/idl/export.h)
configure_file(include/idl/version.h.in include/idl/version.h @ONLY)

target_include_directories(
  idl
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/idl"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/export.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/version.h"
    "${BISON_parser_OUTPUT_HEADER}"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/idl"
  COMPONENT dev)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
