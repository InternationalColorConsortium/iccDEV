#################################################################################
# IccProfLib exported-global definition audit for #1897
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

# RunProfLibExportedGlobalDefinitionsTest.cmake - driver for the exported-global
# definition audit.
#
# #1897: IccUtil.h declared `extern ICCPROFLIB_API CIccInfo icInfo;` from the
# initial 2015 import and nothing ever defined it. Because nothing in-tree used
# it, no build ever tried to resolve it, so ten years of green CI said nothing
# about it -- the failure was reserved for the first downstream consumer to
# write `icInfo.GetTagSigName(...)` and get an unresolved external.
#
# A test that referenced the symbol could not catch this: referencing an
# undefined symbol does not fail an assertion, it fails the link, so the test
# would not build rather than fail. And a test that hard-codes the list of
# globals to check only ever covers the list its author knew about -- exactly
# the drift that let icInfo sit apart from its eight siblings unnoticed.
#
# So this audit derives the list from the headers instead of restating it:
#
#   1. scan the given headers for exported-global declarations, accepting both
#      macro orderings in use -- `ICCPROFLIB_API extern <type> <name>;` (the
#      eight live globals) and `extern ICCPROFLIB_API <type> <name>;` (which is
#      how icInfo was spelled, and is itself a sign it was never touched when
#      the others were);
#   2. read the symbol table of the library those headers describe;
#   3. fail naming any declared global that the library does not define.
#
# A new dangling declaration is therefore caught by adding it, without anyone
# remembering to update this test.
#
# Required -D args:
#   HEADERS  - ';'-separated list of header files to scan
#   LIBRARY  - path to the built IccProfLib (shared or static)
#   NM_TOOL  - path to nm (GNU, llvm or cctools; all three are handled)

# Matches Build/Cmake/CMakeLists.txt. Required rather than inherited: a script
# run with `cmake -P` starts with no policy settings from the project, and the
# `if(NOT <value> IN_LIST <list>)` form used below needs CMP0057 NEW -- without
# a version floor it is not merely unset but a hard "Unknown arguments" error.
cmake_minimum_required(VERSION 3.18...3.29)

foreach(_required HEADERS LIBRARY NM_TOOL)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "[proflib-exported-global-definitions] ${_required} not set")
  endif()
endforeach()

if(NOT EXISTS "${LIBRARY}")
  message(FATAL_ERROR
    "[proflib-exported-global-definitions] library not found: ${LIBRARY}")
endif()

# ---------------------------------------------------------------------------
# 1. Collect the declarations.
#
# Matched on the ICCPROFLIB_API + extern pair in either order. `extern "C" {`
# is excluded by requiring the macro, and function declarations are excluded by
# requiring `extern` -- IccProfLib marks exported functions with the macro
# alone, so only data carries both.
# ---------------------------------------------------------------------------
set(_declared "")
set(_declared_where "")

foreach(_header IN LISTS HEADERS)
  if(NOT EXISTS "${_header}")
    message(FATAL_ERROR
      "[proflib-exported-global-definitions] header not found: ${_header}")
  endif()

  file(STRINGS "${_header}" _lines)
  get_filename_component(_header_name "${_header}" NAME)

  foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "^[ \t]*(ICCPROFLIB_API[ \t]+extern|extern[ \t]+ICCPROFLIB_API)[ \t]+(.+)$")
      continue()
    endif()
    set(_decl "${CMAKE_MATCH_2}")

    # Reduce "const char* icMsgValidateInformation;" and
    # "icFloatNumber icD50XYZ[3];" to the declared name: drop everything from
    # the array subscript or the terminator, turn pointer/reference punctuation
    # into separators, and take the last identifier that remains.
    string(REGEX REPLACE "[;\\[].*$" "" _decl "${_decl}")
    string(REGEX REPLACE "[*&]" " " _decl "${_decl}")
    string(STRIP "${_decl}" _decl)
    if(NOT _decl MATCHES "([A-Za-z_][A-Za-z0-9_]*)$")
      continue()
    endif()

    list(APPEND _declared "${CMAKE_MATCH_1}")
    list(APPEND _declared_where "${CMAKE_MATCH_1} (${_header_name})")
  endforeach()
endforeach()

list(REMOVE_DUPLICATES _declared)
list(LENGTH _declared _declared_count)

# The scan is the part of this test most likely to break silently: a header
# reformat that the regex above stops matching would leave _declared empty and
# every assertion below would pass vacuously. Refuse to report success on a
# scan that found implausibly little, and name the control symbol so the next
# reader knows which invariant was being relied on.
if(_declared_count LESS 8)
  message(FATAL_ERROR
    "[proflib-exported-global-definitions] scanned ${_declared_count} exported "
    "global declaration(s) in the headers, expected at least 8 -- the "
    "declaration syntax has changed and this audit is no longer matching it. "
    "Update the regex in RunProfLibExportedGlobalDefinitionsTest.cmake.")
endif()

