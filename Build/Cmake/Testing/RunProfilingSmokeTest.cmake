#################################################################################
# gprof profiling smoke test
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

cmake_minimum_required(VERSION 3.18...3.29)

foreach(_required
    ICCDEV_DUMP_PROFILE
    ICCDEV_PROFILE
    ICCDEV_GPROF
    ICCDEV_PROFILE_OUTPUT_DIR)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "[profiling-smoke] ${_required} not set")
  endif()
endforeach()

foreach(_required_path ICCDEV_DUMP_PROFILE ICCDEV_PROFILE ICCDEV_GPROF)
  if(NOT EXISTS "${${_required_path}}")
    message(FATAL_ERROR
      "[profiling-smoke] ${_required_path} not found: ${${_required_path}}")
  endif()
endforeach()

file(REMOVE_RECURSE "${ICCDEV_PROFILE_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${ICCDEV_PROFILE_OUTPUT_DIR}")

execute_process(
  COMMAND "${ICCDEV_DUMP_PROFILE}" "${ICCDEV_PROFILE}"
  WORKING_DIRECTORY "${ICCDEV_PROFILE_OUTPUT_DIR}"
  RESULT_VARIABLE _dump_result
  OUTPUT_VARIABLE _dump_stdout
  ERROR_VARIABLE _dump_stderr
)
if(NOT _dump_result EQUAL 0)
  message(FATAL_ERROR
    "[profiling-smoke] iccDumpProfile failed (${_dump_result}):\n"
    "${_dump_stdout}\n${_dump_stderr}")
endif()

set(_gmon "${ICCDEV_PROFILE_OUTPUT_DIR}/gmon.out")
if(NOT EXISTS "${_gmon}")
  message(FATAL_ERROR "[profiling-smoke] gmon.out was not produced")
endif()
file(SIZE "${_gmon}" _gmon_size)
if(_gmon_size EQUAL 0)
  message(FATAL_ERROR "[profiling-smoke] gmon.out is empty")
endif()

execute_process(
  COMMAND "${ICCDEV_GPROF}" "${ICCDEV_DUMP_PROFILE}" "${_gmon}"
  WORKING_DIRECTORY "${ICCDEV_PROFILE_OUTPUT_DIR}"
  RESULT_VARIABLE _gprof_result
  OUTPUT_VARIABLE _gprof_stdout
  ERROR_VARIABLE _gprof_stderr
)
if(NOT _gprof_result EQUAL 0)
  message(FATAL_ERROR
    "[profiling-smoke] gprof failed (${_gprof_result}):\n${_gprof_stderr}")
endif()
if(NOT _gprof_stdout MATCHES "Flat profile:")
  message(FATAL_ERROR "[profiling-smoke] gprof output has no flat profile")
endif()

file(WRITE "${ICCDEV_PROFILE_OUTPUT_DIR}/gprof.txt" "${_gprof_stdout}")
message(STATUS
  "[profiling-smoke] PASS: ${_gmon_size}-byte gmon.out and gprof.txt produced")
