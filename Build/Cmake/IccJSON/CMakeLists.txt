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
             DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
             COMPONENT shared-proof )
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

# Issue #823: install the generated header (with VERSION+GIT_COMMIT_HASH) so
# downstream find_package(RefIccMAX) consumers see commit info, not just version.
IF(ENABLE_INSTALL_RIM)
  INSTALL( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/IccLibJSONVer.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
    COMPONENT shared-proof )
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
    # Issue #823: export to RefIccMAXTargets so find_package(RefIccMAX CONFIG)
    # consumers (e.g. examples/hello-iccdev) can resolve RefIccMAX::IccJSON2.
    INSTALL(TARGETS ${TARGET_NAME}
            EXPORT  RefIccMAXTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT shared-proof
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT shared-proof
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT shared-proof)
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
    # Issue #823: export to RefIccMAXTargets so find_package(RefIccMAX CONFIG)
    # consumers can resolve RefIccMAX::IccJSON2-static.
    INSTALL(TARGETS ${TARGET_NAME}-static
            EXPORT  RefIccMAXTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT shared-proof)
  ENDIF()

  # Issue #823: When ENABLE_SHARED_LIBS is also ON, the SHARED build already
  # produces ${TARGET_NAME}.lib as its import library in the same directory.
  # Aliasing the static archive to that name overwrites the import library,
  # which causes consumers (iccIisIsapi, hello-iccdev) to silently statically
  # link IccJSON2 and lose the IccJSON2.dll runtime dependency. Only create
  # the convenience alias when the shared import library is NOT being built.
  IF(WIN32 AND NOT ENABLE_SHARED_LIBS)
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
