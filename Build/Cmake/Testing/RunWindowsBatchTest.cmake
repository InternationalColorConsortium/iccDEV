# Run one legacy Windows batch test through CTest and validate its output.

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TESTING_DIR
  ICCDEV_TEST_OUTDIR
  ICCDEV_BATCH_SCRIPT
  ICCDEV_BUILD_DIR
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

if(NOT EXISTS "${ICCDEV_BATCH_SCRIPT}")
  message(FATAL_ERROR "Batch script not found: ${ICCDEV_BATCH_SCRIPT}")
endif()

file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_log_file "${ICCDEV_TEST_OUTDIR}/output.log")
set(_source_testing_dir "${ICCDEV_TESTING_DIR}")
get_filename_component(_source_repo_root "${_source_testing_dir}/.." ABSOLUTE)

find_program(GIT_EXECUTABLE git)
set(_source_status_available FALSE)
set(_source_status_before "")
if(GIT_EXECUTABLE AND EXISTS "${_source_repo_root}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --short -- Testing
    WORKING_DIRECTORY "${_source_repo_root}"
    RESULT_VARIABLE _source_status_result
    OUTPUT_VARIABLE _source_status_before
    ERROR_QUIET
  )
  if(_source_status_result EQUAL 0)
    set(_source_status_available TRUE)
  endif()
endif()

set(_candidate_configs)
if(DEFINED ICCDEV_CONFIG AND NOT "${ICCDEV_CONFIG}" STREQUAL "")
  list(APPEND _candidate_configs "${ICCDEV_CONFIG}")
endif()
if(DEFINED ENV{CTEST_CONFIGURATION_TYPE} AND NOT "$ENV{CTEST_CONFIGURATION_TYPE}" STREQUAL "")
  list(APPEND _candidate_configs "$ENV{CTEST_CONFIGURATION_TYPE}")
endif()
list(APPEND _candidate_configs Release RelWithDebInfo Debug MinSizeRel)
list(REMOVE_DUPLICATES _candidate_configs)

set(_resolved_config "")
foreach(_candidate_config IN LISTS _candidate_configs)
  if(EXISTS "${ICCDEV_BUILD_DIR}/Tools/IccFromXml/${_candidate_config}/iccFromXml.exe")
    set(_resolved_config "${_candidate_config}")
    break()
  endif()
endforeach()

if(_resolved_config STREQUAL "")
  message(FATAL_ERROR
    "Could not find iccFromXml.exe under ${ICCDEV_BUILD_DIR}/Tools/IccFromXml. "
    "Build the tools first, or run ctest with -C <config> for multi-config generators.")
endif()

set(_tool_dirs
  IccApplyNamedCmm
  IccApplyProfiles
  IccApplySearch
  IccApplyToLink
  IccDumpProfile
  IccFromCube
  IccFromJson
  IccFromXml
  IccJpegDump
  IccPngDump
  IccRoundTrip
  IccSpecSepToTiff
  IccTiffDump
  IccToJson
  IccToXml
  IccV5DspObsToV4Dsp
)

set(_path_entries)
foreach(_tool_dir IN LISTS _tool_dirs)
  list(APPEND _path_entries "${ICCDEV_BUILD_DIR}/Tools/${_tool_dir}/${_resolved_config}")
endforeach()
list(APPEND _path_entries
  "${ICCDEV_BUILD_DIR}/IccProfLib/${_resolved_config}"
  "${ICCDEV_BUILD_DIR}/IccXML/${_resolved_config}"
  "${ICCDEV_BUILD_DIR}/IccJSON/${_resolved_config}"
)
list(JOIN _path_entries ";" _path_prefix)
set(_run_path "${_path_prefix};$ENV{PATH}")

get_filename_component(_script_name "${ICCDEV_BATCH_SCRIPT}" NAME)
if(_script_name STREQUAL "CreateAllProfiles.bat")
  set(_required_tools iccFromXml.exe)
elseif(_script_name STREQUAL "RunTests.bat")
  set(_required_tools iccApplyNamedCmm.exe iccToJson.exe iccFromJson.exe)
else()
  set(_required_tools)
endif()

foreach(_required_tool IN LISTS _required_tools)
  set(_found_required_tool FALSE)
  foreach(_path_entry IN LISTS _path_entries)
    if(EXISTS "${_path_entry}/${_required_tool}")
      set(_found_required_tool TRUE)
      break()
    endif()
  endforeach()
  if(NOT _found_required_tool)
    message(FATAL_ERROR
      "Required tool ${_required_tool} was not found in ${ICCDEV_BUILD_DIR} for ${_resolved_config}")
  endif()
