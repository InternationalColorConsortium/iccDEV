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

message(STATUS "iccviz DrawAxisPDF axis labels present (${_sz}-byte PDF): Input/Output + L*/% ink")
