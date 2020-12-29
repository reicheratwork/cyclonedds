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

find_package(BISON 3.0.4 REQUIRED)
find_package(Threads REQUIRED)

set(binary_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}")

bison_target(parser src/parser.y "${binary_dir}/parser.c")

add_library(
  idl SHARED
    src/symbol.c
    src/directive.c
    src/expression.c
    src/file.c
    src/processor.c
    src/scanner.c
    src/string.c
    src/annotation.c
    src/scope.c
    src/hashid.c
    src/string.c
    src/tree.c
    ${BISON_parser_OUTPUT_SOURCE})

generate_export_header(idl EXPORT_FILE_NAME include/idl/export.h)

configure_file(include/idl/version.h.in include/idl/version.h @ONLY)

target_include_directories(
  idl
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  INTERFACE
    "$<INSTALL_INTERFACE:include>")

target_link_libraries(idl PRIVATE Threads::Threads)

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/idl"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/export.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/version.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/idl"
  COMPONENT dev)

install(
  TARGETS idl
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
