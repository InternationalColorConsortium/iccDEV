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

# Define the source path
set(SRC_PATH ../../../..)
set(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccJpegDump/iccJpegDump.cpp
)

# Resolve project root directory
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

# Set the target name
set(TARGET_NAME iccJpegDump)

# Add executable
add_executable(${TARGET_NAME} ${SOURCES})

# Include directories — target-scoped
target_include_directories(${TARGET_NAME} PRIVATE
    ${SRC_PATH}/Tools/CmdLine/IccJpegDump/
)

# The tool parses JPEG markers directly and does not call libjpeg.
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIB_ICCPROFLIB})
if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE ws2_32)
endif()

# Optional installation rule
if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
