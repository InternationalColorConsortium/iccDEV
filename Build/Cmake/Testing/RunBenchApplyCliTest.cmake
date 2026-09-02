#################################################################################
# Command-line contract driver, introduced for iccBenchApply by #2254
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

# RunBenchApplyCliTest.cmake - runs one command-line tool once and asserts BOTH
# halves of the outcome: the exit status and what the tool said.
#
# Nothing here is specific to iccBenchApply; the file name records where the
# pattern started, not what it may drive.  #2268 reuses it for iccApplyToLink,
# because the defect it pins is that the two tools disagreed about the same
# intent code and the assertion has to be spelled identically on both to say so.
# The log label is therefore a parameter (LABEL) rather than a literal.
#
# #2254 exists because every defect it collects was an exit status that agreed
# with a wrong outcome. `-suite` with a chain ran the built-in table, ignored the
# profile the caller named, and exited 0. `-suite` alone was rejected outright.
# A run whose nine cases all SKIPped reported success, which is what made
# iccdev.apply-throughput vacuous.
#
# CTest cannot express "nonzero AND this message" on its own: WILL_FAIL passes on
# any nonzero exit -- including a crash, a usage error, or a missing shared
# library -- and PASS_REGULAR_EXPRESSION makes CTest ignore the return code
# entirely. Either one alone would pass against a tool that failed for the wrong
# reason, which is the same class of defect this file is testing for. Asserting
# both here is the only way to keep the assertion honest.
#
# Required -D args:
#   TOOL         - path to the executable under test
#   EXPECT_EXIT  - "zero" or "nonzero"
# Optional -D args:
#   TOOL_ARGS    - ';'-separated argument list passed to the tool
#   EXPECT_MATCH - regex that must appear in stdout or stderr
#   LABEL        - prefix for this driver's own log lines
#                  (default: bench-apply-cli, so existing callers are unchanged)

cmake_minimum_required(VERSION 3.18...3.29)

if(NOT DEFINED LABEL OR "${LABEL}" STREQUAL "")
  set(LABEL "bench-apply-cli")
endif()

foreach(_required TOOL EXPECT_EXIT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "[${LABEL}] ${_required} not set")
  endif()
endforeach()

if(NOT EXISTS "${TOOL}")
  message(FATAL_ERROR "[${LABEL}] tool not found: ${TOOL}")
endif()

if(NOT EXPECT_EXIT STREQUAL "zero" AND NOT EXPECT_EXIT STREQUAL "nonzero")
  message(FATAL_ERROR
    "[${LABEL}] EXPECT_EXIT must be 'zero' or 'nonzero', got '${EXPECT_EXIT}'")
endif()

# EXPECT_MATCH is mandatory for a negative test, enforced rather than advised.
# "nonzero" on its own is satisfied by any failure, and the ones most likely to
# occur are not the one being tested: on Windows a missing IccProfLib2.dll exits
# -1073741515, which is a perfectly ordinary nonzero status. A negative test
# without a message assertion would then pass on every Windows leg while proving
# nothing, which is the exact shape of defect #2254 collects. Making it a
# configuration error means the next such test cannot be written vacuously.
if(EXPECT_EXIT STREQUAL "nonzero"
   AND (NOT DEFINED EXPECT_MATCH OR "${EXPECT_MATCH}" STREQUAL ""))
  message(FATAL_ERROR
    "[${LABEL}] EXPECT_EXIT=nonzero requires EXPECT_MATCH: a bare"
    " nonzero exit does not distinguish the refusal under test from a crash"
    " or a missing runtime library")
endif()

execute_process(
  COMMAND "${TOOL}" ${TOOL_ARGS}
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE  _stderr
)

set(_output "${_stdout}${_stderr}")

message(STATUS "[${LABEL}] ${TOOL} ${TOOL_ARGS}")
message(STATUS "[${LABEL}] exit=${_result}")
message(STATUS "[${LABEL}] output:\n${_output}")

# A non-numeric RESULT_VARIABLE means the process could not be started at all --
# execute_process reports the reason as a string rather than a status. That is a
# narrower case than it looks: a process that starts and then dies for a missing
# DLL still yields a number. The EXPECT_MATCH requirement above is what covers
# that; this check only keeps a failure-to-launch from being read as a status.
if(NOT _result MATCHES "^-?[0-9]+$")
  message(FATAL_ERROR "[${LABEL}] tool did not run: ${_result}")
endif()

if(EXPECT_EXIT STREQUAL "zero" AND NOT _result EQUAL 0)
  message(FATAL_ERROR
    "[${LABEL}] expected success, got exit ${_result}")
endif()

if(EXPECT_EXIT STREQUAL "nonzero" AND _result EQUAL 0)
  message(FATAL_ERROR
    "[${LABEL}] expected a nonzero exit, got 0")
endif()

if(DEFINED EXPECT_MATCH AND NOT "${EXPECT_MATCH}" STREQUAL "")
  if(NOT _output MATCHES "${EXPECT_MATCH}")
    message(FATAL_ERROR
      "[${LABEL}] output did not match '${EXPECT_MATCH}'")
  endif()
endif()

message(STATUS "[${LABEL}] OK")
