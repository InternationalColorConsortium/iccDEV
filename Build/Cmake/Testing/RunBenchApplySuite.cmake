#################################################################################
# Copyright (c) 2026 The International Color Consortium.
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#################################################################################

# Run the bounded iccBenchApply case table as a correctness check. A successful
# process exit alone permits individual cases to skip when profiles are missing.

foreach(_required ICCDEV_BENCH_APPLY ICCDEV_REPO_ROOT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

execute_process(
  COMMAND "${ICCDEV_BENCH_APPLY}" -suite -csv -pixels 8192 -repeats 1
  WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _output
  ERROR_VARIABLE _error
  ECHO_ERROR_VARIABLE
)
if(NOT "${_result}" STREQUAL "0")
  message(FATAL_ERROR "iccBenchApply suite failed (${_result}):\n${_output}\n${_error}")
endif()

string(REPLACE "\r" "" _output "${_output}")
string(REGEX REPLACE "\n$" "" _output "${_output}")
string(REPLACE "\n" ";" _rows "${_output}")
set(_expected_cases
  matrix-trc lut-3d-tetra spectral-6ch monochrome mono-lab mpe-calc
  mpe-tonemap pcs-rel pcs-abs)
list(POP_FRONT _rows _header)
if(NOT _header STREQUAL "case,threads,mpx_per_sec,min,max,checksum,status")
  message(FATAL_ERROR "iccBenchApply CSV header changed: ${_header}")
endif()

list(LENGTH _expected_cases _expected_count)
list(LENGTH _rows _row_count)
if(NOT _row_count EQUAL _expected_count)
  message(FATAL_ERROR
    "iccBenchApply measured ${_row_count}/${_expected_count} cases:\n${_output}\n${_error}")
endif()

math(EXPR _last_case "${_expected_count} - 1")
set(_rate_pattern "^[0-9]+\\.[0-9][0-9][0-9]$")
foreach(_index RANGE 0 ${_last_case})
  list(GET _expected_cases ${_index} _case)
  list(GET _rows ${_index} _row)
  string(REPLACE "," ";" _fields "${_row}")
  list(LENGTH _fields _field_count)
  if(NOT _field_count EQUAL 7)
    message(FATAL_ERROR "iccBenchApply case ${_case} did not complete: ${_row}\n${_error}")
  endif()
  list(GET _fields 0 _name)
  list(GET _fields 1 _threads)
  list(GET _fields 2 _median)
  list(GET _fields 3 _minimum)
  list(GET _fields 4 _maximum)
  list(GET _fields 5 _checksum)
  list(GET _fields 6 _status)
  if(NOT _name STREQUAL _case OR NOT _threads STREQUAL "1" OR
     NOT _status STREQUAL "ok")
    message(FATAL_ERROR "iccBenchApply case ${_case} did not complete: ${_row}\n${_error}")
  endif()
  foreach(_rate IN ITEMS "${_median}" "${_minimum}" "${_maximum}")
    if(NOT _rate MATCHES "${_rate_pattern}")
      message(FATAL_ERROR "iccBenchApply case ${_case} has an invalid rate: ${_row}")
    endif()
  endforeach()
  string(LENGTH "${_checksum}" _checksum_length)
  if(NOT _checksum_length EQUAL 10 OR
     NOT _checksum MATCHES "^0x[0-9a-fA-F]+$")
    message(FATAL_ERROR "iccBenchApply case ${_case} has an invalid checksum: ${_row}")
  endif()
endforeach()

message(STATUS "iccBenchApply: ${_row_count} cases measured with valid checksums")
