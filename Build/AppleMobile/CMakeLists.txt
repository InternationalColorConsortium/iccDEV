# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)
project(IccDevCoreSmoke LANGUAGES CXX OBJCXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS)$" OR NOT CMAKE_GENERATOR STREQUAL "Xcode")
  message(FATAL_ERROR "Use the Xcode generator with -DCMAKE_SYSTEM_NAME=iOS, tvOS, or watchOS")
endif()

find_package(RefIccMAX CONFIG REQUIRED)
if(NOT TARGET RefIccMAX::IccProfLib2-static)
  message(FATAL_ERROR "Build the matching apple-*-core static library first")
endif()

set(ICCDEV_IOS_BUNDLE_IDENTIFIER "org.color.iccdev.CoreSmoke" CACHE STRING
  "Legacy iOS bundle identifier (used unless ICCDEV_APPLE_BUNDLE_IDENTIFIER is set)")
set(ICCDEV_APPLE_RUN_TARGET "" CACHE STRING
  "Default Apple run target: iphone, ipad, tv, or watch. Empty selects the platform default.")
set_property(CACHE ICCDEV_APPLE_RUN_TARGET PROPERTY STRINGS "" iphone ipad tv watch)
set(ICCDEV_APPLE_PLATFORM_PLIST_KEYS "")
if(ICCDEV_APPLE_RUN_TARGET)
  string(TOLOWER "${ICCDEV_APPLE_RUN_TARGET}" _iccdev_run_target)
else()
  if(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
    set(_iccdev_run_target watch)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set(_iccdev_run_target tv)
  else()
    set(_iccdev_run_target iphone)
  endif()
endif()
if(_iccdev_run_target STREQUAL "ios")
  set(_iccdev_run_target iphone)
elseif(_iccdev_run_target STREQUAL "tvos")
  set(_iccdev_run_target tv)
elseif(_iccdev_run_target STREQUAL "watchos")
  set(_iccdev_run_target watch)
endif()

if(_iccdev_run_target STREQUAL "iphone")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "ICCDEV_APPLE_RUN_TARGET=iphone requires -DCMAKE_SYSTEM_NAME=iOS")
  endif()
  set(_targeted_device_family "1")
  set(_bundle_default "${ICCDEV_IOS_BUNDLE_IDENTIFIER}")
  set(_info_plist "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")
elseif(_iccdev_run_target STREQUAL "ipad")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "ICCDEV_APPLE_RUN_TARGET=ipad requires -DCMAKE_SYSTEM_NAME=iOS")
  endif()
  set(_targeted_device_family "2")
  set(_bundle_default "${ICCDEV_IOS_BUNDLE_IDENTIFIER}.ipad")
  set(_info_plist "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")
elseif(_iccdev_run_target STREQUAL "tv")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    message(FATAL_ERROR "ICCDEV_APPLE_RUN_TARGET=tv requires -DCMAKE_SYSTEM_NAME=tvOS")
  endif()
  set(_targeted_device_family "3")
  set(_bundle_default "org.color.iccdev.CoreSmoke.tv")
  set(_info_plist "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")
  set(ICCDEV_APPLE_PLATFORM_PLIST_KEYS [=[
  <key>UILaunchScreen</key>
  <dict/>
  <key>UISupportedInterfaceOrientations</key>
  <array>
    <string>UIInterfaceOrientationLandscapeLeft</string>
    <string>UIInterfaceOrientationLandscapeRight</string>
  </array>
]=])
elseif(_iccdev_run_target STREQUAL "watch")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "watchOS")
    message(FATAL_ERROR "ICCDEV_APPLE_RUN_TARGET=watch requires -DCMAKE_SYSTEM_NAME=watchOS")
  endif()
  enable_language(Swift)
  set(_targeted_device_family "4")
  set(_bundle_default "org.color.iccdev.CoreSmoke.watch")
else()
  message(FATAL_ERROR "Set ICCDEV_APPLE_RUN_TARGET to iphone, ipad, tv, or watch")
endif()
if(_iccdev_run_target STREQUAL "watch")
  set(_info_plist "${CMAKE_CURRENT_SOURCE_DIR}/WatchInfo.plist.in")
