#
# IccVizModel CMakeLists.txt | iccDEV Project
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
#
# IccVizModel is the data-first profile visualization model: it turns an ICC
# profile's plottable content into data (graph point-series + axis hints, and
# ICC-normalized CLUT raster samples) rather than a finished picture.
#
# It is built as a small static library so it can be the SINGLE source of the
# visualization math shared by:
#   - iccProfilePlot      (emits the data as JSON)
#   - iccProfileVisualize (renders the data to PDF/TIFF)
#
# Depends only on IccProfLib + spectralLocus.hpp (no PDF/TIFF/SVG writers, no
# third-party libraries), so it stays reusable as a library component.
#

set(TARGET_NAME IccVizModel)
set(SRC_PATH ../../../.. )
set(SRC_ROOT ${SRC_PATH}/Tools/CmdLine/IccVizModel )
set(SOURCES
	"${SRC_ROOT}/IccVizModel.cpp"
)

add_library(${TARGET_NAME} STATIC ${SOURCES})

target_link_libraries(${TARGET_NAME}
  PUBLIC
    ${TARGET_LIB_ICCPROFLIB}
)

# Consumers that link this library inherit the include directory holding
# IccVizModel.hpp (and spectralLocus.hpp), so they need no extra wiring.
target_include_directories(${TARGET_NAME}
  PUBLIC
    ${SRC_ROOT}
  PRIVATE
    ${PROJECT_ROOT_DIR}/IccProfLib
)
