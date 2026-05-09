#
# iccApplySearch CMakeLists.txt | IccDEV Project
# Copyright (©) 2025 The International Color Consortium. All rights reserved.
#
#
# Last Updated: 21-OCT-2025 at 0300Z by David Hoyt
#
# Changes: 	Asan ODR Bug for IccCmmSearch.cpp
#               Updated to link against IccConnect2 library

project(iccApplySearch)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_ROOT "${CMAKE_SOURCE_DIR}/../..")

# Find nlohmann-json
find_package(nlohmann_json REQUIRED)

# Sources
set(SOURCES
  ${SRC_ROOT}/Tools/CmdLine/IccApplySearch/iccApplySearch.cpp
)

# Define the executable
add_executable(iccApplySearch ${SOURCES})

# Link libraries
target_link_libraries(iccApplySearch PRIVATE
  ${TARGET_LIB_ICCPROFLIB}
  ${TARGET_LIB_ICCCONNECT}
  nlohmann_json::nlohmann_json
)

# Compiler options
target_compile_options(iccApplySearch PRIVATE)

# Install
IF(ENABLE_INSTALL_RIM)
  INSTALL(TARGETS iccApplySearch DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT shared-proof)
ENDIF(ENABLE_INSTALL_RIM)
