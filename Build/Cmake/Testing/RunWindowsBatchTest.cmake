# Run one legacy Windows batch test through CTest and validate its output.

if(POLICY CMP0007)
  cmake_policy(SET CMP0007 NEW)
endif()

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

include("${CMAKE_CURRENT_LIST_DIR}/WindowsRuntimePaths.cmake")

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

function(_iccdev_append_existing_path out_var)
  foreach(_path IN LISTS ARGN)
    if(NOT "${_path}" STREQUAL "" AND EXISTS "${_path}")
      list(APPEND ${out_var} "${_path}")
    endif()
  endforeach()
  set(${out_var} "${${out_var}}" PARENT_SCOPE)
endfunction()

set(_resolved_config "")
set(_resolved_tool_suffix "")
set(_resolved_runtime_dir "")
set(_candidate_tool_suffixes)
foreach(_candidate_config IN LISTS _candidate_configs)
  if(NOT "${_candidate_config}" STREQUAL "")
    list(APPEND _candidate_tool_suffixes "/${_candidate_config}")
  endif()
endforeach()
list(APPEND _candidate_tool_suffixes "")
list(REMOVE_DUPLICATES _candidate_tool_suffixes)

foreach(_candidate_tool_suffix IN LISTS _candidate_tool_suffixes)
  if(EXISTS "${ICCDEV_BUILD_DIR}/bin${_candidate_tool_suffix}/iccFromXml.exe")
    set(_resolved_runtime_dir "${ICCDEV_BUILD_DIR}/bin${_candidate_tool_suffix}")
    set(_resolved_tool_suffix "${_candidate_tool_suffix}")
    if(_candidate_tool_suffix STREQUAL "")
      set(_resolved_config "single-config")
    else()
      string(REGEX REPLACE "^/" "" _resolved_config "${_candidate_tool_suffix}")
    endif()
    break()
  elseif(EXISTS "${ICCDEV_BUILD_DIR}/Tools/IccFromXml${_candidate_tool_suffix}/iccFromXml.exe")
    set(_resolved_tool_suffix "${_candidate_tool_suffix}")
    if(_candidate_tool_suffix STREQUAL "")
      set(_resolved_config "single-config")
    else()
      string(REGEX REPLACE "^/" "" _resolved_config "${_candidate_tool_suffix}")
    endif()
    break()
  endif()
endforeach()

if(_resolved_config STREQUAL "")
  message(FATAL_ERROR
    "Could not find iccFromXml.exe under ${ICCDEV_BUILD_DIR}/bin, "
    "${ICCDEV_BUILD_DIR}/Tools/IccFromXml, or their config subdirectories. "
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
  IccProfileVisualize
  IccRoundTrip
  IccSpecSepToTiff
  IccTiffDump
  IccToJson
  IccToXml
  IccV5DspObsToV4Dsp
)

set(_path_entries)
if(NOT "${_resolved_runtime_dir}" STREQUAL "")
  _iccdev_append_existing_path(_path_entries "${_resolved_runtime_dir}")
  set(_tools_dir_env "${_resolved_runtime_dir}")
else()
  foreach(_tool_dir IN LISTS _tool_dirs)
    _iccdev_append_existing_path(_path_entries
      "${ICCDEV_BUILD_DIR}/Tools/${_tool_dir}${_resolved_tool_suffix}")
  endforeach()
  _iccdev_append_existing_path(_path_entries
    "${ICCDEV_BUILD_DIR}/IccProfLib${_resolved_tool_suffix}"
    "${ICCDEV_BUILD_DIR}/IccXML${_resolved_tool_suffix}"
    "${ICCDEV_BUILD_DIR}/IccJSON${_resolved_tool_suffix}"
    "${ICCDEV_BUILD_DIR}/IccConnect${_resolved_tool_suffix}"
  )
  set(_tools_dir_env "${ICCDEV_BUILD_DIR}/Tools")
endif()

iccdev_collect_cache_runtime_path_entries(_runtime_path_entries "${ICCDEV_BUILD_DIR}")
_iccdev_append_existing_path(_path_entries ${_runtime_path_entries})

_iccdev_append_existing_path(_path_entries
  "${_source_repo_root}/installed/x64-windows/debug/bin"
  "${_source_repo_root}/installed/x64-windows/bin"
  "$ENV{SystemRoot}/System32"
  "$ENV{SystemRoot}"
  "C:/Windows/System32"
  "C:/Windows")

