#################################################################################
# iccPawgReport section-H (HDR Profile, ICC.1 clause 8.10) test
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################
#
# The section-H items are computed inside PawgReport.cpp's anonymous namespace
# and are deliberately NOT part of the header's public assessment API, so this
# drives the built tool end to end instead - which is also the shape a consumer
# (profiletool) actually reads.
#
# Three contracts are pinned, and each one is invisible from a report that
# merely "looks right":
#
#  1. ABSENCE for a non-HDR profile.  The section must not appear at all for an
#     SDR profile, and the item count must stay exactly what it was before the
#     section existed - emitting eight NOT RUN placeholders would change every
#     existing report's summary to say nothing.
#
#  2. The conforming/intended split, and that its consequences land on the right
#     items.  A profile whose cicp TransferCharacteristics is outside {8,16,18}
#     is an INTENDED HDR Profile (H1 WARN) with the violation on H4, not on H1.
#
#  3. That an HDR finding is no longer quoted under item C3's tag-type title.
#     CIccProfile::Validate() calls CheckHdrProfile(), so before section H
#     existed the first HDR message in that report surfaced verbatim as C3's
#     detail line - reading as though the clause-8.10 text were the tag-type
#     finding.  The verdict is deliberately unchanged; only the attribution is.
#
# The fixtures are generated (Testing/HDR/mkprofiles.sh, driven by
# Testing/CreateAllProfiles.sh).  Their absence means the generator has not run,
# not that the code is wrong, so the test skips rather than fails - exactly as
# the iccdev.hdr-profile-classification regression does.

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_BUILD_DIR
  ICCDEV_PAWG_EXE
  ICCDEV_HDR_DIR
  ICCDEV_SDR_PROFILE
  ICCDEV_SDR_ITEM_COUNT
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

if(NOT EXISTS "${ICCDEV_PAWG_EXE}")
  message(FATAL_ERROR "iccPawgReport not found: ${ICCDEV_PAWG_EXE}")
endif()

if(NOT EXISTS "${ICCDEV_SDR_PROFILE}")
  message(FATAL_ERROR "SDR control profile not found: ${ICCDEV_SDR_PROFILE}")
endif()

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_log_file "${ICCDEV_TEST_OUTDIR}/output.log")
file(WRITE "${_log_file}" "CTest test: ${ICCDEV_TEST_NAME}\nTool: ${ICCDEV_PAWG_EXE}\n\n")

# The generated HDR fixtures this test needs.  Checked up front so a partial
# generation skips as cleanly as no generation at all.
set(_fixtures
  HagcDisplay.icc
  HdrCicpUnspecified.icc
  HdrDisplayMetadata.icc
  HdrInvalidTransfer.icc
  HdrMissingBToA0.icc
)
foreach(_fixture IN LISTS _fixtures)
  if(NOT EXISTS "${ICCDEV_HDR_DIR}/${_fixture}")
    message(STATUS
      "SKIP ${ICCDEV_TEST_NAME}: ${ICCDEV_HDR_DIR}/${_fixture} is absent "
      "(run Testing/CreateAllProfiles.sh to generate the HDR fixtures)")
    return()
  endif()
endforeach()

# On Windows the tool needs the shared IccProfLib/IccXML/... beside it on PATH;
# elsewhere the loader finds them by rpath and this whole block is inert.
set(_env_args)
if(WIN32)
  include("${CMAKE_CURRENT_LIST_DIR}/WindowsRuntimePaths.cmake")
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
  set(_env_args "${CMAKE_COMMAND}" -E env "PATH=${_path_prefix};$ENV{PATH}")
endif()

