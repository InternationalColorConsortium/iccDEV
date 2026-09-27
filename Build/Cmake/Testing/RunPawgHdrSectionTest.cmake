#################################################################################
# iccPawgReport section-H (HDR ColorSpace Profile, ICC.1 clause 8.7.1) test
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################
#
# The section-H items are computed inside PawgReport.cpp's anonymous namespace
# and are deliberately NOT part of the header's public assessment API, so this
# drives the built tool end to end instead - which is also the shape a consumer
# (profiletool) actually reads.
#
# Four contracts are pinned, and each one is invisible from a report that
# merely "looks right":
#
#  1. ABSENCE for a non-HDR profile.  The section must not appear at all for an
#     SDR profile, and the item count must stay exactly what it was before the
#     section existed - emitting eight NOT RUN placeholders would change every
#     existing report's summary to say nothing.
#
#  2. Membership is a classification, not a finding.  A profile outside the
#     clause 8.7.1 sub-class - a TransferCharacteristics outside {8,16,18}, say -
#     gets H1 as N/A with a description of what it is, and no H2-H8 at all.
#
#  3. That an HDR finding is no longer quoted under item C3's tag-type title.
#     CIccProfile::Validate() calls CheckHdrProfile(), so before section H
#     existed the first HDR message in that report surfaced verbatim as C3's
#     detail line - reading as though the clause-8.7.1 text were the tag-type
#     finding.  The verdict is deliberately unchanged; only the attribution is.
#
#  4. The converse of 3: every clause 8.7.1 finding C3 defers is stated by some
#     H item, so a deferred finding cannot vanish from the report.
#
# The fixtures are generated (Testing/HDR/mkprofiles.sh, driven by
# Testing/CreateAllProfiles.sh).  The test declares FIXTURES_REQUIRED
# iccdev_profiles, so a fixture still absent here means generation failed, and
# the script fails rather than skipping - see the check below.

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_BUILD_DIR
  ICCDEV_PAWG_EXE
  ICCDEV_FROMXML_EXE
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

if(NOT EXISTS "${ICCDEV_FROMXML_EXE}")
  message(FATAL_ERROR "iccFromXml not found: ${ICCDEV_FROMXML_EXE}")
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
  HagcColorSpace.icc
  HdrCicpUnspecified.icc
  HdrColorSpaceClass.icc
  HdrClassDisplayNegative.icc
  HdrClassInputNegative.icc
  HdrInvalidTransfer.icc
  HdrLinearHagcCrwlDisagree.icc
  HdrMissingBToA0.icc
  HdrMissingBToA1.icc
  HdrMissingLutPair.icc
  HdrNarrowRangeFlag.icc
)
# FATAL_ERROR, not a bare return().  This used to `return()`, which in `cmake -P`
# is exit 0, so a run that asserted nothing reported a green PASS - the same
# skip-to-green defect the C++ HDR tests had.  Those now exit 77 and let
# SKIP_RETURN_CODE turn it into a ctest Skipped; a cmake script cannot, because
# cmake_language(EXIT) needs 3.29 and this project requires 3.18.
#
# Failing is the right answer anyway now that the test declares
# FIXTURES_REQUIRED iccdev_profiles: ctest runs the generation first, so a
# fixture still absent here means the generation actually failed, and that
# should be red rather than quietly skipped.
foreach(_fixture IN LISTS _fixtures)
  if(NOT EXISTS "${ICCDEV_HDR_DIR}/${_fixture}")
    message(FATAL_ERROR
      "${ICCDEV_TEST_NAME}: ${ICCDEV_HDR_DIR}/${_fixture} is absent. "
      "FIXTURES_REQUIRED iccdev_profiles should have generated it; if you are "
      "running this test directly, run Testing/CreateAllProfiles.sh first.")
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
  # Escaped, because _env_args is a CMake list: an unescaped ";" inside the
  # PATH value split it into separate arguments, so cmake -E env received a
  # truncated PATH and the rest of the directories as the command to run.
  string(REPLACE ";" "\\;" _path_value "${_path_prefix};$ENV{PATH}")
  set(_env_args "${CMAKE_COMMAND}" -E env "PATH=${_path_value}")
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

