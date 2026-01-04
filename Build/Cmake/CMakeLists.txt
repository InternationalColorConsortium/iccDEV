#################################################################################
# Cmake Project Configuration| iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#
#  Last Updated: 04-JAN-2026 at 2350Z by David Hoyt
#
#  Changes: Bump Version String to v2.3.1.2
#           Recent PRs address range of security & stability issues
#           Stack Corruption, NaN, Undefined Behavior (UB) & Overflows
#
#
#
#  TODO:    Align Version on IccProfLibVer.h, Housekeeping, Debug Options
#           Version Headers, Doxygen Config, Exports etc...
#           Refactor this to .mk's, add research args, other Housekeeping
#
#
#################################################################################

# ----------------------------------------------------------------------
# Noise Level / Warning Suppression
# ----------------------------------------------------------------------

# Note: prefer `target_compile_options()` for scoped suppression
# Uncomment this only for global suppression (not recommended):
# add_compile_options(/w)

# Recommended: suppress known-safe MSVC warnings globally
if(MSVC)
  add_compile_options(/wd5045)         # Suppress Spectre mitigation warnings
  add_link_options("/IGNORE:4044")     # Ignore unrecognized MSVC linker flags
  add_link_options("/IGNORE:4300")     # Suppress ASAN + /INCREMENTAL warnings
endif()

# ----------------------------------------------------------------------
# Global CMake Project Setup
# ----------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)  # Modern minimum for policy safety

# Adopt modern behavior for scoped scripts and alias linking
# cmake_policy(SET CMP0112 NEW)  # Respect parent scope in script mode
# cmake_policy(SET CMP0156 NEW)  # Allow ALIAS targets for link dependencies

# Emit warnings on deprecated or legacy CMake features
set(CMAKE_WARN_DEPRECATED TRUE)

# Optional: clean up obsolete warnings (only enable if really needed)
# set(CMAKE_POLICY_WARNING_CMP0012 TRUE)

# Suppress redundant regeneration messages (less CLI noise)
# set(CMAKE_SUPPRESS_REGENERATION ON)

cmake_policy(SET CMP0135 NEW)
# set(CMAKE_SUPPRESS_DEVELOPER_ERRORS TRUE)
# set(CMAKE_WARN_DEPRECATED TRUE)

# ----------------------------------------------------------------------
# Timestamped Configuration Header
# ----------------------------------------------------------------------

string(TIMESTAMP CURRENT_DATETIME "%Y-%m-%d %H:%M:%S")
message(STATUS "")
message(STATUS "############################################################")
message(STATUS "## Developer tools for ICC color profiles ##")
message(STATUS "############################################################")
message(STATUS "## Configuration and Build started at ${CURRENT_DATETIME} ##")
message(STATUS "")

# ----------------------------------------------------------------------
# Verbose Makefile Output (for Make/Ninja generators)
# ----------------------------------------------------------------------

set(CMAKE_VERBOSE_MAKEFILE ON)

# ----------------------------------------------------------------------
# Define Default Configurations (for MSVC/Xcode generators)
# ----------------------------------------------------------------------

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Available configurations" FORCE)
  set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug)
  set_property(GLOBAL PROPERTY RELEASE_CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
endif()

# ----------------------------------------------------------------------
# Project Identity and Versioning
# ----------------------------------------------------------------------

project(RefIccMAX LANGUAGES C CXX)
set(PROJECT_UP_NAME "REFICCMAX")
set(PROJECT_DOWN_NAME "reficcmax")

# Version components (4-part version: 2.3.1.2)
set(${PROJECT_UP_NAME}_MAJOR_VERSION 2)
set(${PROJECT_UP_NAME}_MINOR_VERSION 3)
set(${PROJECT_UP_NAME}_MICRO_VERSION 1)
set(${PROJECT_UP_NAME}_PATCH_VERSION 2)

# Composite versions
set(${PROJECT_UP_NAME}_VERSION
  "${${PROJECT_UP_NAME}_MAJOR_VERSION}.${${PROJECT_UP_NAME}_MINOR_VERSION}.${${PROJECT_UP_NAME}_MICRO_VERSION}.${${PROJECT_UP_NAME}_PATCH_VERSION}"
)

set(${PROJECT_UP_NAME}_VERSION_MAJORMINOR
  "${${PROJECT_UP_NAME}_MAJOR_VERSION}.${${PROJECT_UP_NAME}_MINOR_VERSION}"
)

# Mirror to ${PROJECT_NAME}
set(${PROJECT_NAME}_VERSION "${${PROJECT_UP_NAME}_VERSION}")
set(${PROJECT_NAME}_VERSION_MAJORMINOR "${${PROJECT_UP_NAME}_VERSION_MAJORMINOR}")

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Descriptive metadata (for packaging, install, CI)
set(PACKAGE_NAME "${PROJECT_DOWN_NAME}")
set(PACKAGE_DESCRIPTION
  "Developer tools for ICC color profiles."
)

# ----------------------------------------------------------------------
# Define project root (relative to CMake source directory)
# This may be overridden on CLI or CI environments
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}/../..")

# ----------------------------------------------------------------------
# Build Configuration Summary
# ----------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== Project Configuration Summary ===")
message(STATUS "  Project Name       : ${PROJECT_NAME}")
message(STATUS "  Internal Name (UP) : ${PROJECT_UP_NAME}")
message(STATUS "  Version (Full)     : ${${PROJECT_UP_NAME}_VERSION}")
message(STATUS "  Version (Short)    : ${${PROJECT_NAME}_VERSION_MAJORMINOR}")
message(STATUS "  C++ Standard       : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Description        : ${PACKAGE_DESCRIPTION}")
message(STATUS "=========================================")
message(STATUS "")

