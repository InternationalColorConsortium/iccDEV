#################################################################################
# Windows DLL data-linkage regression for the eight exported IccProfLib globals
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################
#
# #2154 (from #1888). WINDOWS_EXPORT_ALL_SYMBOLS auto-exports functions but not
# data, so before the eight exported globals carried ICCPROFLIB_DATA_API a
# consumer of IccProfLib2.dll referencing one of them failed to link with
# LNK2019/LNK2001 while every function in the same header thunked normally.
#
# The tree's existing answer is to avoid the DLL: three tools and
# iccdev.proflib-exported-data-linkage link IccProfLib2-static on Windows shared
# builds instead. That workaround means no test exercised data linkage against
# the DLL at all, so the defect could neither fail nor be shown fixed.
#
# This builds the consumer OUT OF TREE, configured and compiled at test time.
# That is deliberate and is the only shape that works: a link error in a target
# built by the main tree fails the BUILD, not a test, so it could never be
# expressed as a failing CTest.  Modelled on RunWindowsSharedExportTest.cmake
# (iccdev.issue-987-shared-mpe-export), which exercises a member FUNCTION across
# the same boundary.

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_REPO_ROOT
  ICCDEV_BUILD_DIR
  ICCDEV_CONFIG
  ICCDEV_ICCPROFLIB_DLL
  ICCDEV_ICCPROFLIB_IMPLIB
  ICCDEV_GENERATOR
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

if(NOT EXISTS "${ICCDEV_ICCPROFLIB_DLL}")
  message(FATAL_ERROR "IccProfLib DLL not found: ${ICCDEV_ICCPROFLIB_DLL}")
endif()

if(NOT EXISTS "${ICCDEV_ICCPROFLIB_IMPLIB}")
  message(FATAL_ERROR "IccProfLib import library not found: ${ICCDEV_ICCPROFLIB_IMPLIB}")
endif()

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_log_file "${ICCDEV_TEST_OUTDIR}/output.log")

# Same %TEMP% relocation and path-digest rationale as
# RunWindowsSharedExportTest.cmake: the nested configure pushes several levels
# deeper and the 260-character limit still applies, and the digest keeps two
# build trees at one configuration from clearing each other's sources.
set(_consumer_root "${ICCDEV_TEST_OUTDIR}/consumer-work")
if(DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
  string(SHA256 _consumer_hash "${ICCDEV_BUILD_DIR}|${ICCDEV_TEST_NAME}|${ICCDEV_CONFIG}")
  string(SUBSTRING "${_consumer_hash}" 0 12 _consumer_hash)
  set(_consumer_root "$ENV{TEMP}/iccdev-expdata-${_consumer_hash}")
endif()
set(_consumer_src_dir "${_consumer_root}/consumer")
set(_consumer_build_dir "${_consumer_root}/consumer-build")
file(REMOVE_RECURSE "${_consumer_root}")
file(MAKE_DIRECTORY "${_consumer_src_dir}")

# ICCPROFLIBDLL_DATA_IMPORTS only -- deliberately NOT ICCPROFLIBDLL_IMPORTS.
# That mirrors exactly what a real consumer receives: the shared IccProfLib
# target carries the data import define as INTERFACE, and nothing sets the
# whole-API one. Setting both here would test a configuration no consumer uses.
file(WRITE "${_consumer_src_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.18...3.29)
project(IccDevExportedDataConsumer LANGUAGES CXX)

add_library(IccProfLib2Runtime SHARED IMPORTED GLOBAL)
set_target_properties(IccProfLib2Runtime PROPERTIES
  IMPORTED_IMPLIB "${ICCPROFLIB_IMPLIB}"
  IMPORTED_LOCATION "${ICCPROFLIB_DLL}"
  INTERFACE_COMPILE_DEFINITIONS "ICCPROFLIBDLL_DATA_IMPORTS"
  INTERFACE_INCLUDE_DIRECTORIES "${ICCDEV_BUILD_DIR}/IccProfLib;${ICCDEV_REPO_ROOT}/IccProfLib;${ICCDEV_REPO_ROOT}"
)

add_executable(iccdev-exported-data-consumer iccdev-exported-data-consumer.cpp)
target_compile_features(iccdev-exported-data-consumer PRIVATE cxx_std_17)
target_link_libraries(iccdev-exported-data-consumer PRIVATE IccProfLib2Runtime)

add_custom_command(TARGET iccdev-exported-data-consumer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ICCPROFLIB_DLL}"
    "$<TARGET_FILE_DIR:iccdev-exported-data-consumer>"
)
]=])

