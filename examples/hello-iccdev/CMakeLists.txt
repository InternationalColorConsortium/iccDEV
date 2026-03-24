#################################################################################
# hello-iccdev Cmake Configuration| iccDEV Project
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-03-24 20:49:38 UTC by David Hoyt
#
# CMake Configuration:
# - PUBLIC target_include_directories for IccProfLib2 and IccXML2
#   (build-dir-first so generated version header with git hash wins)
# - EXPORT RefIccMAXTargets for build-tree and install-tree consumers
# - New FindRefIccMAX.cmake module for standalone find_package()
# - Modern RefIccMAXConfig.cmake.in using target-based imports
# - Tool CMakeLists: target_include_directories(PRIVATE) replaces global
# 
################################################################################
# hello-iccdev — Standalone example consuming IccProfLib2 + IccXML2
#
# Three discovery paths (tried in order):
#
# ── Path 1: Installed RefIccMAX package ──────────────────────────────────────
#
#   cmake -B build -DCMAKE_PREFIX_PATH="/usr/local" ..
#   cmake --build build
#
# ── Path 2: Build-tree export (in-source-tree development) ───────────────────
#
#   # Build iccDEV first:
#   cd <iccDEV>/Build/Cmake && cmake -B build && cmake --build build
#
#   # Then build example pointing at that build dir:
#   cd examples/hello-iccdev
#   cmake -B build -DICCDEV_BUILD_DIR=../../Build/Cmake/build
#   cmake --build build
#
# ── Path 3: Manual library discovery (legacy / custom layouts) ───────────────
#
#   cmake -B build -DICCDEV_ROOT=/path/to/iccDEV \
#         -DICCDEV_BUILD_DIR=/path/to/iccDEV/build
#   cmake --build build
#
################################################################################

