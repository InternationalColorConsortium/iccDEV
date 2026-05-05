#################################################################################
# Windows iccDumpProfile smoke test
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_BUILD_DIR
  ICCDEV_ICCDUMP_EXE
  ICCDEV_PROFILE
  ICCDEV_EXPECTED_PROFILE_SIZE
  ICCDEV_EXPECTED_TAG_COUNT
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

if(NOT EXISTS "${ICCDEV_ICCDUMP_EXE}")
  message(FATAL_ERROR "iccDumpProfile not found: ${ICCDEV_ICCDUMP_EXE}")
endif()

if(NOT EXISTS "${ICCDEV_PROFILE}")
  message(FATAL_ERROR "Smoke-test ICC profile not found: ${ICCDEV_PROFILE}")
endif()

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_log_file "${ICCDEV_TEST_OUTDIR}/output.log")

get_filename_component(_tool_dir "${ICCDEV_ICCDUMP_EXE}" DIRECTORY)
set(_tool_path
  "${_tool_dir}"
  "${ICCDEV_BUILD_DIR}/IccProfLib"
  "${ICCDEV_BUILD_DIR}/IccXML"
  "${ICCDEV_BUILD_DIR}/IccJSON"
)
list(JOIN _tool_path ";" _path_prefix)

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
      "PATH=${_path_prefix};$ENV{PATH}"
      "${ICCDEV_ICCDUMP_EXE}"
      --read
      --diag
      1
      "${ICCDEV_PROFILE}"
  RESULT_VARIABLE _dump_result
  OUTPUT_VARIABLE _dump_stdout
  ERROR_VARIABLE _dump_stderr
)

file(WRITE "${_log_file}"
  "CTest test: ${ICCDEV_TEST_NAME}\n"
  "Tool: ${ICCDEV_ICCDUMP_EXE}\n"
  "Profile: ${ICCDEV_PROFILE}\n\n"
  "----- iccDumpProfile stdout -----\n${_dump_stdout}\n"
  "----- iccDumpProfile stderr -----\n${_dump_stderr}\n"
)
message(STATUS "Wrote ${ICCDEV_TEST_NAME} log to ${_log_file}")

if(NOT _dump_result EQUAL 0)
  message(FATAL_ERROR "iccDumpProfile exited with ${_dump_result}; see ${_log_file}")
endif()

if(_dump_stdout MATCHES "Unable to parse")
  message(FATAL_ERROR "iccDumpProfile could not parse ${ICCDEV_PROFILE}; see ${_log_file}")
endif()

if(NOT _dump_stdout MATCHES "Size:[ \t]+${ICCDEV_EXPECTED_PROFILE_SIZE}")
  message(FATAL_ERROR "iccDumpProfile output is missing expected profile size ${ICCDEV_EXPECTED_PROFILE_SIZE}; see ${_log_file}")
endif()

if(NOT _dump_stdout MATCHES "Profile Tags \\(${ICCDEV_EXPECTED_TAG_COUNT}\\)")
  message(FATAL_ERROR "iccDumpProfile output is missing expected tag count ${ICCDEV_EXPECTED_TAG_COUNT}; see ${_log_file}")
endif()

if(NOT _dump_stderr MATCHES "Header size matches file stat: ${ICCDEV_EXPECTED_PROFILE_SIZE} bytes")
  message(FATAL_ERROR "iccDumpProfile diagnostic is missing expected header/file-size match; see ${_log_file}")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