if(DEFINED ENV{ICCDEV_WINDOWS_EXTRA_PATH} AND NOT "$ENV{ICCDEV_WINDOWS_EXTRA_PATH}" STREQUAL "")
  list(APPEND _path_entries "$ENV{ICCDEV_WINDOWS_EXTRA_PATH}")
endif()

list(REMOVE_DUPLICATES _path_entries)
list(JOIN _path_entries ";" _path_prefix)
set(_run_path "${_path_prefix}")
string(REPLACE ";" "\\;" _run_path_env "${_run_path}")

get_filename_component(_script_name "${ICCDEV_BATCH_SCRIPT}" NAME)
if(_script_name STREQUAL "CreateAllProfiles.bat")
  set(_required_tools iccFromXml.exe)
elseif(_script_name STREQUAL "RunTests.bat")
  set(_required_tools iccApplyNamedCmm.exe iccToJson.exe iccFromJson.exe iccProfileVisualize.exe)
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

  # The work directory has to reflect the repository, not whatever the source
  # Testing/ tree has accumulated locally.
  #
  # Profiles generated by CreateAllProfiles.bat and friends are written into
  # Testing/ and are gitignored (`Testing/**/*.icc` and similar), so a clean
  # checkout carries none of them.  In a working copy that has run generation
  # even once they persist, and the copy above pulls them in.  Two things then
  # go wrong: the batch script regenerates over files that already exist rather
  # than creating them, which makes the generated-profile count below undercount
  # by however many collided; and, more seriously, wherever a stale artifact is
  # NOT regenerated the tests run against a profile built by an older version of
  # the tools.  Neither is visible in CI, which always starts clean.
  #
  # So prune ignored files from the copy.  This cannot be a blanket "*.icc"
  # exclusion on the copy itself: many .icc files under Testing/ are tracked
  # fixtures that the tests legitimately need.  Only git can distinguish the two,
  # so ask it -- and when git is unavailable (a release tarball or export) leave
  # the copy alone, since such a tree has no ignored files in it to begin with.
  if(GIT_EXECUTABLE AND EXISTS "${_source_repo_root}/.git")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -c core.quotepath=off
              ls-files --others --ignored --exclude-standard -- Testing
      WORKING_DIRECTORY "${_source_repo_root}"
      RESULT_VARIABLE _ignored_result
      OUTPUT_VARIABLE _ignored_output
      ERROR_QUIET
    )

    set(_pruned_count 0)
    if(_ignored_result EQUAL 0 AND NOT "${_ignored_output}" STREQUAL "")
      string(REGEX REPLACE "\r?\n" ";" _ignored_paths "${_ignored_output}")
      foreach(_ignored_path IN LISTS _ignored_paths)
        if(NOT "${_ignored_path}" STREQUAL "")
          # Paths come back relative to the repository root (Testing/...); the
          # work directory mirrors the contents of Testing/ itself.
          string(REGEX REPLACE "^Testing/" "" _work_rel "${_ignored_path}")
          if(NOT "${_work_rel}" STREQUAL "${_ignored_path}"
              AND EXISTS "${ICCDEV_WINDOWS_WORK_DIR}/${_work_rel}")
            file(REMOVE "${ICCDEV_WINDOWS_WORK_DIR}/${_work_rel}")
            math(EXPR _pruned_count "${_pruned_count} + 1")
          endif()
        endif()
      endforeach()
    endif()

    if(_pruned_count GREATER 0)
      message(STATUS
        "${ICCDEV_TEST_NAME} pruned ${_pruned_count} gitignored file(s) copied "
        "from ${_source_testing_dir}")
    endif()
  endif()
endif()

# Baseline for the generated-profile count below.  Snapshot the work directory
# as the batch script will find it, rather than differencing against the source
# Testing/ tree: the source tree's contents depend on whether generation has been
# run there before, while this measures exactly what the batch produces.
file(GLOB_RECURSE _profiles_before_batch
  LIST_DIRECTORIES FALSE
  "${ICCDEV_WINDOWS_WORK_DIR}/*.icc")
list(LENGTH _profiles_before_batch _profile_count_before_batch)