# --- 2. A conforming HDR ColorSpace Profile: all eight items, HAGC tone mapping ----------
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HagcColorSpace.icc" _hagc)
iccdev_expect("${_hagc}" "\\[ HDR \\(ICC.1 clause 8.7.1\\) \\]"
  "a conforming HDR ColorSpace Profile did not print the HDR section")
# H8 IS GONE: it asked which rule of clause 8.10.5 resolved the display
# headroom, and the 23-09-2026 revision deletes that clause.  Section H now
# ends at H7.  The IDs of H1-H7 are deliberately unchanged - downstream
# readers key on them - and H2's QUESTION was repurposed rather than
# renumbered when the profileVersionField condition was withdrawn.
foreach(_id H1 H2 H3 H4 H5 H6 H7)
  iccdev_expect("${_hagc}" "\\][ \t]+${_id}[ \t]"
    "item ${_id} is missing from a conforming HDR ColorSpace Profile's report")
endforeach()
iccdev_expect("${_hagc}" "\\[OK[ \t]*\\][ \t]+H1[ \t]"
  "a conforming HDR ColorSpace Profile was not classified OK at H1")
# "meets clause 8.7.1.1" rather than "conforming": 8.7.1.1's conditions are the
# membership test, and a profile that meets them can still break 8.7.1.5's
# pairing rule (case 6 below, where H1 is OK and H6 FAILs).  Calling H1
# "conforming" would overstate what the item has established.
iccdev_expect("${_hagc}" "HDR ColorSpace Profile: meets clause 8\\.7\\.1\\.1"
  "H1 did not state that the profile meets the clause 8.7.1.1 membership conditions")
iccdev_expect("${_hagc}" "\\[OK[ \t]*\\][ \t]+H4[ \t]"
  "H4 did not accept TransferCharacteristics 16")
iccdev_expect("${_hagc}" "8\\.7\\.1\\.3 a\\): headroomAdaptiveGainCurveTag"
  "H6 did not identify the HAGC tag as the highest-ranked tone-mapping descriptor")
# And no H8 at all.  A stale item ID surviving a clause deletion is exactly the
# kind of thing that goes unnoticed, so its absence is asserted rather than
# assumed.
iccdev_expect_not("${_hagc}" "\\][ \t]+H8[ \t]"
  "H8 still prints; clause 8.10.5 is deleted and the item with it")

# --- 3. ColourPrimaries 2 is REFUSED, and H5 says why ------------------------
# Inverted by the 23-09-2026 revision.  8.10.1 routed ColourPrimaries 2 to the
# profile's own matrix column tags and H5 reported an OK resolution; 8.7.1.1
# routes it to the cicpType custom chromaticity extension of 10.3, a ColorSpace
# profile has no matrix column tags to fall back on, and this build cannot read
# that extension - so H5 FAILs and names both the clause and the limitation.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrCicpUnspecified.icc" _unspec)
iccdev_expect("${_unspec}" "\\[FAIL[ \t]*\\][ \t]+H5[ \t]"
  "H5 accepted ColourPrimaries 2 without the clause 10.3 chromaticity extension")
iccdev_expect("${_unspec}" "custom chromaticity extension of 10\\.3"
  "H5 did not name the extension clause 8.7.1.1 requires for ColourPrimaries 2")
iccdev_expect_not("${_unspec}" "resolved from the profile's own matrix column tags"
  "H5 still reports the withdrawn matrix-column resolution for ColourPrimaries 2")
# Nothing states a content reference white, so 8.7.1.4's 203 cd/m^2 default
# applies: conformant, but assumed rather than stated, which is the whole point
# of the item.
iccdev_expect("${_unspec}" "\\[WARN[ \t]*\\][ \t]+H7[ \t]"
  "H7 presented the 203 cd/m^2 default as a stated value")

