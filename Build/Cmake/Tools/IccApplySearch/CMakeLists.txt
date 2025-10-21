# 
# iccApplySearch CMakeLists.txt | IccDEV Project
# Copyright (©) 2025 The International Color Consortium. All rights reserved.
# 
#
# Last Updated: 21-OCT-2025 at 0300Z by David Hoyt
#
# Changes: 	Asan ODR Bug for IccCmmSearch.cpp
#						
#						
# TODO: Refactor Debug, Release, Asan 
# 			Collapse 3-tier CMakeLists.txt, Housekeeping
#				Refactor Base Configs & logging to .mk's 
#

# cmake_minimum_required(VERSION 3.2)
project(iccApplySearch)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_ROOT "${CMAKE_SOURCE_DIR}/../..")

# Find nlohmann-json
find_package(nlohmann_json REQUIRED)

# Sources
set(SOURCES
  ${SRC_ROOT}/Tools/CmdLine/IccApplySearch/iccApplySearch.cpp
  ${SRC_ROOT}/Tools/CmdLine/IccCommon/IccJsonUtil.cpp
  ${SRC_ROOT}/Tools/CmdLine/IccCommon/IccCmmConfig.cpp
)

# Define the executable
add_executable(iccApplySearch ${SOURCES})

# Link libraries
target_link_libraries(iccApplySearch PRIVATE
  IccProfLib2
  IccXML2
  nlohmann_json::nlohmann_json
)

# Compiler options
target_compile_options(iccApplySearch PRIVATE)
