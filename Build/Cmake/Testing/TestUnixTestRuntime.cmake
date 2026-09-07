# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

if(NOT IS_ABSOLUTE "${ICCDEV_TEST_OUTDIR}")
  message(FATAL_ERROR "ICCDEV_TEST_OUTDIR must be absolute")
endif()
set(_build "${ICCDEV_TEST_OUTDIR}/build with spaces")
file(MAKE_DIRECTORY "${_build}/Tools/Probe/Release" "${_build}/Tools/Probe/Debug")
file(WRITE "${_build}/CMakeCache.txt" "runtime-layout-test\n")
file(WRITE "${_build}/Tools/Probe/Release/probe" "Release\n")
file(WRITE "${_build}/Tools/Probe/Debug/probe" "Debug\n")

function(stage CONFIG SOURCE DESTINATION EXPECT_SUCCESS)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DICCDEV_BINARY_DIR=${_build}" "-DICCDEV_CONFIG=${CONFIG}"
      "-DICCDEV_RUNTIME_SOURCES=${SOURCE}"
      "-DICCDEV_RUNTIME_DESTINATIONS=${DESTINATION}"
      -P "${CMAKE_CURRENT_LIST_DIR}/StageUnixTestRuntime.cmake"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
  if(EXPECT_SUCCESS AND NOT _result STREQUAL "0")
    message(FATAL_ERROR "Runtime staging failed: ${_stdout}${_stderr}")
  elseif(NOT EXPECT_SUCCESS AND _result STREQUAL "0")
    message(FATAL_ERROR "Invalid runtime staging unexpectedly succeeded")
  endif()
endfunction()

foreach(_config IN ITEMS Release Debug)
  stage("${_config}"
    "${_build}/Tools/Probe/${_config}/probe;${_build}/CMakeCache.txt"
    "Tools/Probe/probe;CMakeCache.txt" TRUE)
endforeach()
foreach(_config IN ITEMS Release Debug)
  set(_view "${_build}/Testing/ctest-runtime/${_config}")
  if(NOT EXISTS "${_view}/Tools/Probe/probe" OR
      NOT EXISTS "${_view}/CMakeCache.txt" OR
      IS_SYMLINK "${_view}/Tools/Probe/probe" OR
      IS_SYMLINK "${_view}/CMakeCache.txt")
    message(FATAL_ERROR "Runtime view must expose regular artifact/cache files")
  endif()
  execute_process(COMMAND find "${_view}/Tools" -maxdepth 2 -name probe -type f
    RESULT_VARIABLE _find_result OUTPUT_VARIABLE _found OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _find_result STREQUAL "0" OR NOT _found STREQUAL "${_view}/Tools/Probe/probe")
    message(FATAL_ERROR "Shell find -type f cannot discover the staged tool")
  endif()
  file(READ "${_view}/Tools/Probe/probe" _content)
  if(NOT _content STREQUAL "${_config}\n")
    message(FATAL_ERROR "Runtime view selected the wrong configuration")
  endif()
endforeach()

file(CREATE_LINK "${_build}/Tools/Probe/Release/probe" "${_build}/probe-alias" SYMBOLIC)
stage("Alias" "${_build}/probe-alias" "Tools/Probe/probe" TRUE)
if(IS_SYMLINK "${_build}/Testing/ctest-runtime/Alias/Tools/Probe/probe")
  message(FATAL_ERROR "A symlinked source was not resolved to a regular runtime file")
endif()

stage("Release" "${_build}/not-built;${_build}/CMakeCache.txt"
  "Tools/Probe/probe;CMakeCache.txt" TRUE)
if(EXISTS "${_build}/Testing/ctest-runtime/Release/Tools/Probe/probe" OR
    IS_SYMLINK "${_build}/Testing/ctest-runtime/Release/Tools/Probe/probe")
  message(FATAL_ERROR "Missing artifact retained a stale or dangling alias")
endif()
file(READ "${_build}/Testing/ctest-runtime/Debug/Tools/Probe/probe" _debug)
if(NOT _debug STREQUAL "Debug\n")
  message(FATAL_ERROR "Refreshing Release modified the Debug view")
endif()
file(READ "${_build}/Tools/Probe/Release/probe" _release)
if(NOT _release STREQUAL "Release\n")
  message(FATAL_ERROR "Refreshing the view modified an actual build artifact")
endif()

stage("../outside" "${_build}/CMakeCache.txt" "CMakeCache.txt" FALSE)
stage("Release" "${_build}/CMakeCache.txt" "../outside" FALSE)
stage("Release" "${_build}/CMakeCache.txt" "one;two" FALSE)
file(MAKE_DIRECTORY "${_build}/Testing/ctest-runtime/Unmanaged")
stage("Unmanaged" "${_build}/CMakeCache.txt" "CMakeCache.txt" FALSE)
file(CREATE_LINK "${_build}/Tools" "${_build}/Testing/ctest-runtime/Linked" SYMBOLIC)
stage("Linked" "${_build}/CMakeCache.txt" "CMakeCache.txt" FALSE)
message(STATUS "Unix runtime layout: configuration isolation and failure controls passed")