set(_run_batch_script "${ICCDEV_BATCH_SCRIPT}")
set(_copied_batch_script "${ICCDEV_WINDOWS_WORK_DIR}/${_script_name}")
if(EXISTS "${_copied_batch_script}")
  set(_run_batch_script "${_copied_batch_script}")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "PATH=${_run_path_env}"
    "Path=${_run_path_env}"
    "ICCDEV_TOOLS_DIR=${_tools_dir_env}"
    "ICCDEV_TESTING_DIR=${ICCDEV_WINDOWS_WORK_DIR}"
    "ICCDEV_BUILD_DIR=${ICCDEV_BUILD_DIR}"
    cmd.exe /c cd /d "${ICCDEV_WINDOWS_WORK_DIR}" && call "${_run_batch_script}"
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
  string(REGEX MATCHALL "[^\r\n]*\\[FAIL\\][^\r\n]*"
    _failure_lines "${_combined_output}")
  if(_failure_lines)
    list(JOIN _failure_lines "\n  " _failure_summary)
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} exited with ${_result}; failing commands:\n"
      "  ${_failure_summary}\n"
      "Captured output:\n${_combined_output}\n"
      "Full output: ${_log_file}")
  endif()
  message(FATAL_ERROR "${ICCDEV_TEST_NAME} exited with ${_result}; see ${_log_file}")
endif()

if(ICCDEV_VERIFY_GENERATED_PROFILE_MANIFEST)
  set(_manifest "${ICCDEV_WINDOWS_WORK_DIR}/qa-profile-manifest.tsv")
  if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} requires qa-profile-manifest.tsv; see ${_log_file}")
  endif()

  file(STRINGS "${_manifest}" _manifest_lines)
  set(_manifest_generated_count 0)
  set(_missing_generated_profiles)
  foreach(_manifest_line IN LISTS _manifest_lines)
    if(_manifest_line MATCHES "^#" OR _manifest_line STREQUAL "")
      continue()
    endif()

    string(REPLACE "\t" ";" _manifest_fields "${_manifest_line}")
    list(LENGTH _manifest_fields _manifest_field_count)
    if(_manifest_field_count LESS 6)
      message(FATAL_ERROR
        "${ICCDEV_TEST_NAME} found a malformed manifest row; see ${_log_file}")
    endif()

    list(GET _manifest_fields 0 _manifest_profile)
    list(GET _manifest_fields 5 _manifest_source)
    if(NOT _manifest_source STREQUAL "generated")
      continue()
    endif()

    math(EXPR _manifest_generated_count "${_manifest_generated_count} + 1")
    if(NOT EXISTS "${ICCDEV_WINDOWS_WORK_DIR}/${_manifest_profile}")
      list(APPEND _missing_generated_profiles "${_manifest_profile}")
    endif()
  endforeach()

  if(_manifest_generated_count EQUAL 0)
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} manifest declares no generated profiles; see ${_log_file}")
  endif()
  if(_missing_generated_profiles)
    list(JOIN _missing_generated_profiles "\n  " _missing_generated_summary)
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} is missing manifest-declared generated profile(s):\n"
      "  ${_missing_generated_summary}\n"
      "See ${_log_file}")
  endif()
  message(STATUS
    "${ICCDEV_TEST_NAME} satisfied generated profile manifest "
    "(${_manifest_generated_count} profiles)")
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

if(DEFINED ICCDEV_EXPECTED_GENERATED_PROFILE_COUNT
    AND NOT "${ICCDEV_EXPECTED_GENERATED_PROFILE_COUNT}" STREQUAL "")
  file(GLOB_RECURSE _generated_profiles
    LIST_DIRECTORIES FALSE
    "${ICCDEV_WINDOWS_WORK_DIR}/*.icc")
  list(LENGTH _generated_profiles _generated_profile_count)
  math(EXPR _generated_profile_delta
    "${_generated_profile_count} - ${_profile_count_before_batch}")
  if(NOT _generated_profile_delta EQUAL ICCDEV_EXPECTED_GENERATED_PROFILE_COUNT)
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME} generated ${_generated_profile_delta} profiles, "
      "expected ${ICCDEV_EXPECTED_GENERATED_PROFILE_COUNT}; see ${_log_file}")
  endif()
  message(STATUS "${ICCDEV_TEST_NAME} generated ${_generated_profile_delta} profiles")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
