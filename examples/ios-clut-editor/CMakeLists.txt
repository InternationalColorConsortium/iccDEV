# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# iccCLUTEditor mobile proof of concept. This standalone Xcode consumer uses
# the exported Apple iOS or Mac Catalyst RefIccMAX core package and is a
# manual-review example, not a CI-gated CTest target.

cmake_minimum_required(VERSION 3.21)
project(IccClutEditorPOC LANGUAGES CXX OBJCXX)
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES Carbon IOKit)
list(REMOVE_ITEM CMAKE_OBJCXX_IMPLICIT_LINK_LIBRARIES objc Carbon IOKit)

option(ICCDEV_CLUTEDITOR_ENABLE_MACCATALYST
  "Build the UIKit app for Mac Catalyst" OFF)

set(_iccdev_cluteditor_ios OFF)
set(_iccdev_cluteditor_maccatalyst OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(_iccdev_cluteditor_ios ON)
elseif(ICCDEV_CLUTEDITOR_ENABLE_MACCATALYST AND
       CMAKE_OSX_SYSROOT MATCHES "macosx")
  set(_iccdev_cluteditor_maccatalyst ON)
endif()

if(NOT CMAKE_GENERATOR STREQUAL "Xcode" OR
   (NOT _iccdev_cluteditor_ios AND NOT _iccdev_cluteditor_maccatalyst))
  message(FATAL_ERROR
    "Use examples/ios-clut-editor/build-ios.sh or configure with "
    "-G Xcode -DCMAKE_SYSTEM_NAME=iOS for iPhone/iPad, or with "
    "-G Xcode -DICCDEV_CLUTEDITOR_ENABLE_MACCATALYST=ON "
    "-DCMAKE_OSX_SYSROOT=macosx for Mac Catalyst")
endif()

find_package(RefIccMAX CONFIG REQUIRED)
if(NOT TARGET RefIccMAX::IccProfLib2-static)
  message(FATAL_ERROR
    "Build the matching iOS or Mac Catalyst static library first, or run "
    "examples/ios-clut-editor/build-ios.sh")
endif()
if(_iccdev_cluteditor_maccatalyst)
  get_target_property(_iccdev_proflib_links
    RefIccMAX::IccProfLib2-static INTERFACE_LINK_LIBRARIES)
  if(_iccdev_proflib_links)
    list(FILTER _iccdev_proflib_links EXCLUDE REGEX
      "Carbon\\.framework|IOKit\\.framework")
    set_target_properties(RefIccMAX::IccProfLib2-static PROPERTIES
      INTERFACE_LINK_LIBRARIES "${_iccdev_proflib_links}")
  endif()
  unset(_iccdev_proflib_links)
endif()

set(ICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER "org.color.iccdev.ClutEditorPOC"
  CACHE STRING "Bundle identifier registered with your Apple development team")
set(ICCDEV_CLUTEDITOR_IOS_DEPLOYMENT_TARGET "17.0"
  CACHE STRING "Minimum iOS deployment target for the app")
set(ICCDEV_CLUTEDITOR_MACOS_DEPLOYMENT_TARGET "14.0"
  CACHE STRING "Minimum macOS deployment target for Mac Catalyst")
if(_iccdev_cluteditor_ios)
  if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND
     NOT CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(ICCDEV_CLUTEDITOR_IOS_DEPLOYMENT_TARGET
      "${CMAKE_OSX_DEPLOYMENT_TARGET}" CACHE STRING
      "Minimum iOS deployment target for the app" FORCE)
  else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET
      "${ICCDEV_CLUTEDITOR_IOS_DEPLOYMENT_TARGET}" CACHE STRING
      "Minimum iOS deployment target for the app" FORCE)
  endif()
endif()
set(_iccdev_cluteditor_supports_maccatalyst NO)
set(ICCDEV_CLUTEDITOR_REQUIRES_IPHONEOS true)
if(_iccdev_cluteditor_maccatalyst)
  set(_iccdev_cluteditor_supports_maccatalyst YES)
  set(ICCDEV_CLUTEDITOR_REQUIRES_IPHONEOS false)
endif()
message(STATUS
  "IccClutEditorPOC bundle identifier: "
  "${ICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER}")
message(STATUS
  "IccClutEditorPOC iOS deployment target: "
  "${ICCDEV_CLUTEDITOR_IOS_DEPLOYMENT_TARGET}")
if(_iccdev_cluteditor_maccatalyst)
  message(STATUS
    "IccClutEditorPOC Mac Catalyst deployment target: "
    "${ICCDEV_CLUTEDITOR_MACOS_DEPLOYMENT_TARGET}")
endif()

set(_source "${CMAKE_CURRENT_SOURCE_DIR}")
set(_assets "${CMAKE_CURRENT_SOURCE_DIR}/Assets.xcassets")
set(_src_profile "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/sRGB_v4_ICC_preference.icc")
set(_profile "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc")
set_source_files_properties("${_assets}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties("${_src_profile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties("${_profile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(IccClutEditorPOC MACOSX_BUNDLE
  "${_source}/main.mm"
  "${_source}/ClutEditorHost.h"
  "${_source}/ClutEditorHost.mm"
  "${_assets}"
  "${_src_profile}"
  "${_profile}")

target_link_libraries(IccClutEditorPOC PRIVATE
  RefIccMAX::IccProfLib2-static
  "-framework CoreGraphics"
  "-framework UIKit")
target_compile_features(IccClutEditorPOC PRIVATE cxx_std_17)
target_compile_options(IccClutEditorPOC PRIVATE -Wall -Wextra -Werror)
target_link_options(IccClutEditorPOC PRIVATE LINKER:-no_warn_duplicate_libraries)

set_target_properties(IccClutEditorPOC PROPERTIES
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_ICON_FILE AppIcon
  MACOSX_BUNDLE_INFO_PLIST "${_source}/Info.plist.in"
  XCODE_GENERATE_SCHEME YES
  XCODE_SCHEME_LAUNCH_CONFIGURATION Release
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
  XCODE_ATTRIBUTE_DISABLE_MANUAL_TARGET_ORDER_BUILD_WARNING YES
  XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST
    "${_iccdev_cluteditor_supports_maccatalyst}"
  XCODE_ATTRIBUTE_DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER NO
  XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET
    "${ICCDEV_CLUTEDITOR_IOS_DEPLOYMENT_TARGET}"
  XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET
    "${ICCDEV_CLUTEDITOR_MACOS_DEPLOYMENT_TARGET}"
  XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