file(WRITE "${_consumer_src_dir}/iccdev-exported-data-consumer.cpp" [=[
// Reference every one of the eight exported IccProfLib globals across the DLL
// boundary. The LINK is the assertion this test exists for; the value checks
// then confirm the data actually crossed intact rather than resolving to a
// second copy. Values are pinned by iccdev.proflib-exported-data-linkage.
#include "IccUtil.h"
#include "IccSolve.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static int gFailures = 0;

static void expectNear(const char *what, double got, double want)
{
  if (std::fabs(got - want) > 1e-4) {
    std::fprintf(stderr, "FAIL %s: got %f want %f\n", what, got, want);
    ++gFailures;
  }
}

static void expectText(const char *what, const char *got, const char *want)
{
  if (!got || std::strcmp(got, want) != 0) {
    std::fprintf(stderr, "FAIL %s: got '%s' want '%s'\n", what, got ? got : "(null)", want);
    ++gFailures;
  }
}

int main()
{
  expectNear("icD50XYZ[0]", icD50XYZ[0], 0.9642);
  expectNear("icD50XYZ[1]", icD50XYZ[1], 1.0000);
  expectNear("icD50XYZ[2]", icD50XYZ[2], 0.8249);

  expectNear("icD50XYZxx[0]", icD50XYZxx[0], 96.42);
  expectNear("icD50XYZxx[1]", icD50XYZxx[1], 100.00);
  expectNear("icD50XYZxx[2]", icD50XYZxx[2], 82.49);

  expectText("icMsgValidateWarning", icMsgValidateWarning, "Warning! - ");
  expectText("icMsgValidateNonCompliant", icMsgValidateNonCompliant, "NonCompliant! - ");
  expectText("icMsgValidateCriticalError", icMsgValidateCriticalError, "Error! - ");
  expectText("icMsgValidateInformation", icMsgValidateInformation, "Information - ");

  if (!g_pIccMatrixSolver) {
    std::fprintf(stderr, "FAIL g_pIccMatrixSolver is null\n");
    ++gFailures;
  }
  if (!g_pIccMatrixInverter) {
    std::fprintf(stderr, "FAIL g_pIccMatrixInverter is null\n");
    ++gFailures;
  }

  if (gFailures) {
    std::fprintf(stderr, "exported-data-consumer: %d failure(s)\n", gFailures);
    return 1;
  }

  std::printf("exported-data-consumer: all 8 globals linked and matched\n");
  return 0;
}
]=])

set(_configure_args
  -S "${_consumer_src_dir}"
  -B "${_consumer_build_dir}"
  -G "${ICCDEV_GENERATOR}"
  "-DICCDEV_REPO_ROOT=${ICCDEV_REPO_ROOT}"
  "-DICCDEV_BUILD_DIR=${ICCDEV_BUILD_DIR}"
  "-DICCPROFLIB_IMPLIB=${ICCDEV_ICCPROFLIB_IMPLIB}"
  "-DICCPROFLIB_DLL=${ICCDEV_ICCPROFLIB_DLL}"
)
if(DEFINED ICCDEV_GENERATOR_PLATFORM AND NOT "${ICCDEV_GENERATOR_PLATFORM}" STREQUAL "")
  list(APPEND _configure_args -A "${ICCDEV_GENERATOR_PLATFORM}")