# --- 4. The class negative: a Display profile is not an HDR ColorSpace Profile
# HdrClassDisplayNegative is HdrColorSpaceClass with the class set to 'mntr' and
# nothing else changed.  It pins that carrying a conforming PQ cicpTag and HDR
# Image metadata does NOT make a Display profile a member - the amendment's
# Issues section says exactly this shape "is not one" - and that nothing is
# reported against it for that.  It replaces HdrDisplayMetadata, which was the
# conforming base under clause 8.10 and modelled the retired Display format.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrClassDisplayNegative.icc" _meta)
iccdev_expect("${_meta}" "\\[N/A[ \t]*\\][ \t]+H1[ \t]"
  "a Display-class profile was admitted to the clause 8.7.1 sub-class")
iccdev_expect("${_meta}" "Display profile, "
  "H1 did not name the profile's actual class")
iccdev_expect_not("${_meta}" "[ \t]+H[2-7][ \t]"
  "an HDR ColorSpace Profile question was asked of a Display-class profile")
# And no trace of the deleted clause anywhere in the report.
iccdev_expect_not("${_meta}" "8\\.10\\.5"
  "the report still cites clause 8.10.5, which the revision deletes")
iccdev_expect_not("${_meta}" "no HDR Display category until the amendment is accepted"
  "the HDR Display registry caveat outlived its clause")

# --- 5. Outside the sub-class: H1 is N/A, descriptive, and stands alone -------
# Clause 8.7.1.1's conditions are definitional, so a profile that misses one is a
# valid ICC profile of another class and nothing may be reported against it.
# H1 states the classification; H2 onward have no subject and must not print.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrInvalidTransfer.icc" _bad_tc)
iccdev_expect("${_bad_tc}" "\\[N/A[ \t]*\\][ \t]+H1[ \t]"
  "a profile outside the clause 8.7.1 sub-class was not reported as N/A at H1")
iccdev_expect("${_bad_tc}" "not of the clause 8\\.7\\.1 HDR ColorSpace Profile sub-class"
  "H1 did not state the classification")
iccdev_expect("${_bad_tc}" "classification, not a finding"
  "H1 did not make clear that non-membership is not a defect")
iccdev_expect_not("${_bad_tc}" "\\[FAIL[ \t]*\\][ \t]+H[0-9]"
  "a membership condition was reported as a failure")
iccdev_expect_not("${_bad_tc}" "\\[WARN[ \t]*\\][ \t]+H[0-9]"
  "a membership condition was reported as a warning")
iccdev_expect_not("${_bad_tc}" "[ \t]+H[2-7][ \t]"
  "an HDR ColorSpace Profile question was asked of a profile outside the sub-class")
# The C3 attribution fix: no clause-8.7.1 finding may be quoted under the
# tag-type question.  Every such finding CheckHdrProfile() emits starts "HDR: ".
# A profile outside the sub-class draws none, so this is a guard, not the pin;
# section 6b below is the fixture that can actually fail.
iccdev_expect_not("${_bad_tc}" "C3[^\n]*\n[ \t]*[^\n]*HDR: "
  "C3 quotes an HDR finding under its tag-type title")

# --- 6. The 8.7.1.5 pairing rule, the one shall an HDR ColorSpace Profile can break -------
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrMissingBToA0.icc" _unpaired)
iccdev_expect("${_unpaired}" "\\[OK[ \t]*\\][ \t]+H1[ \t]"
  "the pairing violation was folded into the H1 classification; 8.7.1.5 sits outside 8.7.1.1")
iccdev_expect("${_unpaired}" "\\[FAIL[ \t]*\\][ \t]+H6[ \t]"
  "H6 did not fail an AToB0Tag with no paired BToA0Tag")
iccdev_expect_not("${_unpaired}" "C3[^\n]*\n[ \t]*[^\n]*HDR: "
  "C3 quotes the HDR pairing finding under its tag-type title")

