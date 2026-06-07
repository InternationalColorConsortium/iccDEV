#
# IccProfilePlot CMakeLists.txt | iccDEV Project
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
#
# iccProfilePlot is a thin CLI over IccVizModel — the data-first profile
# visualization API, now built as the shared IccVizModel static library and
# consumed by both this tool (JSON output) and iccProfileVisualize (PDF/TIFF).
#

set(TARGET_NAME iccProfilePlot)
set(SRC_PATH ../../../.. )
set(SRC_ROOT ${SRC_PATH}/Tools/CmdLine/IccProfilePlot )
set(SOURCES
	"${SRC_ROOT}/iccProfilePlot.cpp"
)

add_executable(${TARGET_NAME} ${SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
  OUTPUT_NAME "iccProfilePlot"
)

# IccVizModel (PUBLIC) carries the IccVizModel.hpp/spectralLocus.hpp include
# directory and links IccProfLib transitively.
target_link_libraries(${TARGET_NAME}
  PRIVATE
    ${TARGET_LIB_ICCVIZMODEL}
    ${TARGET_LIB_ICCPROFLIB}
)

target_include_directories(${TARGET_NAME}
  PRIVATE
    ${PROJECT_ROOT_DIR}/IccProfLib
)

install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

# Emscripten WASM support
if(EMSCRIPTEN)
  set_target_properties(${TARGET_NAME} PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "-sMODULARIZE=1 -sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain,FS -sALLOW_MEMORY_GROWTH=1"
  )
endif()
