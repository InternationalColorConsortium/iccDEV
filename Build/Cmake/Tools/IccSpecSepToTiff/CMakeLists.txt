###############################################################
#
# Copyright (c) 2026 International Color Consortium. 
#                 All rights reserved. 
#                 https://color.org
# 
#  Last Updated: 2026-03-24 20:49:38 UTC by David Hoyt
#
# CMake Configuration:
# - PUBLIC target_include_directories for IccProfLib2 and IccXML2
#   (build-dir-first so generated version header with git hash wins)
# - EXPORT RefIccMAXTargets for build-tree and install-tree consumers
# - New FindRefIccMAX.cmake module for standalone find_package()
# - Modern RefIccMAXConfig.cmake.in using target-based imports
# - Tool CMakeLists: target_include_directories(PRIVATE) replaces global
#
#################################################################################

SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/Tools/CmdLine/IccSpecSepToTiff/iccSpecSepToTiff.cpp
              ${SRC_PATH}/Tools/CmdLine/IccApplyProfiles/TiffImg.cpp )
SET( TARGET_NAME iccSpecSepToTiff )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )
target_include_directories( ${TARGET_NAME} PRIVATE
  ${SRC_PATH}/Tools/CmdLine/IccApplyProfiles/
  ${TIFF_INCLUDE_DIR}
)
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} ${TIFF_LIBRARY} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
