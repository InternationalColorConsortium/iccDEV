#################################################################################
# iccviz DrawAxisPDF axis-label regression for PR #1712
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

# RunIccVizPdfAxisTest.cmake - driver for the DrawAxisPDF axis-label regression.
#
# Runs iccProfileVisualizePlot on a profile and asserts the emitted "_luts.pdf"
# carries the axis labels produced by BOTH DrawAxisPDF code paths introduced in the
# #1711 sync / #1712 review:
#   * default identity axis (drawIdentity=true)   -> the tone-curve plot's
#     "(Input)" / "(Output)" axes;
#   * parameterized custom-tick axis (drawIdentity=false + explicit start/mid/end
#     tick labels) -> the neutral-axis-inking plot's "(L* (100 to 0))" / "(% ink)".
#
# This is a *content* pin, not a byte-for-byte golden PDF: it fails if the axis
# parameterization regresses (a label goes missing, the drawIdentity / custom-tick
# wiring breaks, or a plot stops emitting its axis) without false-failing when the
# underlying LUT curve sampling legitimately shifts across IccProfLib versions.  It
# relies on MiniPDF emitting the content stream uncompressed (no FlateDecode), so
# the Tj label operators appear as literal ASCII that file(STRINGS) can match.
#
# Required -D args: VIZ_TOOL (tool path), PROFILE (input .icc), WORKDIR (writable
# scratch dir; the tool writes its outputs next to the input it is given).

if(NOT EXISTS "${VIZ_TOOL}")
  message(FATAL_ERROR "iccProfileVisualizePlot not found: ${VIZ_TOOL}")
endif()
if(NOT EXISTS "${PROFILE}")
  message(FATAL_ERROR "input profile not found: ${PROFILE}")
endif()

file(MAKE_DIRECTORY "${WORKDIR}")
get_filename_component(_pname "${PROFILE}" NAME)
get_filename_component(_stem  "${PROFILE}" NAME_WE)
# The tool writes outputs beside its input; copy the fixture into the scratch dir so
# nothing lands in the source tree.
configure_file("${PROFILE}" "${WORKDIR}/${_pname}" COPYONLY)

execute_process(
  COMMAND "${VIZ_TOOL}" "${_pname}"
  WORKING_DIRECTORY "${WORKDIR}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE  _err
)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "iccProfileVisualizePlot exited ${_rc}\nstdout:\n${_out}\nstderr:\n${_err}")
endif()

set(_pdf "${WORKDIR}/${_stem}_luts.pdf")
if(NOT EXISTS "${_pdf}")
  message(FATAL_ERROR "expected LUT PDF was not emitted: ${_pdf}")
endif()
file(SIZE "${_pdf}" _sz)
if(_sz LESS 1000)
  message(FATAL_ERROR "emitted PDF is implausibly small (${_sz} bytes): ${_pdf}")
endif()

# Guard against a silently-compressed content stream making the label match vacuous.
file(STRINGS "${_pdf}" _flate REGEX "FlateDecode")
if(_flate)
  message(FATAL_ERROR "PDF content stream is compressed (FlateDecode); the plaintext "
                      "axis-label assertion below would be unreliable")
endif()

# human label -> regex (parens / '*' escaped for file(STRINGS REGEX)).
set(_labels
  "(Input)|\\(Input\\)"
  "(Output)|\\(Output\\)"
  "(L* (100 to 0))|\\(L\\* \\(100 to 0\\)\\)"
  "(% ink)|\\(% ink\\)"
)
foreach(_pair IN LISTS _labels)
  string(REPLACE "|" ";" _pp "${_pair}")
  list(GET _pp 0 _human)
  list(GET _pp 1 _rx)
  file(STRINGS "${_pdf}" _hit REGEX "${_rx}")
  if(NOT _hit)
    message(FATAL_ERROR "DrawAxisPDF label '${_human}' missing from ${_stem}_luts.pdf "
                        "-- the axis parameterization regressed")
  endif()
endforeach()

# Beyond the axis *titles* above, pin the parameterized *tick* operators so a
# regression in DrawAxisPDF's start/mid/end tick wiring is caught. The two axis
# modes emit distinguishable tick-label draw operators "(<text>) Tj":
#   * default identity axes (drawIdentity=true: Input / Output / % ink) use the
#     0 / 50% / 100% defaults -> "(50%) Tj" and "(100%) Tj" must appear;
#   * the reversed neutral L* axis (drawIdentity=false, start=100 mid=50 end=0)
#     overrides them -> "(100) Tj" and "(50) Tj" (NOT "(100%)"/"(50%)") must appear.
# Requiring all four proves BOTH the default and the custom-tick code paths ran --
# i.e. the drawIdentity toggle actually selected different tick labels, which the
# title-only check could not distinguish.
set(_ticks
  "(50%) Tj|\\(50%\\) Tj"     # default-axis mid tick
  "(100%) Tj|\\(100%\\) Tj"   # default-axis end tick
  "(50) Tj|\\(50\\) Tj"       # reversed L* mid tick (not 50%)
  "(100) Tj|\\(100\\) Tj"     # reversed L* start tick (not 100%)
)
foreach(_pair IN LISTS _ticks)
  string(REPLACE "|" ";" _pp "${_pair}")
  list(GET _pp 0 _human)
  list(GET _pp 1 _rx)
  file(STRINGS "${_pdf}" _hit REGEX "${_rx}")
  if(NOT _hit)
    message(FATAL_ERROR "DrawAxisPDF tick operator '${_human}' missing from "
                        "${_stem}_luts.pdf -- the custom start/mid/end tick "
                        "parameterization regressed")
  endif()
endforeach()

message(STATUS "iccviz DrawAxisPDF axis labels + tick operators present (${_sz}-byte PDF): "
               "Input/Output + L*/% ink, default (50%/100%) and reversed (100/50) ticks")
