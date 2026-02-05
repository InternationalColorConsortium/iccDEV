#################################################################################
# IccXML Cmake Project Configuration| iccMAX Project
# Copyright (©) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 04-FEB-2026 at 0430Z by David Hoyt
#
#  Changes: Modify Version to contain Commit Hash
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

IF(ENABLE_INSTALL_RIM)
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

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IccXML/IccLibXML/IccLibXMLVer.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IccXML/IccLibXML/IccLibXMLVer.h"
    @ONLY
)

# message(STATUS "Starting CMake debug for target: ${TARGET_NAME}")
# message(STATUS "CFILES = ${CFILES}")
# message(STATUS "ENABLE_SHARED_LIBS = ${ENABLE_SHARED_LIBS}")
# message(STATUS "ENABLE_STATIC_LIBS = ${ENABLE_STATIC_LIBS}")
# message(STATUS "ENABLE_INSTALL_RIM = ${ENABLE_INSTALL_RIM}")
# message(STATUS "LIBXML2_LIBRARIES = ${LIBXML2_LIBRARIES}")
# message(STATUS "PROJECT_UP_NAME = ${PROJECT_UP_NAME}")

SET(SOURCES ${CFILES})

IF(APPLE)
  message(STATUS "Configuring for Apple platform")
  INCLUDE_DIRECTORIES(/Developer/Headers/FlatCarbon)
  FIND_LIBRARY(CARBON_LIBRARY Carbon)
  FIND_LIBRARY(IOKIT_LIBRARY IOKit)
  MARK_AS_ADVANCED(CARBON_LIBRARY)
  MARK_AS_ADVANCED(IOKIT_LIBRARY)
  SET(EXTRA_LIBS_CS ${CARBON_LIBRARY} ${IOKIT_LIBRARY})
  message(STATUS "Apple libs: CARBON = ${CARBON_LIBRARY}, IOKIT = ${IOKIT_LIBRARY}")
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

  TARGET_LINK_LIBRARIES(${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
#    message(STATUS "Installing SHARED library")
    INSTALL(TARGETS ${TARGET_NAME}
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

  TARGET_LINK_LIBRARIES(${TARGET_NAME}-static ${TARGET_LIB_ICCPROFLIB} ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
#    message(STATUS "Installing STATIC library")
    INSTALL(TARGETS ${TARGET_NAME}-static
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
ENDIF()

# Resolve linking target for parent scope
IF(ENABLE_SHARED_LIBS)
  SET(TARGET_LIB ${TARGET_NAME})
ELSE()
  SET(TARGET_LIB ${TARGET_NAME}-static)
ENDIF()

SET(TARGET_LIB ${TARGET_LIB} PARENT_SCOPE)
# message(STATUS "Final TARGET_LIB = ${TARGET_LIB}")

# Debug logging for output paths
GET_TARGET_PROPERTY(SHARED_OUTPUT_NAME ${TARGET_NAME} OUTPUT_NAME)
GET_TARGET_PROPERTY(SHARED_OUTPUT_DIR ${TARGET_NAME} LIBRARY_OUTPUT_DIRECTORY)
GET_TARGET_PROPERTY(SHARED_ARCHIVE_DIR ${TARGET_NAME} ARCHIVE_OUTPUT_DIRECTORY)
# message(STATUS "SHARED lib output name: ${SHARED_OUTPUT_NAME}")
# message(STATUS "SHARED lib output dir: ${SHARED_OUTPUT_DIR}")
# message(STATUS "SHARED archive dir: ${SHARED_ARCHIVE_DIR}")
