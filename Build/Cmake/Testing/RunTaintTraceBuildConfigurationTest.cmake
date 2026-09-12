#################################################################################
# Taint-trace build-configuration regression test
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

cmake_minimum_required(VERSION 3.18...3.29)

foreach(_required ICCDEV_SOURCE_DIR ICCDEV_TEST_BINARY_DIR ICCDEV_NINJA_EXECUTABLE)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "[taint-trace-config] ${_required} not set")
  endif()
endforeach()

function(iccdev_configure_trace_case CASE_NAME BUILD_TYPE TRACE_OPTION EXPECTED_OPTION EXPECTED_DEFINITION)
  set(_build_dir "${ICCDEV_TEST_BINARY_DIR}/${CASE_NAME}")
  file(REMOVE_RECURSE "${_build_dir}")

  set(_configure_args
    -S "${ICCDEV_SOURCE_DIR}/Build/Cmake"
    -B "${_build_dir}"
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=${ICCDEV_NINJA_EXECUTABLE}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DENABLE_TOOLS=OFF
    -DENABLE_TESTS=OFF
    -DENABLE_ICCXML=OFF
    -DENABLE_ICCJSON=OFF
    -DENABLE_IMAGE_TOOLS=OFF
    -DENABLE_CMM_TOOLS=OFF
    -DENABLE_IIS_TOOLS=OFF
    -DICC_USE_ZLIB=OFF
    -DICCDEV_ENABLE_TAINT_TRACE=${TRACE_OPTION}
  )

  if(DEFINED ICCDEV_PARENT_CMAKE_TOOLCHAIN_FILE AND
     NOT "${ICCDEV_PARENT_CMAKE_TOOLCHAIN_FILE}" STREQUAL "" AND
     NOT "${ICCDEV_PARENT_CMAKE_TOOLCHAIN_FILE}" MATCHES
       "[/\\\\]scripts[/\\\\]buildsystems[/\\\\]vcpkg[.]cmake$")
    list(APPEND _configure_args
      "-DCMAKE_TOOLCHAIN_FILE=${ICCDEV_PARENT_CMAKE_TOOLCHAIN_FILE}")
  else()
    if(DEFINED ICCDEV_PARENT_C_COMPILER AND
       NOT "${ICCDEV_PARENT_C_COMPILER}" STREQUAL "")
      list(APPEND _configure_args
        "-DCMAKE_C_COMPILER=${ICCDEV_PARENT_C_COMPILER}")
    endif()
    if(DEFINED ICCDEV_PARENT_CXX_COMPILER AND
       NOT "${ICCDEV_PARENT_CXX_COMPILER}" STREQUAL "")
      list(APPEND _configure_args
        "-DCMAKE_CXX_COMPILER=${ICCDEV_PARENT_CXX_COMPILER}")
    endif()
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_configure_args}
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_stdout
    ERROR_VARIABLE _configure_stderr
  )
  if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
      "[taint-trace-config] ${CASE_NAME} configure failed:\n"
      "${_configure_stdout}\n${_configure_stderr}")
  endif()

  file(READ "${_build_dir}/CMakeCache.txt" _cache)
  if(NOT _cache MATCHES "ICCDEV_ENABLE_TAINT_TRACE:BOOL=${EXPECTED_OPTION}([\r\n]|$)")
    message(FATAL_ERROR
      "[taint-trace-config] ${CASE_NAME} retained the wrong cache value; "
      "expected ${EXPECTED_OPTION}")
  endif()

  file(READ "${_build_dir}/compile_commands.json" _compile_commands)
  string(FIND "${_compile_commands}" "ICC_TAINT_TRACE_ENABLED" _definition_index)
  if(EXPECTED_DEFINITION AND _definition_index EQUAL -1)
    message(FATAL_ERROR
      "[taint-trace-config] ${CASE_NAME} omitted ICC_TAINT_TRACE_ENABLED")
  elseif(NOT EXPECTED_DEFINITION AND NOT _definition_index EQUAL -1)
    message(FATAL_ERROR
      "[taint-trace-config] ${CASE_NAME} enabled diagnostics in an optimized build")
  endif()

  if(TRACE_OPTION AND NOT EXPECTED_OPTION AND
     NOT _configure_stdout MATCHES "ICCDEV_ENABLE_TAINT_TRACE forced OFF")
    message(FATAL_ERROR
      "[taint-trace-config] ${CASE_NAME} did not explain why the option was forced OFF")
  endif()
endfunction()

iccdev_configure_trace_case(release-default Release OFF OFF FALSE)
foreach(_build_type Release RelWithDebInfo MinSizeRel)
  string(TOLOWER "${_build_type}" _case_name)
  iccdev_configure_trace_case("${_case_name}-explicit-on" "${_build_type}" ON OFF FALSE)
endforeach()
iccdev_configure_trace_case(debug-explicit-on Debug ON ON TRUE)

message(STATUS
  "[taint-trace-config] optimized configurations compile diagnostics out; Debug retains them")