# ----------------------------------------------------------------------
# Host System Diagnostics
# Print key info for reproducibility and debugging
# ----------------------------------------------------------------------
message(STATUS "")
message(STATUS "===  Build Environment Diagnostics ===")
message(STATUS "Source Directory       : ${CMAKE_SOURCE_DIR}")
message(STATUS "Project Root Directory : ${PROJECT_ROOT_DIR}")
message(STATUS "System Name            : ${CMAKE_SYSTEM_NAME}")
message(STATUS "System Version         : ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Processor Architecture : ${CMAKE_SYSTEM_PROCESSOR}")
execute_process(COMMAND hostname OUTPUT_VARIABLE BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND whoami OUTPUT_VARIABLE BUILD_USER OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Build Host             : ${BUILD_HOST} (${BUILD_USER})")
message(STATUS "MSBuild Path            : $ENV{MSBUILD_PATH}")
message(STATUS "=========================================")
message(STATUS "")

# ----------------------------------------------------------------------
# Sanitize broken MSVC linker flags (e.g., misused /LF:)
# This ensures clean linker invocation on Windows hosts
foreach(flag_var IN ITEMS
    CMAKE_EXE_LINKER_FLAGS
    CMAKE_SHARED_LINKER_FLAGS
    CMAKE_MODULE_LINKER_FLAGS)
  string(REPLACE "/LF:" "/LIBPATH:" ${flag_var} "${${flag_var}}")
endforeach()

# ----------------------------------------------------------------------
# Spectre Variant 1 Mitigation (Optional Hardening)
# ----------------------------------------------------------------------

option(ENABLE_SPECTRE_MITIGATION "Enable /Qspectre on MSVC for Spectre V1 mitigation" OFF)

if(MSVC AND ENABLE_SPECTRE_MITIGATION)
  message(STATUS "[HARDENING] Spectre mitigation enabled: /Qspectre")
  add_compile_options(/Qspectre)
endif()

# ----------------------------------------------------------------------
# Host Environment Diagnostics
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Environment Diagnostics ###")

# Detect path separator
set(PATH_SEPARATOR $<IF:$<BOOL:${WIN32}>,";",":">)

# Pretty-print the PATH variable (numbered entries)
string(REPLACE "${PATH_SEPARATOR}" ";" PATH_LIST "$ENV{PATH}")
list(LENGTH PATH_LIST PATH_LENGTH)

message(STATUS "PATH Entries:")
foreach(INDEX RANGE 0 ${PATH_LENGTH})
  math(EXPR IDX "${INDEX} - 1")
  if(IDX GREATER_EQUAL 0)
    list(GET PATH_LIST ${IDX} PATH_ENTRY)
    message(STATUS "  [${IDX}] ${PATH_ENTRY}")
  endif()
endforeach()

# Core CMake Environment Variables
message(STATUS "CMAKE_PREFIX_PATH        : ${CMAKE_PREFIX_PATH}")
message(STATUS "CMAKE_INSTALL_PREFIX     : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_MODULE_PATH        : ${CMAKE_MODULE_PATH}")

# ----------------------------------------------------------------------
# Compiler and Toolchain Diagnostics
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Compiler & Toolchain Info ###")
message(STATUS "C++ Compiler            : ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Compiler ID         : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ Compiler Version    : ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C Compiler              : ${CMAKE_C_COMPILER}")
message(STATUS "C Compiler Version      : ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "Linker                  : ${CMAKE_LINKER}")

# ----------------------------------------------------------------------
# Platform-Specific Diagnostics
# ----------------------------------------------------------------------

if(APPLE)
  message(STATUS "")
  message(STATUS "### macOS Specific Details ###")
  message(STATUS "macOS SDK Path         : ${CMAKE_OSX_SYSROOT}")
  message(STATUS "Deployment Target       : ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

if(WIN32)
  message(STATUS "")
  message(STATUS "### Windows Specific Details ###")
  message(STATUS "Windows SDK Version     : ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

  # ----------------------------------------------------------------------
  # MSBuild Detection for Windows
  # ----------------------------------------------------------------------
  if(NOT DEFINED ENV{MSBUILD_PATH})
    # Attempt to auto-detect common VS2022+ MSBuild path
    set(_default_msbuild "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe")

    if(EXISTS "${_default_msbuild}")
      set(ENV{MSBUILD_PATH} "${_default_msbuild}")
      message(STATUS "MSBuild path auto-detected and set to: ${_default_msbuild}")
    else()
      message(WARNING "MSBuild Path is not defined in environment variables and default path not found.")
    endif()
  endif()

  # Always print final value
  message(STATUS "MSBuild Path            : $ENV{MSBUILD_PATH}")
  message(STATUS "Linker Flags            : ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(UNIX AND NOT APPLE)
  message(STATUS "")
  message(STATUS "### Linux/Unix Specific Details ###")
  execute_process(
    COMMAND uname -a
    OUTPUT_VARIABLE LINUX_SYSTEM_INFO
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "System Info            : ${LINUX_SYSTEM_INFO}")
  message(STATUS "C++ Compiler           : ${CMAKE_CXX_COMPILER}")
  message(STATUS "Linker Flags            : ${CMAKE_EXE_LINKER_FLAGS}")
endif()

# ----------------------------------------------------------------------
# Cross-Platform Debugging Summary
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Cross-Platform Debugging ###")
message(STATUS "Build Type              : ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix          : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build Generator         : ${CMAKE_GENERATOR}")
message(STATUS "Build Tool              : ${CMAKE_MAKE_PROGRAM}")

# ----------------------------------------------------------------------
# Build Configuration Summary
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Build Configuration ###")
message(STATUS "Build Type              : ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard            : C++${CMAKE_CXX_STANDARD}")
message(STATUS "C++ Flags               : ${CMAKE_CXX_FLAGS}")
message(STATUS "C Flags                 : ${CMAKE_C_FLAGS}")
message(STATUS "Active Config Flags     : ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")


# ----------------------------------------------------------------------
# Third-Party Dependencies
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Dependency Summary ###")

# --- LibXml2 ---
find_package(LibXml2 REQUIRED)
message(STATUS "LibXml2                : ${LIBXML2_LIBRARIES} (version: ${LIBXML2_VERSION})")

# --- nlohmann_json ---
find_package(nlohmann_json REQUIRED)
message(STATUS "nlohmann_json          : ${nlohmann_json_DIR} (version: ${nlohmann_json_VERSION})")

# --- TIFF ---
find_package(TIFF REQUIRED)
message(STATUS "TIFF                   : ${TIFF_LIBRARIES} (version: ${TIFF_VERSION})")
message(STATUS "TIFF Include Directory : ${TIFF_INCLUDE_DIR}")

# --- PNG ---
message(STATUS "Checking for PNG...")
find_package(PNG REQUIRED)

if(PNG_FOUND)
  message(STATUS "PNG Library            : ${PNG_LIBRARIES}")
  message(STATUS "PNG Include Directory  : ${PNG_INCLUDE_DIRS}")
  message(STATUS "PNG Version            : ${PNG_VERSION}")
else()
  message(FATAL_ERROR " PNG not found. Please install libpng-dev or install via vcpkg.")
endif()

# --- JPEG ---
message(STATUS "Checking for JPEG...")
find_package(JPEG REQUIRED)
if(JPEG_FOUND)
  message(STATUS "JPEG Library           : ${JPEG_LIBRARIES}")
  message(STATUS "JPEG Include Directory : ${JPEG_INCLUDE_DIR}")
else()
  message(FATAL_ERROR " JPEG not found. Please install libjpeg-dev or install via vcpkg.")
endif()

# ----------------------------------------------------------------------
# Include Directories
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Include Paths ###")
message(STATUS "IccProfLib             : ${PROJECT_ROOT_DIR}/IccProfLib")
message(STATUS "IccXML                 : ${PROJECT_ROOT_DIR}/IccXML")

# ----------------------------------------------------------------------
# Link Directories
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Link Paths ###")
message(STATUS "IccProfLib             : ${CMAKE_BINARY_DIR}/IccProfLib")
message(STATUS "IccXML                 : ${CMAKE_BINARY_DIR}/IccXML")
message(STATUS "Fallback               : /usr/local/lib")

# ----------------------------------------------------------------------
# Debug Library Paths
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Debug Library Paths ###")
message(STATUS "IccProfLib Debug Path : ${PROJECT_ROOT_DIR}/Build/IccProfLib")
message(STATUS "IccXML Debug Path     : ${PROJECT_ROOT_DIR}/Build/IccXML")

# ----------------------------------------------------------------------
# CI/CD Environment Detection
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### CI/CD Integration ###")

if(DEFINED ENV{CI})
  message(STATUS "CI/CD Detected")
  message(STATUS "Build Number           : $ENV{BUILD_NUMBER}")
  message(STATUS "Branch Name            : $ENV{BRANCH_NAME}")
  message(STATUS "Commit Hash            : $ENV{GIT_COMMIT}")
  message(STATUS "Pipeline URL           : $ENV{CI_PIPELINE_URL}")
else()
  message(STATUS " CI/CD Integration      : Not Detected")
endif()

# ----------------------------------------------------------------------
# System Install Paths via GNUInstallDirs
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "Initializing Install Paths with GNUInstallDirs...")
include(GNUInstallDirs)
message(STATUS "GNU Install Include Dir: ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "GNU Install Library Dir: ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "GNU Install CMake Dir  : ${CMAKE_INSTALL_DATAROOTDIR}/cmake")

# ----------------------------------------------------------------------
# Build Feature Toggles
# ----------------------------------------------------------------------
message(STATUS "")
message(STATUS "### Build Options ###")
# Build and feature flags
option(ENABLE_TESTS               "Build tests (requires static libs)"                  ON)
option(ENABLE_TOOLS               "Build tools (command-line + GUI)"                    ON)
option(ENABLE_SHARED_LIBS         "Build dynamic link libraries"                        ON)
option(ENABLE_STATIC_LIBS         "Build static libraries"                              ON)
option(ENABLE_ICCXML              "Build IccXML library support"                        ON)
option(ICC_TRACE_NAN_ENABLED      "Enable tracing NaN inputs in debug builds"           OFF)
option(ICC_CLUT_DEBUG             "Enable CLUT debugging support"                       OFF)
option(ICC_ENABLE_ASSERTS         "Enable ICC_ASSERT traps and debug assertions"        OFF)
option(ICC_LOG_SAFE               "Enable ICC_LOG_SAFE_VAL bounds-checked logging"      OFF)
option(ENABLE_SANITIZERS          "Enable runtime sanitizers (ASan, UBSan, etc.)"       OFF)
option(ENABLE_ASAN                "Enable AddressSanitizer only"                        OFF)
option(ENABLE_UBSAN               "Enable UndefinedBehaviorSanitizer only"              OFF)
option(ENABLE_TSAN                "Enable ThreadSanitizer (conflicts with ASan)"        OFF)
option(ENABLE_MSAN                "Enable MemorySanitizer (Clang only)"                 OFF)
option(ENABLE_LSAN                "Enable LeakSanitizer standalone"                     OFF)
option(ENABLE_FUZZING             "Enable fuzzing instrumentation (libFuzzer)"          OFF)
option(ENABLE_SPECTRE_MITIGATION  "Enable /Qspectre on MSVC for Spectre V1 mitigation"  OFF)
option(ENABLE_USEICCDEVNAMESPACE  "Use iccDEV namespace wrapping"                       OFF)

if(ENABLE_USEICCDEVNAMESPACE)
  message(STATUS ">>> Namespace wrapping enabled (iccDEV)")
  add_compile_definitions(USEICCDEVNAMESPACE=1)
endif()

# Apply compile definitions for feature toggles
if(ICC_TRACE_NAN_ENABLED)
  add_compile_definitions(ICC_TRACE_NAN_ENABLED=1)
endif()

if(ICC_CLUT_DEBUG)
  add_compile_definitions(ICC_CLUT_DEBUG=1)
endif()

if(ICC_ENABLE_ASSERTS)
  add_compile_definitions(ICC_ENABLE_ASSERTS=1)
endif()

if(ICC_LOG_SAFE)
  add_compile_definitions(ICC_LOG_SAFE=1)
endif()

# ----------------------------------------------------------------------
# Build Status Output
# ----------------------------------------------------------------------
message(STATUS "ENABLE_TESTS               = ${ENABLE_TESTS}")
message(STATUS "ENABLE_TOOLS               = ${ENABLE_TOOLS}")
message(STATUS "ENABLE_SHARED_LIBS         = ${ENABLE_SHARED_LIBS}")
message(STATUS "ENABLE_STATIC_LIBS         = ${ENABLE_STATIC_LIBS}")
message(STATUS "ENABLE_ICCXML              = ${ENABLE_ICCXML}")
message(STATUS "ICC_TRACE_NAN_ENABLED      = ${ICC_TRACE_NAN_ENABLED}")
message(STATUS "ICC_CLUT_DEBUG             = ${ICC_CLUT_DEBUG}")
message(STATUS "ICC_ENABLE_ASSERTS         = ${ICC_ENABLE_ASSERTS}")
message(STATUS "ICC_LOG_SAFE               = ${ICC_LOG_SAFE}")
message(STATUS "ENABLE_SANITIZERS          = ${ENABLE_SANITIZERS}")
message(STATUS "ENABLE_ASAN                = ${ENABLE_ASAN}")
message(STATUS "ENABLE_UBSAN               = ${ENABLE_UBSAN}")
message(STATUS "ENABLE_TSAN                = ${ENABLE_TSAN}")
message(STATUS "ENABLE_MSAN                = ${ENABLE_MSAN}")
message(STATUS "ENABLE_LSAN                = ${ENABLE_LSAN}")
message(STATUS "ENABLE_FUZZING             = ${ENABLE_FUZZING}")
message(STATUS "ENABLE_SPECTRE_MITIGATION  = ${ENABLE_SPECTRE_MITIGATION}")
message(STATUS "")

# ----------------------------------------------------------------------
# Sanitizer Configuration
# ----------------------------------------------------------------------

# Validate conflicting sanitizer options
if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_SANITIZERS))
  message(FATAL_ERROR "ThreadSanitizer cannot be used with AddressSanitizer")
endif()

if(ENABLE_MSAN AND (ENABLE_ASAN OR ENABLE_SANITIZERS OR ENABLE_TSAN))
  message(FATAL_ERROR "MemorySanitizer cannot be combined with other sanitizers")
endif()

# Compose sanitizer flags
set(SANITIZER_FLAGS "")
set(SANITIZER_LIST "")

if(ENABLE_FUZZING)
  message(STATUS ">>> Fuzzing instrumentation enabled (libFuzzer + ASan + UBSan)")
  if(MSVC)
    message(WARNING "Fuzzing with libFuzzer is not supported on MSVC")
  else()
    list(APPEND SANITIZER_LIST "fuzzer" "address" "undefined")
  endif()
elseif(ENABLE_SANITIZERS)
  message(STATUS ">>> Sanitizers enabled: AddressSanitizer and UndefinedBehaviorSanitizer active")
  if(MSVC)
    list(APPEND SANITIZER_FLAGS "/fsanitize=address")
    # MSVC doesn't have UBSan, but has runtime checks via /RTC1 in debug
  else()
    list(APPEND SANITIZER_LIST "address" "undefined")
  endif()
else()
  # Individual sanitizer options
  if(ENABLE_ASAN)
    message(STATUS ">>> AddressSanitizer enabled")
    if(MSVC)
      list(APPEND SANITIZER_FLAGS "/fsanitize=address")
    else()
      list(APPEND SANITIZER_LIST "address")
    endif()
  endif()
  
  if(ENABLE_UBSAN)
    message(STATUS ">>> UndefinedBehaviorSanitizer enabled")
    if(MSVC)
      message(WARNING "UndefinedBehaviorSanitizer not available on MSVC")
    else()
      list(APPEND SANITIZER_LIST "undefined")
    endif()
  endif()
  
  if(ENABLE_TSAN)
    message(STATUS ">>> ThreadSanitizer enabled")
    if(MSVC)
      message(WARNING "ThreadSanitizer not available on MSVC")
    else()
      list(APPEND SANITIZER_LIST "thread")
    endif()
  endif()
  
  if(ENABLE_MSAN)
    message(STATUS ">>> MemorySanitizer enabled (Clang only)")
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "MemorySanitizer requires Clang compiler")
    endif()
    if(MSVC)
      message(FATAL_ERROR "MemorySanitizer not available on MSVC")
    else()
      list(APPEND SANITIZER_LIST "memory")
    endif()
  endif()
  
  if(ENABLE_LSAN)
    message(STATUS ">>> LeakSanitizer enabled")
    if(MSVC)
      message(WARNING "LeakSanitizer not available on MSVC")
    else()
      list(APPEND SANITIZER_LIST "leak")
    endif()
  endif()
endif()

# Apply sanitizer flags for GCC/Clang
if(SANITIZER_LIST)
  string(REPLACE ";" "," SANITIZER_STRING "${SANITIZER_LIST}")
  list(APPEND SANITIZER_FLAGS "-fsanitize=${SANITIZER_STRING}")
  list(APPEND SANITIZER_FLAGS "-g" "-O1" "-fno-omit-frame-pointer")
endif()

# Apply all sanitizer flags
if(SANITIZER_FLAGS)
  string(JOIN " " FINAL_SAN_FLAGS ${SANITIZER_FLAGS})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FINAL_SAN_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FINAL_SAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FINAL_SAN_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FINAL_SAN_FLAGS}")
  message(STATUS ">>> Final sanitizer flags: ${FINAL_SAN_FLAGS}")
