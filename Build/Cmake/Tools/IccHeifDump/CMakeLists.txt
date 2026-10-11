#################################################################################
# iccHeifDump CMake Project Configuration | iccDEV Project
#################################################################################

project(iccHeifDump)

set(SRC_PATH ../../../..)
set(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccHeifDump/iccHeifDump.cpp
)
set(TARGET_NAME iccHeifDump)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HEIF REQUIRED IMPORTED_TARGET libheif)

add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE
    ${SRC_PATH}/Tools/CmdLine/IccHeifDump/
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${TARGET_LIB_ICCPROFLIB}
    PkgConfig::HEIF
)

if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
