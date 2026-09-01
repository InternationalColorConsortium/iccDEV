#################################################################################
# Device-link write-then-read contract driver, introduced for #2341
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

# RunDeviceLinkRoundTripTest.cmake - writes a device link with one tool, then
# reads it back with another and asserts the verdict the reader reports.
#
# The existing single-tool drivers cannot express this. RunBenchApplyCliTest
# asserts an exit status and a message from ONE process, and #2341 is precisely a
# defect that both of those agree with: CDevLinkWriter::begin() left the curve-set
# slots for input channels 2..n-1 NULL, CIccMpeCurveSet::Write() skips a NULL slot
# rather than failing, so the writer printed "LUT successfully written" and exited
# 0 while emitting a link every reader rejects. Measured on the unfixed tool, all
# four cases registered below report success at the CLI; only reading the file
# back separates them. An assertion on the writer alone is therefore vacuous here
# by construction, not by oversight.
#
# The verdict oracle is iccDumpProfile's validation report, which is the same
# string the corpus manifest is expressed in (Testing/qa-profile-manifest.tsv), so
# a link this driver calls valid is valid in the sense the rest of the suite
# already uses.
#
# The verdict ALONE is too coarse to be the whole assertion, which is worth
# spelling out because the first version of this file made exactly that mistake.
# "Parses and validates" is satisfied by a link that simply omits the structure
# under test: dropping the curve set entirely produces a perfectly valid device
# link, so a verdict-only suite stayed green through a mutation that deleted the
# feature. The reader is therefore invoked with ALL, which dumps tag contents, and
# EXPECT_DUMP/EXPECT_DUMP_COUNT/REJECT_DUMP assert the structure itself. The
# validation report is present in the same output, so this still costs one process.
#
# EXPECT_DUMP_COUNT is what makes the assertion exact rather than merely present:
# a per-channel structure is only correct if it appears once PER CHANNEL, and
# "at least one" would pass a link whose remaining channels were wrong or absent.
#
# Required -D args:
#   WRITER          - path to the device-link writer (iccApplyToLink)
#   WRITER_ARGS     - ';'-separated argument list; the FIRST element must be the
#                     output path, which is what gets read back
#   READER          - path to the validating reader (iccDumpProfile)
#   EXPECT_VERDICT  - regex the reader's report must match
# Optional -D args:
#   REJECT_VERDICT    - regex the reader's report must NOT match
#   EXPECT_DUMP       - regex that must appear in the tag dump
#   EXPECT_DUMP_COUNT - exact number of times EXPECT_DUMP must appear; when unset,
#                       EXPECT_DUMP need only appear at least once
#   REJECT_DUMP       - regex that must NOT appear in the tag dump
#   LABEL             - prefix for this driver's own log lines

cmake_minimum_required(VERSION 3.18...3.29)

if(NOT DEFINED LABEL OR "${LABEL}" STREQUAL "")
  set(LABEL "devicelink-roundtrip")
endif()

foreach(_required WRITER WRITER_ARGS READER EXPECT_VERDICT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "[${LABEL}] ${_required} not set")
  endif()
endforeach()

foreach(_tool WRITER READER)
  if(NOT EXISTS "${${_tool}}")
    message(FATAL_ERROR "[${LABEL}] ${_tool} not found: ${${_tool}}")
  endif()
endforeach()

if(DEFINED EXPECT_DUMP_COUNT AND NOT "${EXPECT_DUMP_COUNT}" STREQUAL "")
  if(NOT EXPECT_DUMP_COUNT MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "[${LABEL}] EXPECT_DUMP_COUNT must be a non-negative integer,"
      " got '${EXPECT_DUMP_COUNT}'")
  endif()
  if(NOT DEFINED EXPECT_DUMP OR "${EXPECT_DUMP}" STREQUAL "")
    message(FATAL_ERROR
      "[${LABEL}] EXPECT_DUMP_COUNT requires EXPECT_DUMP: a count with nothing"
      " to count is not an assertion")
  endif()
endif()

list(GET WRITER_ARGS 0 _output)

# Re-created at run time rather than relying on the configure-time mkdir, matching
# every sibling driver: the directory can be wiped between configure and test, and
# without it the writer fails to open its output and the run dies pointing at the
# tool instead of at the missing directory.
get_filename_component(_output_dir "${_output}" DIRECTORY)
if(_output_dir)
  file(MAKE_DIRECTORY "${_output_dir}")
endif()

# Remove any output left by an earlier run before writing. Without this a writer
# that silently produced nothing would be validated against the previous run's
# file, which is a stale-artifact pass of exactly the kind #2254 collects.
file(REMOVE "${_output}")
if(EXISTS "${_output}")
  message(FATAL_ERROR "[${LABEL}] could not clear stale output: ${_output}")
