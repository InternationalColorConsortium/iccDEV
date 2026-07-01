#################################################################################
# Cmake Project Configuration| iccDEV Project
# Copyright (c) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-04-02 02:20:35 UTC by David Hoyt
#
# CMake Configuration:
# - Fix: Change IccApplyNamedCmm to iccApplyNamedCmm
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

cmake_minimum_required(VERSION 3.18...3.29)
# VERSION 3.18: minimum for Raspberry Pi OS Bullseye and other embedded Linux
# distributions shipping cmake 3.18.4.  The ...3.29 upper bound activates all
# modern policies automatically when a newer cmake is available, so CI and
# developer machines with cmake 3.21+ continue to get full policy enforcement
# without any source change.  FATAL_ERROR is not needed with range syntax.

# Adopt modern behavior for scoped scripts and alias linking
# cmake_policy(SET CMP0112 NEW)  # Respect parent scope in script mode
# cmake_policy(SET CMP0156 NEW)  # Allow ALIAS targets for link dependencies

# Emit warnings on deprecated or legacy CMake features
set(CMAKE_WARN_DEPRECATED TRUE)

# Optional: clean up obsolete warnings (only enable if really needed)
# set(CMAKE_POLICY_WARNING_CMP0012 TRUE)

# Suppress redundant regeneration messages (less CLI noise)
# set(CMAKE_SUPPRESS_REGENERATION ON)

# CMP0135: controls FetchContent/ExternalProject URL download timestamps.
# Introduced in cmake 3.24 -- guard so cmake 3.18/3.19/3.20/3.21/3.22/3.23
# do not fatal-error on an unknown policy name.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  cmake_policy(SET CMP0135 NEW)
endif()
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
# Define Default Configurations (for MSVC/Xcode/Ninja Multi-Config generators)
# ----------------------------------------------------------------------
# CMAKE_CONFIGURATION_TYPES is only meaningful for multi-config generators.
# Setting it on single-config generators (Make, Ninja) causes downstream
# logic to misidentify the generator as multi-config, leading to incorrect
# LTO configuration and static libraries containing LLVM bitcode instead
# of native object code.  (See GitHub issue #814.)

get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_is_multi_config AND NOT CMAKE_CONFIGURATION_TYPES)
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

# Version components (4-part version: 2.3.2.1)
set(${PROJECT_UP_NAME}_MAJOR_VERSION 2)
set(${PROJECT_UP_NAME}_MINOR_VERSION 3)
set(${PROJECT_UP_NAME}_MICRO_VERSION 2)
set(${PROJECT_UP_NAME}_PATCH_VERSION 1)

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
    RESULT_VARIABLE GIT_COMMIT_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_RESULT EQUAL 0)
    set(GIT_COMMIT_HASH "")
endif()

if(NOT GIT_COMMIT_HASH)
    set(_ICCDEV_ENV_COMMIT "")
    if(DEFINED ENV{GIT_COMMIT})
        set(_ICCDEV_ENV_COMMIT "$ENV{GIT_COMMIT}")
    elseif(DEFINED ENV{GITHUB_SHA})
        set(_ICCDEV_ENV_COMMIT "$ENV{GITHUB_SHA}")
    endif()
    string(LENGTH "${_ICCDEV_ENV_COMMIT}" _ICCDEV_ENV_COMMIT_LEN)
    if(_ICCDEV_ENV_COMMIT MATCHES "^[0-9A-Fa-f]+$" AND
       _ICCDEV_ENV_COMMIT_LEN GREATER_EQUAL 7 AND
       _ICCDEV_ENV_COMMIT_LEN LESS_EQUAL 40)
        string(SUBSTRING "${_ICCDEV_ENV_COMMIT}" 0 7 GIT_COMMIT_HASH)
    endif()
endif()

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
# Debug Library Postfix (disambiguate Debug vs Release install)
# ----------------------------------------------------------------------
# Appends "d" to Debug library names (e.g., libIccProfLib2d.so) so that
# Debug and Release libraries can coexist in the same install prefix.
# This is especially important for multi-config generators (Visual Studio, Xcode).

set(CMAKE_DEBUG_POSTFIX "d")

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
get_filename_component(_ICCDEV_PROJECT_ROOT_DIR "${PROJECT_ROOT_DIR}" ABSOLUTE)
get_filename_component(_ICCDEV_BINARY_DIR "${CMAKE_BINARY_DIR}" ABSOLUTE)

if(_ICCDEV_BINARY_DIR STREQUAL _ICCDEV_PROJECT_ROOT_DIR)
  message(FATAL_ERROR
    "Refusing to use the repository root as the CMake build directory. "
    "Use -B out/<name>, run legacy builds from Build/, or use a preset with "
    "an isolated binaryDir."
  )
endif()

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

# Pretty-print the PATH variable (numbered entries) only with VERBOSE_CONFIG
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
    # Attempt to auto-detect common VS2022+ MSBuild paths.
    set(_msbuild_candidates)
    foreach(_vs_edition Enterprise Professional Community BuildTools)
      list(APPEND _msbuild_candidates
        "C:/Program Files/Microsoft Visual Studio/2022/${_vs_edition}/MSBuild/Current/Bin/MSBuild.exe"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/${_vs_edition}/MSBuild/Current/Bin/MSBuild.exe"
      )
    endforeach()

    foreach(_msbuild_candidate IN LISTS _msbuild_candidates)
      if(EXISTS "${_msbuild_candidate}")
        set(ENV{MSBUILD_PATH} "${_msbuild_candidate}")
        if(VERBOSE_CONFIG)
          message(STATUS "MSBuild path auto-detected and set to: ${_msbuild_candidate}")
        endif()
        break()
      endif()
    endforeach()

    if(NOT DEFINED ENV{MSBUILD_PATH})
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
# Build Feature Toggles
# ----------------------------------------------------------------------
# Defined before dependency discovery so optional components can avoid
# requiring packages they do not build.
option(ENABLE_TESTS               "Build tests (requires static libs)"                  ON)
option(ENABLE_TOOLS               "Build tools (command-line + GUI)"                    ON)
option(ENABLE_WXWIDGETS           "Build wxProfileDump GUI tool (requires wxWidgets)"   ON)
option(ENABLE_SHARED_LIBS         "Build dynamic link libraries"                        ON)
option(ENABLE_STATIC_LIBS         "Build static libraries"                              ON)
option(ENABLE_ICCXML              "Build IccXML library support"                        ON)
option(ENABLE_ICCJSON             "Build IccJSON library support"                       ON)
option(ENABLE_IMAGE_TOOLS         "Build TIFF/PNG/JPEG image-dependent tools"           ON)
option(ENABLE_CMM_TOOLS           "Build Windows CMM DLL tool"                          ON)
option(ENABLE_IIS_TOOLS           "Build Windows IIS ISAPI tool"                        ON)
option(ICCDEV_WINDOWS_UNIFIED_RUNTIME
  "Place Windows executable and DLL runtime artifacts in one build-tree bin directory"
  ON)

