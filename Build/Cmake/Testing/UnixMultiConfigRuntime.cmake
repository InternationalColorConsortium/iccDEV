# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

# CMakeLists.txt uses ICCDEV_SCRIPT_BUILD_DIR (via ICCDEV_BUILD_DIR and
# ICCDEV_SHARED_LIBRARY_PATH) for every platform, so the default must be set
# before any early return below.
set(ICCDEV_SCRIPT_BUILD_DIR "${CMAKE_BINARY_DIR}")

# This runtime view and its layout regression rely on POSIX semantics (hard
# links, `find -type f`) that Windows multi-config generators (Visual Studio)
# do not provide. Bail out early so Windows CTest runs neither register an
# unsatisfiable iccdev_unix_runtime fixture nor the Unix-only layout test,
# while still leaving ICCDEV_SCRIPT_BUILD_DIR defined above.
if(WIN32)
  return()
endif()

function(iccdev_collect_runtime_targets DIRECTORY_PATH OUTPUT_VAR)
  get_property(_targets DIRECTORY "${DIRECTORY_PATH}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(_children DIRECTORY "${DIRECTORY_PATH}" PROPERTY SUBDIRECTORIES)
  foreach(_child IN LISTS _children)
    iccdev_collect_runtime_targets("${_child}" _child_targets)
    list(APPEND _targets ${_child_targets})
  endforeach()
  set("${OUTPUT_VAR}" "${_targets}" PARENT_SCOPE)
endfunction()

function(iccdev_append_runtime_entry SOURCE_PATH DESTINATION_PATH)
  string(APPEND _iccdev_runtime_manifest
    "list(APPEND ICCDEV_RUNTIME_SOURCES [==[${SOURCE_PATH}]==])\n"
    "list(APPEND ICCDEV_RUNTIME_DESTINATIONS [==[${DESTINATION_PATH}]==])\n")
  set(_iccdev_runtime_manifest "${_iccdev_runtime_manifest}" PARENT_SCOPE)
endfunction()

if(ICCDEV_IS_MULTI_CONFIG)
  foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
    if(NOT _config MATCHES "^[A-Za-z0-9_+-][A-Za-z0-9_.+-]*$")
      message(FATAL_ERROR "Unsupported CTest runtime configuration: ${_config}")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ctest-output/${_config}")
  endforeach()
  set(ICCDEV_SCRIPT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ctest-runtime/$<CONFIG>")
  set(ICCDEV_TOOLS_DIR "${ICCDEV_SCRIPT_BUILD_DIR}/Tools")
  set(ICCDEV_TEST_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/ctest-output/$<CONFIG>")

  set(_iccdev_runtime_manifest
    "set(ICCDEV_BINARY_DIR [==[${CMAKE_BINARY_DIR}]==])\nset(ICCDEV_CONFIG [==[$<CONFIG>]==])\n")
  iccdev_append_runtime_entry("${CMAKE_BINARY_DIR}/CMakeCache.txt" "CMakeCache.txt")
  foreach(_header IN ITEMS
      IccProfLib/IccProfLibVer.h IccXML/IccLibXMLVer.h
      IccJSON/IccLibJSONVer.h IccConnect/IccLibConnectVer.h)
    if(EXISTS "${CMAKE_BINARY_DIR}/${_header}")
      iccdev_append_runtime_entry("${CMAKE_BINARY_DIR}/${_header}" "${_header}")
    endif()
  endforeach()

  iccdev_collect_runtime_targets("${CMAKE_SOURCE_DIR}" _runtime_targets)
  foreach(_target IN LISTS _runtime_targets)
    get_target_property(_type "${_target}" TYPE)
    get_target_property(_binary_dir "${_target}" BINARY_DIR)
    file(RELATIVE_PATH _relative_dir "${CMAKE_BINARY_DIR}" "${_binary_dir}")
    if(NOT _relative_dir MATCHES "^(Tools/|IccProfLib$|IccXML$|IccJSON$|IccConnect$)")
      continue()
    endif()
    if(_type STREQUAL "EXECUTABLE" OR
        _type STREQUAL "STATIC_LIBRARY" OR _type STREQUAL "SHARED_LIBRARY")
      iccdev_append_runtime_entry("$<TARGET_FILE:${_target}>"
        "${_relative_dir}/$<TARGET_FILE_NAME:${_target}>")
    endif()
    if(_type STREQUAL "SHARED_LIBRARY")
      iccdev_append_runtime_entry("$<TARGET_LINKER_FILE:${_target}>"
        "${_relative_dir}/$<TARGET_LINKER_FILE_NAME:${_target}>")
      iccdev_append_runtime_entry("$<TARGET_SONAME_FILE:${_target}>"
        "${_relative_dir}/$<TARGET_SONAME_FILE_NAME:${_target}>")
    endif()
  endforeach()
  string(APPEND _iccdev_runtime_manifest
    "include([==[${CMAKE_CURRENT_LIST_DIR}/StageUnixTestRuntime.cmake]==])\n")
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ctest-runtime-$<CONFIG>.cmake"
    CONTENT "${_iccdev_runtime_manifest}")
  add_test(NAME iccdev.unix-multi-config-runtime
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/ctest-runtime-$<CONFIG>.cmake")
  set_tests_properties(iccdev.unix-multi-config-runtime PROPERTIES
    FIXTURES_SETUP iccdev_unix_runtime
    LABELS "iccdev;build;fixture"
    TIMEOUT 30)
endif()

add_test(NAME iccdev.unix-runtime-layout
  COMMAND "${CMAKE_COMMAND}"
    "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/unix-runtime-layout"
    -P "${CMAKE_CURRENT_LIST_DIR}/TestUnixTestRuntime.cmake")
set_tests_properties(iccdev.unix-runtime-layout PROPERTIES
  LABELS "iccdev;build;regression"
  TIMEOUT 30)
