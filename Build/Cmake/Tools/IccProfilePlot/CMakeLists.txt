SET( SRC_PATH ../../../.. )
# IccVizModel.cpp and the IccProfilePlot include directory are not listed here:
# both arrive through the IccVizEngine INTERFACE target declared in the parent
# CMakeLists (#1975).  Only this tool's own sources belong in this list.
SET( SOURCES
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/iccProfilePlot.cpp )
SET( TARGET_NAME iccProfilePlot )
SET( ICC_PLOT_LINK_LIB ${TARGET_LIB_ICCPROFLIB} )

IF(WIN32 AND ENABLE_SHARED_LIBS AND TARGET IccProfLib2-static)
  # Mirror iccDumpProfile: link the static lib on Windows shared builds, since
  # the tool references exported data symbols that do not dllimport cleanly.
  SET( ICC_PLOT_LINK_LIB IccProfLib2-static )
ENDIF()

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )

# IccVizEngine carries IccVizModel.cpp plus the include directory that makes
# IccVizModel.hpp / IccVizMath.hpp / spectralLocus.hpp reachable.
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${ICC_PLOT_LINK_LIB} IccVizEngine )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)

# Second tool: a data-first PDF/TIFF report generator that drives off IccVizModel
# and draws with the dependency-free Mini{PDF,SVG,TIFF} writers. Output name is
# distinct from the standalone iccProfileVisualize tool to avoid an install clash.
SET( VIZ_SOURCES
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/iccProfileVisualize.cpp
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/MiniPDF.cpp
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/MiniSVG.cpp
  ${SRC_PATH}/Tools/CmdLine/IccProfilePlot/MiniTIFF.cpp )
SET( VIZ_TARGET_NAME iccProfileVisualizePlot )

ADD_EXECUTABLE( ${VIZ_TARGET_NAME} ${VIZ_SOURCES} )

# The Mini* writers stay listed above: they are this tool's rendering back end,
# not part of the engine, and no other consumer compiles them.
TARGET_LINK_LIBRARIES( ${VIZ_TARGET_NAME} ${ICC_PLOT_LINK_LIB} IccVizEngine )
IF(ENABLE_INSTALL_RIM)
  INSTALL (TARGETS ${VIZ_TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(ENABLE_INSTALL_RIM)
