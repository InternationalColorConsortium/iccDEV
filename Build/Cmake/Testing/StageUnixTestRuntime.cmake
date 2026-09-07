# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

if(NOT IS_ABSOLUTE "${ICCDEV_BINARY_DIR}" OR
    NOT EXISTS "${ICCDEV_BINARY_DIR}/CMakeCache.txt")
  message(FATAL_ERROR "ICCDEV_BINARY_DIR must identify the configured build tree")
endif()
if(NOT ICCDEV_CONFIG MATCHES "^[A-Za-z0-9_+-][A-Za-z0-9_.+-]*$")
  message(FATAL_ERROR "ICCDEV_CONFIG must identify one configuration")
endif()
list(LENGTH ICCDEV_RUNTIME_SOURCES _source_count)
list(LENGTH ICCDEV_RUNTIME_DESTINATIONS _destination_count)
if(NOT _source_count EQUAL _destination_count)
  message(FATAL_ERROR "Runtime source/destination manifest lengths differ")
endif()
foreach(_destination IN LISTS ICCDEV_RUNTIME_DESTINATIONS)
  if(IS_ABSOLUTE "${_destination}" OR
      _destination MATCHES "(^|/)\\.\\.(/|$)" OR _destination STREQUAL "")
    message(FATAL_ERROR "Invalid runtime destination: ${_destination}")
  endif()
endforeach()

# Only this dedicated, marked view is replaced; actual build outputs are never moved.
set(_parent "${ICCDEV_BINARY_DIR}/Testing/ctest-runtime")
set(_root "${_parent}/${ICCDEV_CONFIG}")
if(IS_SYMLINK "${ICCDEV_BINARY_DIR}/Testing" OR
    IS_SYMLINK "${_parent}" OR IS_SYMLINK "${_root}")
  message(FATAL_ERROR "Refusing a symlinked CTest runtime directory")
endif()
file(MAKE_DIRECTORY "${_parent}")
file(LOCK "${_parent}/${ICCDEV_CONFIG}.lock" GUARD PROCESS TIMEOUT 30)
if(EXISTS "${_root}")
  if(NOT EXISTS "${_root}/.iccdev-runtime")
    message(FATAL_ERROR "Refusing to replace an unmarked CTest runtime directory: ${_root}")
  endif()
  file(READ "${_root}/.iccdev-runtime" _marker)
  if(NOT _marker STREQUAL "iccdev CTest runtime\n")
    message(FATAL_ERROR "Invalid CTest runtime ownership marker")
  endif()
  file(REMOVE_RECURSE "${_root}")
endif()
file(MAKE_DIRECTORY "${_root}/Tools")
file(WRITE "${_root}/.iccdev-runtime" "iccdev CTest runtime\n")

while(_source_count GREATER 0)
  math(EXPR _source_count "${_source_count} - 1")
  list(GET ICCDEV_RUNTIME_SOURCES ${_source_count} _source)
  list(GET ICCDEV_RUNTIME_DESTINATIONS ${_source_count} _destination)
  # A focused build need not build every configured target. Do not expose dangling
  # aliases that shell library discovery could mistake for a linkable archive.
  if(EXISTS "${_source}")
    get_filename_component(_directory "${_root}/${_destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${_directory}")
    file(CREATE_LINK "${_source}" "${_root}/${_destination}" SYMBOLIC RESULT _result)
    if(NOT _result STREQUAL "0")
      message(FATAL_ERROR "Cannot stage ${_destination}: ${_result}")
    endif()
  endif()
endwhile()