# Runs the report over one profile into _report.  The tool exits 1 whenever any
# item FAILs, and two of the fixtures below are negative cases that are SUPPOSED
# to fail, so only a crash-shaped exit code is treated as an error here; the
# assertions are all on content.
function(iccdev_run_pawg _profile _out_var)
  execute_process(
    COMMAND ${_env_args} "${ICCDEV_PAWG_EXE}" "${_profile}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  file(APPEND "${_log_file}"
    "===== ${_profile} (rc=${_rc}) =====\n${_stdout}\n${_stderr}\n")
  if(NOT _rc EQUAL 0 AND NOT _rc EQUAL 1)
    message(FATAL_ERROR "iccPawgReport exited with ${_rc} on ${_profile}; see ${_log_file}")
  endif()
  set(${_out_var} "${_stdout}" PARENT_SCOPE)
endfunction()

# Functions, not macros, on purpose: a macro would substitute the whole report
# textually into the if(), and these reports carry semicolons and quotes in
# their detail lines.  A function argument is a real variable and survives both.
function(iccdev_expect _text _regex _what)
  if(NOT "${_text}" MATCHES "${_regex}")
    message(FATAL_ERROR "${_what}; see ${_log_file}")
  endif()
endfunction()

function(iccdev_expect_not _text _regex _what)
  if("${_text}" MATCHES "${_regex}")
    message(FATAL_ERROR "${_what}; see ${_log_file}")
  endif()
endfunction()

# --- 1. The SDR control: the section must be absent, item count unchanged -----
iccdev_run_pawg("${ICCDEV_SDR_PROFILE}" _sdr)
iccdev_expect("${_sdr}" "ICC PROFILE ASSESSMENT REPORT \\(PAWG\\)"
  "the SDR control report is missing its PAWG header")
iccdev_expect_not("${_sdr}" "\\[ HDR"
  "an SDR profile printed an HDR section; the section must be absent, not empty")
iccdev_expect_not("${_sdr}" "\n[ \t]+\\[[A-Z/ ]+\\][ \t]+H[0-9]"
  "an SDR profile emitted section-H items")
iccdev_expect("${_sdr}" "Total checklist items:[ \t]+${ICCDEV_SDR_ITEM_COUNT}"
  "the SDR control item count changed; section H must not alter a non-HDR report")

# --- 2. A conforming HDR Profile: all eight items, HAGC tone mapping ----------
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HagcDisplay.icc" _hagc)
iccdev_expect("${_hagc}" "\\[ HDR \\(ICC.1 clause 8.10\\) \\]"
  "a conforming HDR Profile did not print the HDR section")
foreach(_id H1 H2 H3 H4 H5 H6 H7 H8)
  iccdev_expect("${_hagc}" "\\][ \t]+${_id}[ \t]"
    "item ${_id} is missing from a conforming HDR Profile's report")
endforeach()
iccdev_expect("${_hagc}" "\\[OK[ \t]*\\][ \t]+H1[ \t]"
  "a conforming HDR Profile was not classified OK at H1")
# "meets clause 8.10.1" rather than "conforming": 8.10.1's conditions are the
# membership test, and a profile that meets them can still break 8.10.6's
# pairing rule (case 6 below, where H1 is OK and H6 FAILs).  Calling H1
# "conforming" would overstate what the item has established.
iccdev_expect("${_hagc}" "HDR Profile: meets clause 8\\.10\\.1"
  "H1 did not state that the profile meets the clause 8.10.1 membership conditions")
iccdev_expect("${_hagc}" "\\[OK[ \t]*\\][ \t]+H4[ \t]"
  "H4 did not accept TransferCharacteristics 16")
iccdev_expect("${_hagc}" "8\\.10\\.3 a\\): headroomAdaptiveGainCurveTag"
  "H6 did not identify the HAGC tag as the highest-ranked tone-mapping descriptor")
# No HDR Display entries: 8.10.5 d) sends the consumer to the destination
# device, which is correct rather than deficient - so N/A, not WARN.
iccdev_expect("${_hagc}" "\\[N/A[ \t]*\\][ \t]+H8[ \t]"
  "H8 warned about a profile that simply carries no clause 8.10.5 entries")