# ---------------------------------------------------------------------------
# 2. Read the library's symbol table.
#
# Both the dynamic table (-D, GNU/llvm only) and the full one are queried and
# the results pooled: a shared build may be stripped of everything but the
# dynamic table, a static archive has no dynamic table at all, and cctools nm
# rejects -D outright. Whichever invocation is meaningless here simply
# contributes nothing, so no per-platform branching is needed.
# ---------------------------------------------------------------------------
set(_symbols "")

foreach(_nm_args "-DC" "-C")
  execute_process(
    COMMAND "${NM_TOOL}" ${_nm_args} "${LIBRARY}"
    OUTPUT_VARIABLE _nm_out
    ERROR_VARIABLE _nm_err
    RESULT_VARIABLE _nm_rc
  )
  if(_nm_rc EQUAL 0)
    string(APPEND _symbols "${_nm_out}")
  endif()
endforeach()

if("${_symbols}" STREQUAL "")
  message(FATAL_ERROR
    "[proflib-exported-global-definitions] ${NM_TOOL} produced no symbols for "
    "${LIBRARY} -- cannot audit. Last error was: ${_nm_err}")
endif()

# Keep only names the library actually DEFINES. nm prints one entry per symbol
# as "<address> <type> <name>", where the type letter is upper case for a
# defined symbol; 'U' (undefined), 'u', 'w' and 'v' (weak/undefined references)
# mean the library merely mentions the symbol. Counting those as definitions
# would defeat the whole audit -- an undefined reference is precisely what a
# dangling declaration produces.
string(REPLACE "\n" ";" _symbol_lines "${_symbols}")
set(_defined "")

foreach(_sym_line IN LISTS _symbol_lines)
  if(NOT _sym_line MATCHES "^[0-9a-fA-F]* +([A-Za-z]) +(.+)$")
    continue()
  endif()
  set(_type "${CMAKE_MATCH_1}")
  set(_name "${CMAKE_MATCH_2}")
  if(_type MATCHES "^[Uuwv]$")
    continue()
  endif()
  string(STRIP "${_name}" _name)

  # Record the name in every spelling a supported toolchain can emit, because
  # the comparison below is exact and these differ per object format:
  #
  #   * Mach-O prefixes every symbol with an underscore, so what is icD50XYZ in
  #     an ELF .so is _icD50XYZ in a .dylib. Matching only the ELF spelling
  #     makes the macOS leg -- which IS in the PR matrix -- fail the control
  #     check and report the audit as broken on every run.
  #   * a USEICCDEVNAMESPACE build (IccProfLibConf.h:65, off by default) wraps
  #     these in namespace iccDEV, which nm -C demangles to iccDEV::icD50XYZ.
  #
  # Only that one namespace prefix is stripped, never a generic "everything up
  # to the last ::". Dropping any qualifier would let an unrelated class member
  # -- some CFoo::icInfo -- satisfy a lookup for a global named icInfo and turn
  # a real dangling declaration into a silent pass.
  set(_variants "${_name}")
  if(_name MATCHES "^_(.+)$")
    list(APPEND _variants "${CMAKE_MATCH_1}")
  endif()
  foreach(_variant IN LISTS _variants)
    list(APPEND _defined "${_variant}")
    if(_variant MATCHES "^iccDEV::(.+)$")
      list(APPEND _defined "${CMAKE_MATCH_1}")
    endif()
  endforeach()
endforeach()

list(REMOVE_DUPLICATES _defined)

# ---------------------------------------------------------------------------
# 3. Compare, and prove the comparison works before trusting it.
#
# icMsgValidateWarning is used as the control: it is one of the eight globals
# known to be both declared and defined, so if THIS lookup fails the fault is
# in the audit (wrong library, stripped binary, unparsed nm format) and every
# "missing" verdict below would be a false alarm. Distinguishing the two is the
# difference between reporting a defect and reporting a broken test.
# ---------------------------------------------------------------------------
if(NOT "icMsgValidateWarning" IN_LIST _defined)
  message(FATAL_ERROR
    "[proflib-exported-global-definitions] control symbol icMsgValidateWarning "
    "is not defined in ${LIBRARY}. The audit itself is broken -- not reporting "
    "the declarations below as dangling. Check that LIBRARY points at "
    "IccProfLib and that ${NM_TOOL} can read it.")
endif()

set(_missing "")
foreach(_name IN LISTS _declared)
  if(NOT "${_name}" IN_LIST _defined)
    list(APPEND _missing "${_name}")
  endif()
endforeach()

if(_missing)
  string(REPLACE ";" "\n  " _missing_text "${_missing}")
  message(FATAL_ERROR
    "[proflib-exported-global-definitions] FAIL: the following global(s) are "
    "declared ICCPROFLIB_API extern in the public headers but defined nowhere "
    "in ${LIBRARY}:\n  ${_missing_text}\n"
    "Any consumer that references one gets an unresolved external on every "
    "platform. Either define it in the corresponding .cpp or remove the "
    "declaration -- see #1897, which removed icInfo for the same reason.")
endif()

message(STATUS
  "[proflib-exported-global-definitions] ${_declared_count} exported global "
  "declaration(s) scanned, all defined in ${LIBRARY}")
