#################################################################################
# iccMAXCmm Cmake Project Configuration | iccMAX Project
# Copyright (©) 2024-2025 The International Color Consortium.
#                                        All rights reserved.
# Last Updated: 13-AUGUST-2025 at 1600Z by David Hoyt
#
# Changes:  Runner not building
# TODO :    Adjusted CMM build, wrapped build type in conditional
#           Debug to MinSizeRel and x64-Debug - IN PROCESS
#           Library path.part slightly hardcoded
#           Refactor for x64-MinSizeRel, x64-Debug, Unix/Win/macOS wrap
#           HARDCODED - Release or Debug, needs rel-debug wrapper
#           Refactor Debug, Release, Asan
#           Collapse 3-tier CMakeLists.txt, Housekeeping
#           Refactor Base Configs & logging to .mk's
#################################################################################

# Set the Target Name
set(TARGET_NAME RefIccMAX)

# Detect OS and Set Source Paths (Only build on Windows, bypass macOS and Unix)
if (WIN32)
    message(STATUS "Configuring for Windows...")
    set(OS_SPECIFIC_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../../../Tools/Winnt/DemoIccMAXCmm")
else()
    message(STATUS "Skipping DemoIccMAXCmm build on non-Windows platforms.")
    add_custom_target(${TARGET_NAME})  # dummy target to prevent dependency errors
    return()
endif()

# Compiler Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Output Directory based on Configuration
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tools/DemoIccMAXCmm/x64/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tools/DemoIccMAXCmm/x64/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tools/DemoIccMAXCmm/x64/$<CONFIG>")

# Source Files
set(SOURCES
    "${OS_SPECIFIC_SOURCES}/DemoIccMAXCmm.cpp"
    "${OS_SPECIFIC_SOURCES}/DemoIccMAXCmm.rc"
    "${OS_SPECIFIC_SOURCES}/resource.h"
    "${OS_SPECIFIC_SOURCES}/DemoIccMAXCmm.h"
    "${OS_SPECIFIC_SOURCES}/stdafx.cpp"
    "${OS_SPECIFIC_SOURCES}/stdafx.h"
)

# Define Shared Library
add_library(${TARGET_NAME} SHARED ${SOURCES})

# MSVC-Specific Settings
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")

    # Precompiled header: create with stdafx.cpp, use for other .cpp files
    set_source_files_properties("${OS_SPECIFIC_SOURCES}/stdafx.cpp"
        PROPERTIES COMPILE_FLAGS "/Ycstdafx.h /Fp${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RefIccMAX.pch")
    foreach(SRC ${SOURCES})
        if (NOT SRC STREQUAL "${OS_SPECIFIC_SOURCES}/stdafx.cpp" AND SRC MATCHES "^.*\\.cpp$")
            set_source_files_properties("${SRC}"
                PROPERTIES COMPILE_FLAGS "/Yustdafx.h /Fp${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RefIccMAX.pch")
        endif()
    endforeach()
    set_source_files_properties("${OS_SPECIFIC_SOURCES}/DemoIccMAXCmm.rc" PROPERTIES COMPILE_FLAGS "")
endif()

# Compiler Definitions
target_compile_definitions(${TARGET_NAME} PRIVATE WIN32 _WINDOWS _USRDLL DemoIccMAXCmm_EXPORTS _MBCS)

# Include Directories
# include_directories(${OS_SPECIFIC_SOURCES} ${TOP_SOURCE_DIR}/IccProfLib)
target_include_directories(${TARGET_NAME} PRIVATE ${OS_SPECIFIC_SOURCES} ${TOP_SOURCE_DIR}/IccProfLib)

# Library Linking
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIB_ICCPROFLIB})
set(LINK_LIBS
    Mscms.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
    advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib
)
target_link_libraries(${TARGET_NAME} PRIVATE ${LINK_LIBS})

# ---- Name & PDB via properties (no /OUT or /PDB overrides) -------------------
set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME "RefIccMAX"                               # DLL name → RefIccMAX.dll
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
    PDB_OUTPUT_DIRECTORY     "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# Keep non-path link flags
target_link_options(${TARGET_NAME} PRIVATE "/DYNAMICBASE:NO" "/NODEFAULTLIB:LIBCMT")

message(STATUS "RefIccMAX.dll build configuration complete.")
