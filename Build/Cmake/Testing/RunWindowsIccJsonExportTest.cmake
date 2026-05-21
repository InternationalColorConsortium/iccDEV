#################################################################################
# Windows IccJSON shared-library export regression for issue #1009
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_ICCJSON_DLL
  ICCDEV_ICCJSON_IMPLIB
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

if(NOT EXISTS "${ICCDEV_ICCJSON_DLL}")
  message(FATAL_ERROR "IccJSON DLL not found: ${ICCDEV_ICCJSON_DLL}")
endif()

if(NOT EXISTS "${ICCDEV_ICCJSON_IMPLIB}")
  message(FATAL_ERROR "IccJSON import library not found: ${ICCDEV_ICCJSON_IMPLIB}")
endif()

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_exports_log "${ICCDEV_TEST_OUTDIR}/iccjson-exports.txt")

set(_dumpbin "")
if(CMAKE_HOST_WIN32)
  foreach(_vswhere_candidate IN ITEMS
      "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer/vswhere.exe")
    if(EXISTS "${_vswhere_candidate}")
      set(_vswhere "${_vswhere_candidate}")
      break()
    endif()
  endforeach()

  if(DEFINED _vswhere AND NOT "${_vswhere}" STREQUAL "")
    execute_process(
      COMMAND
        "${_vswhere}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
      OUTPUT_VARIABLE _vs_path
      ERROR_VARIABLE _vswhere_stderr
      RESULT_VARIABLE _vswhere_result
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_vswhere_result EQUAL 0 AND NOT "${_vs_path}" STREQUAL "")
      file(GLOB _dumpbin_candidates "${_vs_path}/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe")
      list(SORT _dumpbin_candidates)
      list(REVERSE _dumpbin_candidates)
      list(LENGTH _dumpbin_candidates _dumpbin_count)
      if(_dumpbin_count GREATER 0)
        list(GET _dumpbin_candidates 0 _dumpbin)
      endif()
    endif()
  endif()

  if(NOT EXISTS "${_dumpbin}")
    find_program(_dumpbin dumpbin)
  endif()
endif()

if(_dumpbin)
  execute_process(
    COMMAND "${_dumpbin}" /exports "${ICCDEV_ICCJSON_DLL}"
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
    message(FATAL_ERROR "dumpbin, objdump, or llvm-objdump not found")
  endif()

  execute_process(
    COMMAND "${_objdump}" -p "${ICCDEV_ICCJSON_DLL}"
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
    _exports_text MATCHES "\\?\\?1CIccProfileJson" OR
    _exports_text MATCHES "CIccProfileJson.*~CIccProfileJson" OR
    _exports_text MATCHES "~CIccProfileJson.*CIccProfileJson"))
  message(FATAL_ERROR "issue #1009 export missing from ${ICCDEV_ICCJSON_DLL}: CIccProfileJson destructor")
endif()

message(STATUS "${ICCDEV_TEST_NAME} completed successfully; wrote ${_exports_log}")
