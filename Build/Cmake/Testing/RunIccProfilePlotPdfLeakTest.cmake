################################################################################
# iccProfileVisualizePlot leak regression driver
# Copyright (c) 2026 The International Color Consortium.
#                                       All rights reserved.
################################################################################

if(NOT EXISTS "${VIZ_TOOL}")
  message(FATAL_ERROR "iccProfileVisualizePlot not found: ${VIZ_TOOL}")
endif()
if(NOT EXISTS "${PROFILE}")
  message(FATAL_ERROR "input profile not found: ${PROFILE}")
endif()
if(NOT DEFINED WORKDIR OR "${WORKDIR}" STREQUAL "")
  message(FATAL_ERROR "WORKDIR is required")
endif()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
get_filename_component(_profile_name "${PROFILE}" NAME)
configure_file("${PROFILE}" "${WORKDIR}/${_profile_name}" COPYONLY)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "ASAN_OPTIONS=detect_leaks=1"
    "${VIZ_TOOL}" "${_profile_name}"
  WORKING_DIRECTORY "${WORKDIR}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr
)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR
    "iccProfileVisualizePlot exited ${_result}\n"
    "stdout:\n${_stdout}\n"
    "stderr:\n${_stderr}")
endif()