if(WIN32 AND ICCDEV_WINDOWS_UNIFIED_RUNTIME)
  set(ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR
      "${CMAKE_BINARY_DIR}/bin"
      CACHE PATH "Windows build-tree directory for iccDEV executable and DLL runtime artifacts")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR}")
  set(CMAKE_PDB_OUTPUT_DIRECTORY "${ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR}")
  message(STATUS "Windows runtime output directory: ${ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR}")
endif()

include(CheckCXXSourceCompiles)
set(_ICC_JSON_ORDERED_DEFAULT OFF)

set(_ICC_CHECK_GLIBCXX_SOURCE "
#if defined(__has_include)
#  if __has_include(<bits/c++config.h>)
#    include <bits/c++config.h>
#  endif
#endif
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE < 11
#  error libstdc++ before 11 cannot instantiate unordered basic_json here
#endif
int main() { return 0; }
")
check_cxx_source_compiles("${_ICC_CHECK_GLIBCXX_SOURCE}" _ICC_JSON_UNORDERED_STDLIB_OK)
if(NOT _ICC_JSON_UNORDERED_STDLIB_OK)
  set(_ICC_JSON_ORDERED_DEFAULT ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  set(_ICC_JSON_ORDERED_DEFAULT ON)
endif()

option(ICC_JSON_ORDERED           "Use ordered_json for deterministic key order (auto-ON on libstdc++<11)" ${_ICC_JSON_ORDERED_DEFAULT})
if(ICC_JSON_ORDERED AND _ICC_JSON_ORDERED_DEFAULT)
  message(STATUS "ICC_JSON_ORDERED auto-enabled: libstdc++<11 cannot build unordered basic_json")
endif()
option(ICC_TRACE_NAN_ENABLED      "Enable tracing NaN inputs in debug builds"           OFF)
option(ICC_CLUT_DEBUG             "Enable CLUT debugging support"                       OFF)
option(ICC_ENABLE_ASSERTS         "Enable ICC_ASSERT traps and debug assertions"        OFF)
option(ICC_LOG_SAFE               "Enable ICC_LOG_SAFE_VAL bounds-checked logging"      OFF)
option(ICC_VERBOSE_CALC_APPLY     "Enable verbose CIccMpeCalculator Apply debugging"    OFF)
option(ICC_USE_ZLIB               "Enable zlib-backed compressed ICC text tags"         ON)
option(ENABLE_SANITIZERS          "Enable runtime sanitizers (ASan, UBSan, etc.)"       OFF)
option(ENABLE_ASAN                "Enable AddressSanitizer only"                        OFF)
option(ENABLE_FUZZING             "Enable libFuzzer harnesses (Clang only)"             OFF)
option(ENABLE_UBSAN               "Enable UndefinedBehaviorSanitizer only"              OFF)
option(ENABLE_INTEGER_SANITIZER   "Enable IntegerSanitizer (unsigned overflow, etc.)"   OFF)
option(ENABLE_FLOAT_SANITIZER     "Enable float-divide-by-zero + float-cast-overflow"   OFF)
option(ENABLE_MACOS_GUARD_MALLOC  "Configure a macOS GuardMalloc/libgmalloc debug build" OFF)

IF(ICC_JSON_ORDERED)
  add_definitions(-DICC_JSON_ORDERED=1)
ENDIF()
option(ENABLE_TSAN                "Enable ThreadSanitizer (conflicts with ASan)"        OFF)
option(ENABLE_MSAN                "Enable MemorySanitizer (Clang only)"                 OFF)
option(ENABLE_LSAN                "Enable LeakSanitizer standalone"                     OFF)
option(ENABLE_COVERAGE            "Enable code coverage instrumentation"                OFF)
option(ENABLE_PROFILING           "Enable gprof/perf profiling instrumentation"         OFF)
option(ENABLE_SPECTRE_MITIGATION  "Enable /Qspectre on MSVC for Spectre V1 mitigation"  OFF)
option(SANITIZER_RECOVER          "Allow sanitizer violations to continue (runtime control)" OFF)
set(UBSAN_IGNORELIST "" CACHE STRING "Path to UBSAN ignorelist file for compile-time suppression")
option(ENABLE_USEICCDEVNAMESPACE  "Use iccDEV namespace wrapping"                       OFF)
option(ENABLE_LTO                 "Enable Link-Time Optimization for Release builds"     ON)
# Pass -DENABLE_LTO=OFF to disable without needing to know the internal
# CMAKE_INTERPROCEDURAL_OPTIMIZATION variable.  Useful on Raspberry Pi,
# constrained build environments, or when linking against third-party
# archives that do not carry LTO bitcode.

# ----------------------------------------------------------------------
# Third-Party Dependencies
# ----------------------------------------------------------------------

if(VERBOSE_CONFIG)
  message(STATUS "")
  message(STATUS "### Dependency Summary ###")
endif()

# --- LibXml2 ---
if(ENABLE_ICCXML)
  find_package(LibXml2 REQUIRED)
  if(VERBOSE_CONFIG)
    message(STATUS "LibXml2                : ${LIBXML2_LIBRARIES} (version: ${LIBXML2_VERSION})")
  endif()
endif()

# --- nlohmann_json ---
if(ENABLE_ICCJSON)
  find_package(nlohmann_json REQUIRED)
endif()

# --- zlib ---
if(ICC_USE_ZLIB)
  find_package(ZLIB REQUIRED)
  if(VERBOSE_CONFIG)
    message(STATUS "ZLIB                   : ${ZLIB_LIBRARIES} (version: ${ZLIB_VERSION})")
  endif()
endif()

# --- TIFF ---
if(ENABLE_TOOLS AND ENABLE_IMAGE_TOOLS)
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

  if(VERBOSE_CONFIG)
    message(STATUS "PNG Library            : ${PNG_LIBRARIES}")
    message(STATUS "PNG Include Directory  : ${PNG_INCLUDE_DIRS}")
    message(STATUS "PNG Version            : ${PNG_VERSION}")
  endif()

  # --- JPEG ---
  if(VERBOSE_CONFIG)
    message(STATUS "Checking for JPEG...")
  endif()
  find_package(JPEG REQUIRED)

  if(VERBOSE_CONFIG)
    message(STATUS "JPEG Library           : ${JPEG_LIBRARIES}")
    message(STATUS "JPEG Include Directory : ${JPEG_INCLUDE_DIR}")
  endif()
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

set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PACKAGE_NAME}" CACHE PATH "Documentation install directory")
set(ICCDEV_RUNTIME_COMPONENT runtime)
set(ICCDEV_DEVELOPMENT_COMPONENT dev)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "${ICCDEV_RUNTIME_COMPONENT}")

