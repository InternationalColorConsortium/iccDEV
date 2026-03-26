#################################################################################
# Cmake Project Configuration| iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-03-25 13:56:11 UTC by David Hoyt
#
# CMake Configuration:
# - Fix: CMake LTO override and build type matching
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

set(CMAKE_VERBOSE_MAKEFILE OFF)

# ----------------------------------------------------------------------
# Define Default Configurations (for MSVC/Xcode generators)
# ----------------------------------------------------------------------

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Available configurations" FORCE)
  set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug)
  set_property(GLOBAL PROPERTY RELEASE_CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
endif()

# ----------------------------------------------------------------------
# Compiler Selection for Unix (Clang preferred, GCC fallback)
# Skip when cross-compiling with Emscripten (toolchain file sets em++/emcc)
# ----------------------------------------------------------------------

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    # Check if compiler was specified via environment or command line
    if(NOT DEFINED CMAKE_C_COMPILER OR NOT DEFINED CMAKE_CXX_COMPILER)
        if(NOT DEFINED ENV{CC} AND NOT DEFINED ENV{CXX})
            # No user preference - auto-detect with Clang preferred
            find_program(CLANG_C_COMPILER NAMES clang-18 clang-17 clang-16 clang-15 clang-14 clang)
            find_program(CLANG_CXX_COMPILER NAMES clang++-18 clang++-17 clang++-16 clang++-15 clang++-14 clang++)
            if(CLANG_C_COMPILER AND CLANG_CXX_COMPILER)
                set(CMAKE_C_COMPILER "${CLANG_C_COMPILER}" CACHE FILEPATH "C compiler" FORCE)
                set(CMAKE_CXX_COMPILER "${CLANG_CXX_COMPILER}" CACHE FILEPATH "C++ compiler" FORCE)
                message(STATUS "Compiler auto-selection: Clang (${CLANG_CXX_COMPILER})")
            else()
                # Fallback to GCC
                find_program(GCC_C_COMPILER NAMES gcc-13 gcc-12 gcc-11 gcc-10 gcc)
                find_program(GCC_CXX_COMPILER NAMES g++-13 g++-12 g++-11 g++-10 g++)
                if(GCC_C_COMPILER AND GCC_CXX_COMPILER)
                    set(CMAKE_C_COMPILER "${GCC_C_COMPILER}" CACHE FILEPATH "C compiler" FORCE)
                    set(CMAKE_CXX_COMPILER "${GCC_CXX_COMPILER}" CACHE FILEPATH "C++ compiler" FORCE)
                    message(STATUS "Compiler auto-selection: GCC fallback (${GCC_CXX_COMPILER})")
                else()
                    message(STATUS "Compiler auto-selection: System default")
                endif()
            endif()
        else()
            message(STATUS "Compiler selection: User-specified via environment")
        endif()
    else()
        message(STATUS "Compiler selection: User-specified via CMake variables")
    endif()
endif()

# ----------------------------------------------------------------------
# Project Identity and Versioning
# ----------------------------------------------------------------------

project(RefIccMAX LANGUAGES C CXX)
set(PROJECT_UP_NAME "REFICCMAX")
set(PROJECT_DOWN_NAME "reficcmax")

# Version components (4-part version: 2.3.1.5)
set(${PROJECT_UP_NAME}_MAJOR_VERSION 2)
set(${PROJECT_UP_NAME}_MINOR_VERSION 3)
set(${PROJECT_UP_NAME}_MICRO_VERSION 1)
set(${PROJECT_UP_NAME}_PATCH_VERSION 5)

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

# Get Git commit hash for build traceability
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Add git commit to version string if available
if(GIT_COMMIT_HASH)
    set(${PROJECT_UP_NAME}_VERSION_FULL "${${PROJECT_UP_NAME}_VERSION}+${GIT_COMMIT_HASH}")
    if(EMSCRIPTEN)
        set(${PROJECT_UP_NAME}_VERSION_FULL "${${PROJECT_UP_NAME}_VERSION_FULL}.wasm")
    endif()
else()
    set(${PROJECT_UP_NAME}_VERSION_FULL "${${PROJECT_UP_NAME}_VERSION}")
endif()

# ----------------------------------------------------------------------
# Default Build Type (set early, before any display messages)
# ----------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# ----------------------------------------------------------------------
# Verbose Configuration Toggle
# ----------------------------------------------------------------------

option(VERBOSE_CONFIG "Enable verbose CMake configuration output" OFF)

if(VERBOSE_CONFIG)
  message(STATUS ">>> Verbose configuration output enabled")
endif()

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IDE/LSP support (clangd, vscode)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Position Independent Code globally for static library compatibility
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
if(GIT_COMMIT_HASH)
message(STATUS "  Version (Build)    : ${${PROJECT_UP_NAME}_VERSION_FULL}")
message(STATUS "  Git Commit         : ${GIT_COMMIT_HASH}")
endif()
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

# Pretty-print the PATH variable (numbered entries) — only with VERBOSE_CONFIG
if(VERBOSE_CONFIG)
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
endif()

# ----------------------------------------------------------------------
# Compiler and Toolchain Diagnostics
# ----------------------------------------------------------------------

message(STATUS "")
if(VERBOSE_CONFIG)
  message(STATUS "### Compiler & Toolchain Info ###")
  message(STATUS "C++ Compiler            : ${CMAKE_CXX_COMPILER}")
  message(STATUS "C++ Compiler ID         : ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "C++ Compiler Version    : ${CMAKE_CXX_COMPILER_VERSION}")
  message(STATUS "C Compiler              : ${CMAKE_C_COMPILER}")
  message(STATUS "C Compiler Version      : ${CMAKE_C_COMPILER_VERSION}")
  message(STATUS "Linker                  : ${CMAKE_LINKER}")
endif()

# ----------------------------------------------------------------------
# Platform-Specific Diagnostics
# ----------------------------------------------------------------------

if(APPLE)
  if(VERBOSE_CONFIG)
    message(STATUS "")
    message(STATUS "### macOS Specific Details ###")
    message(STATUS "macOS SDK Path         : ${CMAKE_OSX_SYSROOT}")
    message(STATUS "Deployment Target       : ${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()
endif()

if(WIN32)
  if(VERBOSE_CONFIG)
    message(STATUS "")
    message(STATUS "### Windows Specific Details ###")
    message(STATUS "Windows SDK Version     : ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
  endif()

  # ----------------------------------------------------------------------
  # MSBuild Detection for Windows
  # ----------------------------------------------------------------------
  if(NOT DEFINED ENV{MSBUILD_PATH})
    # Attempt to auto-detect common VS2022+ MSBuild path
    set(_default_msbuild "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe")

    if(EXISTS "${_default_msbuild}")
      set(ENV{MSBUILD_PATH} "${_default_msbuild}")
      if(VERBOSE_CONFIG)
        message(STATUS "MSBuild path auto-detected and set to: ${_default_msbuild}")
      endif()
    else()
      message(WARNING "MSBuild Path is not defined in environment variables and default path not found.")
    endif()
  endif()

  # Always print final value
  if(VERBOSE_CONFIG)
    message(STATUS "MSBuild Path            : $ENV{MSBUILD_PATH}")
    message(STATUS "Linker Flags            : ${CMAKE_EXE_LINKER_FLAGS}")
  endif()
endif()

if(UNIX AND NOT APPLE)
  if(VERBOSE_CONFIG)
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
endif()

# ----------------------------------------------------------------------
# Cross-Platform Debugging Summary
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Cross-Platform Debugging ###")
  message(STATUS "Build Type              : ${CMAKE_BUILD_TYPE}")
  message(STATUS "Install Prefix          : ${CMAKE_INSTALL_PREFIX}")
  message(STATUS "Build Generator         : ${CMAKE_GENERATOR}")
  message(STATUS "Build Tool              : ${CMAKE_MAKE_PROGRAM}")
endif()

# ----------------------------------------------------------------------
# Build Configuration Summary
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Build Configuration ###")
  message(STATUS "Build Type              : ${CMAKE_BUILD_TYPE}")
  message(STATUS "C++ Standard            : C++${CMAKE_CXX_STANDARD}")
  message(STATUS "C++ Flags               : ${CMAKE_CXX_FLAGS}")
  message(STATUS "C Flags                 : ${CMAKE_C_FLAGS}")
  message(STATUS "Active Config Flags     : ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
endif()


# ----------------------------------------------------------------------
# Third-Party Dependencies
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Dependency Summary ###")
endif()

# --- LibXml2 ---
find_package(LibXml2 REQUIRED)
if(VERBOSE_CONFIG)
  message(STATUS "LibXml2                : ${LIBXML2_LIBRARIES} (version: ${LIBXML2_VERSION})")
endif()

# --- nlohmann_json ---
find_package(nlohmann_json REQUIRED)
if(VERBOSE_CONFIG)
  message(STATUS "nlohmann_json          : ${nlohmann_json_DIR} (version: ${nlohmann_json_VERSION})")
endif()

# --- TIFF ---
find_package(TIFF REQUIRED)
if(VERBOSE_CONFIG)
  message(STATUS "TIFF                   : ${TIFF_LIBRARIES} (version: ${TIFF_VERSION})")
  message(STATUS "TIFF Include Directory : ${TIFF_INCLUDE_DIR}")
endif()

# --- PNG ---
if(VERBOSE_CONFIG)
  message(STATUS "Checking for PNG...")
endif()
find_package(PNG REQUIRED)

if(PNG_FOUND)
  if(VERBOSE_CONFIG)
    message(STATUS "PNG Library            : ${PNG_LIBRARIES}")
    message(STATUS "PNG Include Directory  : ${PNG_INCLUDE_DIRS}")
    message(STATUS "PNG Version            : ${PNG_VERSION}")
  endif()
else()
  message(FATAL_ERROR " PNG not found. Please install libpng-dev or install via vcpkg.")
endif()

# --- JPEG ---
if(VERBOSE_CONFIG)
  message(STATUS "Checking for JPEG...")
endif()
find_package(JPEG REQUIRED)
if(JPEG_FOUND)
  if(VERBOSE_CONFIG)
    message(STATUS "JPEG Library           : ${JPEG_LIBRARIES}")
    message(STATUS "JPEG Include Directory : ${JPEG_INCLUDE_DIR}")
  endif()
else()
  message(FATAL_ERROR " JPEG not found. Please install libjpeg-dev or install via vcpkg.")
endif()

# ----------------------------------------------------------------------
# Include Directories
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Include Paths ###")
  message(STATUS "IccProfLib             : ${PROJECT_ROOT_DIR}/IccProfLib")
  message(STATUS "IccXML                 : ${PROJECT_ROOT_DIR}/IccXML")
endif()

# ----------------------------------------------------------------------
# Link Directories
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Link Paths ###")
  message(STATUS "IccProfLib             : ${CMAKE_BINARY_DIR}/IccProfLib")
  message(STATUS "IccXML                 : ${CMAKE_BINARY_DIR}/IccXML")
  message(STATUS "Fallback               : /usr/local/lib")
endif()

# ----------------------------------------------------------------------
# Debug Library Paths
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Debug Library Paths ###")
  message(STATUS "IccProfLib Debug Path : ${PROJECT_ROOT_DIR}/Build/IccProfLib")
  message(STATUS "IccXML Debug Path     : ${PROJECT_ROOT_DIR}/Build/IccXML")
endif()

# ----------------------------------------------------------------------
# CI/CD Environment Detection
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
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
option(ENABLE_FUZZING             "Enable libFuzzer harnesses (Clang only)"             OFF)
option(ENABLE_UBSAN               "Enable UndefinedBehaviorSanitizer only"              OFF)
option(ENABLE_TSAN                "Enable ThreadSanitizer (conflicts with ASan)"        OFF)
option(ENABLE_MSAN                "Enable MemorySanitizer (Clang only)"                 OFF)
option(ENABLE_LSAN                "Enable LeakSanitizer standalone"                     OFF)
option(ENABLE_COVERAGE            "Enable code coverage instrumentation"                OFF)
option(ENABLE_PROFILING           "Enable gprof/perf profiling instrumentation"         OFF)
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
message(STATUS "ENABLE_COVERAGE            = ${ENABLE_COVERAGE}")
message(STATUS "ENABLE_PROFILING           = ${ENABLE_PROFILING}")
message(STATUS "ENABLE_SPECTRE_MITIGATION  = ${ENABLE_SPECTRE_MITIGATION}")
message(STATUS "")

# ----------------------------------------------------------------------
# Sanitizer Configuration
# ----------------------------------------------------------------------

# Validate conflicting sanitizer options
if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_SANITIZERS OR ENABLE_LSAN OR ENABLE_FUZZING))
  message(FATAL_ERROR "ThreadSanitizer cannot be used with AddressSanitizer or LeakSanitizer (ENABLE_FUZZING and ENABLE_SANITIZERS include AddressSanitizer)")
endif()

if(ENABLE_MSAN AND (ENABLE_ASAN OR ENABLE_SANITIZERS OR ENABLE_TSAN OR ENABLE_LSAN OR ENABLE_FUZZING))
  message(FATAL_ERROR "MemorySanitizer cannot be combined with other sanitizers (ENABLE_FUZZING and ENABLE_SANITIZERS include AddressSanitizer)")
endif()

# Warn about ignored individual sanitizer options when using combined options
if(ENABLE_FUZZING OR ENABLE_SANITIZERS)
  set(_ignored_options "")
  if(ENABLE_ASAN)
    list(APPEND _ignored_options "ENABLE_ASAN")
  endif()
  if(ENABLE_UBSAN)
    list(APPEND _ignored_options "ENABLE_UBSAN")
  endif()
  if(ENABLE_TSAN)
    list(APPEND _ignored_options "ENABLE_TSAN")
  endif()
  if(ENABLE_MSAN)
    list(APPEND _ignored_options "ENABLE_MSAN")
  endif()
  if(ENABLE_LSAN)
    list(APPEND _ignored_options "ENABLE_LSAN")
  endif()
  if(_ignored_options)
    string(REPLACE ";" ", " _ignored_str "${_ignored_options}")
    if(ENABLE_FUZZING)
      message(WARNING "ENABLE_FUZZING overrides individual sanitizer options. Ignoring: ${_ignored_str}")
    else()
      message(WARNING "ENABLE_SANITIZERS overrides individual sanitizer options. Ignoring: ${_ignored_str}")
    endif()
  endif()
  unset(_ignored_options)
  unset(_ignored_str)
endif()

# Compose sanitizer flags
set(SANITIZER_FLAGS "")
set(SANITIZER_LIST "")

if(ENABLE_FUZZING)
  message(STATUS ">>> Fuzzing instrumentation enabled (libFuzzer + ASan + UBSan)")
  message(STATUS ">>> Fuzzer harnesses live on the CFL branch")
  # DO NOT apply fuzzer flags globally - causes linker conflicts with regular tools
  # Fuzzer targets get flags via target_compile_options/target_link_options
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
  list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer")
  # Make UndefinedBehaviorSanitizer violations fatal (abort on first UB)
  list(FIND SANITIZER_LIST "undefined" _ubsan_idx)
  if(NOT _ubsan_idx EQUAL -1)
    list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=undefined")
  endif()
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

# Apply coverage instrumentation flags
if(ENABLE_COVERAGE)
  message(STATUS ">>> Coverage instrumentation enabled")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang source-based coverage (preferred: precise, mergeable)
    set(COVERAGE_COMPILE_FLAGS "-fprofile-instr-generate -fcoverage-mapping")
    set(COVERAGE_LINK_FLAGS "-fprofile-instr-generate")
    message(STATUS ">>> Using Clang source-based coverage (llvm-profdata/llvm-cov)")
  else()
    # GCC gcov-based coverage
    set(COVERAGE_COMPILE_FLAGS "--coverage")
    set(COVERAGE_LINK_FLAGS "--coverage")
    message(STATUS ">>> Using GCC gcov-based coverage")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILE_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILE_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}")
endif()

# Apply profiling instrumentation flags
if(ENABLE_PROFILING)
  message(STATUS ">>> Profiling instrumentation enabled (-pg)")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -fno-omit-frame-pointer")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg -fno-omit-frame-pointer")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
endif()

# ----------------------------------------------------------------------
# Link-Time Optimization (LTO/IPO)
# ----------------------------------------------------------------------
#
# LTO is enabled by default for Release builds when the compiler supports it
# and no coverage/profiling/sanitizer instrumentation is active.
#
# Users can override this by passing -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
# on the cmake command line.  This is useful when consuming static archives
# (.a files) with a linker that does not support LLVM bitcode (e.g. /usr/bin/ld).

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)

