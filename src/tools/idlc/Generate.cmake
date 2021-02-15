#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
function(IDLC_GENERATE)
  cmake_parse_arguments(IDLC "" "TARGET" "FILES" "" ${ARGN})

  if(NOT IDLC_TARGET)
    message(FATAL_ERROR "idlc_generate was called without TARGET")
  endif()
  if(NOT IDLC_FILES)
    message(FATAL_ERROR "idlc_generate was called without FILES")
  endif()

  set(_dir ${CMAKE_CURRENT_BINARY_DIR})
  set(_target ${IDLC_TARGET})
  foreach(_file ${IDLC_FILES})
    get_filename_component(_path ${_file} ABSOLUTE)
    get_filename_component(_name ${_file} NAME_WE)
    set(_source "${_dir}/${_name}.c")
    set(_header "${_dir}/${_name}.h")
    list(APPEND _sources "${_source}")
    list(APPEND _headers "${_header}")
    add_custom_command(
      OUTPUT   "${_source}" "${_header}"
      COMMAND  $<TARGET_FILE:CycloneDDS::idlc>
      ARGS     ${_path})
  endforeach()

  add_custom_target("${_target}_generate" DEPENDS "${_sources}" "${_headers}")
  add_library(${_target} INTERFACE)
  target_sources(${_target} INTERFACE ${_sources} ${_headers})
  target_include_directories(${_target} INTERFACE "${_dir}")
  add_dependencies(${_target} "${_target}_generate")
endfunction()
