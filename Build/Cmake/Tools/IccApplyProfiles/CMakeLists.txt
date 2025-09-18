# 
# IccApplyProfiles CMakeLists.txt | DemoIccMAX Project
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

if(NOT TARGET iccApplyProfiles)
  # Top of repo is two dirs above Build/Cmake
  set(SRC_ROOT "${CMAKE_SOURCE_DIR}/../..")

  # Find deps
  find_package(nlohmann_json CONFIG REQUIRED) # provides nlohmann_json::nlohmann_json
  # If TIFF wasn't already found/created elsewhere, this makes sure we have either the target or the plain var
  if(NOT TARGET TIFF::TIFF AND NOT TIFF_LIBRARY)
    find_package(TIFF REQUIRED)
  endif()

  set(SOURCES
    "${SRC_ROOT}/Tools/CmdLine/IccApplyProfiles/iccApplyProfiles.cpp"
    "${SRC_ROOT}/Tools/CmdLine/IccApplyProfiles/TiffImg.cpp"
    "${SRC_ROOT}/Tools/CmdLine/IccCommon/IccJsonUtil.cpp"
    "${SRC_ROOT}/Tools/CmdLine/IccCommon/IccCmmConfig.cpp"
  )

  foreach(f IN LISTS SOURCES)
    if(NOT EXISTS "${f}")
      message(FATAL_ERROR "iccApplyProfiles: missing source: ${f}")
    endif()
  endforeach()

  set(TARGET_NAME iccApplyProfiles)
  add_executable(${TARGET_NAME} ${SOURCES})

  # Normalize TIFF link name
  if(TARGET TIFF::TIFF)
    set(_TIFF_LIB TIFF::TIFF)
  else()
    set(_TIFF_LIB ${TIFF_LIBRARY})
  endif()

  target_link_libraries(${TARGET_NAME}
    PRIVATE
      ${TARGET_LIB_ICCPROFLIB}
      ${_TIFF_LIB}
      nlohmann_json::nlohmann_json
  )

  if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
else()
  message(WARNING "Target iccApplyProfiles already exists. Skipping duplicate addition.")
endif()
