SET( SRC_PATH ../../../.. )
SET( SOURCES
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/iccProfilePlot.cpp
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/IccVizModel.cpp )
SET( TARGET_NAME iccProfilePlot )
SET( ICC_PLOT_LINK_LIB ${TARGET_LIB_ICCPROFLIB} )

IF(WIN32 AND ENABLE_SHARED_LIBS AND TARGET IccProfLib2-static)
  # Mirror iccDumpProfile: link the static lib on Windows shared builds, since
  # the tool references exported data symbols that do not dllimport cleanly.
  SET( ICC_PLOT_LINK_LIB IccProfLib2-static )
ENDIF()

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

# IccVizModel.hpp / IccVizMath.hpp / spectralLocus.hpp live alongside the sources.
TARGET_INCLUDE_DIRECTORIES( ${TARGET_NAME} PRIVATE
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot )

TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${ICC_PLOT_LINK_LIB} )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
