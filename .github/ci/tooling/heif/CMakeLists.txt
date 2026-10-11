#[[ @file
    File:       CMakeLists.txt for iccHeifDump.cpp

    Contains:   CMake build rules for the iccHeifDump research tool

    Version:    V1

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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 * Notes - Added 21-Sept-2026 by @xsscx
 *       - Research Tooling for HEIF InterOp
 ]]

add_executable(heifIccDump IccHeifDump.cpp)
set_property(TARGET heifIccDump PROPERTY CXX_STANDARD 11)
target_link_libraries(heifIccDump heif_static)

option(ICCDEV_HEIF_ENABLE_TESTS "Register the iccHeifDump carrier QA test" OFF)
if(ICCDEV_HEIF_ENABLE_TESTS)
  find_program(BASH_EXECUTABLE bash REQUIRED)
  add_test(
    NAME iccdev.heif-carrier-qa
    COMMAND "${CMAKE_COMMAND}" -E env
      "ICCDEV_HEIF_TOOL=$<TARGET_FILE:heifIccDump>"
      "ICCDEV_HEIF_CORPUS=${CMAKE_CURRENT_LIST_DIR}/img"
      "ICCDEV_TEST_OUTDIR=${CMAKE_CURRENT_BINARY_DIR}/qa-output"
      "${BASH_EXECUTABLE}"
      "${CMAKE_CURRENT_LIST_DIR}/../../../scripts/iccdev-heif-carrier-qa.sh"
  )
  set_tests_properties(iccdev.heif-carrier-qa PROPERTIES
    LABELS "iccdev;heif;icc;issue-2647;regression"
    TIMEOUT 120)
endif()

option(ICCDEV_HEIF_BUILD_NATIVE_TOOL
  "Build the release libheif and IccProfLib-backed iccHeifDump tool" OFF)
set(ICCDEV_PROFLIB_LIBRARY "" CACHE FILEPATH
  "Path to a locally built IccProfLib2 static library")
set(ICCDEV_PROFLIB_INCLUDE_DIR "" CACHE PATH
  "Directory containing generated IccProfLib build headers")

if(ICCDEV_HEIF_BUILD_NATIVE_TOOL)
  if(NOT EXISTS "${ICCDEV_PROFLIB_LIBRARY}" OR
     NOT EXISTS "${ICCDEV_PROFLIB_INCLUDE_DIR}/IccProfLibVer.h")
    message(FATAL_ERROR
      "ICCDEV_HEIF_BUILD_NATIVE_TOOL requires ICCDEV_PROFLIB_LIBRARY and "
      "ICCDEV_PROFLIB_INCLUDE_DIR from a local iccDEV build")
  endif()

  find_package(Threads REQUIRED)
  find_package(ZLIB REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBHEIF REQUIRED IMPORTED_TARGET libheif)

  set(ICCDEV_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
  add_executable(iccHeifDump
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccHeifDump/iccHeifDump.cpp")
  target_compile_features(iccHeifDump PRIVATE cxx_std_17)
  target_include_directories(iccHeifDump PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccHeifDump"
    "${ICCDEV_PROFLIB_INCLUDE_DIR}")
  target_link_libraries(iccHeifDump PRIVATE
    "${ICCDEV_PROFLIB_LIBRARY}"
    PkgConfig::LIBHEIF
    Threads::Threads
    ZLIB::ZLIB)

  if(ICCDEV_HEIF_ENABLE_TESTS)
    add_test(
      NAME iccdev.heif-carrier-ab-qa
      COMMAND "${CMAKE_COMMAND}" -E env
        "ICCDEV_HEIF_CARRIER_TOOL=$<TARGET_FILE:heifIccDump>"
        "ICCDEV_HEIF_NATIVE_TOOL=$<TARGET_FILE:iccHeifDump>"
        "ICCDEV_HEIF_CORPUS=${CMAKE_CURRENT_LIST_DIR}/img"
        "ICCDEV_TEST_OUTDIR=${CMAKE_CURRENT_BINARY_DIR}/ab-qa-output"
        "${BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/../../../scripts/iccdev-heif-ab-qa.sh")
    set_tests_properties(iccdev.heif-carrier-ab-qa PROPERTIES
      LABELS "iccdev;heif;icc;ab;local"
      TIMEOUT 120)
  endif()
endif()