if(DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  # User explicitly set LTO preference via -D flag — respect it unconditionally
  message(STATUS ">>> Link-Time Optimization (LTO): ${CMAKE_INTERPROCEDURAL_OPTIMIZATION} (user override)")
elseif(ipo_supported)
  if(SANITIZER_FLAGS OR ENABLE_COVERAGE OR ENABLE_PROFILING)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    set(_lto_reason "")
    if(ENABLE_COVERAGE)
      set(_lto_reason "ENABLE_COVERAGE is ON")
    elseif(ENABLE_PROFILING)
      set(_lto_reason "ENABLE_PROFILING is ON")
    elseif(SANITIZER_FLAGS)
      set(_lto_reason "sanitizers are enabled")
    endif()
    message(STATUS ">>> Link-Time Optimization (LTO) disabled: ${_lto_reason}")
  elseif(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode): enable LTO for Release only
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS ">>> Link-Time Optimization (LTO) enabled for Release configuration (multi-config)")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message(STATUS ">>> Link-Time Optimization (LTO) enabled for Release builds")
  else()
    message(STATUS ">>> Link-Time Optimization (LTO) disabled: build type is '${CMAKE_BUILD_TYPE}'")
  endif()
else()
  message(STATUS ">>> Link-Time Optimization (LTO) not supported: ${ipo_output}")
