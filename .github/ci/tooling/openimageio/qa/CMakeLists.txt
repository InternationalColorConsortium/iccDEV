#[[ @file
    File:       OpenImageIO ICC QA CMakeLists.txt

    Contains:   CTest wrapper for pinned OpenImageIO regression checks

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
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
project(iccdev_openimageio_icc_qa LANGUAGES NONE)

include(CTest)

set(OPENIMAGEIO_SOURCE_DIR "" CACHE PATH "Path to OpenImageIO source")
set(OPENIMAGEIO_BUILD_DIR "" CACHE PATH "Path to OpenImageIO build")

if(NOT IS_ABSOLUTE "${OPENIMAGEIO_SOURCE_DIR}" OR
   NOT EXISTS "${OPENIMAGEIO_SOURCE_DIR}/src/libOpenImageIO/icc.cpp")
  message(FATAL_ERROR
    "OPENIMAGEIO_SOURCE_DIR must be an absolute OpenImageIO checkout")
endif()
if(NOT IS_ABSOLUTE "${OPENIMAGEIO_BUILD_DIR}" OR
   NOT EXISTS "${OPENIMAGEIO_BUILD_DIR}/bin/oiiotool" OR
   NOT EXISTS "${OPENIMAGEIO_BUILD_DIR}/bin/iinfo")
  message(FATAL_ERROR
    "OPENIMAGEIO_BUILD_DIR must contain bin/oiiotool and bin/iinfo")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME iccdev.openimageio-icc-qa
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/openimageio_icc_qa.py"
    --mode fixed
    --oiiotool "${OPENIMAGEIO_BUILD_DIR}/bin/oiiotool"
    --iinfo "${OPENIMAGEIO_BUILD_DIR}/bin/iinfo"
    --source-dir "${OPENIMAGEIO_SOURCE_DIR}"
    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/openimageio-icc-qa")
set_tests_properties(iccdev.openimageio-icc-qa PROPERTIES
  LABELS "openimageio;icc;exif;jpeg;jpeg2000;issue-2657;regression")
