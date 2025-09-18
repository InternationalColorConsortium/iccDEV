# 
# iccTiffDump CMakeLists.txt | DemoIccMAX Project
# Copyright (©) 2024-2025 The International Color Consortium. All rights reserved.
# 
#
# Last Updated: 12-AUGUST-2025 at 2300Z by David Hoyt
#
# Changes: 	Added JPEG & ZLIB Lib Links
#                     
#           
#						
#						
# TODO: Refactor Debug, Release, Asan 
# 			Collapse 3-tier CMakeLists.txt, Housekeeping
#				Refactor Base Configs & logging to .mk's 
#

SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/Tools/CmdLine/IccTiffDump/iccTiffDump.cpp
              ${SRC_PATH}/Tools/CmdLine/IccApplyProfiles/TiffImg.cpp )
SET( TARGET_NAME iccTiffDump )
INCLUDE_DIRECTORIES ( ${SRC_PATH}/Tools/CmdLine/IccApplyProfiles/ )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )
# TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} ${TIFF_LIBRARY} )
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} ${TIFF_LIBRARY} ${JPEG_LIBRARY} ${ZLIB_LIBRARY} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)

