#
# IccHdrFallback CMakeLists.txt | iccDEV Project
# Copyright (©) The International Color Consortium. All rights reserved.
#
# Bakes an HDR profile's tone-mapped SDR rendering into an A2B0/B2A0 pair.
#

# Check if the target is already defined before adding it
IF(NOT TARGET iccHdrFallback)
  SET(SRC_PATH ../../../..)
  SET(SOURCES ${SRC_PATH}/Tools/CmdLine/IccHdrFallback/iccHdrFallback.cpp)
  SET(TARGET_NAME iccHdrFallback)

  ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})
  TARGET_LINK_LIBRARIES(${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
  ENDIF(ENABLE_INSTALL_RIM)
ENDIF()