endif()

# ----------------------------------------------------------------------
# Build Context Detection: Top-Level vs Subproject
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Build Context Check ###")
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  # Top-level project: full install/build mode
  option(ENABLE_INSTALL_RIM "Enable install target for RefIccMAX." ON)
  if(VERBOSE_CONFIG)
    message(STATUS "RefIccMAX is being built as a top-level project.")
    message(STATUS "ENABLE_INSTALL_RIM      = ${ENABLE_INSTALL_RIM}")
  endif()
else()
  # Subproject: disable install to avoid polluting parent build
  set(ENABLE_INSTALL_RIM OFF CACHE BOOL "Install disabled in subproject mode" FORCE)
  set(ENABLE_STATIC_LIBS ON  CACHE BOOL "Force static linking in subproject" FORCE)
  set(ENABLE_SHARED_LIBS OFF CACHE BOOL "Disable shared libs in subproject" FORCE)

  if(VERBOSE_CONFIG)
    message(STATUS "RefIccMAX included as subproject - installation is disabled.")
    message(STATUS "ENABLE_INSTALL_RIM      = ${ENABLE_INSTALL_RIM}")
    message(STATUS "ENABLE_STATIC_LIBS      = ${ENABLE_STATIC_LIBS}")
    message(STATUS "ENABLE_SHARED_LIBS      = ${ENABLE_SHARED_LIBS}")
  endif()
