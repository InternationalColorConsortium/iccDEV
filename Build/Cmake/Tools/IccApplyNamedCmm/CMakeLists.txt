# 
# IccApplyNamedCmm CMakeLists.txt | iccDEV Project
# Copyright (©) 2024-2026 The International Color Consortium. All rights reserved.
# 
#
# Last Updated: 02-JAN-2026 at 0230Z by David Hoyt
#
# Changes: 	Fixups for ApplyNamedCmm & NamedProfiles CMakeLists.txt
#		Make windows ASAN Changes				
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

# ---- Sources root ----
# Reminder: CMAKE_SOURCE_DIR is Build/Cmake; the repo root is two levels up.
set(SRC_ROOT "${CMAKE_SOURCE_DIR}/../..")

# ---- Dependencies ----
find_package(nlohmann_json REQUIRED)

# ---- Target ----
if(NOT TARGET iccApplyNamedCmm)

  set(SOURCES
    "${SRC_ROOT}/Tools/CmdLine/IccApplyNamedCmm/iccApplyNamedCmm.cpp"
    "${SRC_ROOT}/Tools/CmdLine/IccCommon/IccCmmConfig.cpp"
    "${SRC_ROOT}/Tools/CmdLine/IccCommon/IccJsonUtil.cpp"
  )

  add_executable(iccApplyNamedCmm ${SOURCES})

  set_target_properties(iccApplyNamedCmm PROPERTIES
    LINKER_LANGUAGE CXX
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
  )

  target_include_directories(iccApplyNamedCmm PRIVATE
    "${SRC_ROOT}/Tools/CmdLine/IccApplyProfiles"
  )

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