elseif(NOT _iccdev_run_target STREQUAL "tv")
  set(ICCDEV_APPLE_PLATFORM_PLIST_KEYS [=[
  <key>LSRequiresIPhoneOS</key>
  <true/>
  <key>UILaunchScreen</key>
  <dict/>
  <key>UISupportedInterfaceOrientations</key>
  <array>
    <string>UIInterfaceOrientationPortrait</string>
    <string>UIInterfaceOrientationLandscapeLeft</string>
    <string>UIInterfaceOrientationLandscapeRight</string>
  </array>
  <key>UISupportedInterfaceOrientations~ipad</key>
  <array>
    <string>UIInterfaceOrientationPortrait</string>
    <string>UIInterfaceOrientationPortraitUpsideDown</string>
    <string>UIInterfaceOrientationLandscapeLeft</string>
    <string>UIInterfaceOrientationLandscapeRight</string>
  </array>
]=])
endif()
set(ICCDEV_APPLE_BUNDLE_IDENTIFIER "" CACHE STRING
  "Override the platform default bundle identifier registered with your Apple development team")
if(ICCDEV_APPLE_BUNDLE_IDENTIFIER)
  set(_bundle_identifier "${ICCDEV_APPLE_BUNDLE_IDENTIFIER}")
else()
  set(_bundle_identifier "${_bundle_default}")
endif()
set(_source "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/AppleMobile")
set(_profile "${CMAKE_CURRENT_SOURCE_DIR}/../../Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc")
set_source_files_properties("${_profile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(IccDevCoreSmoke MACOSX_BUNDLE
  "${_source}/SmokeTests.mm" "${_source}/SmokeTests.h" "${_profile}")
set(_iccdev_core_smoke_libs "-framework Foundation")
if(TARGET RefIccMAX::IccConnect2-static)
  list(APPEND _iccdev_core_smoke_libs RefIccMAX::IccConnect2-static)
  target_compile_definitions(IccDevCoreSmoke PRIVATE ICCDEV_APPLE_HAS_CONNECT=1)
endif()
if(TARGET RefIccMAX::IccJSON2-static)
  if(NOT TARGET RefIccMAX::IccConnect2-static)
    list(APPEND _iccdev_core_smoke_libs RefIccMAX::IccJSON2-static)
  endif()
  target_compile_definitions(IccDevCoreSmoke PRIVATE ICCDEV_APPLE_HAS_JSON=1)
endif()
if(TARGET RefIccMAX::IccXML2-static)
  list(APPEND _iccdev_core_smoke_libs RefIccMAX::IccXML2-static)
  target_compile_definitions(IccDevCoreSmoke PRIVATE ICCDEV_APPLE_HAS_XML=1)
endif()
if(NOT TARGET RefIccMAX::IccConnect2-static AND
    NOT TARGET RefIccMAX::IccJSON2-static AND
    NOT TARGET RefIccMAX::IccXML2-static)
  list(APPEND _iccdev_core_smoke_libs RefIccMAX::IccProfLib2-static)
endif()
target_link_libraries(IccDevCoreSmoke PRIVATE ${_iccdev_core_smoke_libs})
target_compile_features(IccDevCoreSmoke PRIVATE cxx_std_17)
target_compile_options(IccDevCoreSmoke PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wall;-Wextra;-Werror>"
  "$<$<COMPILE_LANGUAGE:Swift>:-warnings-as-errors>")
if(_iccdev_run_target STREQUAL "watch")
  target_sources(IccDevCoreSmoke PRIVATE "${_source}/WatchApp.swift")
  target_link_libraries(IccDevCoreSmoke PRIVATE "-framework SwiftUI")
  set_target_properties(IccDevCoreSmoke PROPERTIES
    Swift_LANGUAGE_VERSION 5
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${_source}/SmokeTests.h"
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "$(inherited) @executable_path/Frameworks")
else()
  target_sources(IccDevCoreSmoke PRIVATE "${_source}/main.mm")
  target_link_libraries(IccDevCoreSmoke PRIVATE "-framework UIKit")
  set_target_properties(IccDevCoreSmoke PROPERTIES
    XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST NO)
endif()
set_target_properties(IccDevCoreSmoke PROPERTIES
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_INFO_PLIST "${_info_plist}"
  XCODE_GENERATE_SCHEME YES
  XCODE_SCHEME_LAUNCH_CONFIGURATION Release
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "${_targeted_device_family}"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${_bundle_identifier}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
