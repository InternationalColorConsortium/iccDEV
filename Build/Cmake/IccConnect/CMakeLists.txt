#################################################################################
# IccConnect CMake Project Configuration | iccMAX Project
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Library providing JSON-configurable CMM transform construction.
#  Absorbs IccCmmConfig and IccJsonUtil from Tools/CmdLine/IccCommon.
#
#################################################################################

SET( TARGET_NAME IccConnect2 )

SET( SRC_PATH ../../.. )
SET( CFILES
        ${SRC_PATH}/IccConnect/IccLibConnect/IccJsonUtil.cpp
        ${SRC_PATH}/IccConnect/IccLibConnect/IccCmmConfig.cpp
        ${SRC_PATH}/IccConnect/IccLibConnect/IccConnect.cpp
  )

IF(ENABLE_INSTALL_RIM)
  SET( HEADERS_PUBLIC
        ${SRC_PATH}/IccConnect/IccLibConnect/IccJsonUtil.h
        ${SRC_PATH}/IccConnect/IccLibConnect/IccCmmConfig.h
        ${SRC_PATH}/IccConnect/IccLibConnect/IccConnect.h
  )
  INSTALL( FILES
             ${HEADERS_PUBLIC}
             DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
             COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT} )
ENDIF(ENABLE_INSTALL_RIM)

# Generate version header
if(GIT_COMMIT_HASH)
    set(ICCLIBCONNECT_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}+${GIT_COMMIT_HASH}")
else()
    set(ICCLIBCONNECT_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}")
endif()
set(ICCPROFLIBCONNECT_VERSION_STRING "${ICCLIBCONNECT_VERSION_STRING}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IccConnect/IccLibConnect/IccLibConnectVer.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/IccLibConnectVer.h"
    @ONLY
)

IF(ENABLE_INSTALL_RIM)
  INSTALL( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/IccLibConnectVer.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
    COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT} )
ENDIF(ENABLE_INSTALL_RIM)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

SET(SOURCES ${CFILES})

IF(APPLE)
  INCLUDE_DIRECTORIES(/Developer/Headers/FlatCarbon)
  FIND_LIBRARY(CARBON_LIBRARY Carbon)
  FIND_LIBRARY(IOKIT_LIBRARY IOKit)
  MARK_AS_ADVANCED(CARBON_LIBRARY)
  MARK_AS_ADVANCED(IOKIT_LIBRARY)
  SET(EXTRA_LIBS_CS ${CARBON_LIBRARY} ${IOKIT_LIBRARY})
ENDIF()

IF(ENABLE_SHARED_LIBS)
  ADD_LIBRARY(${TARGET_NAME} SHARED ${SOURCES})
  set_target_properties(${TARGET_NAME}
      PROPERTIES
          VERSION   "${${PROJECT_UP_NAME}_VERSION}"
          SOVERSION "${${PROJECT_UP_NAME}_MAJOR_VERSION}"
  )

  if(WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()

  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

  # Issue #1856. The JSON config reader in IccJsonUtil.cpp shares the bound
  # applied to profile JSON, so a -cfg document and a profile document are
  # accepted or refused at the same size rather than at 100 MiB and 64 MiB.
  target_compile_definitions(${TARGET_NAME} PRIVATE
    ICC_JSON_MAX_FILE_BYTES=${ICC_JSON_MAX_FILE_BYTES})

  target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccConnect/IccLibConnect>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  TARGET_LINK_LIBRARIES(${TARGET_NAME} PUBLIC ${TARGET_LIB_ICCPROFLIB} ${TARGET_LIB_ICCJSON} nlohmann_json::nlohmann_json ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME}
            EXPORT  RefIccMAXTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT})
  ENDIF()
ENDIF()

IF(ENABLE_STATIC_LIBS)
  ADD_LIBRARY(${TARGET_NAME}-static STATIC ${SOURCES})
  SET_TARGET_PROPERTIES(${TARGET_NAME}-static PROPERTIES OUTPUT_NAME "${TARGET_NAME}-static")

  # Static archives must contain native object code, not LLVM IR bitcode.
  # Disable LTO per-target as defense-in-depth.  (See GitHub issue #814.)
  set_target_properties(${TARGET_NAME}-static PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

  target_compile_features(${TARGET_NAME}-static PUBLIC cxx_std_17)

  # Keep the static archive on the same bound as the shared library above.
  target_compile_definitions(${TARGET_NAME}-static PRIVATE
    ICC_JSON_MAX_FILE_BYTES=${ICC_JSON_MAX_FILE_BYTES})

  target_include_directories(${TARGET_NAME}-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccConnect/IccLibConnect>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  TARGET_LINK_LIBRARIES(${TARGET_NAME}-static PUBLIC ${TARGET_LIB_ICCPROFLIB} ${TARGET_LIB_ICCJSON} nlohmann_json::nlohmann_json ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME}-static
            EXPORT  RefIccMAXTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT})
  ENDIF()

  # When the shared library is also built, MSVC already creates
  # IccConnect2.lib as the DLL import library. Do not overwrite it with the
  # static archive alias.
  IF(MSVC AND NOT ENABLE_SHARED_LIBS)
    ADD_CUSTOM_COMMAND(TARGET ${TARGET_NAME}-static POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${TARGET_NAME}-static>"
        "$<TARGET_FILE_DIR:${TARGET_NAME}-static>/${TARGET_NAME}.lib"
      COMMENT "Aliasing ${TARGET_NAME}-static.lib to ${TARGET_NAME}.lib for MSVC compatibility")
  ENDIF()

  IF(NOT ENABLE_SHARED_LIBS)
    ADD_LIBRARY(${TARGET_NAME} ALIAS ${TARGET_NAME}-static)
  ENDIF()
ENDIF()

# Resolve linking target for parent scope
IF(ENABLE_SHARED_LIBS)
  SET(TARGET_LIB ${TARGET_NAME})
ELSE()
  SET(TARGET_LIB ${TARGET_NAME}-static)
ENDIF()

SET(TARGET_LIB ${TARGET_LIB} PARENT_SCOPE)