endif()


# ----------------------------------------------------------------------
# Build Context Detection: Top-Level vs Subproject
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Build Context Check ###")

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  # Top-level project: full install/build mode
  option(ENABLE_INSTALL_RIM "Enable install target for RefIccMAX." ON)
  message(STATUS "RefIccMAX is being built as a top-level project.")
  message(STATUS "ENABLE_INSTALL_RIM      = ${ENABLE_INSTALL_RIM}")
else()
  # Subproject: disable install to avoid polluting parent build
  set(ENABLE_INSTALL_RIM OFF CACHE BOOL "Install disabled in subproject mode" FORCE)
  set(ENABLE_STATIC_LIBS ON  CACHE BOOL "Force static linking in subproject" FORCE)
  set(ENABLE_SHARED_LIBS OFF CACHE BOOL "Disable shared libs in subproject" FORCE)

  message(STATUS "RefIccMAX included as subproject - installation is disabled.")
  message(STATUS "ENABLE_INSTALL_RIM      = ${ENABLE_INSTALL_RIM}")
  message(STATUS "ENABLE_STATIC_LIBS      = ${ENABLE_STATIC_LIBS}")
  message(STATUS "ENABLE_SHARED_LIBS      = ${ENABLE_SHARED_LIBS}")
endif()
# ----------------------------------------------------------------------
# Default Build Type Enforcement
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Enforcing Default Build Type ###")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  message(STATUS "No build type specified. Defaulting to: ${CMAKE_BUILD_TYPE}")
else()
  message(STATUS "Build type explicitly set to: ${CMAKE_BUILD_TYPE}")