# ----------------------------------------------------------------------
# Build Feature Toggles
# ----------------------------------------------------------------------
message(STATUS "")
message(STATUS "### Build Options ###")

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

if(ICC_VERBOSE_CALC_APPLY)
  add_compile_definitions(ICC_VERBOSE_CALC_APPLY=1)
endif()

# ----------------------------------------------------------------------
# Build Status Output
# ----------------------------------------------------------------------
message(STATUS "ENABLE_TESTS               = ${ENABLE_TESTS}")
message(STATUS "ENABLE_TOOLS               = ${ENABLE_TOOLS}")
message(STATUS "ENABLE_WXWIDGETS           = ${ENABLE_WXWIDGETS}")
message(STATUS "ENABLE_SHARED_LIBS         = ${ENABLE_SHARED_LIBS}")
message(STATUS "ENABLE_STATIC_LIBS         = ${ENABLE_STATIC_LIBS}")
message(STATUS "ENABLE_ICCXML              = ${ENABLE_ICCXML}")
message(STATUS "ENABLE_ICCJSON             = ${ENABLE_ICCJSON}")
message(STATUS "ENABLE_IMAGE_TOOLS         = ${ENABLE_IMAGE_TOOLS}")
message(STATUS "ENABLE_CMM_TOOLS           = ${ENABLE_CMM_TOOLS}")
message(STATUS "ENABLE_IIS_TOOLS           = ${ENABLE_IIS_TOOLS}")
message(STATUS "ICCDEV_WINDOWS_UNIFIED_RUNTIME = ${ICCDEV_WINDOWS_UNIFIED_RUNTIME}")
message(STATUS "ICC_TRACE_NAN_ENABLED      = ${ICC_TRACE_NAN_ENABLED}")
message(STATUS "ICC_CLUT_DEBUG             = ${ICC_CLUT_DEBUG}")
message(STATUS "ICC_ENABLE_ASSERTS         = ${ICC_ENABLE_ASSERTS}")
message(STATUS "ICC_LOG_SAFE               = ${ICC_LOG_SAFE}")
message(STATUS "ICC_VERBOSE_CALC_APPLY     = ${ICC_VERBOSE_CALC_APPLY}")
message(STATUS "ICC_USE_ZLIB               = ${ICC_USE_ZLIB}")
message(STATUS "ENABLE_SANITIZERS          = ${ENABLE_SANITIZERS}")
message(STATUS "ENABLE_ASAN                = ${ENABLE_ASAN}")
message(STATUS "ENABLE_UBSAN               = ${ENABLE_UBSAN}")
message(STATUS "ENABLE_INTEGER_SANITIZER   = ${ENABLE_INTEGER_SANITIZER}")
message(STATUS "ENABLE_FLOAT_SANITIZER     = ${ENABLE_FLOAT_SANITIZER}")
message(STATUS "ENABLE_MACOS_GUARD_MALLOC  = ${ENABLE_MACOS_GUARD_MALLOC}")
message(STATUS "ENABLE_TSAN                = ${ENABLE_TSAN}")
message(STATUS "ENABLE_MSAN                = ${ENABLE_MSAN}")
message(STATUS "ENABLE_LSAN                = ${ENABLE_LSAN}")
message(STATUS "ENABLE_FUZZING             = ${ENABLE_FUZZING}")
message(STATUS "ENABLE_COVERAGE            = ${ENABLE_COVERAGE}")
message(STATUS "ENABLE_PROFILING           = ${ENABLE_PROFILING}")
message(STATUS "ENABLE_SPECTRE_MITIGATION  = ${ENABLE_SPECTRE_MITIGATION}")
message(STATUS "SANITIZER_RECOVER          = ${SANITIZER_RECOVER}")
message(STATUS "ENABLE_LTO                 = ${ENABLE_LTO}")
if(UBSAN_IGNORELIST)
  message(STATUS "UBSAN_IGNORELIST           = ${UBSAN_IGNORELIST}")
endif()
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