endif()
# ----------------------------------------------------------------------
# Build Type Status Display (type already set earlier)
# ----------------------------------------------------------------------

message(STATUS "")
message(STATUS "### Build Type Configuration ###")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ----------------------------------------------------------------------
# Compiler Flag Setup (Baseline)
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Initializing Compiler Flags ###")
endif()

# Platform-Specific Flags - MINIMAL (no extra flags for Release/Debug)
if(MSVC)
    # Debug: CMake defaults only (no extra flags)
    # CMAKE_CXX_FLAGS_DEBUG uses CMake defaults
    # Release: CMake defaults only (no extra flags)
    # CMAKE_CXX_FLAGS_RELEASE uses CMake defaults
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    message(STATUS "Configuring MSVC runtime and flags (CMake defaults)")
    message(STATUS "  DEBUG:          CMake defaults only")
    message(STATUS "  RELEASE:        CMake defaults only")
    message(STATUS "  RELWITHDEBINFO: CMake defaults only")
    message(STATUS "  MINSIZEREL:     CMake defaults only")
elseif(UNIX)
    # Debug: CMake defaults only (no extra flags)
    # CMAKE_CXX_FLAGS_DEBUG uses CMake defaults
    # Release: CMake defaults only (no extra flags)
    # CMAKE_CXX_FLAGS_RELEASE uses CMake defaults
    message(STATUS "Configuring GCC/Clang runtime and flags (CMake defaults)")
    message(STATUS "  DEBUG:          CMake defaults only")
    message(STATUS "  RELEASE:        CMake defaults only")
    message(STATUS "  RELWITHDEBINFO: CMake defaults only")
    message(STATUS "  MINSIZEREL:     CMake defaults only")
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

  # macOS hardening flags (parity with Linux)
  add_compile_options(
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -Wimplicit-fallthrough
    -Wsign-compare
    -fstack-protector-strong
  )

  # Fortify source for optimized builds (requires -O1 or higher)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(_FORTIFY_SOURCE=2)
  endif()

  message(STATUS "macOS build: Warning flags and security hardening applied.")

elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  message(STATUS "Detected platform: Linux")

  # Recommended flags for Linux/Clang or GCC
  add_compile_options(
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -Wimplicit-fallthrough
    -Wsign-compare
    -fstack-protector-strong
    -fstack-clash-protection
  )

  # Hardening: RELRO and BIND_NOW for executables
  add_link_options(-Wl,-z,relro,-z,now)

  # Fortify source for optimized builds (requires -O1 or higher)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(_FORTIFY_SOURCE=2)
  endif()

  message(STATUS "Linux build: Warning flags and security hardening applied.")

elseif(WIN32)
  message(STATUS "Detected platform: Windows")

  # Windows + MSVC flags
  add_compile_options(/std:c++17 /D_WINDOWS /EHsc /GS)

  # ASLR + DEP + CET shadow stacks
  add_link_options(/DYNAMICBASE /NXCOMPAT)
  # CET (Control-flow Enforcement Technology) — requires VS 2019 16.7+
  if(MSVC_VERSION GREATER_EQUAL 1927)
    add_link_options(/CETCOMPAT)
  endif()

  message(STATUS "Windows build: Compiler flags set for MSVC with C++17, buffer security, and ASLR/DEP.")

else()
  message(FATAL_ERROR "Unsupported or unknown platform. Aborting build configuration.")
