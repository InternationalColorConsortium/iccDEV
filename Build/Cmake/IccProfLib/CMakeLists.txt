#################################################################################
# IccProfLib Cmake Project Configuration| iccMAX Project
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

SET( TARGET_NAME IccProfLib2)

SET( SRC_PATH ../../.. )
SET( CFILES
    ${SRC_PATH}/IccProfLib/IccApplyBPC.cpp
    ${SRC_PATH}/IccProfLib/IccArrayBasic.cpp
    ${SRC_PATH}/IccProfLib/IccArrayFactory.cpp
    ${SRC_PATH}/IccProfLib/IccCAM.cpp
    ${SRC_PATH}/IccProfLib/IccCmm.cpp
    ${SRC_PATH}/IccProfLib/IccConvertUTF.cpp
    ${SRC_PATH}/IccProfLib/IccEncoding.cpp
    ${SRC_PATH}/IccProfLib/IccEnvVar.cpp
    ${SRC_PATH}/IccProfLib/IccEval.cpp
    ${SRC_PATH}/IccProfLib/IccIO.cpp
    ${SRC_PATH}/IccProfLib/IccMatrixMath.cpp
    ${SRC_PATH}/IccProfLib/IccMpeACS.cpp
    ${SRC_PATH}/IccProfLib/IccMpeBasic.cpp
    ${SRC_PATH}/IccProfLib/IccMpeCalc.cpp
    ${SRC_PATH}/IccProfLib/IccMpeFactory.cpp
    ${SRC_PATH}/IccProfLib/IccMpeSpectral.cpp
    ${SRC_PATH}/IccProfLib/IccPrmg.cpp
    ${SRC_PATH}/IccProfLib/IccPcc.cpp
    ${SRC_PATH}/IccProfLib/IccProfile.cpp
    ${SRC_PATH}/IccProfLib/IccSolve.cpp
    ${SRC_PATH}/IccProfLib/IccSparseMatrix.cpp
    ${SRC_PATH}/IccProfLib/IccStructBasic.cpp
    ${SRC_PATH}/IccProfLib/IccStructFactory.cpp
    ${SRC_PATH}/IccProfLib/IccTagBasic.cpp
    ${SRC_PATH}/IccProfLib/IccTagComposite.cpp
    ${SRC_PATH}/IccProfLib/IccTagDict.cpp
    ${SRC_PATH}/IccProfLib/IccTagEmbedIcc.cpp
    ${SRC_PATH}/IccProfLib/IccTagFactory.cpp
    ${SRC_PATH}/IccProfLib/IccTagLut.cpp
    ${SRC_PATH}/IccProfLib/IccTagMPE.cpp
    ${SRC_PATH}/IccProfLib/IccTagProfSeqId.cpp
    ${SRC_PATH}/IccProfLib/IccUtil.cpp
    ${SRC_PATH}/IccProfLib/IccXformFactory.cpp
    ${SRC_PATH}/IccProfLib/IccMD5.cpp
    ${SRC_PATH}/IccProfLib/IccCmmSearch.cpp
   )

# Public headers — defined unconditionally for target_sources / install
SET( HEADERS_PUBLIC
  ${SRC_PATH}/IccProfLib/IccApplyBPC.h
  ${SRC_PATH}/IccProfLib/IccArrayBasic.h
  ${SRC_PATH}/IccProfLib/IccArrayFactory.h
  ${SRC_PATH}/IccProfLib/IccCAM.h
  ${SRC_PATH}/IccProfLib/IccCmm.h
  ${SRC_PATH}/IccProfLib/IccConvertUTF.h
  ${SRC_PATH}/IccProfLib/IccDefs.h
  ${SRC_PATH}/IccProfLib/IccEncoding.h
  ${SRC_PATH}/IccProfLib/IccEnvVar.h
  ${SRC_PATH}/IccProfLib/IccEval.h
  ${SRC_PATH}/IccProfLib/IccIO.h
  ${SRC_PATH}/IccProfLib/IccMatrixMath.h
  ${SRC_PATH}/IccProfLib/IccMpeACS.h
  ${SRC_PATH}/IccProfLib/IccMpeBasic.h
  ${SRC_PATH}/IccProfLib/IccMpeCalc.h
  ${SRC_PATH}/IccProfLib/IccMpeFactory.h
  ${SRC_PATH}/IccProfLib/IccMpeSpectral.h
  ${SRC_PATH}/IccProfLib/IccPcc.h
  ${SRC_PATH}/IccProfLib/IccPrmg.h
  ${SRC_PATH}/IccProfLib/IccProfile.h
  ${SRC_PATH}/IccProfLib/IccProfLibConf.h
  ${SRC_PATH}/IccProfLib/IccSolve.h
  ${SRC_PATH}/IccProfLib/IccSparseMatrix.h
  ${SRC_PATH}/IccProfLib/IccStructBasic.h
  ${SRC_PATH}/IccProfLib/IccStructFactory.h
  ${SRC_PATH}/IccProfLib/IccTagBasic.h
  ${SRC_PATH}/IccProfLib/IccTagComposite.h
  ${SRC_PATH}/IccProfLib/IccTagDict.h
  ${SRC_PATH}/IccProfLib/IccTagEmbedIcc.h
  ${SRC_PATH}/IccProfLib/IccTagFactory.h
  ${SRC_PATH}/IccProfLib/IccTag.h
  ${SRC_PATH}/IccProfLib/IccTagLut.h
  ${SRC_PATH}/IccProfLib/IccTagMPE.h
  ${SRC_PATH}/IccProfLib/IccTagProfSeqId.h
  ${SRC_PATH}/IccProfLib/IccUtil.h
  ${SRC_PATH}/IccProfLib/IccWrapper.h
  ${SRC_PATH}/IccProfLib/IccXformFactory.h
  ${SRC_PATH}/IccProfLib/IccMD5.h
  ${SRC_PATH}/IccProfLib/IccSignatureUtils.h
  ${SRC_PATH}/IccProfLib/IccSearch.h
  ${SRC_PATH}/IccProfLib/IccCmmSearch.h
  ${SRC_PATH}/IccProfLib/icProfileHeader.h
)

