#################################################################################
# iccFromXml Cmake Configuration| iccDEV Project
# Copyright (c) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
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
#
#################################################################################

SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/IccXML/CmdLine/IccFromXml/IccFromXml.cpp )
SET( TARGET_NAME iccFromXml )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

# Ensure IccXML2 libraries are built before linking
# When both shared+static are enabled, depend on BOTH targets so the
# static-lib POST_BUILD alias (.lib copy) completes before tool link phase.
IF(ENABLE_SHARED_LIBS AND ENABLE_STATIC_LIBS)
  ADD_DEPENDENCIES( ${TARGET_NAME} IccXML2 IccXML2-static )
ELSEIF(ENABLE_SHARED_LIBS)
  ADD_DEPENDENCIES( ${TARGET_NAME} IccXML2 )
ELSE()
  ADD_DEPENDENCIES( ${TARGET_NAME} IccXML2-static )
ENDIF()

TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCXML} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
