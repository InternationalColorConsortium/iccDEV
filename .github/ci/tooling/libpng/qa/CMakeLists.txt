#[[ @file
    File:       libpng iCCP QA CMakeLists.txt

    Contains:   Reproducible wrapper for pinned libpng iCCP regression tests

    Copyright:  (c) see Software License
]]

#[[
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 ]]

cmake_minimum_required(VERSION 3.18)
project(iccdev_libpng_iccp_qa LANGUAGES C)

include(CTest)

set(LIBPNG_SOURCE_DIR "" CACHE PATH "Path to a libpng source checkout")
if(NOT IS_ABSOLUTE "${LIBPNG_SOURCE_DIR}" OR
   NOT EXISTS "${LIBPNG_SOURCE_DIR}/pngrutil.c")
  message(FATAL_ERROR
    "LIBPNG_SOURCE_DIR must be an absolute libpng source checkout")
endif()

option(ICCDEV_LIBPNG_ENABLE_SANITIZERS
  "Build libpng and pngtest with ASan and UBSan" OFF)
if(ICCDEV_LIBPNG_ENABLE_SANITIZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "libpng sanitizers require Clang or GCC")
  endif()
  add_compile_options(
    -fsanitize=address,undefined
    -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(PNG_SHARED ON CACHE BOOL "Build the shared libpng target" FORCE)
set(PNG_STATIC OFF CACHE BOOL "Do not build duplicate static targets" FORCE)
set(PNG_TESTS ON CACHE BOOL "Build pngtest" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "Do not build unrelated libpng tools" FORCE)
add_subdirectory("${LIBPNG_SOURCE_DIR}" libpng)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME iccdev.libpng-iccp-qa
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/libpng_iccp_qa.py"
    --mode fixed
    --pngtest "$<TARGET_FILE:pngtest>"
    --profile
      "${CMAKE_CURRENT_LIST_DIR}/../fixtures/valid-hybrid-rgb.icc.b64"
    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/libpng-iccp-qa")
set_tests_properties(iccdev.libpng-iccp-qa PROPERTIES
  LABELS "libpng;icc;iccp;regression")