endif()

# Prevent old-style path-based linking
unset(IccProfLib2_LIBRARIES CACHE)
unset(IccProfLib2_LIBRARY CACHE)
unset(IccProfLib2_LIB CACHE)
unset(IccProfLib2 CACHE)

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
# NOTE: IccProfLib include now propagated via target_include_directories(PUBLIC) on IccProfLib2

#
# Core library
#
if(VERBOSE_CONFIG)
  message(STATUS "## Project Configuration ##")
  message(STATUS "Adding subdirectory for IccProfLib.")
endif()
add_subdirectory(IccProfLib)

# Set default link target for IccProfLib (matches build configuration)
# Use CACHE INTERNAL so that it's available in Tools/* subdirectories
IF(ENABLE_SHARED_LIBS)
  set(TARGET_LIB_ICCPROFLIB IccProfLib2 CACHE INTERNAL "Link target for IccProfLib2")
ELSE()
  set(TARGET_LIB_ICCPROFLIB IccProfLib2-static CACHE INTERNAL "Link target for IccProfLib2")
ENDIF()

#
# Optional external dependency: LibXML2
#
if(ENABLE_ICCXML)
  if(VERBOSE_CONFIG)
    message(STATUS "ICCXML support enabled")
  endif()

  # LibXml2 already found unconditionally above — resolve target vs legacy variables
  if(LibXml2_FOUND)
    if(VERBOSE_CONFIG)
      message(STATUS "Found LibXml2 via system or toolchain integration")
    endif()
    set(HAVE_LIBXML2 TRUE)

    # Use imported CMake target if available
    if(TARGET LibXml2::LibXml2)
      set(LIBXML2_LIBRARIES LibXml2::LibXml2)
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
if(VERBOSE_CONFIG)
  message(STATUS "Adding subdirectory for IccXML.")