cmake_minimum_required(VERSION 3.16)
project(hello-iccdev
    LANGUAGES CXX
    DESCRIPTION "Minimal example consuming RefIccMAX (IccProfLib2 + IccXML2)"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Resolve iccDEV root and build dir defaults ──────────────────────────────
if(NOT DEFINED ICCDEV_ROOT)
    get_filename_component(ICCDEV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
else()
    get_filename_component(ICCDEV_ROOT "${ICCDEV_ROOT}" ABSOLUTE)
endif()
if(NOT DEFINED ICCDEV_BUILD_DIR)
    # Default: sibling build/ under Build/Cmake (standard cmake -B build layout)
    set(ICCDEV_BUILD_DIR "${ICCDEV_ROOT}/Build/Cmake/build")
else()
    get_filename_component(ICCDEV_BUILD_DIR "${ICCDEV_BUILD_DIR}" ABSOLUTE)
endif()

set(_FOUND_VIA "")

# ═══════════════════════════════════════════════════════════════════════════════
# Path 1: Installed package — find_package(RefIccMAX CONFIG)
# ═══════════════════════════════════════════════════════════════════════════════
find_package(RefIccMAX QUIET CONFIG)

if(RefIccMAX_FOUND)
    set(_FOUND_VIA "installed RefIccMAX package (v${REFICCMAX_VERSION})")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Path 2: Build-tree export — load RefIccMAXConfig.cmake from build dir
# ═══════════════════════════════════════════════════════════════════════════════
if(NOT RefIccMAX_FOUND AND EXISTS "${ICCDEV_BUILD_DIR}/RefIccMAXConfig.cmake")
    message(STATUS "Loading build-tree config from: ${ICCDEV_BUILD_DIR}")
    set(RefIccMAX_DIR "${ICCDEV_BUILD_DIR}")
    find_package(RefIccMAX QUIET CONFIG
        PATHS "${ICCDEV_BUILD_DIR}"
        NO_DEFAULT_PATH
    )
    if(RefIccMAX_FOUND)
        set(_FOUND_VIA "build-tree export (${ICCDEV_BUILD_DIR})")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Path 3: Manual fallback — find_library + target_include_directories
# ═══════════════════════════════════════════════════════════════════════════════
if(NOT RefIccMAX_FOUND)
    message(STATUS "RefIccMAX package not found, trying manual discovery...")
    message(STATUS "  ICCDEV_ROOT:      ${ICCDEV_ROOT}")
    message(STATUS "  ICCDEV_BUILD_DIR: ${ICCDEV_BUILD_DIR}")

    find_package(LibXml2 REQUIRED)

    # Search both Release and Debug subdirs (MSVC), plus flat dir (Makefiles)
    find_library(ICCPROFLIB2_LIB NAMES IccProfLib2
        PATHS "${ICCDEV_BUILD_DIR}/IccProfLib/Release"
              "${ICCDEV_BUILD_DIR}/IccProfLib/Debug"
              "${ICCDEV_BUILD_DIR}/IccProfLib"
        NO_DEFAULT_PATH
    )
    find_library(ICCXML2_LIB NAMES IccXML2
        PATHS "${ICCDEV_BUILD_DIR}/IccXML/Release"
              "${ICCDEV_BUILD_DIR}/IccXML/Debug"
              "${ICCDEV_BUILD_DIR}/IccXML"
        NO_DEFAULT_PATH
    )

    if(NOT ICCPROFLIB2_LIB)
        message(FATAL_ERROR
            "Could not find IccProfLib2.\n"
            "Options:\n"
            "  1. Build iccDEV: cd Build/Cmake && cmake -B build && cmake --build build\n"
            "  2. Set -DICCDEV_BUILD_DIR=<path-to-iccDEV-build>\n"
            "  3. Install iccDEV and set -DCMAKE_PREFIX_PATH=<install-prefix>"
        )
    endif()
    if(NOT ICCXML2_LIB)
        message(FATAL_ERROR "Could not find IccXML2 — see IccProfLib2 error above for options.")
    endif()

    message(STATUS "  IccProfLib2: ${ICCPROFLIB2_LIB}")
    message(STATUS "  IccXML2:     ${ICCXML2_LIB}")

    # Create IMPORTED targets to keep target_link_libraries clean
    add_library(RefIccMAX::IccProfLib2 UNKNOWN IMPORTED)
    set_target_properties(RefIccMAX::IccProfLib2 PROPERTIES
        IMPORTED_LOCATION "${ICCPROFLIB2_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${ICCDEV_ROOT}/IccProfLib;${ICCDEV_BUILD_DIR}/IccProfLib"
    )

    add_library(RefIccMAX::IccXML2 UNKNOWN IMPORTED)
    set_target_properties(RefIccMAX::IccXML2 PROPERTIES
        IMPORTED_LOCATION "${ICCXML2_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${ICCDEV_ROOT}/IccXML/IccLibXML;${ICCDEV_BUILD_DIR}/IccXML"
        INTERFACE_LINK_LIBRARIES "RefIccMAX::IccProfLib2;LibXml2::LibXml2"
    )

    set(_FOUND_VIA "manual discovery (${ICCDEV_BUILD_DIR})")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Build the example
# ═══════════════════════════════════════════════════════════════════════════════
message(STATUS "hello-iccdev: using ${_FOUND_VIA}")

add_executable(hello-iccdev hello-iccdev.cpp)
target_link_libraries(hello-iccdev PRIVATE
    RefIccMAX::IccProfLib2
    RefIccMAX::IccXML2
)

# ── Windows: copy DLLs next to executable ────────────────────────────────────
if(WIN32 AND ICCDEV_BUILD_DIR)
    file(GLOB _ICCDEV_DLLS
        "${ICCDEV_BUILD_DIR}/IccProfLib/Release/*.dll"
        "${ICCDEV_BUILD_DIR}/IccXML/Release/*.dll"
        "${ICCDEV_BUILD_DIR}/IccProfLib/Debug/*.dll"
        "${ICCDEV_BUILD_DIR}/IccXML/Debug/*.dll"
    )
    foreach(_DLL ${_ICCDEV_DLLS})
        add_custom_command(TARGET hello-iccdev POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_DLL}" "$<TARGET_FILE_DIR:hello-iccdev>"
        )
    endforeach()
endif()
