#################################################################################
# Windows shared-library export regression for issue #987
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

# Run the issue-987 Windows shared-library export regression.

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
set(_exports_log "${ICCDEV_TEST_OUTDIR}/iccproflib-exports.txt")
set(_consumer_root "${ICCDEV_TEST_OUTDIR}/consumer-work")
if(DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
  set(_consumer_root "$ENV{TEMP}/iccdev-issue-987-shared-mpe-export-${ICCDEV_CONFIG}")
endif()
set(_consumer_src_dir "${_consumer_root}/consumer")
set(_consumer_build_dir "${_consumer_root}/consumer-build")
file(REMOVE_RECURSE "${_consumer_root}")
file(MAKE_DIRECTORY "${_consumer_src_dir}")

set(_compiler_id "")
if(DEFINED ICCDEV_CXX_COMPILER_ID)
  set(_compiler_id "${ICCDEV_CXX_COMPILER_ID}")
endif()

if("${_compiler_id}" STREQUAL "MSVC")
  set(_vswhere "")
  foreach(_vswhere_candidate IN ITEMS
      "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer/vswhere.exe")
    if(EXISTS "${_vswhere_candidate}")
      set(_vswhere "${_vswhere_candidate}")
      break()
    endif()
  endforeach()
  if("${_vswhere}" STREQUAL "")
    message(FATAL_ERROR "vswhere.exe not found")
  endif()

  execute_process(
    COMMAND
      "${_vswhere}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    OUTPUT_VARIABLE _vs_path
    ERROR_VARIABLE _vswhere_stderr
    RESULT_VARIABLE _vswhere_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT _vswhere_result EQUAL 0 OR "${_vs_path}" STREQUAL "")
    message(FATAL_ERROR "Unable to locate Visual Studio C++ tools: ${_vswhere_stderr}")
  endif()

  file(GLOB _dumpbin_candidates "${_vs_path}/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe")
  list(SORT _dumpbin_candidates)
  list(REVERSE _dumpbin_candidates)
  set(_dumpbin "")
  list(LENGTH _dumpbin_candidates _dumpbin_count)
  if(_dumpbin_count GREATER 0)
    list(GET _dumpbin_candidates 0 _dumpbin)
  endif()
  if(NOT EXISTS "${_dumpbin}")
    find_program(_dumpbin dumpbin)
  endif()
  if(NOT _dumpbin)
    message(FATAL_ERROR "dumpbin.exe not found")
  endif()

  execute_process(
    COMMAND
      "${_dumpbin}" /exports "${ICCDEV_ICCPROFLIB_DLL}"
    RESULT_VARIABLE _exports_result
    OUTPUT_VARIABLE _exports_stdout
    ERROR_VARIABLE _exports_stderr
  )
  set(_exports_command "dumpbin /exports")
else()
  set(_objdump "")
  if(DEFINED ICCDEV_OBJDUMP AND NOT "${ICCDEV_OBJDUMP}" STREQUAL "" AND EXISTS "${ICCDEV_OBJDUMP}")
    set(_objdump "${ICCDEV_OBJDUMP}")
  else()
    set(_objdump_hints)
    if(DEFINED ICCDEV_CXX_COMPILER AND NOT "${ICCDEV_CXX_COMPILER}" STREQUAL "")
      get_filename_component(_compiler_dir "${ICCDEV_CXX_COMPILER}" DIRECTORY)
      list(APPEND _objdump_hints "${_compiler_dir}")
    endif()
    find_program(_objdump_candidate NAMES llvm-objdump objdump HINTS ${_objdump_hints})
    if(_objdump_candidate)
      set(_objdump "${_objdump_candidate}")
    endif()
  endif()
  if(NOT _objdump)
    message(FATAL_ERROR "objdump or llvm-objdump not found")
  endif()

  execute_process(
    COMMAND
      "${_objdump}" -p "${ICCDEV_ICCPROFLIB_DLL}"
    RESULT_VARIABLE _exports_result
    OUTPUT_VARIABLE _exports_stdout
    ERROR_VARIABLE _exports_stderr
  )
  set(_exports_command "objdump -p")
endif()

if(NOT _exports_result EQUAL 0)
  message(FATAL_ERROR "${_exports_command} failed with ${_exports_result}: ${_exports_stderr}")
endif()
file(WRITE "${_exports_log}" "${_exports_stdout}")

file(READ "${_exports_log}" _exports_text)
if(NOT (
    _exports_text MATCHES "NumElements@CIccTagMultiProcessElement" OR
    _exports_text MATCHES "CIccTagMultiProcessElement.*NumElements" OR
    _exports_text MATCHES "NumElements.*CIccTagMultiProcessElement"))
  message(FATAL_ERROR "issue #987 export missing from ${ICCDEV_ICCPROFLIB_DLL}: CIccTagMultiProcessElement::NumElements")
endif()

file(WRITE "${_consumer_src_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.18...3.29)
project(Issue987SharedMpeConsumer LANGUAGES CXX)

add_library(IccProfLib2Runtime SHARED IMPORTED GLOBAL)
set_target_properties(IccProfLib2Runtime PROPERTIES
  IMPORTED_IMPLIB "${ICCPROFLIB_IMPLIB}"
  IMPORTED_LOCATION "${ICCPROFLIB_DLL}"
  INTERFACE_COMPILE_DEFINITIONS "ICCPROFLIBDLL_IMPORTS"
  INTERFACE_INCLUDE_DIRECTORIES "${ICCDEV_BUILD_DIR}/IccProfLib;${ICCDEV_REPO_ROOT}/IccProfLib;${ICCDEV_REPO_ROOT}"
)

add_executable(issue987-shared-mpe-consumer issue987-shared-mpe-consumer.cpp)
target_link_libraries(issue987-shared-mpe-consumer PRIVATE IccProfLib2Runtime)

add_custom_command(TARGET issue987-shared-mpe-consumer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ICCPROFLIB_DLL}"
    "$<TARGET_FILE_DIR:issue987-shared-mpe-consumer>"
)
]=])