endif()
add_subdirectory(IccXML)

# Set default link target for IccXML (matches build configuration)
IF(ENABLE_SHARED_LIBS)
  set(TARGET_LIB_ICCXML IccXML2 CACHE INTERNAL "Link target for IccXML2")
ELSE()
  set(TARGET_LIB_ICCXML IccXML2-static CACHE INTERNAL "Link target for IccXML2")
ENDIF()

# NOTE: IccXML and generated-header includes now propagated via
# target_include_directories(PUBLIC) on IccProfLib2 and IccXML2 targets

# Diagnostic: Output current linker flags
if(VERBOSE_CONFIG)
  message(STATUS "CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")
  message(STATUS "CMAKE_SHARED_LINKER_FLAGS = ${CMAKE_SHARED_LINKER_FLAGS}")
  message(STATUS "CMAKE_MODULE_LINKER_FLAGS = ${CMAKE_MODULE_LINKER_FLAGS}")
endif()


# nlohmann_json already found above
if(VERBOSE_CONFIG)
  message(STATUS "nlohmann_json library found: ${nlohmann_json_DIR}")
endif()

 # Function to add and log subdirectories
function(add_tool_subdirectory TOOL_SUBDIRECTORY)
    message(STATUS "Adding Subdirectory: ${TOOL_SUBDIRECTORY}...")
    add_subdirectory(${TOOL_SUBDIRECTORY})
    message(STATUS "Successfully added Subdirectory: ${TOOL_SUBDIRECTORY}")
endfunction()

