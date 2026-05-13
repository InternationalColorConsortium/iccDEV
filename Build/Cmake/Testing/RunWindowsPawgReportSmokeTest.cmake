#################################################################################
# Windows iccPawgReport smoke test
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_BUILD_DIR
  ICCDEV_PAWG_EXE
  ICCDEV_PROFILE
  ICCDEV_EXPECTED_ITEM_COUNT
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

if(NOT EXISTS "${ICCDEV_PAWG_EXE}")
  message(FATAL_ERROR "iccPawgReport not found: ${ICCDEV_PAWG_EXE}")
endif()

if(NOT EXISTS "${ICCDEV_PROFILE}")
  message(FATAL_ERROR "Smoke-test ICC profile not found: ${ICCDEV_PROFILE}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/WindowsRuntimePaths.cmake")

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_log_file "${ICCDEV_TEST_OUTDIR}/output.log")

get_filename_component(_tool_dir "${ICCDEV_PAWG_EXE}" DIRECTORY)
get_filename_component(_tool_config "${_tool_dir}" NAME)
set(_tool_path
  "${_tool_dir}"
  "${ICCDEV_BUILD_DIR}/IccProfLib"
  "${ICCDEV_BUILD_DIR}/IccProfLib/${_tool_config}"
  "${ICCDEV_BUILD_DIR}/IccXML"
  "${ICCDEV_BUILD_DIR}/IccXML/${_tool_config}"
  "${ICCDEV_BUILD_DIR}/IccJSON"
  "${ICCDEV_BUILD_DIR}/IccJSON/${_tool_config}"
  "${ICCDEV_BUILD_DIR}/IccConnect"
  "${ICCDEV_BUILD_DIR}/IccConnect/${_tool_config}"
)
iccdev_collect_cache_runtime_path_entries(_runtime_path_entries "${ICCDEV_BUILD_DIR}")
list(APPEND _tool_path ${_runtime_path_entries})
list(REMOVE_DUPLICATES _tool_path)
list(JOIN _tool_path ";" _path_prefix)

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
      "PATH=${_path_prefix};$ENV{PATH}"
      "${ICCDEV_PAWG_EXE}"
      "${ICCDEV_PROFILE}"
  RESULT_VARIABLE _pawg_result
  OUTPUT_VARIABLE _pawg_stdout
  ERROR_VARIABLE _pawg_stderr
)

file(WRITE "${_log_file}"
  "CTest test: ${ICCDEV_TEST_NAME}\n"
  "Tool: ${ICCDEV_PAWG_EXE}\n"
  "Profile: ${ICCDEV_PROFILE}\n\n"
  "----- iccPawgReport stdout -----\n${_pawg_stdout}\n"
  "----- iccPawgReport stderr -----\n${_pawg_stderr}\n"
)
message(STATUS "Wrote ${ICCDEV_TEST_NAME} log to ${_log_file}")

if(NOT _pawg_result EQUAL 0)
  message(FATAL_ERROR "iccPawgReport exited with ${_pawg_result}; see ${_log_file}")
endif()

if(NOT _pawg_stdout MATCHES "ICC PROFILE ASSESSMENT REPORT \\(PAWG\\)")
  message(FATAL_ERROR "iccPawgReport output is missing the PAWG report header; see ${_log_file}")
endif()

if(NOT _pawg_stdout MATCHES "Total checklist items:[ \t]+${ICCDEV_EXPECTED_ITEM_COUNT}")
  message(FATAL_ERROR
    "iccPawgReport output is missing expected item count ${ICCDEV_EXPECTED_ITEM_COUNT}; see ${_log_file}")
endif()

string(REGEX MATCHALL "(^|\n)[ \t]+\\[(OK|WARN|FAIL|N/A|GAP|NOT RUN)[ \t]*\\]" _item_matches "${_pawg_stdout}")
list(LENGTH _item_matches _item_count)
if(NOT _item_count EQUAL ICCDEV_EXPECTED_ITEM_COUNT)
  message(FATAL_ERROR
    "iccPawgReport rendered ${_item_count} checklist items, "
    "expected ${ICCDEV_EXPECTED_ITEM_COUNT}; see ${_log_file}")
endif()

if(NOT _pawg_stdout MATCHES "FAIL:[ \t]+0")
  message(FATAL_ERROR "iccPawgReport summary reported one or more failures; see ${_log_file}")
endif()

if(NOT _pawg_stdout MATCHES "NOT RUN:[ \t]+0")
  message(FATAL_ERROR "iccPawgReport summary reported one or more NOT RUN items; see ${_log_file}")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
