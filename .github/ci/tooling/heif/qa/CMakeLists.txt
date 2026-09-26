#[[ @file
    File:       HEIF QA CMakeLists.txt

    Contains:   Reproducible wrapper for the pinned Nokia HEIF reader and
                iccHeifDump carrier regression tests

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
project(iccdev_heif_qa LANGUAGES C CXX)

include(CTest)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
    "Compatibility floor for the pinned Nokia HEIF CMake files" FORCE)
endif()

set(HEIF_SOURCE_DIR "" CACHE PATH "Path to a Nokia HEIF source checkout")
if(NOT IS_ABSOLUTE "${HEIF_SOURCE_DIR}" OR
   NOT EXISTS "${HEIF_SOURCE_DIR}/srcs/CMakeLists.txt")
  message(FATAL_ERROR
    "HEIF_SOURCE_DIR must be an absolute Nokia HEIF source checkout")
endif()

option(ICCDEV_HEIF_ENABLE_SANITIZERS
  "Build the Nokia reader and iccHeifDump with ASan and UBSan" OFF)
if(ICCDEV_HEIF_ENABLE_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "HEIF sanitizers require Clang or GCC")
  endif()
  add_compile_options(
    -fsanitize=address,undefined
    -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(BUILD_ONLY_STATIC_LIB ON CACHE BOOL "Build only Nokia HEIF static libraries" FORCE)
set(ICCDEV_HEIF_ENABLE_TESTS ON CACHE BOOL "Register iccHeifDump carrier QA" FORCE)
add_subdirectory("${HEIF_SOURCE_DIR}/srcs" nokia-heif)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." iccdev-heif-tool)