IF(ENABLE_TOOLS)
  # Verify nlohmann_json is configured properly
  IF(nlohmann_json_DIR)
    IF(VERBOSE_CONFIG)
      MESSAGE(STATUS "Found nlohmann JSON library at: ${nlohmann_json_DIR}")
    ENDIF()
    # nlohmann_json is header-only — use via target_link_libraries(PRIVATE nlohmann_json::nlohmann_json)
    # in individual tool CMakeLists (NOT via include_directories)
  ELSE()
    MESSAGE(WARNING "nlohmann JSON library not found. Skipping..")
    RETURN() # Exit script for tools if dependency is not found
  ENDIF()

  # Add XML-related tools
  ADD_SUBDIRECTORY(Tools/IccFromXml)
  ADD_SUBDIRECTORY(Tools/IccToXml)
  ADD_SUBDIRECTORY(Tools/IccJpegDump)
  ADD_SUBDIRECTORY(Tools/IccApplyNamedCmm)
  ADD_SUBDIRECTORY(Tools/IccDumpProfile)
  ADD_SUBDIRECTORY(Tools/IccRoundTrip)
  ADD_SUBDIRECTORY(Tools/IccFromCube)
  ADD_SUBDIRECTORY(Tools/IccV5DspObsToV4Dsp)
  ADD_SUBDIRECTORY(Tools/IccApplyToLink)
  ADD_SUBDIRECTORY(Tools/IccApplySearch)

  # Ensure IccXML-dependent tools are built after IccXML library (safety net)
  IF(TARGET iccFromXml AND TARGET ${TARGET_LIB_ICCXML})
    ADD_DEPENDENCIES(iccFromXml ${TARGET_LIB_ICCXML})
  ENDIF()
  IF(TARGET iccToXml AND TARGET ${TARGET_LIB_ICCXML})
    ADD_DEPENDENCIES(iccToXml ${TARGET_LIB_ICCXML})
  ENDIF()


# PNG already found above — just add the tool
if(NOT PNG_FOUND)
  message(FATAL_ERROR "PNG not found. Please install libpng-dev or install via vcpkg.")
endif()
if(VERBOSE_CONFIG)
  message(STATUS "Adding Subdirectory IccPngDump.")
endif()
ADD_SUBDIRECTORY(Tools/IccPngDump)


    if(VERBOSE_CONFIG)
      message(STATUS "Adding Subdirectory IccDEVCmm.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccDEVCmm)


  # TIFF already found above — scope includes to targets, not globally
  if (TIFF_FOUND)
    # TIFF includes are now scoped to each tool via target_include_directories

    # Add TIFF-dependent tools
    if(VERBOSE_CONFIG)
      message(STATUS "Adding Subdirectory IccApplyProfiles.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccApplyProfiles)
    if(VERBOSE_CONFIG)
      message(STATUS "Adding subdirectory IccSpecSepToTiff.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccSpecSepToTiff)
    if(VERBOSE_CONFIG)
      message(STATUS "Adding Subdirectory IccTiffDump.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccTiffDump)
  else()
    message(FATAL_ERROR "TIFF library not found. Please install libtiff-dev.")
  endif()

  # wxWidgets GUI tool (optional)
  find_package(wxWidgets QUIET COMPONENTS core base)
  if(wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
    message(STATUS "wxWidgets found: ${wxWidgets_LIBRARIES}")
    message(STATUS "Adding Subdirectory wxProfileDump.")
    ADD_SUBDIRECTORY(Tools/wxProfileDump)
  else()
    message(STATUS "wxWidgets not found - skipping wxProfileDump.")
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
MESSAGE( "Configured ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION_FULL}" )

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
include(CMakePackageConfigHelpers)

CONFIGURE_FILE (
                "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                @ONLY
        )

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION "${REFICCMAX_MAJOR_VERSION}.${REFICCMAX_MINOR_VERSION}.${REFICCMAX_MICRO_VERSION}"
  COMPATIBILITY SameMajorVersion
)

IF(ENABLE_INSTALL_RIM)
  INSTALL ( FILES
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
                DESTINATION ${CMAKE_MODULES_INSTALL_DIR} COMPONENT dev
        )

  # Install exported targets file alongside the Config
  INSTALL( EXPORT RefIccMAXTargets
           NAMESPACE RefIccMAX::
           DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
           COMPONENT dev
  )
ENDIF(ENABLE_INSTALL_RIM)

# Build-tree export — allows find_package() from build dir without install
export( EXPORT RefIccMAXTargets
        NAMESPACE RefIccMAX::
        FILE "${CMAKE_CURRENT_BINARY_DIR}/RefIccMAXTargets.cmake"
)
