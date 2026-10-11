#################################################################################
# iccHeifDump CMake Project Configuration | iccDEV Project
#################################################################################

project(iccHeifDump)

set(SRC_PATH ../../../..)
set(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccHeifDump/iccHeifDump.cpp
)
set(TARGET_NAME iccHeifDump)

if(WIN32)
    find_package(libheif CONFIG QUIET)
    if(TARGET libheif::libheif)
        set(ICCDEV_HEIF_TARGET libheif::libheif)
    elseif(TARGET heif)
        set(ICCDEV_HEIF_TARGET heif)
    else()
        message(STATUS "libheif not found - skipping iccHeifDump.")
        return()
    endif()
else()
    find_package(PkgConfig QUIET)
    if(NOT PkgConfig_FOUND)
        message(STATUS "pkg-config not found - skipping iccHeifDump.")
        return()
    endif()
    pkg_check_modules(HEIF QUIET IMPORTED_TARGET libheif)
    if(NOT HEIF_FOUND)
        message(STATUS "libheif not found - skipping iccHeifDump.")
        return()
    endif()
    set(ICCDEV_HEIF_TARGET PkgConfig::HEIF)
endif()

add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE
    ${SRC_PATH}/Tools/CmdLine/IccHeifDump/
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${TARGET_LIB_ICCPROFLIB}
    ${ICCDEV_HEIF_TARGET}
)

if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