# --- 3. ColourPrimaries 2 resolves through the profile's own matrix columns ---
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrCicpUnspecified.icc" _unspec)
iccdev_expect("${_unspec}" "\\[OK[ \t]*\\][ \t]+H5[ \t]"
  "H5 failed to resolve primaries for ColourPrimaries 2")
iccdev_expect("${_unspec}" "resolved from the profile's own matrix column tags"
  "H5 did not report that ColourPrimaries 2 resolved through clause 9.2.17 / 10.3")
# Nothing states a content reference white, so 8.10.4's 203 cd/m^2 default
# applies: conformant, but assumed rather than stated, which is the whole point
# of the item.
iccdev_expect("${_unspec}" "\\[WARN[ \t]*\\][ \t]+H7[ \t]"
  "H7 presented the 203 cd/m^2 default as a stated value")

# --- 4. HDR Display metadata: the 8.10.5 precedence and the registry caveat ---
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrDisplayMetadata.icc" _meta)
iccdev_expect("${_meta}" "\\[OK[ \t]*\\][ \t]+H8[ \t]"
  "H8 did not resolve a display headroom from the HDR Display entries")
iccdev_expect("${_meta}" "8\\.10\\.5 a\\)"
  "H8 did not report DERH as the rule that fired; NOTE 13 makes the provenance normative")
iccdev_expect("${_meta}" "no HDR Display category until the amendment is accepted"
  "an unregistered HDR Display value was presented without its caveat")
iccdev_expect_not("${_meta}" "Content HDR Reference White Luminance entry \\["
  "a registered HDR Image value was presented with a caveat it does not need")

# --- 5. Outside the sub-class: H1 is N/A, descriptive, and stands alone -------
# Clause 8.10.1's conditions are definitional, so a profile that misses one is a
# valid ICC profile of another class and nothing may be reported against it.
# H1 states the classification; H2 onward have no subject and must not print.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrInvalidTransfer.icc" _bad_tc)
iccdev_expect("${_bad_tc}" "\\[N/A[ \t]*\\][ \t]+H1[ \t]"
  "a profile outside the clause 8.10 sub-class was not reported as N/A at H1")
iccdev_expect("${_bad_tc}" "not of the clause 8\\.10 HDR Profile sub-class"
  "H1 did not state the classification")
iccdev_expect("${_bad_tc}" "classification, not a finding"
  "H1 did not make clear that non-membership is not a defect")
iccdev_expect_not("${_bad_tc}" "\\[FAIL[ \t]*\\][ \t]+H[0-9]"
  "a membership condition was reported as a failure")
iccdev_expect_not("${_bad_tc}" "\\[WARN[ \t]*\\][ \t]+H[0-9]"
  "a membership condition was reported as a warning")
iccdev_expect_not("${_bad_tc}" "[ \t]+H[2-8][ \t]"
  "an HDR Profile question was asked of a profile outside the sub-class")
# The C3 attribution fix: the clause-8.10 text must no longer be quoted under
# the tag-type question.  The verdict there is intentionally left alone.
iccdev_expect_not("${_bad_tc}" "C3[^\n]*\n[ \t]*[^\n]*clause 8\\.10\\.1 permits only"
  "C3 still quotes an HDR finding under its tag-type title")

# --- 6. The 8.10.6 pairing rule, the one shall an HDR Profile can break -------
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrMissingBToA0.icc" _unpaired)
iccdev_expect("${_unpaired}" "\\[OK[ \t]*\\][ \t]+H1[ \t]"
  "the pairing violation was folded into the H1 classification; 8.10.6 sits outside 8.10.1")
iccdev_expect("${_unpaired}" "\\[FAIL[ \t]*\\][ \t]+H6[ \t]"
  "H6 did not fail an AToB0Tag with no paired BToA0Tag")
iccdev_expect_not("${_unpaired}" "C3[^\n]*\n[ \t]*[^\n]*clause 8\\.10\\.6 requires the pair"
  "C3 still quotes the HDR pairing finding under its tag-type title")

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
