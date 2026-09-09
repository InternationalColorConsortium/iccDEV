# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# iccBenchApply mobile proof of concept -- standalone Xcode consumer of the
# exported apple-*-core RefIccMAX package, following the same pattern as
# Build/AppleMobile. This is a manual-review example, not CI-gated: no CTest
# registration, no CI workflow entry. Build and run it by hand on a Mac with
# Xcode, then review on device.

cmake_minimum_required(VERSION 3.21)
project(IccBenchApplyPOC LANGUAGES CXX OBJCXX)
list(REMOVE_ITEM CMAKE_OBJCXX_IMPLICIT_LINK_LIBRARIES objc)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" OR NOT CMAKE_GENERATOR STREQUAL "Xcode")
  message(FATAL_ERROR "Use the Xcode generator with -DCMAKE_SYSTEM_NAME=iOS")
endif()

find_package(RefIccMAX CONFIG REQUIRED)
if(NOT TARGET RefIccMAX::IccProfLib2-static)
  message(FATAL_ERROR "Build the matching apple-ios-*-core static library first")
endif()

set(ICCDEV_BENCHAPPLY_BUNDLE_IDENTIFIER "org.color.iccdev.BenchApplyPOC" CACHE STRING
  "Bundle identifier registered with your Apple development team")

set(_source "${CMAKE_CURRENT_SOURCE_DIR}")
set(_assets "${CMAKE_CURRENT_SOURCE_DIR}/Assets.xcassets")
set(_profile "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc")
set(_logo "${CMAKE_CURRENT_SOURCE_DIR}/Assets.xcassets/ICCLogo.imageset/icc-logo.png")
set_source_files_properties("${_assets}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties("${_profile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties("${_logo}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(IccBenchApplyPOC MACOSX_BUNDLE
  "${_source}/main.mm" "${_source}/BenchApplyHost.h" "${_source}/BenchApplyHost.mm"
  "${_assets}" "${_logo}" "${_profile}")

# BenchTimer.h is the desktop iccBenchApply CLI's timing/checksum/fill header
# (Tools/CmdLine/IccBenchApply); this POC includes it directly rather than
# copying it, so the two stay identical.
target_include_directories(IccBenchApplyPOC PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../Tools/CmdLine/IccBenchApply")

target_link_libraries(IccBenchApplyPOC PRIVATE
  RefIccMAX::IccProfLib2-static "-framework UIKit")
target_compile_features(IccBenchApplyPOC PRIVATE cxx_std_17)
target_compile_options(IccBenchApplyPOC PRIVATE -Wall -Wextra -Werror)

set_target_properties(IccBenchApplyPOC PROPERTIES
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_INFO_PLIST "${_source}/Info.plist.in"
  XCODE_GENERATE_SCHEME YES
  XCODE_SCHEME_LAUNCH_CONFIGURATION Release
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
  XCODE_ATTRIBUTE_DISABLE_MANUAL_TARGET_ORDER_BUILD_WARNING YES
  XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST NO
  XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
  # "1,2": one universal app runs on iPhone and iPad, matching the "easy
  # additional devices" scope of this proof of concept without a per-platform
  # run-target selector like Build/AppleMobile's.
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ICCDEV_BENCHAPPLY_BUNDLE_IDENTIFIER}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
