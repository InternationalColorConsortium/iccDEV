#################################################################################
# IccJSON CMake Project Configuration | iccMAX Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Parallel implementation to IccXML using nlohmann/json (JSON for Modern C++).
#
#################################################################################

SET( TARGET_NAME IccJSON2 )

SET( SRC_PATH ../../.. )
SET( CFILES
        ${SRC_PATH}/IccJSON/IccLibJSON/IccIoJson.cpp
        ${SRC_PATH}/IccJSON/IccLibJSON/IccMpeJson.cpp
        ${SRC_PATH}/IccJSON/IccLibJSON/IccMpeJsonFactory.cpp
        ${SRC_PATH}/IccJSON/IccLibJSON/IccProfileJson.cpp
        ${SRC_PATH}/IccJSON/IccLibJSON/IccTagJson.cpp
        ${SRC_PATH}/IccJSON/IccLibJSON/IccTagJsonFactory.cpp
        ${SRC_PATH}/IccJSON/IccLibJSON/IccUtilJson.cpp
  )

IF(ENABLE_INSTALL_RIM)
  SET( HEADERS_PUBLIC
        ${SRC_PATH}/IccJSON/IccLibJSON/IccIoJson.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccJsonConfig.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccMpeJson.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccMpeJsonFactory.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccProfileJson.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccTagJson.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccTagJsonFactory.h
        ${SRC_PATH}/IccJSON/IccLibJSON/IccUtilJson.h
  )
  INSTALL( FILES
             ${HEADERS_PUBLIC}
             DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME} )
ENDIF(ENABLE_INSTALL_RIM)

# Generate version header with git commit hash (reuses the project version string)
if(GIT_COMMIT_HASH)
    set(ICCLIBJSON_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}+${GIT_COMMIT_HASH}")
    set(ICCPROFLIBJSON_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}+${GIT_COMMIT_HASH}")
else()
    set(ICCLIBJSON_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}")
    set(ICCPROFLIBJSON_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IccJSON/IccLibJSON/IccLibJSONVer.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/IccLibJSONVer.h"
    @ONLY
)

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

# IccJSON has no IccXML dependency -- all utilities are in IccProfLib

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

  target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccJSON/IccLibJSON>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  TARGET_LINK_LIBRARIES(${TARGET_NAME} PUBLIC ${TARGET_LIB_ICCPROFLIB} nlohmann_json::nlohmann_json ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  ENDIF()
ENDIF()

IF(ENABLE_STATIC_LIBS)
  ADD_LIBRARY(${TARGET_NAME}-static STATIC ${SOURCES})
  SET_TARGET_PROPERTIES(${TARGET_NAME}-static PROPERTIES OUTPUT_NAME "${TARGET_NAME}-static")

  # Static archives must contain native object code, not LLVM IR bitcode.
  # Disable LTO per-target as defense-in-depth.  (See GitHub issue #814.)
  set_target_properties(${TARGET_NAME}-static PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
  target_compile_features(${TARGET_NAME}-static PUBLIC cxx_std_17)

  target_include_directories(${TARGET_NAME}-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccJSON/IccLibJSON>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  TARGET_LINK_LIBRARIES(${TARGET_NAME}-static PUBLIC ${TARGET_LIB_ICCPROFLIB} nlohmann_json::nlohmann_json ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME}-static
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  ENDIF()

  IF(WIN32 AND ENABLE_SHARED_LIBS)
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
