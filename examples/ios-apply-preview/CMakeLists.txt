# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# iccApplyProfiles-inspired mobile visual proof of concept. This standalone
# Xcode consumer uses the exported apple-*-core RefIccMAX package and is a
# manual-review example, not a CI-gated CTest target.

cmake_minimum_required(VERSION 3.21)
project(IccApplyPreviewPOC LANGUAGES CXX OBJCXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" OR NOT CMAKE_GENERATOR STREQUAL "Xcode")
  message(FATAL_ERROR "Use the Xcode generator with -DCMAKE_SYSTEM_NAME=iOS")
endif()

find_package(RefIccMAX CONFIG REQUIRED)
if(NOT TARGET RefIccMAX::IccProfLib2-static)
  message(FATAL_ERROR "Build the matching apple-ios-*-core static library first")
endif()

set(ICCDEV_APPLYPREVIEW_BUNDLE_IDENTIFIER "org.color.iccdev.ApplyPreviewPOC"
  CACHE STRING "Bundle identifier registered with your Apple development team")

set(_source "${CMAKE_CURRENT_SOURCE_DIR}")
set(_profile "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc")
set_source_files_properties("${_profile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(IccApplyPreviewPOC MACOSX_BUNDLE
  "${_source}/main.mm"
  "${_source}/ApplyPreviewHost.h"
  "${_source}/ApplyPreviewHost.mm"
  "${_profile}")

target_link_libraries(IccApplyPreviewPOC PRIVATE
  RefIccMAX::IccProfLib2-static
  "-framework CoreGraphics"
  "-framework UIKit")
target_compile_features(IccApplyPreviewPOC PRIVATE cxx_std_17)
target_compile_options(IccApplyPreviewPOC PRIVATE -Wall -Wextra -Werror)
target_link_options(IccApplyPreviewPOC PRIVATE LINKER:-no_warn_duplicate_libraries)

set_target_properties(IccApplyPreviewPOC PROPERTIES
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_INFO_PLIST "${_source}/Info.plist.in"
  XCODE_GENERATE_SCHEME YES
  XCODE_SCHEME_LAUNCH_CONFIGURATION Release
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
  XCODE_ATTRIBUTE_DISABLE_MANUAL_TARGET_ORDER_BUILD_WARNING YES
  XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST NO
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ICCDEV_APPLYPREVIEW_BUNDLE_IDENTIFIER}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