IF(ENABLE_INSTALL_RIM)
  INSTALL( FILES
    ${HEADERS_PUBLIC}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
    COMPONENT shared-proof )
ENDIF(ENABLE_INSTALL_RIM)

# Generate version header with git commit hash
if(GIT_COMMIT_HASH)
    set(ICCPROFLIB_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}+${GIT_COMMIT_HASH}")
else()
    set(ICCPROFLIB_VERSION_STRING "${${PROJECT_UP_NAME}_VERSION}")
endif()

# Generate version header into build directory (not source tree)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IccProfLib/IccProfLibVer.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/IccProfLibVer.h"
    @ONLY
)

# Issue #823: install the generated header (with VERSION+GIT_COMMIT_HASH) so
# downstream find_package(RefIccMAX) consumers see commit info, not just version.
IF(ENABLE_INSTALL_RIM)
  INSTALL( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/IccProfLibVer.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
    COMPONENT shared-proof )
ENDIF(ENABLE_INSTALL_RIM)

# Generated header dir — propagated via target_include_directories(PUBLIC) below

SET(SOURCES ${CFILES})

IF(APPLE)
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

  # Shared library symbol visibility:
  # - On Linux/macOS: ICCPROFLIBDLL_EXPORTS activates visibility("default")
  #   on classes/functions marked ICCPROFLIB_API (see IccProfLibConf.h)
  # - On Windows: WINDOWS_EXPORT_ALL_SYMBOLS handles exports; do NOT define
  #   ICCPROFLIBDLL_EXPORTS (several headers have __declspec placement issues)
  if(NOT WIN32)
    target_compile_definitions(${TARGET_NAME} PRIVATE ICCPROFLIBDLL_EXPORTS)
  endif()

  if(WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()

  # Enforce C++17 standard per-target
  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

  # PUBLIC include dirs — propagate to consumers via target_link_libraries()
  # Build dir FIRST so generated IccProfLibVer.h (with git hash) wins over source tree copy
  target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccProfLib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  TARGET_LINK_LIBRARIES(${TARGET_NAME} PUBLIC ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
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

  # Static archives must contain native object code, not LLVM IR bitcode,
  # so consumers can link with any toolchain.  Disable LTO per-target as
  # defense-in-depth regardless of the global CMAKE_INTERPROCEDURAL_OPTIMIZATION
  # setting.  (See GitHub issue #814.)
  set_target_properties(${TARGET_NAME}-static PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

  # Enforce C++17 standard per-target
  target_compile_features(${TARGET_NAME}-static PUBLIC cxx_std_17)

  # PUBLIC include dirs — build dir FIRST for generated version header
  target_include_directories(${TARGET_NAME}-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../IccProfLib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  TARGET_LINK_LIBRARIES(${TARGET_NAME}-static PUBLIC ${EXTRA_LIBS} ${EXTRA_LIBS_CS})

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME}-static
            EXPORT  RefIccMAXTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT shared-proof)
  ENDIF()

  # Create ALIAS target when no shared library (STATIC_ONLY configuration)
  # This ensures ${TARGET_NAME} always exists for target_link_libraries
  IF(NOT ENABLE_SHARED_LIBS)
    ADD_LIBRARY(${TARGET_NAME} ALIAS ${TARGET_NAME}-static)
  ENDIF()

ENDIF()

# Target selection logic for parent use
IF(ENABLE_SHARED_LIBS)
  SET(TARGET_LIB ${TARGET_NAME})
ELSE()
  SET(TARGET_LIB ${TARGET_NAME}-static)
ENDIF()

SET(TARGET_LIB ${TARGET_LIB} PARENT_SCOPE)