endif()
if(DEFINED ICCDEV_CXX_COMPILER AND NOT "${ICCDEV_CXX_COMPILER}" STREQUAL "")
  list(APPEND _configure_args "-DCMAKE_CXX_COMPILER=${ICCDEV_CXX_COMPILER}")
endif()
if(NOT ICCDEV_GENERATOR MATCHES "Visual Studio|Xcode|Multi-Config")
  list(APPEND _configure_args "-DCMAKE_BUILD_TYPE=${ICCDEV_CONFIG}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${_configure_args}
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${_consumer_build_dir}" --config "${ICCDEV_CONFIG}" --parallel
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_stdout
  ERROR_VARIABLE _build_stderr
)

set(_consumer_exe "${_consumer_build_dir}/iccdev-exported-data-consumer.exe")
if(NOT EXISTS "${_consumer_exe}" AND NOT "${ICCDEV_CONFIG}" STREQUAL "")
  set(_consumer_exe "${_consumer_build_dir}/${ICCDEV_CONFIG}/iccdev-exported-data-consumer.exe")
endif()

set(_consumer_result "not run")
set(_consumer_stdout "")
set(_consumer_stderr "")
if(_build_result EQUAL 0 AND EXISTS "${_consumer_exe}")
  get_filename_component(_dll_dir "${ICCDEV_ICCPROFLIB_DLL}" DIRECTORY)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env
        "PATH=${_dll_dir};$ENV{PATH}"
        "${_consumer_exe}"
    RESULT_VARIABLE _consumer_result
    OUTPUT_VARIABLE _consumer_stdout
    ERROR_VARIABLE _consumer_stderr
  )
endif()

file(WRITE "${_log_file}"
  "CTest test: ${ICCDEV_TEST_NAME}\n"
  "Configuration: ${ICCDEV_CONFIG}\n"
  "DLL: ${ICCDEV_ICCPROFLIB_DLL}\n"
  "Import library: ${ICCDEV_ICCPROFLIB_IMPLIB}\n\n"
  "----- consumer configure stdout -----\n${_configure_stdout}\n"
  "----- consumer configure stderr -----\n${_configure_stderr}\n"
  "----- consumer build stdout -----\n${_build_stdout}\n"
  "----- consumer build stderr -----\n${_build_stderr}\n"
  "----- consumer stdout -----\n${_consumer_stdout}\n"
  "----- consumer stderr -----\n${_consumer_stderr}\n"
)
message(STATUS "Wrote ${ICCDEV_TEST_NAME} log to ${_log_file}")

if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR "Consumer configure failed with ${_configure_result}; see ${_log_file}")
endif()

# The expected pre-fix failure mode. Name it explicitly so a future reader does
# not have to rediscover that LNK2019 here means exported data, not a missing
# function: WINDOWS_EXPORT_ALL_SYMBOLS covers functions and never covered data.
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "Consumer failed to link against ${ICCDEV_ICCPROFLIB_DLL} (result ${_build_result}).\n"
    "If this is LNK2019/LNK2001 on one of the eight exported globals, the "
    "ICCPROFLIB_DATA_API dllexport/dllimport annotation has been lost -- see "
    "IccProfLib/IccProfLibConf.h and Build/Cmake/IccProfLib/CMakeLists.txt (#2154).\n"
    "See ${_log_file}")
endif()

if(NOT EXISTS "${_consumer_exe}")
  message(FATAL_ERROR "Consumer executable not found: ${_consumer_exe}; see ${_log_file}")
endif()

if(NOT _consumer_result EQUAL 0)
  message(FATAL_ERROR "Consumer exited with ${_consumer_result}; see ${_log_file}")
endif()

if(NOT _consumer_stdout MATCHES "all 8 globals linked and matched")
  message(FATAL_ERROR "Consumer did not confirm all eight globals; see ${_log_file}")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
