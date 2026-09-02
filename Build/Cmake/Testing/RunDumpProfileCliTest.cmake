#################################################################################
# iccDumpProfile command-line contract driver for #2248
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

# RunDumpProfileCliTest.cmake - runs iccDumpProfile once and asserts BOTH the exit
# status and what the tool reported.
#
# The oracle for #2248 is the verbosity iccDumpProfile says it is using, read from
# its own --diag line, rather than the size of the dump. Line counts were the
# obvious choice and are the wrong one: they are a proxy that depends on which
# profile is passed, and most tag dumps are byte-identical across the whole
# verbosity range -- 'desc' is 49 lines at both 0 and 100 -- so a count-based
# assertion would silently pass on the majority of fixtures while claiming to pin
# the default. --diag reports the parsed value directly, so the assertion fails
# for exactly one reason.
#
# Asserting the status alone would not do either: the defect being pinned never
# changed the exit code. Every invocation below exited 0 both before and after the
# fix, and printed a profile either way. Only the value differed.
#
# Required -D args:
#   TOOL         - path to the iccDumpProfile executable
#   EXPECT_MATCH - regex that must appear in stdout or stderr
# Optional -D args:
#   TOOL_ARGS    - ';'-separated argument list passed to the tool
#   WORKDIR      - directory to run the tool from (default: current directory)

cmake_minimum_required(VERSION 3.18...3.29)

foreach(_required TOOL EXPECT_MATCH)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "[dumpprofile-cli] ${_required} not set")
  endif()
endforeach()

if(NOT EXISTS "${TOOL}")
  message(FATAL_ERROR "[dumpprofile-cli] tool not found: ${TOOL}")
endif()

set(_workdir_arg)
if(DEFINED WORKDIR AND NOT "${WORKDIR}" STREQUAL "")
  if(NOT IS_DIRECTORY "${WORKDIR}")
    message(FATAL_ERROR "[dumpprofile-cli] WORKDIR not a directory: ${WORKDIR}")
  endif()
  set(_workdir_arg WORKING_DIRECTORY "${WORKDIR}")
endif()

execute_process(
  COMMAND "${TOOL}" ${TOOL_ARGS}
  ${_workdir_arg}
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE  _stderr
)

set(_output "${_stdout}${_stderr}")

message(STATUS "[dumpprofile-cli] ${TOOL} ${TOOL_ARGS}")
message(STATUS "[dumpprofile-cli] exit=${_result}")
message(STATUS "[dumpprofile-cli] output:\n${_output}")

# A non-numeric RESULT_VARIABLE means the process never started; execute_process
# reports that as a string rather than a status. Checked separately so a
# failure-to-launch is not read as an exit code.
if(NOT _result MATCHES "^-?[0-9]+$")
  message(FATAL_ERROR "[dumpprofile-cli] tool did not run: ${_result}")
endif()

# Every case here is a successful dump, so the status is pinned to 0 outright.
#
# Do NOT read that check as the guard against a missing or truncated fixture: it
# is not one. iccDumpProfile returns 0 whether or not the profile parsed --
# nValid stays 0 unless -v was given -- so `iccDumpProfile --diag nosuch.icc`
# prints "Unable to parse ... as ICC profile!" and still exits 0, and three of
# the four registered cases take that no--v path. What actually catches a bad
# fixture is EXPECT_MATCH: the "Verbosity:" line is emitted only on the branch
# where the profile opened, so a fixture that failed to copy takes the match from
# present to absent. The status check is retained for the -v case and as a cheap
# guard against a crash or a signal, not as fixture validation.
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "[dumpprofile-cli] expected success, got exit ${_result}")
endif()

if(NOT _output MATCHES "${EXPECT_MATCH}")
  message(FATAL_ERROR
    "[dumpprofile-cli] output did not match '${EXPECT_MATCH}'")
endif()

message(STATUS "[dumpprofile-cli] OK")
