#################################################################################
# iccPngDump Cmake Project Configuration| iccDEV Project
# Copyright (©) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#
# Last Updated: 2026-02-17 16:35:43 UTC by David Hoyt
#
# Changes: Housekeeping
#
#
#################################################################################

# Set the project name
project(iccPngDump)

# Define the source path
set(SRC_PATH ../../../..)
set(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccPngDump/iccPngDump.cpp
)

# Resolve project root directory
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
message(STATUS "PROJECT_ROOT_DIR resolved as: ${PROJECT_ROOT_DIR}")

# Set the target name
set(TARGET_NAME iccPngDump)
message(STATUS "Added executable target: ${TARGET_NAME}")

# Add executable
add_executable(${TARGET_NAME} ${SOURCES})

# Include directories
target_include_directories(${TARGET_NAME} PRIVATE
    ${SRC_PATH}/Tools/CmdLine/IccPngDump/
    ${SRC_PATH}/Libraries/IccProfLib/
)

# Unix check
if(UNIX AND NOT EMSCRIPTEN)
  target_include_directories(${TARGET_NAME} PRIVATE /usr/include /usr/local/include)
endif()

# Find PNG
find_package(PNG REQUIRED)

# Windows check
# Find the location of png.h
if(WIN32)
  set(_libpng16_candidates
    "$ENV{VCPKG_ROOT}/installed/$ENV{VCPKG_DEFAULT_TRIPLET}/include/libpng16"
    "$ENV{VCPKG_ROOT}/installed/$ENV{VCPKG_TARGET_TRIPLET}/include/libpng16"
    "C:/vcpkg/installed/x64-windows/include/libpng16"
    "C:/vcpkg/installed/x64-windows-static/include/libpng16"
  )
  if(DEFINED CMAKE_TOOLCHAIN_FILE)
    get_filename_component(_tc_dir "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    list(APPEND _libpng16_candidates
      "${_tc_dir}/../installed/x64-windows/include/libpng16"
      "${_tc_dir}/../installed/x64-windows-static/include/libpng16"
    )
  endif()
  foreach(_cand IN LISTS _libpng16_candidates)
    if(EXISTS "${_cand}/png.h")
      target_include_directories(${TARGET_NAME} PRIVATE "${_cand}")
      message(STATUS "Added Windows libpng16 include: ${_cand}")
      break()
    endif()
  endforeach()
endif()

# Link libraries
if(TARGET PNG::PNG)
  target_link_libraries(${TARGET_NAME} PRIVATE PNG::PNG)
else()
  target_link_libraries(${TARGET_NAME} PRIVATE ${PNG_LIBRARY})
endif()

# Target Link Lib
if(DEFINED TARGET_LIB_ICCPROFLIB)
  target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIB_ICCPROFLIB})
endif()

message(STATUS "Libraries linked for ${TARGET_NAME}")

message(STATUS "Output directories set for ${TARGET_NAME}")

# RIM Install
if(ENABLE_INSTALL_RIM)
    include(GNUInstallDirs)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