endif()

# ----------------------------------------------------------------------
# Compiler Flag Setup (Baseline)
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Initializing Compiler Flags ###")

# Platform-Specific Flags
if(MSVC)
    # Debug: Full debug info, no optimization, runtime checks
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /Zi /Ob0 /Od /RTC1")
    
    # Release: Full optimization, no debug info
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /O2 /Ob2 /DNDEBUG")
    
    # RelWithDebInfo: Optimized with debug info (for profiling/debugging release builds)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD /Zi /O2 /Ob1 /DNDEBUG")
    
    # MinSizeRel: Minimize binary size
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MD /O1 /Ob1 /DNDEBUG")
    
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    message(STATUS "Configuring MSVC runtime and flags")
    message(STATUS "  DEBUG:          /MDd /Zi /Ob0 /Od /RTC1")
    message(STATUS "  RELEASE:        /MD /O2 /Ob2 /DNDEBUG")
    message(STATUS "  RELWITHDEBINFO: /MD /Zi /O2 /Ob1 /DNDEBUG")
    message(STATUS "  MINSIZEREL:     /MD /O1 /Ob1 /DNDEBUG")
    
elseif(UNIX)
    # Debug: Full debug info, no optimization, extra warnings
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -Wall -Wextra -DDEBUG")
    
    # Release: Full optimization (-O3 for maximum performance on ICC profile processing)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    
    # RelWithDebInfo: Moderate optimization with debug info
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -O2 -DNDEBUG")
    
    # MinSizeRel: Optimize for size
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG")
    
    message(STATUS "Configuring GCC/Clang runtime and flags")
    message(STATUS "  DEBUG:          -g -O0 -Wall -Wextra -DDEBUG")
    message(STATUS "  RELEASE:        -O3 -DNDEBUG")
    message(STATUS "  RELWITHDEBINFO: -g -O2 -DNDEBUG")
    message(STATUS "  MINSIZEREL:     -Os -DNDEBUG")
