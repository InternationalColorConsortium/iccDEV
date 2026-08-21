SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/Tools/CmdLine/IccDumpProfile/iccDumpProfile.cpp )
SET( TARGET_NAME iccDumpProfile )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

# Links ${TARGET_LIB_ICCPROFLIB} on every platform, including Windows shared
# builds.  Until #2219 this tool linked IccProfLib2-static there instead: it
# references the exported globals icMsgValidateWarning / icMsgValidateNonCompliant,
# and WINDOWS_EXPORT_ALL_SYMBOLS auto-exports functions but never data, so those
# references failed with LNK2019 against the DLL.  The eight exported globals now
# carry ICCPROFLIB_DATA_API dllexport/dllimport, so the fallback is gone (#2154).
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
