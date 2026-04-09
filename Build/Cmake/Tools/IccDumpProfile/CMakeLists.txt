SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/Tools/CmdLine/IccDumpProfile/iccDumpProfile.cpp )
SET( TARGET_NAME iccDumpProfile )
SET( ICC_DUMP_LINK_LIB ${TARGET_LIB_ICCPROFLIB} )

IF(WIN32 AND ENABLE_SHARED_LIBS AND TARGET IccProfLib2-static)
  # iccDumpProfile references exported data symbols that do not build
  # cleanly through dllimport in the current public headers.
  SET( ICC_DUMP_LINK_LIB IccProfLib2-static )
ENDIF()

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${ICC_DUMP_LINK_LIB} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