if(ENABLE_MACOS_GUARD_MALLOC)
  if(NOT APPLE)
    message(FATAL_ERROR "ENABLE_MACOS_GUARD_MALLOC is only supported on macOS")
  endif()
  if(ENABLE_ASAN OR ENABLE_SANITIZERS OR ENABLE_FUZZING OR ENABLE_TSAN OR ENABLE_MSAN OR ENABLE_LSAN)
    message(FATAL_ERROR "GuardMalloc interposes malloc at runtime; use a non-sanitizer Debug build")
  endif()
  foreach(_iccdev_macos_link_flags
      CMAKE_EXE_LINKER_FLAGS
      CMAKE_SHARED_LINKER_FLAGS
      CMAKE_MODULE_LINKER_FLAGS)
    if("${${_iccdev_macos_link_flags}}" MATCHES "(^|[ ,])-Wl,-no_uuid($|[ ,])|(^|[ ,])-no_uuid($|[ ,])")
      message(FATAL_ERROR "GuardMalloc requires Mach-O LC_UUID load commands; remove -no_uuid from ${_iccdev_macos_link_flags}")
    endif()
  endforeach()
  add_compile_options(-g -fno-omit-frame-pointer)
  message(STATUS ">>> macOS GuardMalloc mode enabled")
  message(STATUS ">>> Run tools with DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib")
  message(STATUS ">>> Build target check-macos-guard-malloc verifies LC_UUID on built tools")
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
  if(ENABLE_INTEGER_SANITIZER)
    list(APPEND _ignored_options "ENABLE_INTEGER_SANITIZER")
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
set(ICCDEV_COMPILER_IS_CLANG FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(ICCDEV_COMPILER_IS_CLANG TRUE)
endif()

if(ENABLE_FUZZING)
  message(STATUS ">>> Fuzzing instrumentation enabled (libFuzzer + ASan + UBSan)")
  message(STATUS ">>> Fuzzer harnesses live on the CFL branch")
  # DO NOT apply fuzzer flags globally - causes linker conflicts with regular tools
  # Fuzzer targets get flags via target_compile_options/target_link_options
elseif(ENABLE_SANITIZERS)
  if(MSVC)
    message(STATUS ">>> Sanitizers enabled: ASan")
    list(APPEND SANITIZER_FLAGS "/fsanitize=address")
    # MSVC doesn't have UBSan/IntSan, but has runtime checks via /RTC1 in debug
  else()
    list(APPEND SANITIZER_LIST "address" "undefined")
    if(ICCDEV_COMPILER_IS_CLANG)
      message(STATUS ">>> Sanitizers enabled: ASan + UBSan + IntSan + float-divide-by-zero + float-cast-overflow")
      list(APPEND SANITIZER_LIST "integer" "float-divide-by-zero" "float-cast-overflow")
    else()
      message(STATUS ">>> Sanitizers enabled: ASan + UBSan + float-divide-by-zero + float-cast-overflow")
      message(STATUS ">>> IntegerSanitizer requires Clang; skipping for ${CMAKE_CXX_COMPILER_ID}")
      list(APPEND SANITIZER_LIST "float-divide-by-zero" "float-cast-overflow")
    endif()
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
  if(ENABLE_INTEGER_SANITIZER)
    message(STATUS ">>> IntegerSanitizer enabled (unsigned-integer-overflow, implicit conversions)")
    if(ICCDEV_COMPILER_IS_CLANG)
      list(APPEND SANITIZER_LIST "integer")
    else()
      message(FATAL_ERROR "IntegerSanitizer requires Clang compiler")
    endif()
  endif()
  if(ENABLE_FLOAT_SANITIZER)
    message(STATUS ">>> Float sanitizers enabled (float-divide-by-zero, float-cast-overflow)")
    if(MSVC)
      message(WARNING "Float sanitizers not available on MSVC")
    else()
      list(APPEND SANITIZER_LIST "float-divide-by-zero" "float-cast-overflow")
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
  if(NOT SANITIZER_RECOVER)
    # Make UndefinedBehaviorSanitizer violations fatal (abort on first UB)
    list(FIND SANITIZER_LIST "undefined" _ubsan_idx)
    if(NOT _ubsan_idx EQUAL -1)
      list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=undefined")
    endif()
    # Make IntegerSanitizer violations fatal (abort on unsigned overflow)
    list(FIND SANITIZER_LIST "integer" _intsan_idx)
    if(NOT _intsan_idx EQUAL -1)
      list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=integer")
    endif()
    # Make float sanitizer violations fatal (div-by-zero, cast overflow)
    list(FIND SANITIZER_LIST "float-divide-by-zero" _fdz_idx)
    if(NOT _fdz_idx EQUAL -1)
      list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=float-divide-by-zero")
    endif()
    list(FIND SANITIZER_LIST "float-cast-overflow" _fco_idx)
    if(NOT _fco_idx EQUAL -1)
      list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=float-cast-overflow")
    endif()
  else()
    message(STATUS ">>> Sanitizer recovery enabled (runtime UBSAN_OPTIONS controls halt behavior)")
  endif()
  # Apply UBSAN ignorelist for compile-time suppression of known-benign findings
  if(UBSAN_IGNORELIST)
    if(EXISTS "${UBSAN_IGNORELIST}")
      list(APPEND SANITIZER_FLAGS "-fsanitize-ignorelist=${UBSAN_IGNORELIST}")
      message(STATUS ">>> UBSAN ignorelist: ${UBSAN_IGNORELIST}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/../../${UBSAN_IGNORELIST}")
      set(_abs_ignorelist "${CMAKE_SOURCE_DIR}/../../${UBSAN_IGNORELIST}")
      get_filename_component(_abs_ignorelist "${_abs_ignorelist}" ABSOLUTE)
      list(APPEND SANITIZER_FLAGS "-fsanitize-ignorelist=${_abs_ignorelist}")
      message(STATUS ">>> UBSAN ignorelist: ${_abs_ignorelist}")
    else()
      message(WARNING "UBSAN_IGNORELIST set to '${UBSAN_IGNORELIST}' but file not found")
    endif()
  endif()
endif()

# Apply all sanitizer flags
if(SANITIZER_FLAGS)
  string(JOIN " " FINAL_SAN_FLAGS ${SANITIZER_FLAGS})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FINAL_SAN_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FINAL_SAN_FLAGS}")
  if(NOT MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FINAL_SAN_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FINAL_SAN_FLAGS}")
  endif()
  message(STATUS ">>> Final sanitizer flags: ${FINAL_SAN_FLAGS}")
endif()

