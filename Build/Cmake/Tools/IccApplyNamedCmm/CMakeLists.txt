# 
# IccApplyNamedCmm CMakeLists.txt | DemoIccMAX Project
# Copyright (©) 2024-2025 The International Color Consortium. All rights reserved.
# 
#
# Last Updated: 10-AUGUST-2025 at 1500Z by David Hoyt
#
# Changes: 	Fixups for ApplyNamedCmm & NamedProfiles CMakeLists.txt
#						
#
#
#
#
#
#
#
#
#
#
#

# project(iccApplyNamedCmm LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(nlohmann_json REQUIRED)  # provides nlohmann_json::nlohmann_json

# ---- Sources ----
# Reminder: CMAKE_SOURCE_DIR is Build/Cmake; the repo root is two levels up.
set(SRC_ROOT "${CMAKE_SOURCE_DIR}/../..")

set(SOURCES
  "${SRC_ROOT}/Tools/CmdLine/IccApplyNamedCmm/iccApplyNamedCmm.cpp"
  "${SRC_ROOT}/Tools/CmdLine/IccCommon/IccCmmConfig.cpp"
  "${SRC_ROOT}/Tools/CmdLine/IccCommon/IccJsonUtil.cpp"
)

# Optional (if headers used from this dir)
set(PRIVATE_INCLUDES
  "${SRC_ROOT}/Tools/CmdLine/IccApplyProfiles"
)

# ---- Target ----
if(NOT TARGET iccApplyNamedCmm)
  add_executable(iccApplyNamedCmm ${SOURCES})

  target_include_directories(iccApplyNamedCmm PRIVATE ${PRIVATE_INCLUDES})

  target_link_libraries(iccApplyNamedCmm PRIVATE
    ${TARGET_LIB_ICCPROFLIB}
    IccXML2
    nlohmann_json::nlohmann_json
  )

  if(ENABLE_INSTALL_RIM)
    install(TARGETS iccApplyNamedCmm DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
else()
  message(STATUS "iccApplyNamedCmm already defined; skipping.")
endif()
