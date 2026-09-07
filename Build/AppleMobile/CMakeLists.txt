# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)
project(IccDevCoreSmoke LANGUAGES CXX OBJCXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" OR NOT CMAKE_GENERATOR STREQUAL "Xcode")
  message(FATAL_ERROR "Use the Xcode generator with -DCMAKE_SYSTEM_NAME=iOS")
endif()

find_package(RefIccMAX CONFIG REQUIRED)
if(NOT TARGET RefIccMAX::IccProfLib2-static)
  message(FATAL_ERROR "Build the matching apple-ios-*-core static library first")
endif()

set(ICCDEV_IOS_BUNDLE_IDENTIFIER "org.color.iccdev.CoreSmoke" CACHE STRING
  "Bundle identifier registered with your Apple development team")
set(_source "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/AppleMobile")
set(_profile "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc")
set_source_files_properties("${_profile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(IccDevCoreSmoke MACOSX_BUNDLE "${_source}/main.mm" "${_profile}")
target_link_libraries(IccDevCoreSmoke PRIVATE
  RefIccMAX::IccProfLib2-static "-framework UIKit" "-framework Foundation")
target_compile_features(IccDevCoreSmoke PRIVATE cxx_std_17)
target_compile_options(IccDevCoreSmoke PRIVATE -fobjc-arc -Wall -Wextra -Werror)
set_target_properties(IccDevCoreSmoke PROPERTIES
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  XCODE_GENERATE_SCHEME YES
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ICCDEV_IOS_BUNDLE_IDENTIFIER}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST NO)