endif()

# ----------------------------------------------------------------------
# Architecture and Platform Detection
# ----------------------------------------------------------------------
if(APPLE)
  message(STATUS "Detected platform: macOS")

  # Host architecture (where CMake is running)
  set(_HOST_ARCH "${CMAKE_HOST_SYSTEM_PROCESSOR}")
  if(NOT _HOST_ARCH)
    execute_process(COMMAND uname -m
      OUTPUT_VARIABLE _HOST_ARCH
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  string(TOLOWER "${_HOST_ARCH}" _HOST_ARCH_LC)
  if(_HOST_ARCH_LC MATCHES "^arm64e?$")
    set(_HOST_ARCH "arm64")
  elseif(_HOST_ARCH_LC MATCHES "^(x86_64|amd64)$")
    set(_HOST_ARCH "x86_64")
  endif()
  message(STATUS "Host architecture       : ${_HOST_ARCH}")

  # Detect Rosetta (1 means translated)
  execute_process(
    COMMAND /usr/sbin/sysctl -n sysctl.proc_translated
    OUTPUT_VARIABLE _ROSETTA
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_ROSETTA STREQUAL "1")
    message(STATUS "Rosetta translation     : active (terminal running under Rosetta)")
  else()
    message(STATUS "Rosetta translation     : not active")
  endif()

  # Target architectures: respect user/toolchain if already set
  if(DEFINED CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "")
    set(_REQ_ARCHS "${CMAKE_OSX_ARCHITECTURES}")
    # Normalize list elements
    separate_arguments(_REQ_ARCHS)
    set(_NORM_ARCHS "")
    foreach(a IN LISTS _REQ_ARCHS)
      string(TOLOWER "${a}" a_lc)
      if(a_lc MATCHES "^arm64e?$")
        list(APPEND _NORM_ARCHS "arm64")
      elseif(a_lc MATCHES "^(x86_64|amd64)$")
        list(APPEND _NORM_ARCHS "x86_64")
      else()
        message(FATAL_ERROR "Unsupported requested macOS arch: '${a}'")
      endif()
    endforeach()
    list(REMOVE_DUPLICATES _NORM_ARCHS)
    list(JOIN _NORM_ARCHS ";" _ARCHS_FINAL)
    if(NOT _ARCHS_FINAL STREQUAL "${CMAKE_OSX_ARCHITECTURES}")
      set(CMAKE_OSX_ARCHITECTURES "${_ARCHS_FINAL}" CACHE STRING "macOS architectures" FORCE)
    endif()
    message(STATUS "Requested target archs  : ${CMAKE_OSX_ARCHITECTURES}")
  else()
    # Default to host arch if nothing specified
    set(CMAKE_OSX_ARCHITECTURES "${_HOST_ARCH}" CACHE STRING "macOS architectures" FORCE)
    message(STATUS "Defaulting target archs : ${CMAKE_OSX_ARCHITECTURES}")
  endif()

  # Add compile definitions for single-arch builds
  list(LENGTH CMAKE_OSX_ARCHITECTURES _ARCHLEN)
  if(_ARCHLEN EQUAL 1)
    list(GET CMAKE_OSX_ARCHITECTURES 0 _ONLY_ARCH)
    if(_ONLY_ARCH STREQUAL "arm64")
      add_compile_definitions(ARM64)
      message(STATUS "Macro defined           : ARM64")
    endif()
  endif()

  message(STATUS "Final macOS architectures: ${CMAKE_OSX_ARCHITECTURES}")

elseif(UNIX AND NOT APPLE)
  message(STATUS "Detected platform: Linux")

  # Recommended flags for Linux/Clang or GCC
  add_compile_options(
    -Wall
    -Wno-overloaded-virtual
    -Wno-switch
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-missing-field-initializers
  )
  message(STATUS "Linux build: Standard warning flags applied.")

elseif(WIN32)
  message(STATUS "Detected platform: Windows")

  # Windows + MSVC flags
  add_compile_options(/std:c++17 /D_WINDOWS /EHsc)
  message(STATUS "Windows build: Compiler flags set for MSVC with C++17 and exceptions enabled.")

else()
  message(FATAL_ERROR "Unsupported or unknown platform. Aborting build configuration.")
endif()

# Prevent old-style path-based linking
unset(IccProfLib2_LIBRARIES CACHE)
unset(IccProfLib2_LIBRARY CACHE)
unset(IccProfLib2_LIB CACHE)
unset(IccProfLib2 CACHE)

# ----------------------------------------------------------------------
# Third-Party Dependencies
# ----------------------------------------------------------------------
message(STATUS "")
message(STATUS "## Third Party Dependencies ##")
find_package(LibXml2 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(TIFF REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

message(STATUS "")
#
# Target installation folders
#

SET (TARGET_INCLUDE_FOLDER
    ${PROJECT_NAME}
    CACHE STRING
    "Optional folder below system include folder to install include files."
    )

IF(NOT CMAKE_MODULES_INSTALL_DIR)
  SET (CMAKE_MODULES_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_DOWN_NAME}"
    CACHE STRING
    "The folder where to install cmake files."
    )
ENDIF(NOT CMAKE_MODULES_INSTALL_DIR)


SET (TARGET_PKGCONFIG_FOLDER
    "pkgconfig"
    CACHE STRING
    "The folder below system library folder where to install pkgconfig files."
    )

SET (TARGET_DOCUMENTATION_FOLDER
    "share/doc/${PACKAGE_NAME}-api"
    CACHE STRING
    "The folder below system prefix where to install api documentation files."
    )

# additional cmake modules for loading libraries
SET (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/Modules/")


SET( TOP_SOURCE_DIR ../.. )
INCLUDE_DIRECTORIES ( ${TOP_SOURCE_DIR}/IccProfLib/ )

# external dependencies
FIND_PROGRAM(CMAKE_AR_TMP NAMES ar)

#
# Core library
#
message(STATUS "## Project Configuration ##")
message(STATUS "Adding subdirectory for IccProfLib.")
add_subdirectory(IccProfLib)

# Set default link target for IccProfLib
# Use CACHE INTERNAL so that it's available in Tools/* subdirectories
set(TARGET_LIB_ICCPROFLIB IccProfLib2 CACHE INTERNAL "Link target for IccProfLib2")

#
# Optional external dependency: LibXML2
#
if(ENABLE_ICCXML)
  message(STATUS "ICCXML support enabled")

  if(USE_SYSTEM_LIBXML2)
    find_package(LibXml2 REQUIRED)
  else()
    find_package(LibXml2 QUIET)
  endif()

  if(LibXml2_FOUND)
    message(STATUS "Found LibXml2 via system or toolchain integration")
    set(HAVE_LIBXML2 TRUE)

    # Use imported CMake target if available
    if(TARGET LibXml2::LibXml2)
      set(LIBXML2_LIBRARIES LibXml2::LibXml2)
    else()
      # Legacy fallback (non-target based CMake find)
      include_directories(${LIBXML2_INCLUDE_DIR})
      set(LIBXML2_LIBRARIES ${LIBXML2_LIBRARIES})
    endif()
  else()
    message(WARNING "LibXml2 not found; falling back to internal IccXML/libxml2")

    set(HAVE_LIBXML2 TRUE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/IccXML/libxml2)
    add_subdirectory(IccXML/libxml2)
    set(LIBXML2_LIBRARIES xml2-static)
  endif()
endif()

#
# XML library
#
message(STATUS "Adding subdirectory for IccXML.")
add_subdirectory(IccXML)

# Set default link target for IccXML (matches build configuration)
IF(ENABLE_SHARED_LIBS)
  set(TARGET_LIB_ICCXML IccXML2 CACHE INTERNAL "Link target for IccXML2")
ELSE()
  set(TARGET_LIB_ICCXML IccXML2-static CACHE INTERNAL "Link target for IccXML2")
ENDIF()

# Ensure IccXML headers are available globally
include_directories(${TOP_SOURCE_DIR}/IccXML/IccLibXML/)

# Diagnostic: Output current linker flags

message(STATUS "CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS = ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "CMAKE_MODULE_LINKER_FLAGS = ${CMAKE_MODULE_LINKER_FLAGS}")


# Ensure nlohmann_json is found
find_package(nlohmann_json REQUIRED)
message(STATUS "nlohmann_json library found: ${nlohmann_json_DIR}")

 # Function to add and log subdirectories
function(add_tool_subdirectory TOOL_SUBDIRECTORY)
    message(STATUS "Adding Subdirectory: ${TOOL_SUBDIRECTORY}...")
    add_subdirectory(${TOOL_SUBDIRECTORY})
    message(STATUS "Successfully added Subdirectory: ${TOOL_SUBDIRECTORY}")
endfunction()

IF(ENABLE_TOOLS)
  # Verify nlohmann_json is configured properly
  IF(nlohmann_json_DIR)
    MESSAGE(STATUS "Found nlohmann JSON library at: ${nlohmann_json_DIR}")

    # Use nlohmann_json include directory
    include_directories(${nlohmann_json_DIR})
  ELSE()
    MESSAGE(WARNING "nlohmann JSON library not found. Skipping..")
    RETURN() # Exit script for tools if dependency is not found
  ENDIF()

  # Add XML-related tools
  message(STATUS "Adding Subdirectory IccFromXml.")
  ADD_SUBDIRECTORY(Tools/IccFromXml)
  message(STATUS "Adding Subdirectory IccToXml.")
  ADD_SUBDIRECTORY(Tools/IccToXml)
  message(STATUS "Adding Subdirectory IccJpegDump.")
  ADD_SUBDIRECTORY(Tools/IccJpegDump)
  message(STATUS "Adding Subdirectory IccApplyNamedCmm.")
  ADD_SUBDIRECTORY(Tools/IccApplyNamedCmm)
  message(STATUS "Adding Subdirectory IccDumpProfile.")
  ADD_SUBDIRECTORY(Tools/IccDumpProfile)
  message(STATUS "Adding Subdirectory IccDumpRoundTrip.")
  ADD_SUBDIRECTORY(Tools/IccRoundTrip)
  message(STATUS "Adding Subdirectory IccFromCube.")
  ADD_SUBDIRECTORY(Tools/IccFromCube)
  message(STATUS "Adding Subdirectory IccV5DspObsToV4Dsp.")
  ADD_SUBDIRECTORY(Tools/IccV5DspObsToV4Dsp)
  message(STATUS "Adding Subdirectory IccApplyToLink.")
  ADD_SUBDIRECTORY(Tools/IccApplyToLink)
  message(STATUS "Adding Subdirectory IccApplySearch.")
  ADD_SUBDIRECTORY(Tools/IccApplySearch)

  # Ensure IccXML-dependent tools are built after IccXML library (safety net)
  IF(TARGET iccFromXml AND TARGET ${TARGET_LIB_ICCXML})
    ADD_DEPENDENCIES(iccFromXml ${TARGET_LIB_ICCXML})
  ENDIF()
  IF(TARGET iccToXml AND TARGET ${TARGET_LIB_ICCXML})
    ADD_DEPENDENCIES(iccToXml ${TARGET_LIB_ICCXML})
  ENDIF()


# --- PNG ---
message(STATUS "Checking for PNG...")
find_package(PNG REQUIRED)

if(PNG_FOUND)
  message(STATUS "PNG Library            : ${PNG_LIBRARIES}")
  message(STATUS "PNG Include Directory  : ${PNG_INCLUDE_DIRS}")
  message(STATUS "PNG Version            : ${PNG_VERSION}")
else()
  message(FATAL_ERROR " PNG not found. Please install libpng-dev or install via vcpkg.")
endif()
message(STATUS "Adding Subdirectory IccPngDump.")
ADD_SUBDIRECTORY(Tools/IccPngDump)


    message(STATUS "Adding Subdirectory IccDEVCmm.")
    ADD_SUBDIRECTORY(Tools/IccDEVCmm)


  # Check for TIFF library
  find_package(TIFF REQUIRED)
  if (TIFF_FOUND)
    include_directories(${TIFF_INCLUDE_DIR})
    message(STATUS "TIFF library found: ${TIFF_LIBRARIES}")

    # Add TIFF-dependent tools
    message(STATUS "Adding Subdirectory IccApplyProfiles.")
    ADD_SUBDIRECTORY(Tools/IccApplyProfiles)
    message(STATUS "Adding subdirectory IccSpecSepToTiff.")
    ADD_SUBDIRECTORY(Tools/IccSpecSepToTiff)
    message(STATUS "Adding Subdirectory IccTiffDump.")
    ADD_SUBDIRECTORY(Tools/IccTiffDump)
  else()
    message(FATAL_ERROR "TIFF library not found. Please install libtiff-dev.")
  endif()

  # Configure wxWidgets
  find_package(wxWidgets COMPONENTS core base REQUIRED)
  if (wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
      ADD_SUBDIRECTORY(Tools/wxProfileDump)
    message(STATUS "wxWidgets found and configured")
  else()
    message(FATAL_ERROR "wxWidgets not found. Please install it manually or via vcpkg.")
  endif()

ENDIF(ENABLE_TOOLS)

IF( ENABLE_TESTS )
  ADD_SUBDIRECTORY( Testing )
ENDIF( ENABLE_TESTS )

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Uninstall.cmake"
    IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET( uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Uninstall.cmake")

IF( CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "64" )
  SET( ARCH 64 )
ELSE()
  SET( ARCH 32 )
ENDIF()
IF( NOT CMAKE_SYSTEM_NAME MATCHES Darwin )
  SET( GNUTAR_OPTS "--owner user:1000 --group group:500" )
ENDIF()
MESSAGE( "Configured ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}" )

ADD_CUSTOM_TARGET( dist-bin
    COMMAND test -d bin-temp && rm -rv bin-temp || echo "start fresh"
    COMMAND mkdir bin-temp && cd bin-temp && cmake -DENABLE_STATIC_LIBS=ON -DENABLE_SHARED_LIBS=OFF ${CMAKE_SOURCE_DIR} && make -j8 && make dist-bin2
    COMMAND ls -l bin-temp/${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing
    COMMAND echo "Dependencies for iccTiffDump:"
    COMMAND test ${CMAKE_SYSTEM_NAME} = Linux && ldd bin-temp/${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/iccTiffDump || echo ""
    COMMAND test ${CMAKE_SYSTEM_NAME} = Darwin && otool -L bin-temp/${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/iccTiffDump || echo ""
    COMMAND rm -r bin-temp
    COMMAND tar tzvf ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}.tar.gz
    COMMAND echo "We are in:"
    COMMAND pwd
    COMMAND ls -l ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}.tar.gz
    COMMENT Make a ${PROJECT_NAME} binary package
    VERBATIM
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

ADD_CUSTOM_TARGET( dist-bin2
    DEPENDS check
    COMMAND mkdir -p ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/IccApplyNamedCmm ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccApplyProfiles ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccApplySearch ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccApplyToLink ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccDumpProfile ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccDumpProfileGui ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/ || echo ""
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccFromXML ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccFromCube ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccToXml ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccRoundTrip ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccSpecSepToTiff ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccTiffDump ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccPngDump ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccV5DspObsToV4Dsp ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/CreateAllProfiles.sh ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/RunTests.sh ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
    COMMAND echo tar cf - ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/ --owner user:1000 --group group:500 | gzip -9 > ../${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}.tar.gz
    COMMAND tar cf - ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/ ${GNUTAR_OPTS} | gzip -9 > ../${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}.tar.gz
                COMMENT Helper for making a ${PROJECT_NAME} binary package
                VERBATIM
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                )

#
# Misc.
#

#cmake settings for use by find_package
CONFIGURE_FILE (
                "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                @ONLY
        )

IF(ENABLE_INSTALL_RIM)
  INSTALL ( FILES
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                DESTINATION ${CMAKE_MODULES_INSTALL_DIR} COMPONENT dev
        )
ENDIF(ENABLE_INSTALL_RIM)