# --- 6b. C3 on a report whose ONLY findings are clause 8.7.1 ones --------------
# The two C3 guards above cannot fail: HdrInvalidTransfer is outside the
# sub-class, and HdrMissingBToA0's first report line is the non-HDR "Critical
# tag(s) missing".  HdrMissingBToA1's first line IS the HDR pairing finding, so
# a C3 that quoted the first report line (FirstReportLine() rather than
# FirstNonHdrReportLine()) would quote it here.  The needles are the current
# wording; the old ones ("clause 8.7.1.1 permits only", "clause 8.7.1.5 requires
# the pair") were removed from the library in 44d5d590 and matched nothing.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrMissingBToA1.icc" _hdr_only)
iccdev_expect("${_hdr_only}" "C3[^\n]*\n[ \t]*profile validation reported only HDR ColorSpace Profile \\(ICC\\.1 clause 8\\.7\\.1\\) findings; see the HDR section"
  "C3 did not refer an HDR-only validation report to the HDR section")
iccdev_expect_not("${_hdr_only}" "C3[^\n]*\n[ \t]*[^\n]*HDR: "
  "C3 quotes an HDR finding under its tag-type title")

# --- 7. Two carriers of the content reference white that disagree ------------
# HAGC HDRReferenceWhite 300 against a metadataTag CRWL of 203.  The only
# fixture that reaches H7's disagreement branch: every other profile carries at
# most one of the two, so the branch never ran.  The HAGC value is the one
# reported, because 8.7.1.3 ranks the tag highest (HDR-10).
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrLinearHagcCrwlDisagree.icc" _disagree)
iccdev_expect("${_disagree}" "\\[WARN[ \t]*\\][ \t]+H7[ \t]"
  "H7 did not warn about two reference-white carriers that disagree")
iccdev_expect("${_disagree}" "content HDR reference white = 300 cd/m\\^2"
  "H7 did not report the headroomAdaptiveGainCurveTag's white as the resolved value")
iccdev_expect("${_disagree}" "two carriers DISAGREE: the metadataTag CRWL entry says 203 cd/m\\^2"
  "H7 did not name the disagreeing CRWL value")

# --- 8. Every clause 8.7.1 finding C3 defers is stated by an H item -----------
# C3 refers an HDR-only validation report to this section (6b), so each finding
# CheckHdrProfile() emits has to appear under some H item.  These used to appear
# nowhere: H6 checked the pairing rule only at x = 0 and called a missing
# AToB0Tag "not a violation", H5 said only that primaries were undetermined, and
# the narrow-range warning had no item at all.
iccdev_expect("${_hdr_only}" "\\[FAIL[ \t]*\\][ \t]+H6[ \t]"
  "H6 did not fail an AToB1Tag with no paired BToA1Tag")
iccdev_expect("${_hdr_only}" "AToB1Tag present without its paired BToA1Tag"
  "H6 did not name the unpaired AToB1Tag")

# THE AToB0/BToA0 REQUIREMENT MOVED, and with it the messages this block
# used to look for.  8.7.1.5 defers the pair to 8.7 - "both already
# unconditionally required of every ColorSpace profile by 8.7" - so
# CheckRequiredTags() raises it as a CRITICAL error and CheckHdrProfile() no
# longer emits "AToB0Tag missing; clause 8.10.6 requires it" at all.  H6 still
# FAILs, because a profile with no AToB0Tag has no descriptor c) either, and
# the absence of the old duplicate messages is asserted so that a build which
# reinstated them fails here.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrMissingLutPair.icc" _pairless)
iccdev_expect("${_pairless}" "\\[FAIL[ \t]*\\][ \t]+H6[ \t]"
  "H6 did not fail an HDR ColorSpace Profile carrying no AToB0Tag")
iccdev_expect("${_pairless}" "no tone-mapping descriptor of clause 8\\.7\\.1\\.3 and no AToB0Tag"
  "H6 did not say why a profile with neither tag has no descriptor")
iccdev_expect_not("${_pairless}" "AToB0Tag missing; clause"
  "H6 repeats an AToB0Tag requirement that 8.7 now owns")
iccdev_expect_not("${_pairless}" "not a violation"
  "H6 still describes a missing AToB0Tag as not a violation")

iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrNarrowRangeFlag.icc" _narrow)
iccdev_expect("${_narrow}" "\\[WARN[ \t]*\\][ \t]+H4[ \t]"
  "H4 did not state the library's narrow-range warning")

