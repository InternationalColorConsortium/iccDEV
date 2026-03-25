#################################################################################
# IccXML Cmake Project Configuration| iccMAX Project
# Copyright (c) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-03-24 20:49:38 UTC by David Hoyt
#
# CMake Configuration:
# - PUBLIC target_include_directories for IccProfLib2 and IccXML2
#   (build-dir-first so generated version header with git hash wins)
# - EXPORT RefIccMAXTargets for build-tree and install-tree consumers
# - New FindRefIccMAX.cmake module for standalone find_package()
# - Modern RefIccMAXConfig.cmake.in using target-based imports
# - Tool CMakeLists: target_include_directories(PRIVATE) replaces global
#
#################################################################################

SET( TARGET_NAME IccXML2 )

SET( SRC_PATH ../../.. )
SET( CFILES
        ${SRC_PATH}/IccXML/IccLibXML/IccIoXml.cpp
        ${SRC_PATH}/IccXML/IccLibXML/IccMpeXml.cpp
        ${SRC_PATH}/IccXML/IccLibXML/IccMpeXmlFactory.cpp
        ${SRC_PATH}/IccXML/IccLibXML/IccProfileXml.cpp
        ${SRC_PATH}/IccXML/IccLibXML/IccTagXml.cpp
        ${SRC_PATH}/IccXML/IccLibXML/IccTagXmlFactory.cpp
        ${SRC_PATH}/IccXML/IccLibXML/IccUtilXml.cpp
  )

# Public headers — defined unconditionally for target properties
SET( HEADERS_PUBLIC
      ${SRC_PATH}/IccXML/IccLibXML/IccIoXml.h
      ${SRC_PATH}/IccXML/IccLibXML/IccLibXMLVer.h
      ${SRC_PATH}/IccXML/IccLibXML/IccMpeXmlFactory.h
      ${SRC_PATH}/IccXML/IccLibXML/IccMpeXml.h
      ${SRC_PATH}/IccXML/IccLibXML/IccProfileXml.h
      ${SRC_PATH}/IccXML/IccLibXML/IccTagXmlFactory.h
      ${SRC_PATH}/IccXML/IccLibXML/IccTagXml.h
      ${SRC_PATH}/IccXML/IccLibXML/IccUtilXml.h
      ${SRC_PATH}/IccXML/IccLibXML/IccXmlConfig.h
)

IF(ENABLE_INSTALL_RIM)
  INSTALL( FILES
             ${HEADERS_PUBLIC}
             DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME} )
ENDIF(ENABLE_INSTALL_RIM)

# Generate version header with git commit hash
if(GIT_COMMIT_HASH)
    set(ICCLIBXML_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}+${GIT_COMMIT_HASH}")
else()
    set(ICCLIBXML_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}")
endif()

# Generate version header into build directory (not source tree)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IccXML/IccLibXML/IccLibXMLVer.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/IccLibXMLVer.h"
    @ONLY
)

# Generated header dir — propagated via target_include_directories(PUBLIC) below

# message(STATUS "Starting CMake debug for target: ${TARGET_NAME}")
# message(STATUS "CFILES = ${CFILES}")
# message(STATUS "ENABLE_SHARED_LIBS = ${ENABLE_SHARED_LIBS}")
# message(STATUS "ENABLE_STATIC_LIBS = ${ENABLE_STATIC_LIBS}")
# message(STATUS "ENABLE_INSTALL_RIM = ${ENABLE_INSTALL_RIM}")
# message(STATUS "LIBXML2_LIBRARIES = ${LIBXML2_LIBRARIES}")
# message(STATUS "PROJECT_UP_NAME = ${PROJECT_UP_NAME}")

SET(SOURCES ${CFILES})

IF(APPLE)
  FIND_LIBRARY(CARBON_LIBRARY Carbon)
  FIND_LIBRARY(IOKIT_LIBRARY IOKit)
  MARK_AS_ADVANCED(CARBON_LIBRARY)
  MARK_AS_ADVANCED(IOKIT_LIBRARY)
  SET(EXTRA_LIBS_CS ${CARBON_LIBRARY} ${IOKIT_LIBRARY})
ENDIF()

SET(EXTRA_LIBS ${EXTRA_LIBS} ${LIBXML2_LIBRARIES})
# message(STATUS "EXTRA_LIBS = ${EXTRA_LIBS}")

IF(ENABLE_SHARED_LIBS)
#  message(STATUS "Building SHARED library: ${TARGET_NAME}")
  ADD_LIBRARY(${TARGET_NAME} SHARED ${SOURCES})
set_target_properties(${TARGET_NAME}
    PROPERTIES
        VERSION   "${${PROJECT_UP_NAME}_VERSION}"
        SOVERSION "${${PROJECT_UP_NAME}_MAJOR_VERSION}"
)

  # Enforce C++17 standard per-target
  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

  # PUBLIC include dirs — propagate to consumers via target_link_libraries()
  # PUBLIC include dirs — build dir FIRST for generated version header
  target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccXML/IccLibXML>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
  )

  # PUBLIC linkage — consumers transitively get IccProfLib2 headers + libxml2
  TARGET_LINK_LIBRARIES(${TARGET_NAME} PUBLIC ${TARGET_LIB_ICCPROFLIB} ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
#    message(STATUS "Installing SHARED library")
    INSTALL(TARGETS ${TARGET_NAME}
            EXPORT  RefIccMAXTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  ENDIF()
ENDIF()

IF(ENABLE_STATIC_LIBS)
#  message(STATUS "Building STATIC library: ${TARGET_NAME}-static")
  ADD_LIBRARY(${TARGET_NAME}-static STATIC ${SOURCES})
  SET_TARGET_PROPERTIES(${TARGET_NAME}-static PROPERTIES OUTPUT_NAME "${TARGET_NAME}-static")

  # Enforce C++17 standard per-target
  target_compile_features(${TARGET_NAME}-static PUBLIC cxx_std_17)

  # PUBLIC include dirs — same as shared target
  # PUBLIC include dirs — build dir FIRST for generated version header
  target_include_directories(${TARGET_NAME}-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccXML/IccLibXML>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
  )

  # PUBLIC linkage — consumers transitively get IccProfLib2 headers + libxml2
  TARGET_LINK_LIBRARIES(${TARGET_NAME}-static PUBLIC ${TARGET_LIB_ICCPROFLIB} ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
#    message(STATUS "Installing STATIC library")
    INSTALL(TARGETS ${TARGET_NAME}-static
            EXPORT  RefIccMAXTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  ENDIF()

  # Provide alias for MSVC .lib linking fallback
  IF(WIN32 AND ENABLE_SHARED_LIBS)
    ADD_CUSTOM_COMMAND(TARGET ${TARGET_NAME}-static POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${TARGET_NAME}-static>"
        "$<TARGET_FILE_DIR:${TARGET_NAME}-static>/${TARGET_NAME}.lib"
      COMMENT "Aliasing ${TARGET_NAME}-static.lib to ${TARGET_NAME}.lib for MSVC compatibility")
  ENDIF()
  # If no shared library, create alias so IccXML2 always exists
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
