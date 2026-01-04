#################################################################################
# iccToXml Cmake Configuration| iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#
#  Last Updated: 04-JAN-2026 at 2350Z by David Hoyt
#
#  Changes: Bump Version String to v2.3.1.2
#           Recent PRs address range of security & stability issues
#           Stack Corruption, NaN, Undefined Behavior (UB) & Overflows
#
#
#
#  TODO:    Align Version on IccProfLibVer.h, Housekeeping, Debug Options
#           Version Headers, Doxygen Config, Exports etc...
#           Refactor this to .mk's, add research args, other Housekeeping
#
#
#################################################################################

SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/IccXML/CmdLine/IccToXml/IccToXml.cpp )
SET( TARGET_NAME iccToXml )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

# Ensure IccXML2 is built before linking (fixes race condition)
IF(ENABLE_SHARED_LIBS)
  ADD_DEPENDENCIES( ${TARGET_NAME} IccXML2 )
ELSE()
  ADD_DEPENDENCIES( ${TARGET_NAME} IccXML2-static )
ENDIF()

TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCXML} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
