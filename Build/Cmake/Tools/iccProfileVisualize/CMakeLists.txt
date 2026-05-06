# 
# iccProfileVisualize.cpp CMakeLists.txt | iccDEV Project
# Copyright (©) 2026 The International Color Consortium. All rights reserved.
# 

set(TARGET_NAME iccProfileVisualize)
set(SRC_PATH ../../../.. )
set(SRC_ROOT ${SRC_PATH}/Tools/CmdLine/iccProfileVisualize )
set(SOURCES
	"${SRC_ROOT}/iccProfileVisualize.cpp"
	"${SRC_ROOT}/MiniTIFF.cpp"
	"${SRC_ROOT}/MiniSVG.cpp"
)

add_executable(${TARGET_NAME} ${SOURCES})

target_link_libraries(${TARGET_NAME}
  PRIVATE
    IccProfLib2-static
    $<$<BOOL:${ENABLE_ICCXML}>:IccXML2-static>
)

target_include_directories(${TARGET_NAME}
  PRIVATE
    ${PROJECT_SOURCE_DIR}/IccProfLib
    $<$<BOOL:${ENABLE_ICCXML}>:${PROJECT_SOURCE_DIR}/IccXML/IccLibXML>
)

install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

# Emscripten WASM support
if(EMSCRIPTEN)
  set_target_properties(${TARGET_NAME} PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "-sMODULARIZE=1 -sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain,FS -sALLOW_MEMORY_GROWTH=1"
  )
endif()
