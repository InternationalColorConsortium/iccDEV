#################################################################################
# iccToJson CMake Configuration | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Converts a binary ICC profile to a JSON file.
#################################################################################

SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/IccJSON/CmdLine/IccToJson/IccToJson.cpp )
SET( TARGET_NAME iccToJson )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

# Ensure IccJSON2 is built before linking
IF(ENABLE_SHARED_LIBS)
  ADD_DEPENDENCIES( ${TARGET_NAME} IccJSON2 )
ELSE()
  ADD_DEPENDENCIES( ${TARGET_NAME} IccJSON2-static )
ENDIF()

TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCJSON} )

IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