endif()

execute_process(
  COMMAND "${WRITER}" ${WRITER_ARGS}
  RESULT_VARIABLE _write_result
  OUTPUT_VARIABLE _write_stdout
  ERROR_VARIABLE  _write_stderr
)
set(_write_output "${_write_stdout}${_write_stderr}")

message(STATUS "[${LABEL}] ${WRITER} ${WRITER_ARGS}")
message(STATUS "[${LABEL}] writer exit=${_write_result}")
message(STATUS "[${LABEL}] writer output:\n${_write_output}")

if(NOT _write_result MATCHES "^-?[0-9]+$")
  message(FATAL_ERROR "[${LABEL}] writer did not run: ${_write_result}")
endif()

if(NOT _write_result EQUAL 0)
  message(FATAL_ERROR "[${LABEL}] writer failed with exit ${_write_result}")
endif()

# Asserted so the test still means something if the writer is ever changed to
# refuse this input: a refusal that wrote no file would otherwise be caught only
# by the EXISTS check below, with a message pointing at the wrong tool.
if(NOT _write_output MATCHES "LUT successfully written")
  message(FATAL_ERROR
    "[${LABEL}] writer exited 0 without reporting a successful write")
endif()

if(NOT EXISTS "${_output}")
  message(FATAL_ERROR
    "[${LABEL}] writer reported success but produced no file: ${_output}")
endif()

# ALL dumps tag contents as well as the header and validation report, so the one
# invocation feeds both the verdict assertions and the structural ones below.
execute_process(
  COMMAND "${READER}" -v "${_output}" ALL
  RESULT_VARIABLE _read_result
  OUTPUT_VARIABLE _read_stdout
  ERROR_VARIABLE  _read_stderr
)
set(_read_output "${_read_stdout}${_read_stderr}")

message(STATUS "[${LABEL}] reader exit=${_read_result}")
message(STATUS "[${LABEL}] reader output:\n${_read_output}")

if(NOT _read_result MATCHES "^-?[0-9]+$")
  message(FATAL_ERROR "[${LABEL}] reader did not run: ${_read_result}")
endif()

if(NOT _read_result EQUAL 0)
  message(FATAL_ERROR "[${LABEL}] reader failed with exit ${_read_result}")
endif()

# REJECT_VERDICT is checked before EXPECT_VERDICT so the failure message names the
# problem the file actually has rather than the string that was missing.
if(DEFINED REJECT_VERDICT AND NOT "${REJECT_VERDICT}" STREQUAL "")
  if(_read_output MATCHES "${REJECT_VERDICT}")
    message(FATAL_ERROR
      "[${LABEL}] reader reported '${REJECT_VERDICT}' for a link the writer"
      " called successful: ${_output}")
  endif()
endif()

if(NOT _read_output MATCHES "${EXPECT_VERDICT}")
  message(FATAL_ERROR
    "[${LABEL}] reader verdict did not match '${EXPECT_VERDICT}': ${_output}")
endif()

# Structural assertions. A valid verdict says the file parses; these say the
# structure under test is actually in it, and in the right quantity.
if(DEFINED REJECT_DUMP AND NOT "${REJECT_DUMP}" STREQUAL "")
  if(_read_output MATCHES "${REJECT_DUMP}")
    message(FATAL_ERROR
      "[${LABEL}] tag dump contains '${REJECT_DUMP}', which this case requires"
      " to be absent: ${_output}")
  endif()
endif()

if(DEFINED EXPECT_DUMP AND NOT "${EXPECT_DUMP}" STREQUAL "")
  if(DEFINED EXPECT_DUMP_COUNT AND NOT "${EXPECT_DUMP_COUNT}" STREQUAL "")
    string(REGEX MATCHALL "${EXPECT_DUMP}" _dump_matches "${_read_output}")
    list(LENGTH _dump_matches _dump_match_count)
    if(NOT _dump_match_count EQUAL EXPECT_DUMP_COUNT)
      message(FATAL_ERROR
        "[${LABEL}] tag dump matched '${EXPECT_DUMP}' ${_dump_match_count}"
        " time(s), expected exactly ${EXPECT_DUMP_COUNT}: ${_output}")
    endif()
  elseif(NOT _read_output MATCHES "${EXPECT_DUMP}")
    message(FATAL_ERROR
      "[${LABEL}] tag dump did not match '${EXPECT_DUMP}': ${_output}")
  endif()
endif()

message(STATUS "[${LABEL}] OK")