# --- 9. H1 names the profile's actual class ---------------------------------
# THE FIXTURE MOVED.  This used HdrColorSpaceClass, which the 23-09-2026
# revision makes a MEMBER - H1 then prints the conforming text and no class
# line at all, so the assertion had nothing to read.  The defect it guards is
# unchanged: H1 once printed "Display" for every class that was not Input.
#
# HdrClassInputNegative is the Input half.  The class that is neither Input nor
# Display - the shape that actually caught the bug - is asserted in section 10
# below, where an Output-class profile is already built.
iccdev_run_pawg("${ICCDEV_HDR_DIR}/HdrClassInputNegative.icc" _scnr)
iccdev_expect("${_scnr}" "Input profile, "
  "H1 did not name an Input-class profile's class")
iccdev_expect_not("${_scnr}" "Display profile, "
  "H1 described an Input-class profile as a Display profile")

# --- 10. C4 and C5 read the Input/Display allowances as allowances ----------
# Both profiles are built here from the tracked fixture XML, since neither shape
# is worth a corpus fixture of its own: one carries a BToA0Tag and no forward
# transform, the other a HAGC tag in a class that may not carry one.
function(iccdev_from_xml _xml_text _name _out_var)
  set(_xml "${ICCDEV_TEST_OUTDIR}/${_name}.xml")
  set(_icc "${ICCDEV_TEST_OUTDIR}/${_name}.icc")
  file(WRITE "${_xml}" "${_xml_text}")
  execute_process(
    COMMAND ${_env_args} "${ICCDEV_FROMXML_EXE}" "${_xml}" "${_icc}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  file(APPEND "${_log_file}"
    "===== iccFromXml ${_name} (rc=${_rc}) =====\n${_stdout}\n${_stderr}\n")
  if(NOT EXISTS "${_icc}")
    message(FATAL_ERROR "iccFromXml did not write ${_icc}; see ${_log_file}")
  endif()
  set(${_out_var} "${_icc}" PARENT_SCOPE)
endfunction()

file(READ "${ICCDEV_HDR_DIR}/HagcColorSpace.xml" _hagc_xml)
string(REGEX MATCH "<headroomAdaptiveGainCurveTag>.*</headroomAdaptiveGainCurveTag>"
  _hagc_block "${_hagc_xml}")
string(REGEX MATCH "<BToA0Tag>.*</BToA0Tag>" _btoa0_block "${_hagc_xml}")
if(_hagc_block STREQUAL "" OR _btoa0_block STREQUAL "")
  message(FATAL_ERROR "HagcColorSpace.xml no longer carries the tags section 10 copies")
endif()

# A BToA0Tag is allowed in a Display profile but is not a forward transform, so
# it cannot stand in for "A2B0 or matrix/TRC".  It used to be listed among the
# any-of alternatives, and C4 passed this profile.  The colorant tags are
# removed as well: C4's alternative set is any-of per TAG, so one matrix column
# tag alone would satisfy it whatever this test is about.
#
# THE CLASS IS FORCED BACK TO 'mntr'.  This borrows HdrMissingLutPair.xml for
# its shape, and the 23-09-2026 revision made that fixture a ColorSpace
# profile - whose rule table requires A2B0 and B2A0 outright, with no
# alternative set at all.  The defect under test here is specific to the
# Input and Display rules, where the LUT pair and the six matrix/TRC tags are
# alternatives to each other, so the profile has to be of one of those classes
# or the test asserts nothing about it.
file(READ "${ICCDEV_HDR_DIR}/HdrMissingLutPair.xml" _pairless_xml)
string(REPLACE "<ProfileDeviceClass>spac</ProfileDeviceClass>"
  "<ProfileDeviceClass>mntr</ProfileDeviceClass>" _pairless_xml "${_pairless_xml}")
if(NOT _pairless_xml MATCHES "<ProfileDeviceClass>mntr</ProfileDeviceClass>")
  message(FATAL_ERROR "section 10 could not set HdrMissingLutPair.xml to the Display class")
endif()
foreach(_colorant red green blue)
  string(REGEX REPLACE "<${_colorant}ColorantTag>.*</${_colorant}ColorantTag>" ""
    _pairless_xml "${_pairless_xml}")
