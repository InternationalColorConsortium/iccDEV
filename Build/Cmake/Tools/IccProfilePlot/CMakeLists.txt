#
# IccProfilePlot CMakeLists.txt | iccDEV Project
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
#
# iccProfilePlot exposes IccVizModel — the data-first profile visualization
# API. IccVizModel.cpp depends only on IccProfLib + spectralLocus.hpp (shared
# with IccProfileVisualize), no PDF/TIFF writers, so it is reusable as a small
# library component by other iccDEV consumers.
#

set(TARGET_NAME iccProfilePlot)
set(SRC_PATH ../../../.. )
set(SRC_ROOT ${SRC_PATH}/Tools/CmdLine/IccProfilePlot )
set(VIZ_ROOT ${SRC_PATH}/Tools/CmdLine/IccProfileVisualize )
set(SOURCES
	"${SRC_ROOT}/iccProfilePlot.cpp"
	"${SRC_ROOT}/IccVizModel.cpp"
)

add_executable(${TARGET_NAME} ${SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
  OUTPUT_NAME "iccProfilePlot"
)

target_link_libraries(${TARGET_NAME}
  PRIVATE
    ${TARGET_LIB_ICCPROFLIB}
)

target_include_directories(${TARGET_NAME}
  PRIVATE
    ${PROJECT_ROOT_DIR}/IccProfLib
    ${VIZ_ROOT}      # spectralLocus.hpp
)

install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

# Emscripten WASM support
if(EMSCRIPTEN)
  set_target_properties(${TARGET_NAME} PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "-sMODULARIZE=1 -sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain,FS -sALLOW_MEMORY_GROWTH=1"
  )
endif()