file(WRITE "${_consumer_src_dir}/issue987-shared-mpe-consumer.cpp" [=[
#include "IccTagMPE.h"

#include <cstdio>

int main()
{
  CIccTagMultiProcessElement tag;
  const icUInt32Number count = tag.NumElements();
  std::printf("NumElements=%u\n", count);
  return count == 0 ? 0 : 1;
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
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR "Consumer configure failed with ${_configure_result}:\n${_configure_stdout}\n${_configure_stderr}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${_consumer_build_dir}" --config "${ICCDEV_CONFIG}" --parallel
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_stdout
  ERROR_VARIABLE _build_stderr
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR "Consumer build failed with ${_build_result}:\n${_build_stdout}\n${_build_stderr}")
endif()

set(_consumer_exe "${_consumer_build_dir}/issue987-shared-mpe-consumer.exe")
if(NOT EXISTS "${_consumer_exe}" AND NOT "${ICCDEV_CONFIG}" STREQUAL "")
  set(_consumer_exe "${_consumer_build_dir}/${ICCDEV_CONFIG}/issue987-shared-mpe-consumer.exe")
endif()
if(NOT EXISTS "${_consumer_exe}")
  message(FATAL_ERROR "Consumer executable not found: ${_consumer_exe}")
endif()

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

file(WRITE "${_log_file}"
  "CTest test: ${ICCDEV_TEST_NAME}\n"
  "Configuration: ${ICCDEV_CONFIG}\n"
  "Compiler ID: ${_compiler_id}\n"
  "DLL: ${ICCDEV_ICCPROFLIB_DLL}\n"
  "Import library: ${ICCDEV_ICCPROFLIB_IMPLIB}\n"
  "Export log: ${_exports_log}\n\n"
  "----- consumer configure stdout -----\n${_configure_stdout}\n"
  "----- consumer configure stderr -----\n${_configure_stderr}\n"
  "----- consumer build stdout -----\n${_build_stdout}\n"
  "----- consumer build stderr -----\n${_build_stderr}\n"
  "----- consumer stdout -----\n${_consumer_stdout}\n"
  "----- consumer stderr -----\n${_consumer_stderr}\n"
)
message(STATUS "Wrote ${ICCDEV_TEST_NAME} log to ${_log_file}")

if(NOT _consumer_result EQUAL 0)
  message(FATAL_ERROR "Consumer exited with ${_consumer_result}; see ${_log_file}")
endif()

if(NOT _consumer_stdout MATCHES "NumElements=0")
  message(FATAL_ERROR "Consumer did not print NumElements=0; see ${_log_file}")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully")
