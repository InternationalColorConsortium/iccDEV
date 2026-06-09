# 
# IccApplyToLink CMakeLists.txt | DemoIccMAX Project
# Copyright (©) 2024 The International Color Consortium. All rights reserved.
# 
#
# Last Updated: 02-DEC-2024 by David Hoyt
#
# Changes: 	Modified Build/Cmake/CMakelists.txt and others
#           Added IccFromCubeProject
#           Added IccV5DspObsToV4Dsp
#						Adjusted Cross Platform Build Args
#						
# TODO: Refactor Debug, Release, Asan 
# 			Collapse 3-tier CMakeLists.txt, Housekeeping
#				Refactor Base Configs & logging to .mk's 
#

# Check if the target is already defined before adding it
IF(NOT TARGET iccApplyToLink)
  # Define the source path relative to this file
  SET(SRC_PATH ../../../..)
  SET(SOURCES ${SRC_PATH}/Tools/CmdLine/IccApplyToLink/iccApplyToLink.cpp)
  SET(TARGET_NAME iccApplyToLink)

  # Define the executable target
  ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})

  # Link the necessary libraries to the target
  TARGET_LINK_LIBRARIES(${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB})

  # Optional: Install the executable if enabled
  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT})
  ENDIF()
ELSE()
  # Log a warning if the target already exists
  MESSAGE(WARNING "Target ${TARGET_NAME} already exists. Skipping duplicate addition.")
ENDIF()
