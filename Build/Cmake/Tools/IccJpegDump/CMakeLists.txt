#################################################################################
# iccJpegDump Cmake Project Configuration| iccMAX Project
# Copyright (c) 2024-2025 The International Color Consortium. 
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
#################################################################################

# Set the project name
project(iccJpegDump)

# Platform-based build gate (skip for Ubuntu < 24.04)
if(UNIX AND EXISTS "/etc/lsb-release")
    execute_process(
        COMMAND lsb_release -rs
        OUTPUT_VARIABLE LSB_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Detected Ubuntu version: ${LSB_VERSION}")
    string(REGEX MATCHALL "[0-9]+" LSB_VERSION_PARTS "${LSB_VERSION}")

    if (NOT LSB_VERSION_PARTS)
        message(WARNING "iccJpegDump is skipped: couldn't be able to detect Ubuntu version")
        return()
    endif()

    list(GET LSB_VERSION_PARTS 0 UBUNTU_MAJOR)
    list(GET LSB_VERSION_PARTS 1 UBUNTU_MINOR)

    if(NOT UBUNTU_MAJOR OR NOT UBUNTU_MINOR)
        message(WARNING "iccJpegDump is skipped: couldn't be able to detect Ubuntu version")
        return()
    endif()

    math(EXPR LSB_VERSION_COMBINED "${UBUNTU_MAJOR} * 100 + ${UBUNTU_MINOR}")
    if(LSB_VERSION_COMBINED LESS 2404)
        message(WARNING "iccJpegDump is skipped: requires Ubuntu 24.04 or later")
        return()
    endif()
endif()

# Define the source path
set(SRC_PATH ../../../..)
set(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccJpegDump/iccJpegDump.cpp
)

# Resolve project root directory
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

# Set the target name
set(TARGET_NAME iccJpegDump)

# Platform-specific JPEG configuration
if(WIN32)
    find_path(JPEG_INCLUDE_DIR jpeglib.h PATH_SUFFIXES include)
    find_library(JPEG_LIBRARY NAMES jpeg turbojpeg PATH_SUFFIXES lib)
    if(NOT JPEG_INCLUDE_DIR)
        message(FATAL_ERROR "JPEG include directory not found on Windows")
    endif()
    set(PLATFORM_LIBS ws2_32)
else()
    find_package(JPEG REQUIRED)
    set(JPEG_LIBRARY ${JPEG_LIBRARIES})
endif()

# Add executable
add_executable(${TARGET_NAME} ${SOURCES})

# Include directories — target-scoped
target_include_directories(${TARGET_NAME} PRIVATE
    ${SRC_PATH}/Tools/CmdLine/IccJpegDump/
    ${SRC_PATH}/Libraries/IccProfLib/
    ${JPEG_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${TARGET_NAME}
    ${TARGET_LIB_ICCPROFLIB}
    ${JPEG_LIBRARY}
    ${PLATFORM_LIBS}
)

# Optional installation rule
if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