endforeach()

if(NOT DEFINED ICCDEV_WINDOWS_WORK_DIR OR "${ICCDEV_WINDOWS_WORK_DIR}" STREQUAL "")
  set(ICCDEV_WINDOWS_WORK_DIR "${ICCDEV_TEST_OUTDIR}/Testing")
endif()

if(_script_name STREQUAL "CreateAllProfiles.bat")
  file(REMOVE_RECURSE "${ICCDEV_WINDOWS_WORK_DIR}")
endif()

if(NOT EXISTS "${ICCDEV_WINDOWS_WORK_DIR}")
  file(MAKE_DIRECTORY "${ICCDEV_WINDOWS_WORK_DIR}")
  file(COPY "${_source_testing_dir}/" DESTINATION "${ICCDEV_WINDOWS_WORK_DIR}")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "PATH=${_run_path}"
    "ICCDEV_TOOLS_DIR=${ICCDEV_BUILD_DIR}/Tools"
    "ICCDEV_TESTING_DIR=${ICCDEV_WINDOWS_WORK_DIR}"
    "ICCDEV_BUILD_DIR=${ICCDEV_BUILD_DIR}"
    cmd.exe /c call "${ICCDEV_BATCH_SCRIPT}"
  WORKING_DIRECTORY "${ICCDEV_WINDOWS_WORK_DIR}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr
)

set(_combined_output
  "CTest test: ${ICCDEV_TEST_NAME}\n"
  "Configuration: ${_resolved_config}\n"
  "Script: ${ICCDEV_BATCH_SCRIPT}\n"
  "Working directory: ${ICCDEV_WINDOWS_WORK_DIR}\n"
  "Result: ${_result}\n\n"
  "----- stdout -----\n${_stdout}\n"
  "----- stderr -----\n${_stderr}\n"
)
file(WRITE "${_log_file}" "${_combined_output}")
message(STATUS "Wrote ${ICCDEV_TEST_NAME} log to ${_log_file}")

if(_source_status_available)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --short -- Testing
    WORKING_DIRECTORY "${_source_repo_root}"
    RESULT_VARIABLE _source_status_after_result
    OUTPUT_VARIABLE _source_status_after
    ERROR_QUIET
  )
  if(_source_status_after_result EQUAL 0
      AND NOT "${_source_status_after}" STREQUAL "${_source_status_before}")
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} changed the source Testing tree; "
      "run output is in ${_log_file}")
  endif()
endif()

if(NOT _result EQUAL 0)
  message(FATAL_ERROR "${ICCDEV_TEST_NAME} exited with ${_result}; see ${_log_file}")
endif()

set(_forbidden_regex
  "not recognized as an internal or external command|The system cannot find the path specified|The system cannot find the file specified|No such file or directory")
if(DEFINED ICCDEV_FORBIDDEN_REGEX AND NOT "${ICCDEV_FORBIDDEN_REGEX}" STREQUAL "")
  set(_forbidden_regex "${_forbidden_regex}|${ICCDEV_FORBIDDEN_REGEX}")
endif()

string(REGEX MATCH "${_forbidden_regex}" _forbidden_match "${_combined_output}")
if(NOT "${_forbidden_match}" STREQUAL "")
  message(FATAL_ERROR
    "${ICCDEV_TEST_NAME} output matched forbidden text '${_forbidden_match}'; see ${_log_file}")
endif()

if(DEFINED ICCDEV_EXPECTED_OUTPUT AND NOT "${ICCDEV_EXPECTED_OUTPUT}" STREQUAL "")
  string(FIND "${_combined_output}" "${ICCDEV_EXPECTED_OUTPUT}" _expected_pos)
  if(_expected_pos EQUAL -1)
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} did not print expected text '${ICCDEV_EXPECTED_OUTPUT}'; see ${_log_file}")
  endif()
endif()

if(DEFINED ICCDEV_EXPECTED_PROFILE_PARSE_COUNT AND NOT "${ICCDEV_EXPECTED_PROFILE_PARSE_COUNT}" STREQUAL "")
  string(REGEX MATCHALL "Profile parsed and saved correctly" _profile_parse_matches "${_combined_output}")
  list(LENGTH _profile_parse_matches _profile_parse_count)
  if(NOT _profile_parse_count EQUAL ICCDEV_EXPECTED_PROFILE_PARSE_COUNT)
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} parsed ${_profile_parse_count} profiles, "
      "expected ${ICCDEV_EXPECTED_PROFILE_PARSE_COUNT}; see ${_log_file}")
  endif()
  message(STATUS "${ICCDEV_TEST_NAME} parsed ${_profile_parse_count} profiles")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
