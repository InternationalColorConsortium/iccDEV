# 
# IccFromCube CMakeLists.txt | DemoIccMAX Project
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
IF(NOT TARGET iccFromCube)
  SET(SRC_PATH ../../../..)
  SET(SOURCES ${SRC_PATH}/Tools/CmdLine/IccFromCube/iccFromCube.cpp)
  SET(TARGET_NAME iccFromCube)

  ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})
  TARGET_LINK_LIBRARIES(${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT})
  ENDIF(ENABLE_INSTALL_RIM)
ENDIF()