if(MSVC)
  set(_iccdev_clangcl_asan OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND (ENABLE_ASAN OR ENABLE_SANITIZERS OR ENABLE_FUZZING))
    set(_iccdev_clangcl_asan ON)
  endif()

  if(_iccdev_clangcl_asan)
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
      message(STATUS ">>> ClangCL AddressSanitizer uses the non-debug MSVC DLL runtime (/MD); clang-cl rejects /MDd with /fsanitize=address")
    elseif(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Debug")
      message(FATAL_ERROR "ClangCL AddressSanitizer rejects the debug MSVC runtime (/MDd). Use -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL or MultiThreaded.")
    endif()

    set(_iccdev_clangcl_asan_arch "x86_64")
    if(CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
      set(_iccdev_clangcl_asan_arch "i386")
    elseif(CMAKE_VS_PLATFORM_NAME MATCHES "ARM64")
      set(_iccdev_clangcl_asan_arch "aarch64")
    endif()

    execute_process(
      COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
      OUTPUT_VARIABLE _iccdev_clangcl_resource_dir
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    set(_iccdev_clangcl_asan_lib_dir "${_iccdev_clangcl_resource_dir}/lib/windows")
    find_library(_iccdev_clangcl_asan_dynamic
      NAMES "clang_rt.asan_dynamic-${_iccdev_clangcl_asan_arch}"
      PATHS "${_iccdev_clangcl_asan_lib_dir}"
      NO_DEFAULT_PATH)
    find_library(_iccdev_clangcl_asan_thunk
      NAMES "clang_rt.asan_dynamic_runtime_thunk-${_iccdev_clangcl_asan_arch}"
      PATHS "${_iccdev_clangcl_asan_lib_dir}"
      NO_DEFAULT_PATH)
    if(NOT _iccdev_clangcl_asan_dynamic OR NOT _iccdev_clangcl_asan_thunk)
      message(FATAL_ERROR "ClangCL AddressSanitizer runtime libraries were not found in ${_iccdev_clangcl_asan_lib_dir}")
    endif()
    find_file(_iccdev_clangcl_asan_runtime_dll
      NAMES "clang_rt.asan_dynamic-${_iccdev_clangcl_asan_arch}.dll"
      PATHS "${_iccdev_clangcl_asan_lib_dir}"
      NO_DEFAULT_PATH)
    if(_iccdev_clangcl_asan_runtime_dll)
      set(ICCDEV_CLANGCL_ASAN_RUNTIME_DLL
          "${_iccdev_clangcl_asan_runtime_dll}"
          CACHE FILEPATH "ClangCL AddressSanitizer runtime DLL staged with Windows tools")
    endif()

    add_link_options(
      "${_iccdev_clangcl_asan_dynamic}"
      "${_iccdev_clangcl_asan_thunk}"
      "/include:__asan_seh_interceptor"
      "/wholearchive:${_iccdev_clangcl_asan_thunk}"
      "/INFERASANLIBS:NO"
    )
  elseif(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  endif()
  unset(_iccdev_clangcl_asan)
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
include(CheckCXXCompilerFlag)

# Helper: probe a compiler flag and add it only if truly accepted without warning/error.
# Uses -Werror=unused-command-line-argument during the probe so that flags which
# Clang silently accepts but marks as "unused" (e.g. -fstack-clash-protection on
# aarch64 Clang 11) are correctly rejected rather than added to the build.
# VAR is a unique cache variable name used to memoize the probe result.
macro(_iccdev_add_cxx_flag_if_supported FLAG VAR)
  set(CMAKE_REQUIRED_FLAGS "-Werror=unused-command-line-argument")
  check_cxx_compiler_flag("${FLAG}" ${VAR})
  unset(CMAKE_REQUIRED_FLAGS)
  if(${VAR})
    add_compile_options("${FLAG}")
  endif()
endmacro()

function(_iccdev_configure_fortify_source PLATFORM_NAME)
  if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
    add_compile_definitions(_FORTIFY_SOURCE=2)
    message(STATUS "${PLATFORM_NAME}: _FORTIFY_SOURCE=2 enabled for optimized ${CMAKE_BUILD_TYPE} build")
  elseif(NOT _is_multi_config)
    add_compile_options(-U_FORTIFY_SOURCE)
    message(STATUS "${PLATFORM_NAME}: _FORTIFY_SOURCE undefined for non-optimized ${CMAKE_BUILD_TYPE} build")
  endif()
endfunction()

if(SANITIZER_FLAGS OR ENABLE_COVERAGE OR ENABLE_PROFILING)
  set(ipo_supported TRUE)
  set(ipo_output "IPO support not probed because instrumentation disables LTO")
else()
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output LANGUAGES CXX)
endif()

if(DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  # User explicitly set LTO preference via -D flag; respect it unconditionally
  message(STATUS ">>> Link-Time Optimization (LTO): ${CMAKE_INTERPROCEDURAL_OPTIMIZATION} (user override)")
elseif(NOT ENABLE_LTO)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
  message(STATUS ">>> Link-Time Optimization (LTO) disabled: ENABLE_LTO=OFF")
elseif(DEFINED VCPKG_TARGET_TRIPLET AND ENABLE_STATIC_LIBS AND NOT ENABLE_SHARED_LIBS)
  # Packaged static vcpkg builds are consumed by non-Clang toolchains and linkers.
  # Keep these archives free of LLVM bitcode even when the port forgets to pass
  # -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF.
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
  message(STATUS ">>> Link-Time Optimization (LTO) disabled: vcpkg static package build")
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
  elseif(WIN32 AND ENABLE_SHARED_LIBS)
    # WINDOWS_EXPORT_ALL_SYMBOLS requires standard COFF objects at link time.
    # MSVC /GL (whole-program optimization) produces LTCG bytecode objects that
    # bypass the WEAS symbol scanner, resulting in missing DLL exports and
    # LNK2001 unresolved externals when consumers link the import library.
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    message(STATUS ">>> Link-Time Optimization (LTO) disabled: WINDOWS_EXPORT_ALL_SYMBOLS requires COFF objects")
  elseif(MINGW AND ENABLE_STATIC_LIBS AND NOT ENABLE_SHARED_LIBS)
    # MinGW static tool links can fail when executable objects use -flto while
    # iccDEV static archives intentionally disable IPO for native COFF output.
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    message(STATUS ">>> Link-Time Optimization (LTO) disabled: MinGW static builds require native COFF objects")
  elseif(_is_multi_config)
    # Multi-config generator (Visual Studio, Xcode, Ninja Multi-Config):
    # enable LTO for Release only
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
  option(ENABLE_CPACK_PACKAGING "Enable CPack native packages and portable archives." ON)
  if(VERBOSE_CONFIG)
    message(STATUS "RefIccMAX is being built as a top-level project.")
    message(STATUS "ENABLE_INSTALL_RIM      = ${ENABLE_INSTALL_RIM}")
    message(STATUS "ENABLE_CPACK_PACKAGING  = ${ENABLE_CPACK_PACKAGING}")
  endif()
else()
  # Subproject: disable install to avoid polluting parent build
  set(ENABLE_INSTALL_RIM OFF CACHE BOOL "Install disabled in subproject mode" FORCE)
  set(ENABLE_CPACK_PACKAGING OFF CACHE BOOL "Packaging disabled in subproject mode" FORCE)
  set(ENABLE_STATIC_LIBS ON  CACHE BOOL "Force static linking in subproject" FORCE)
  set(ENABLE_SHARED_LIBS OFF CACHE BOOL "Disable shared libs in subproject" FORCE)

  if(VERBOSE_CONFIG)
    message(STATUS "RefIccMAX included as subproject - installation is disabled.")
    message(STATUS "ENABLE_INSTALL_RIM      = ${ENABLE_INSTALL_RIM}")
    message(STATUS "ENABLE_CPACK_PACKAGING  = ${ENABLE_CPACK_PACKAGING}")
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
  message(STATUS "Configuring MSVC runtime and flags (CMake defaults)")
  message(STATUS "  MSVC runtime:   ${CMAKE_MSVC_RUNTIME_LIBRARY}")
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
  # Base set: works on any C++17-capable compiler.
  add_compile_options(
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -Wimplicit-fallthrough
    -Wsign-compare
    -Wnon-virtual-dtor
    -Wuninitialized
    -fstack-protector-strong
  )

  # Strict warning tier: requires GCC 15+ or Clang 14+ for reliable analysis.
  # Older compilers still build; they just produce fewer diagnostics.
  set(_iccdev_strict_capable FALSE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15.0")
    set(_iccdev_strict_capable TRUE)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
    set(_iccdev_strict_capable TRUE)
  endif()
  if(_iccdev_strict_capable)
    add_compile_options(
      -Werror=uninitialized
      -Wshadow
      -Wnull-dereference
      -Wundef
      -Wpointer-arith
    )
    message(STATUS "macOS: strict warnings ENABLED (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
  else()
    message(STATUS "macOS: strict warnings DISABLED -- ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} below floor (GCC 15 / Clang 14). Base hardening still active.")
  endif()

  _iccdev_configure_fortify_source("macOS")

  message(STATUS "macOS build: Warning flags and security hardening applied.")

elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  message(STATUS "Detected platform: Linux")

  # Recommended flags for Linux/Clang or GCC.
  # Base set: works on any C++17-capable compiler (GCC 11+, Clang 14+).
  add_compile_options(
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -Wimplicit-fallthrough
    -Wsign-compare
    -Wnon-virtual-dtor
    -Wuninitialized
  )
  # Hardening flags probed individually: constrained toolchains (e.g. Clang 11
  # on Raspberry Pi aarch64) may not support all flags and would emit warnings
  # that confuse non-technical users.  Only add what the compiler accepts.
  _iccdev_add_cxx_flag_if_supported(-fstack-protector-strong _ICCDEV_HAVE_STACK_PROTECTOR_STRONG)
  _iccdev_add_cxx_flag_if_supported(-fstack-clash-protection _ICCDEV_HAVE_STACK_CLASH_PROTECTION)

  # Strict warning tier: requires GCC 15+ or Clang 14+ so all contributors and
  # CI see the same diagnostics. Older toolchains still build; they get the
  # base set above plus reduced analysis. ci-docker (ubuntu:26.04 / GCC 15)
  # and the iccDEV Tool Tests workflow (clang-18) both meet the floor.
  set(_iccdev_strict_capable FALSE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15.0")
    set(_iccdev_strict_capable TRUE)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
    set(_iccdev_strict_capable TRUE)
  endif()
  if(_iccdev_strict_capable)
    add_compile_options(
      -Werror=uninitialized
      -Wshadow
      -Wnull-dereference
      -Wundef
      -Wpointer-arith
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      add_compile_options(-Wlogical-op)
    endif()
    message(STATUS "Linux: strict warnings ENABLED (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
  else()
    message(STATUS "Linux: strict warnings DISABLED -- ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} below floor (GCC 15 / Clang 14). Base hardening still active. Upgrade to match CI diagnostics.")
  endif()

  # Opt-in: Effective C++ ruleset (currently ~3000 sites; multi-PR cleanup)
  option(ENABLE_STRICT_EFFC "Enable -Weffc++ (Effective C++ guidelines)" OFF)
  if(ENABLE_STRICT_EFFC)
    add_compile_options(-Weffc++)
  endif()

  # Opt-in: float-to-double promotion warnings (~2000 sites; library-wide cleanup)
  option(ENABLE_DOUBLE_PROMOTION "Enable -Wdouble-promotion" OFF)
  if(ENABLE_DOUBLE_PROMOTION)
    add_compile_options(-Wdouble-promotion)
  endif()

  # Hardening: RELRO and BIND_NOW for executables
  add_link_options(-Wl,-z,relro,-z,now)

  _iccdev_configure_fortify_source("Linux")

  message(STATUS "Linux build: Warning flags and security hardening applied.")

elseif(EMSCRIPTEN)
  message(STATUS "Detected platform: Emscripten (WebAssembly)")

  # Emscripten uses its own toolchain -- no ELF hardening, no stack-protector.
  # Warning flags only; linker/runtime flags are set by emcmake/emcc.
  # Base set: works on any C++17-capable Emscripten toolchain.
  add_compile_options(
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -Wimplicit-fallthrough
    -Wsign-compare
    -Wnon-virtual-dtor
    -Wuninitialized
  )

  # Strict tier (Emscripten ships clang; gate on Clang 14+ for parity with Linux/macOS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
    add_compile_options(
      -Werror=uninitialized
      -Wshadow
      -Wnull-dereference
      -Wundef
      -Wpointer-arith
    )
    message(STATUS "Emscripten: strict warnings ENABLED (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
  else()
    message(STATUS "Emscripten: strict warnings DISABLED -- ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} below floor (Clang 14).")
  endif()

  message(STATUS "Emscripten build: Warning flags applied (no ELF hardening).")

elseif(WIN32)
  message(STATUS "Detected platform: Windows")

  if(MSVC)
    # Windows + MSVC flags
    add_compile_options(/std:c++17 /D_WINDOWS /EHsc /GS)

    # ASLR + DEP + CET shadow stacks
    add_link_options(/DYNAMICBASE /NXCOMPAT)
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _iccdev_msvc_platform)
    # CET (Control-flow Enforcement Technology) requires VS 2019 16.7+ and is
    # not accepted by the MSVC ARM/ARM64 linker.
    if(MSVC_VERSION GREATER_EQUAL 1927 AND NOT _iccdev_msvc_platform MATCHES "^arm")
      add_link_options(/CETCOMPAT)
    endif()

    message(STATUS "Windows build: Compiler flags set for MSVC with C++17, buffer security, and ASLR/DEP.")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Windows build: Using CMake C++17 defaults for MinGW/Clang.")
  else()
    message(STATUS "Windows build: Using compiler defaults for ${CMAKE_CXX_COMPILER_ID}.")
  endif()

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

  # LibXml2 already found unconditionally above; resolve target vs legacy variables
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
if(ENABLE_ICCXML)
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
endif()

# NOTE: IccXML and generated-header includes now propagated via
# target_include_directories(PUBLIC) on IccProfLib2 and IccXML2 targets

# Ensure generated version headers are available globally
include_directories(${CMAKE_CURRENT_BINARY_DIR}/IccProfLib)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/IccXML)

#
# JSON library (parallel to IccXML, uses nlohmann/json)
#
IF(ENABLE_ICCJSON)
  if(VERBOSE_CONFIG)
    message(STATUS "nlohmann_json          : ${nlohmann_json_DIR} (version: ${nlohmann_json_VERSION})")
  endif()

  # ICC_JSON_ORDERED: use nlohmann::ordered_json for deterministic key order.
  # Default OFF (unordered is faster). Enable for reproducible output / CI logs.
  IF(ICC_JSON_ORDERED)
    message(STATUS "IccJSON key order      : ordered (deterministic)")
  ELSE()
    message(STATUS "IccJSON key order      : unordered (faster)")
  ENDIF()

  message(STATUS "Adding subdirectory for IccJSON.")
  add_subdirectory(IccJSON)

  # Set default link target for IccJSON (matches build configuration)
  IF(ENABLE_SHARED_LIBS)
    set(TARGET_LIB_ICCJSON IccJSON2 CACHE INTERNAL "Link target for IccJSON2")
  ELSE()
    set(TARGET_LIB_ICCJSON IccJSON2-static CACHE INTERNAL "Link target for IccJSON2")
  ENDIF()

  # Ensure IccJSON headers are available globally
  include_directories(${TOP_SOURCE_DIR}/IccJSON/IccLibJSON/)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/IccJSON)
ENDIF(ENABLE_ICCJSON)

#
# IccConnect library (JSON-configurable CMM transform construction)
#
IF(ENABLE_ICCJSON)
  message(STATUS "Adding subdirectory for IccConnect.")
  add_subdirectory(IccConnect)

  # Set default link target for IccConnect (matches build configuration)
  IF(ENABLE_SHARED_LIBS)
    set(TARGET_LIB_ICCCONNECT IccConnect2 CACHE INTERNAL "Link target for IccConnect2")
  ELSE()
    set(TARGET_LIB_ICCCONNECT IccConnect2-static CACHE INTERNAL "Link target for IccConnect2")
  ENDIF()

  # Ensure IccConnect headers are available globally
  include_directories(${TOP_SOURCE_DIR}/IccConnect/IccLibConnect/)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/IccConnect)
ENDIF()

# Diagnostic: Output current linker flags
if(VERBOSE_CONFIG)
  message(STATUS "CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")
  message(STATUS "CMAKE_SHARED_LINKER_FLAGS = ${CMAKE_SHARED_LINKER_FLAGS}")
  message(STATUS "CMAKE_MODULE_LINKER_FLAGS = ${CMAKE_MODULE_LINKER_FLAGS}")
endif()


 # Function to add and log subdirectories
function(add_tool_subdirectory TOOL_SUBDIRECTORY)
    message(STATUS "Adding Subdirectory: ${TOOL_SUBDIRECTORY}...")
    add_subdirectory(${TOOL_SUBDIRECTORY})
    message(STATUS "Successfully added Subdirectory: ${TOOL_SUBDIRECTORY}")
endfunction()

function(iccdev_fixup_msvc_compile_pdb TARGET_NAME)
  if(MSVC AND TARGET "${TARGET_NAME}")
    set_target_properties(${TARGET_NAME} PROPERTIES
      COMPILE_PDB_NAME "${TARGET_NAME}"
      COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/compile-pdb/$<CONFIG>"
    )
  endif()
endfunction()

IF(ENABLE_TOOLS)
  # Add XML-related tools
  IF(ENABLE_ICCXML)
    ADD_SUBDIRECTORY(Tools/IccFromXml)
    ADD_SUBDIRECTORY(Tools/IccToXml)
  ENDIF()

  # Add JSON-related tools
  IF(ENABLE_ICCJSON AND TARGET_LIB_ICCJSON)
    message(STATUS "Adding Subdirectory IccFromJson.")
    ADD_SUBDIRECTORY(Tools/IccFromJson)
    message(STATUS "Adding Subdirectory IccToJson.")
    ADD_SUBDIRECTORY(Tools/IccToJson)
  ENDIF()
  IF(TARGET_LIB_ICCCONNECT)
    ADD_SUBDIRECTORY(Tools/IccApplyNamedCmm)
  ELSE()
    message(STATUS "IccJSON disabled - skipping JSON runtime-config tools.")
  ENDIF()
  ADD_SUBDIRECTORY(Tools/IccDescribeSinkTest)
  ADD_SUBDIRECTORY(Tools/IccDumpProfile)
  ADD_SUBDIRECTORY(Tools/IccPawgReport)
  ADD_SUBDIRECTORY(Tools/IccRoundTrip)
  ADD_SUBDIRECTORY(Tools/IccFromCube)
  ADD_SUBDIRECTORY(Tools/IccV5DspObsToV4Dsp)
  ADD_SUBDIRECTORY(Tools/IccApplyToLink)
  IF(TARGET_LIB_ICCCONNECT)
    ADD_SUBDIRECTORY(Tools/IccApplySearch)
  ENDIF()
  ADD_SUBDIRECTORY(Tools/IccProfileVisualize)
  ADD_SUBDIRECTORY(Tools/IccProfilePlot)

  # Ensure IccXML-dependent tools are built after IccXML library (safety net)
  IF(ENABLE_ICCXML)
    IF(TARGET iccFromXml AND TARGET ${TARGET_LIB_ICCXML})
      ADD_DEPENDENCIES(iccFromXml ${TARGET_LIB_ICCXML})
    ENDIF()
    IF(TARGET iccToXml AND TARGET ${TARGET_LIB_ICCXML})
      ADD_DEPENDENCIES(iccToXml ${TARGET_LIB_ICCXML})
    ENDIF()
  ENDIF()

  # Ensure IccJSON-dependent tools are built after IccJSON library (safety net)
  IF(ENABLE_ICCJSON)
    IF(TARGET iccFromJson AND TARGET ${TARGET_LIB_ICCJSON})
      ADD_DEPENDENCIES(iccFromJson ${TARGET_LIB_ICCJSON})
    ENDIF()
    IF(TARGET iccToJson AND TARGET ${TARGET_LIB_ICCJSON})
      ADD_DEPENDENCIES(iccToJson ${TARGET_LIB_ICCJSON})
    ENDIF()
  ENDIF()


  if(ENABLE_IMAGE_TOOLS)
    message(STATUS "Adding Subdirectory IccJpegDump.")
    ADD_SUBDIRECTORY(Tools/IccJpegDump)

    # PNG already found above when image tools are enabled.
    message(STATUS "Checking for PNG...")
    if(PNG_FOUND)
      message(STATUS "PNG Library            : ${PNG_LIBRARIES}")
      message(STATUS "PNG Include Directory  : ${PNG_INCLUDE_DIRS}")
      message(STATUS "PNG Version            : ${PNG_VERSION}")
    else()
      message(FATAL_ERROR " PNG not found. Please install libpng-dev or install via vcpkg.")
    endif()
    if(VERBOSE_CONFIG)
      message(STATUS "Adding Subdirectory IccPngDump.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccPngDump)

    # TIFF already found above; scope includes to targets, not globally
    if (TIFF_FOUND)
      # TIFF includes are now scoped to each tool via target_include_directories

      # Add TIFF-dependent tools
      if(VERBOSE_CONFIG)
        message(STATUS "Adding Subdirectory IccApplyProfiles.")
      endif()
      IF(TARGET_LIB_ICCCONNECT)
        ADD_SUBDIRECTORY(Tools/IccApplyProfiles)
      ENDIF()
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
  endif()

  if(WIN32 AND ENABLE_CMM_TOOLS)
    if(VERBOSE_CONFIG)
      message(STATUS "Adding Subdirectory IccDEVCmm.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccDEVCmm)
  endif()

  if(WIN32 AND ENABLE_IIS_TOOLS)
    if(VERBOSE_CONFIG)
      message(STATUS "Adding Subdirectory IccIisIsapi.")
    endif()
    ADD_SUBDIRECTORY(Tools/IccIisIsapi)
  endif()

  # wxWidgets GUI tool (optional, controlled by ENABLE_WXWIDGETS)
  if(ENABLE_WXWIDGETS)
    find_package(wxWidgets QUIET COMPONENTS core base)
    if(wxWidgets_FOUND)
      include(${wxWidgets_USE_FILE})
      message(STATUS "wxWidgets found: ${wxWidgets_LIBRARIES}")
      message(STATUS "Adding Subdirectory wxProfileDump.")
      ADD_SUBDIRECTORY(Tools/wxProfileDump)
    else()
      message(STATUS "wxWidgets not found - skipping wxProfileDump.")
    endif()
  else()
    message(STATUS "wxWidgets disabled by ENABLE_WXWIDGETS=OFF - skipping wxProfileDump.")
  endif()

  set(_iccdev_tool_targets
      iccFromXml
      iccToXml
      iccFromJson
      iccToJson
      iccJpegDump
      iccApplyNamedCmm
      iccDescribeSinkTest
      iccDumpProfile
      iccPawgReport
      iccRoundTrip
      iccFromCube
      iccV5DspObsToV4Dsp
      iccApplyToLink
      iccApplySearch
      iccPngDump
      iccIisIsapi
      iccIisIsapiSmoke
      iccApplyProfiles
      iccSpecSepToTiff
      iccTiffDump
      iccDumpProfileGui
      iccProfileVisualize
  )
  foreach(_iccdev_tool_target IN LISTS _iccdev_tool_targets)
    iccdev_fixup_msvc_compile_pdb(${_iccdev_tool_target})
  endforeach()

  if(APPLE AND ENABLE_MACOS_GUARD_MALLOC)
    add_custom_target(check-macos-guard-malloc
      COMMAND "${CMAKE_COMMAND}"
        "-DICCDEV_BINARY_DIR=${CMAKE_BINARY_DIR}"
        -P "${CMAKE_CURRENT_LIST_DIR}/VerifyMacOSGuardMalloc.cmake"
      COMMENT "Verify macOS tool binaries carry LC_UUID for libgmalloc"
      VERBATIM)
    foreach(_iccdev_tool_target IN LISTS _iccdev_tool_targets)
      if(TARGET "${_iccdev_tool_target}")
        add_dependencies(check-macos-guard-malloc "${_iccdev_tool_target}")
      endif()
    endforeach()
  endif()

  if(WIN32 AND ICCDEV_WINDOWS_UNIFIED_RUNTIME)
    if(_is_multi_config)
      set(_iccdev_windows_stage_runtime_dir "${ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR}/$<CONFIG>")
      set(_iccdev_windows_stage_config "$<CONFIG>")
    else()
      set(_iccdev_windows_stage_runtime_dir "${ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR}")
      set(_iccdev_windows_stage_config "${CMAKE_BUILD_TYPE}")
    endif()

    add_custom_target(iccdev-stage-windows-runtime ALL
      COMMAND "${CMAKE_COMMAND}"
        "-DICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DICCDEV_RUNTIME_DIR=${_iccdev_windows_stage_runtime_dir}"
        "-DICCDEV_CONFIG=${_iccdev_windows_stage_config}"
        -P "${CMAKE_CURRENT_LIST_DIR}/StageWindowsRuntime.cmake"
      COMMENT "Stage Windows runtime DLL dependencies into ${ICCDEV_WINDOWS_RUNTIME_OUTPUT_DIR}"
      VERBATIM)

    set(_iccdev_windows_runtime_dependencies
      ${_iccdev_tool_targets}
      IccProfLib2
      IccXML2
      IccJSON2
      IccConnect2
      RefIccMAX
    )
    foreach(_iccdev_windows_runtime_dependency IN LISTS _iccdev_windows_runtime_dependencies)
      if(TARGET "${_iccdev_windows_runtime_dependency}")
        add_dependencies(iccdev-stage-windows-runtime "${_iccdev_windows_runtime_dependency}")
      endif()
    endforeach()
  endif()

ENDIF(ENABLE_TOOLS)

IF( ENABLE_TESTS )
  ENABLE_TESTING()
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
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccApplyNamedCmm ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
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
    COMMAND cp -av ${CMAKE_SOURCE_DIR}/../../Testing/iccProfileVisualize ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}${ARCH}-${${PROJECT_UP_NAME}_VERSION}/Testing/
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
                DESTINATION ${CMAKE_MODULES_INSTALL_DIR} COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT}
        )

  # Install exported targets file alongside the Config
  INSTALL( EXPORT RefIccMAXTargets
           NAMESPACE RefIccMAX::
           DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
           COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT}
  )
ENDIF(ENABLE_INSTALL_RIM)

# Build-tree export; allows find_package() from build dir without install
export( EXPORT RefIccMAXTargets
        NAMESPACE RefIccMAX::
        FILE "${CMAKE_CURRENT_BINARY_DIR}/RefIccMAXTargets.cmake"
)

if(ENABLE_CPACK_PACKAGING AND ENABLE_INSTALL_RIM)
  include("${CMAKE_CURRENT_SOURCE_DIR}/Packaging/RefIccMAXCPack.cmake")
endif()
