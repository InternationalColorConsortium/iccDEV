if(NOT APPLE)
  message(FATAL_ERROR "VerifyMacOSGuardMalloc.cmake is only supported on macOS")
endif()

if(NOT DEFINED ICCDEV_BINARY_DIR OR NOT IS_DIRECTORY "${ICCDEV_BINARY_DIR}")
  message(FATAL_ERROR "ICCDEV_BINARY_DIR must point at the iccDEV build directory")
endif()

find_program(OTOOL_EXECUTABLE otool)
if(NOT OTOOL_EXECUTABLE)
  message(FATAL_ERROR "otool is required to verify Mach-O LC_UUID load commands")
endif()

file(GLOB_RECURSE _iccdev_candidates
  LIST_DIRECTORIES FALSE
  "${ICCDEV_BINARY_DIR}/Tools/*/*"
  "${ICCDEV_BINARY_DIR}/bin/*")

set(_iccdev_checked 0)
foreach(_iccdev_candidate IN LISTS _iccdev_candidates)
  if(NOT EXISTS "${_iccdev_candidate}")
    continue()
  endif()

  execute_process(
    COMMAND file "${_iccdev_candidate}"
    OUTPUT_VARIABLE _iccdev_file_output
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _iccdev_file_output MATCHES "Mach-O .* executable")
    continue()
  endif()

  math(EXPR _iccdev_checked "${_iccdev_checked} + 1")
  execute_process(
    COMMAND "${OTOOL_EXECUTABLE}" -l "${_iccdev_candidate}"
    OUTPUT_VARIABLE _iccdev_otool_output
    ERROR_VARIABLE _iccdev_otool_error
    RESULT_VARIABLE _iccdev_otool_result)
  if(NOT _iccdev_otool_result EQUAL 0)
    message(FATAL_ERROR "otool failed for ${_iccdev_candidate}: ${_iccdev_otool_error}")
  endif()
  if(NOT _iccdev_otool_output MATCHES "LC_UUID")
    message(FATAL_ERROR "Missing LC_UUID load command: ${_iccdev_candidate}")
  endif()
endforeach()

if(_iccdev_checked EQUAL 0)
  message(FATAL_ERROR "No Mach-O executable tools found under ${ICCDEV_BINARY_DIR}")
endif()

message(STATUS "Verified LC_UUID load commands on ${_iccdev_checked} Mach-O executable tool(s)")
