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
#   EXPECT_MATCH - regex that must appear in stdout or stderr (run A only)
#   LABEL        - prefix for this driver's own log lines
#                  (default: bench-apply-cli, so existing callers are unchanged)
# Optional -D args for a two-run differential assertion (#2271):
#   COMPARE_ARGS    - a second ';'-separated argument list. When set, the tool is
#                     run a second time with it and the two runs are compared.
#   COMPARE_EXTRACT - regex with exactly ONE capture group, applied to both
#                     outputs. BOTH runs must match it or the test fails, so a
#                     regex that silently stops matching cannot degrade into a
#                     comparison of two empty strings.
#   COMPARE_MODE    - "differ" (default) or "same".
#
# The differential mode exists because the interesting assertions for #2271 are
# about a value that is not portable enough to write down. iccBenchApply prints
# an FNV-1a checksum over the raw bytes of the applied floats; the last bits of a
# colour transform legitimately differ between compilers and CPUs, so pinning a
# literal 0x7be0e6ba here would be a Windows and macOS flake waiting to happen.
# Comparing two runs of the SAME binary on the SAME machine asserts the thing
# that actually matters -- that the hint changed the result -- and is stable
# everywhere. EXPECT_MATCH cannot express it: the two codes differ by a hint, not
# by any word the tool prints.

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

# One invocation plus the status assertions that apply to every run. Factored out
# so the second, comparison run of a differential test is held to exactly the
# same rules as the first -- a comparison against a run that crashed would
# otherwise "pass" by differing (#2271).
function(icc_run_tool_once _args _tag _out_var)
  execute_process(
    COMMAND "${TOOL}" ${_args}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE  _stderr
  )

  set(_out "${_stdout}${_stderr}")

  message(STATUS "[${LABEL}] ${_tag}: ${TOOL} ${_args}")
  message(STATUS "[${LABEL}] ${_tag}: exit=${_result}")
  message(STATUS "[${LABEL}] ${_tag}: output:\n${_out}")

  # A non-numeric RESULT_VARIABLE means the process could not be started at all
  # -- execute_process reports the reason as a string rather than a status. That
  # is a narrower case than it looks: a process that starts and then dies for a
  # missing DLL still yields a number. The EXPECT_MATCH requirement above is what
  # covers that; this check only keeps a failure-to-launch from being read as a
  # status.
  if(NOT _result MATCHES "^-?[0-9]+$")
    message(FATAL_ERROR "[${LABEL}] ${_tag}: tool did not run: ${_result}")
  endif()

  if(EXPECT_EXIT STREQUAL "zero" AND NOT _result EQUAL 0)
    message(FATAL_ERROR
      "[${LABEL}] ${_tag}: expected success, got exit ${_result}")
  endif()

  if(EXPECT_EXIT STREQUAL "nonzero" AND _result EQUAL 0)
    message(FATAL_ERROR
      "[${LABEL}] ${_tag}: expected a nonzero exit, got 0")
  endif()

  set(${_out_var} "${_out}" PARENT_SCOPE)
endfunction()

icc_run_tool_once("${TOOL_ARGS}" "run" _output)

if(DEFINED EXPECT_MATCH AND NOT "${EXPECT_MATCH}" STREQUAL "")
  if(NOT _output MATCHES "${EXPECT_MATCH}")
    message(FATAL_ERROR
      "[${LABEL}] output did not match '${EXPECT_MATCH}'")
  endif()
endif()

if(DEFINED COMPARE_ARGS AND NOT "${COMPARE_ARGS}" STREQUAL "")
  if(NOT DEFINED COMPARE_EXTRACT OR "${COMPARE_EXTRACT}" STREQUAL "")
    message(FATAL_ERROR
      "[${LABEL}] COMPARE_ARGS requires COMPARE_EXTRACT: comparing whole"
      " outputs would compare the timing figures too, which differ on every"
      " run and would make every 'differ' assertion vacuously true")
  endif()

  if(NOT DEFINED COMPARE_MODE OR "${COMPARE_MODE}" STREQUAL "")
    set(COMPARE_MODE "differ")
  endif()
  if(NOT COMPARE_MODE STREQUAL "differ" AND NOT COMPARE_MODE STREQUAL "same")
    message(FATAL_ERROR
      "[${LABEL}] COMPARE_MODE must be 'differ' or 'same', got '${COMPARE_MODE}'")
  endif()

  icc_run_tool_once("${COMPARE_ARGS}" "compare" _compare_output)

  # Both extractions are mandatory. If the tool's output format changes so the
  # regex no longer matches, CMAKE_MATCH_1 would be left empty in both runs and a
  # "differ" test would fail loudly while a "same" test would pass having
  # measured nothing. Failing here means the second outcome cannot happen.
  string(REGEX MATCH "${COMPARE_EXTRACT}" _matched_a "${_output}")
  if("${_matched_a}" STREQUAL "")
    message(FATAL_ERROR
      "[${LABEL}] run: COMPARE_EXTRACT '${COMPARE_EXTRACT}' matched nothing")
  endif()
  set(_value_a "${CMAKE_MATCH_1}")

  string(REGEX MATCH "${COMPARE_EXTRACT}" _matched_b "${_compare_output}")
  if("${_matched_b}" STREQUAL "")
    message(FATAL_ERROR
      "[${LABEL}] compare: COMPARE_EXTRACT '${COMPARE_EXTRACT}' matched nothing")
  endif()
  set(_value_b "${CMAKE_MATCH_1}")

  message(STATUS "[${LABEL}] extracted run='${_value_a}' compare='${_value_b}'")

  if(COMPARE_MODE STREQUAL "differ" AND "${_value_a}" STREQUAL "${_value_b}")
    message(FATAL_ERROR
      "[${LABEL}] expected the two runs to differ, both gave '${_value_a}'")
  endif()

  if(COMPARE_MODE STREQUAL "same" AND NOT "${_value_a}" STREQUAL "${_value_b}")
    message(FATAL_ERROR
      "[${LABEL}] expected the two runs to agree, got"
      " '${_value_a}' and '${_value_b}'")
  endif()
endif()

message(STATUS "[${LABEL}] OK")
