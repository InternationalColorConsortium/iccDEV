SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/Tools/CmdLine/IccSpecSepToTiff/iccSpecSepToTiff.cpp
              ${SRC_PATH}/Tools/CmdLine/IccApplyProfiles/TiffImg.cpp )
SET( TARGET_NAME iccSpecSepToTiff )
INCLUDE_DIRECTORIES ( ${SRC_PATH}/Tools/CmdLine/IccApplyProfiles/ )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

target_link_libraries( ${TARGET_NAME}
  PRIVATE
    ${TARGET_LIB_ICCPROFLIB}
    ${_TIFF_LIB}
)

IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