endforeach()
if(_pairless_xml MATCHES "ColorantTag>")
  message(FATAL_ERROR "section 10 could not remove HdrMissingLutPair.xml's colorant tags")
endif()
string(REPLACE "<profileDescriptionTag>" "${_btoa0_block}\n\n    <profileDescriptionTag>"
  _btoa0_only_xml "${_pairless_xml}")
iccdev_from_xml("${_btoa0_only_xml}" "BToA0Only" _btoa0_only)
iccdev_run_pawg("${_btoa0_only}" _btoa0_only_report)
iccdev_expect("${_btoa0_only_report}" "\\[WARN[ \t]*\\][ \t]+C4[ \t]"
  "C4 accepted a Display profile whose only transform tag is a BToA0Tag")
iccdev_expect("${_btoa0_only_report}" "missing A2B0 or matrix/TRC transform"
  "C4 did not name the missing forward transform")

# And the allowance itself: a Display profile's BToA0Tag is not an extra tag.
iccdev_expect_not("${_hagc}" "outside the local class rule table: [^\n]*B2A0"
  "C5 reported a Display profile's BToA0Tag as outside its class")

# THE HAGC TAG IS NOW PERMITTED ON 'spac', and this expectation is inverted.
#
# PROPOSAL-ISSUE HDR-22.  The HAGC amendment's own class sentence says "Input
# or Display" and the HDR ColorSpace Profiles amendment does not amend it the
# way its 4.4 amends the cicpTag's 9.2.17 - yet 8.7.1.3 a) ranks the HAGC tag
# as the FIRST tone-mapping descriptor of an HDR ColorSpace Profile and
# 8.7.1.5 lists it among what such a profile "may additionally contain", and
# 8.7.1.5 fixes that profile's class as ColorSpace.  Ruled in favour of the
# HDR clauses: later, more specific, and the alternative makes the
# highest-ranked descriptor unusable in the only class the sub-class admits.
#
# Three passes have to agree on this, and a tag permitted by one and rejected
# by another is worse than either answer alone, so all three are pinned:
# CIccTagHagc::Validate() and CIccProfile::CheckTagExclusion() in the library
# (hagc-codec-roundtrip and the qa manifest), and PAWG C5 here.
file(READ "${ICCDEV_HDR_DIR}/HdrColorSpaceClass.xml" _spac_xml)
string(REPLACE "<profileDescriptionTag>" "${_hagc_block}\n\n    <profileDescriptionTag>"
  _spac_hagc_xml "${_spac_xml}")
iccdev_from_xml("${_spac_hagc_xml}" "ColorSpaceWithHagc" _spac_hagc)
iccdev_run_pawg("${_spac_hagc}" _spac_hagc_report)
iccdev_expect_not("${_spac_hagc_report}" "outside the local class rule table: [^\n]*HAGC"
  "C5 reported a HAGC tag in a ColorSpace-class profile, which 8.7.1.5 permits")
# The discriminator: C5 must still report it where no clause permits it.  An
# Output profile is outside all three of Input, Display and ColorSpace, so a
# build that simply stopped checking the class passes the assertion above and
# fails this one.
string(REPLACE "<ProfileDeviceClass>spac</ProfileDeviceClass>"
  "<ProfileDeviceClass>prtr</ProfileDeviceClass>" _prtr_hagc_xml "${_spac_hagc_xml}")
iccdev_from_xml("${_prtr_hagc_xml}" "OutputWithHagc" _prtr_hagc)
iccdev_run_pawg("${_prtr_hagc}" _prtr_hagc_report)
iccdev_expect("${_prtr_hagc_report}" "outside the local class rule table: [^\n]*HAGC"
  "C5 did not report a HAGC tag in an Output-class profile, which no clause permits")

# And section 9's other half, now that a non-Input, non-Display, non-ColorSpace
# profile exists here: H1 must name the class it actually found.  This is the
# shape that caught H1 printing "Display" for everything that was not Input.
iccdev_expect("${_prtr_hagc_report}" "Output profile, "
  "H1 did not name an Output-class profile's class")
iccdev_expect_not("${_prtr_hagc_report}" "Display profile, "
  "H1 described an Output-class profile as a Display profile")

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
