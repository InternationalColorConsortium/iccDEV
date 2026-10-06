################################################################################
# Curve-tool gamma label regression for issue #2717
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
################################################################################

# RunCurveGammaToolLabelsTest.cmake - the "Y = X ^ g" label the two curve
# describing tools print for a one-entry curveType must carry the exponent the
# profile stores.
#
# A one-entry curveType holds its gamma as a u8Fixed8Number, raw/256.  The
# library normalises the sample by 65535 on read, so the exponent is
# sample * 65535 / 256 (CIccTagCurve::Describe, #808).  Both tools decoded it as
# sample * 256, high by 65536/65535: gamma 2.20703125 was labelled 2.207065.
# The plotted values were right; only the label was wrong.
#
# For each of the four #808 fixtures this script runs
#   iccProfilePlot <icc> graph curve:rTRC        (JSON on stdout, "description")
#   iccProfileVisualize -json <icc>              (<name>_data.json, "label")
# and checks the label's exponent against raw/256 to within 2e-6, in integer
# micro-units: std::to_string prints six decimals, so the fixed decode is within
# 1e-6 of the stored value, while the retired decode is off by gamma/65535,
# 1.5e-5 or more.  Integer arithmetic because CMake's math() has no floats.
#
# Arguments (-D):
#   PLOT_TOOL    path to the built iccProfilePlot
#   VIZ_TOOL     path to the built iccProfileVisualize
#   FIXTURE_DIR  directory holding gamma-*.icc (.github/ci/regression)
#   WORKDIR      scratch directory; recreated.  iccProfileVisualize writes its
#                JSON beside its input, so the fixtures are copied here first.

foreach(_var PLOT_TOOL VIZ_TOOL FIXTURE_DIR WORKDIR)
  if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
    message(FATAL_ERROR "${_var} is required")
  endif()
endforeach()
foreach(_path "${PLOT_TOOL}" "${VIZ_TOOL}" "${FIXTURE_DIR}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "not found: ${_path}")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

# file:raw -- the u8Fixed8Number each fixture stores in rTRC (see
# curve-gamma-u8fixed8.cpp).  Expected exponent is raw/256.  A colon, not a
# semicolon, so the pair survives CMake's list flattening.
set(_fixtures
  "gamma-1.0000000000.icc:256"
  "gamma-1.796875.icc:460"
  "gamma-2.20703125.icc:565"
  "gamma-2.3984375.icc:614"
)

set(_failures 0)

# Pull the exponent out of a "Y = X ^ d.dddddd" label and compare it with
# raw/256 in units of 1e-8: label*1e6 is an integer once the dot is dropped,
# and raw/256*1e8 = raw*390625.  Tolerance 200 = 2e-6.
function(check_label TOOL LABEL RAW)
  if(NOT LABEL MATCHES "^Y = X \\^ ([0-9]+)\\.([0-9][0-9][0-9][0-9][0-9][0-9])$")
    message(SEND_ERROR "${TOOL}: label is not 'Y = X ^ d.dddddd': '${LABEL}'")
    set(_failures 1 PARENT_SCOPE)
    return()
  endif()
  math(EXPR _label_e8 "(${CMAKE_MATCH_1} * 1000000 + ${CMAKE_MATCH_2}) * 100")
  math(EXPR _exact_e8 "${RAW} * 390625")
  math(EXPR _diff "${_label_e8} - ${_exact_e8}")
  if(_diff LESS 0)
    math(EXPR _diff "0 - ${_diff}")
  endif()
  if(_diff GREATER 200)
    message(SEND_ERROR
      "${TOOL}: '${LABEL}' is off from raw ${RAW}/256 by ${_diff}e-8 "
      "(limit 200e-8)")
    set(_failures 1 PARENT_SCOPE)
  else()
    message(STATUS "${TOOL}: '${LABEL}' matches raw ${RAW}/256 (off by ${_diff}e-8)")
  endif()
endfunction()

foreach(_entry IN LISTS _fixtures)
  string(REPLACE ":" ";" _parts "${_entry}")
  list(GET _parts 0 _file)
  list(GET _parts 1 _raw)
  if(NOT EXISTS "${FIXTURE_DIR}/${_file}")
    message(FATAL_ERROR "fixture not found: ${FIXTURE_DIR}/${_file}")
  endif()
  configure_file("${FIXTURE_DIR}/${_file}" "${WORKDIR}/${_file}" COPYONLY)

  # iccProfilePlot: one JSON object on stdout with a "description" member.
  execute_process(
    COMMAND "${PLOT_TOOL}" "${WORKDIR}/${_file}" graph curve:rTRC
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
  )
  if(NOT _rc EQUAL 0)
    message(SEND_ERROR "iccProfilePlot exited ${_rc} on ${_file}\n${_err}")
    set(_failures 1)
  elseif(NOT _out MATCHES "\"description\":\"([^\"]*)\"")
    message(SEND_ERROR "iccProfilePlot: no description in output for ${_file}\n${_out}")
    set(_failures 1)
  else()
    check_label("iccProfilePlot ${_file}" "${CMAKE_MATCH_1}" "${_raw}")
  endif()

  # iccProfileVisualize: -json writes <stem>_data.json beside the input; the
  # rTRC page's "label" is the description.  Not -silent: the tool's own
  # diagnostics are captured, not shown, and name the cause if a page is dropped.
  execute_process(
    COMMAND "${VIZ_TOOL}" -json "${WORKDIR}/${_file}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
  )
  string(REGEX REPLACE "\\.icc$" "_data.json" _json "${WORKDIR}/${_file}")
  if(NOT _rc EQUAL 0)
    message(SEND_ERROR "iccProfileVisualize exited ${_rc} on ${_file}\n${_err}")
    set(_failures 1)
  elseif(NOT EXISTS "${_json}")
    message(SEND_ERROR "iccProfileVisualize wrote no ${_json}\n${_out}${_err}")
    set(_failures 1)
  else()
    file(READ "${_json}" _data)
    # Tolerant of the writer's spacing and member order within the rTRC object.
    if(NOT _data MATCHES "\"rTRC\":[ \t\r\n]*{[^}]*\"label\":[ \t]*\"([^\"]*)\"")
      message(SEND_ERROR "iccProfileVisualize: no rTRC label in ${_json}")
      set(_failures 1)
    else()
      check_label("iccProfileVisualize ${_file}" "${CMAKE_MATCH_1}" "${_raw}")
    endif()
  endif()
endforeach()

if(_failures)
  message(FATAL_ERROR "curve-gamma-tool-labels: FAIL")
endif()
message(STATUS "curve-gamma-tool-labels: PASS")
