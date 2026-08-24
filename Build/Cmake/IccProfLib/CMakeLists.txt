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
    ${SRC_PATH}/IccProfLib/IccCmmThread.cpp
    ${SRC_PATH}/IccProfLib/IccColorimetry.cpp
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
    ${SRC_PATH}/IccProfLib/IccObject.cpp
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
    ${SRC_PATH}/IccProfLib/IccWrapper.cpp
   )

function(iccdev_check_cxx_compiler_flag FLAG RESULT_VAR)
  include(CheckCXXCompilerFlag)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  check_cxx_compiler_flag("${FLAG}" "${RESULT_VAR}")
endfunction()

set(ICCDEV_AVX2_SOURCE "${SRC_PATH}/IccProfLib/IccTagLutAvx2.cpp")
set(ICCDEV_AVX2_ENABLED OFF)
if(ICCDEV_ENABLE_AVX2)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _iccdev_processor)
  if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS
      "[SKIP] ICCDEV_ENABLE_AVX2 is disabled for native MSVC because measured CLUT throughput is slower than the SSE2 fallback; use ClangCL")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8
      AND _iccdev_processor MATCHES "^(amd64|x86_64|x64)$")
    if(MSVC)
      set(_iccdev_avx2_flag "/arch:AVX2")
    else()
      set(_iccdev_avx2_flag "-mavx2")
    endif()
    iccdev_check_cxx_compiler_flag(
      "${_iccdev_avx2_flag}" ICCDEV_COMPILER_SUPPORTS_AVX2)
    if(ICCDEV_COMPILER_SUPPORTS_AVX2)
      list(APPEND CFILES "${ICCDEV_AVX2_SOURCE}")
      set_source_files_properties("${ICCDEV_AVX2_SOURCE}"
        PROPERTIES COMPILE_OPTIONS "${_iccdev_avx2_flag}")
      set(ICCDEV_AVX2_ENABLED ON)
      message(STATUS "AVX2 CLUT interpolation enabled with ${_iccdev_avx2_flag}")
    else()
      message(WARNING
        "ICCDEV_ENABLE_AVX2 was requested, but ${CMAKE_CXX_COMPILER} does not support ${_iccdev_avx2_flag}")
    endif()
  else()
    message(WARNING
      "ICCDEV_ENABLE_AVX2 was requested for unsupported target architecture '${CMAKE_SYSTEM_PROCESSOR}'")
  endif()
endif()
set(ICCDEV_AVX2_EFFECTIVE "${ICCDEV_AVX2_ENABLED}" CACHE INTERNAL
  "Whether the AVX2 CLUT implementation is compiled" FORCE)

set(ICCDEV_AVX512_SOURCE "${SRC_PATH}/IccProfLib/IccTagLutAvx512.cpp")
set(ICCDEV_AVX512_ENABLED OFF)
if(ICCDEV_ENABLE_AVX512)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _iccdev_processor)
  if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS
      "[SKIP] ICCDEV_ENABLE_AVX512 is disabled for native MSVC; use ClangCL")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8
      AND _iccdev_processor MATCHES "^(amd64|x86_64|x64)$")
    if(MSVC)
      set(_iccdev_avx512_flag "/arch:AVX512")
    else()
      set(_iccdev_avx512_flag "-mavx512f")
    endif()
    iccdev_check_cxx_compiler_flag(
      "${_iccdev_avx512_flag}" ICCDEV_COMPILER_SUPPORTS_AVX512)
    if(ICCDEV_COMPILER_SUPPORTS_AVX512)
      list(APPEND CFILES "${ICCDEV_AVX512_SOURCE}")
      set_source_files_properties("${ICCDEV_AVX512_SOURCE}"
        PROPERTIES COMPILE_OPTIONS "${_iccdev_avx512_flag}")
      set(ICCDEV_AVX512_ENABLED ON)
      message(STATUS "AVX-512 CLUT interpolation enabled with ${_iccdev_avx512_flag}")
    else()
      message(WARNING
        "ICCDEV_ENABLE_AVX512 was requested, but ${CMAKE_CXX_COMPILER} does not support ${_iccdev_avx512_flag}")
    endif()
  else()
    message(WARNING
      "ICCDEV_ENABLE_AVX512 was requested for unsupported target architecture '${CMAKE_SYSTEM_PROCESSOR}'")
  endif()
endif()
set(ICCDEV_AVX512_EFFECTIVE "${ICCDEV_AVX512_ENABLED}" CACHE INTERNAL
  "Whether the AVX-512 CLUT implementation is compiled" FORCE)

# Public headers - defined unconditionally for target_sources / install
SET( HEADERS_PUBLIC
  ${SRC_PATH}/IccProfLib/IccApplyBPC.h
  ${SRC_PATH}/IccProfLib/IccArrayBasic.h
  ${SRC_PATH}/IccProfLib/IccArrayFactory.h
  ${SRC_PATH}/IccProfLib/IccCAM.h
  ${SRC_PATH}/IccProfLib/IccCValidation.h
  ${SRC_PATH}/IccProfLib/IccFileUtil.h
  ${SRC_PATH}/IccProfLib/IccCmm.h
  ${SRC_PATH}/IccProfLib/IccCmmThread.h
  ${SRC_PATH}/IccProfLib/IccColorimetry.h
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
  ${SRC_PATH}/IccProfLib/IccObject.h
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
  ${SRC_PATH}/Tools/CmdLine/IccCmdLineUtil.h
)

IF(ENABLE_INSTALL_RIM)
  INSTALL( FILES
    ${HEADERS_PUBLIC}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_INCLUDE_FOLDER}/${TARGET_NAME}
    COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT} )
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
    COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT} )
ENDIF(ENABLE_INSTALL_RIM)

# Generated header dir - propagated via target_include_directories(PUBLIC) below

SET(SOURCES ${CFILES})

IF(APPLE)
  FIND_LIBRARY(CARBON_LIBRARY Carbon)
  FIND_LIBRARY(IOKIT_LIBRARY IOKit)
  MARK_AS_ADVANCED(CARBON_LIBRARY)
  MARK_AS_ADVANCED(IOKIT_LIBRARY)
  SET(EXTRA_LIBS_CS ${CARBON_LIBRARY} ${IOKIT_LIBRARY})
ENDIF()

# IccCmmThread.cpp uses pthread_create. On Linux/aarch64 with Clang the
# pthread library must be linked explicitly; GCC typically pulls it in
# transitively but relying on that is fragile and trips up Clang-based builds.
find_package(Threads REQUIRED)
list(APPEND EXTRA_LIBS Threads::Threads)
if(ICC_USE_ZLIB)
  find_package(ZLIB REQUIRED)
  list(APPEND EXTRA_LIBS ZLIB::ZLIB)
endif()

IF(ENABLE_SHARED_LIBS)
  ADD_LIBRARY(${TARGET_NAME} SHARED ${SOURCES})

  set_target_properties(${TARGET_NAME}
    PROPERTIES
      VERSION   "${${PROJECT_UP_NAME}_VERSION}"
      SOVERSION "${${PROJECT_UP_NAME}_MAJOR_VERSION}"
  )

  # Shared library symbol visibility:
  # - On MSVC: WINDOWS_EXPORT_ALL_SYMBOLS handles exports automatically.
  # - On GCC/Clang: ICCPROFLIBDLL_EXPORTS activates visibility("default")
  #   on classes/functions marked ICCPROFLIB_API (see IccProfLibConf.h).
  # - Do not add -fvisibility=hidden here. IccProfLib export annotations are
  #   still incomplete, so shared builds intentionally keep default visibility.
  if(NOT MSVC)
    target_compile_definitions(${TARGET_NAME} PRIVATE ICCPROFLIBDLL_EXPORTS)
  endif()
  target_compile_definitions(${TARGET_NAME}
    PRIVATE ICC_C_API_EXPORTS
    INTERFACE ICC_C_API_IMPORTS)

  # Exported global DATA (#2154, from #1888). WINDOWS_EXPORT_ALL_SYMBOLS below
  # auto-exports functions but not data, so the eight ICCPROFLIB_DATA_API globals
  # need real dllexport here and real dllimport in consumers. Kept separate from
  # ICCPROFLIBDLL_EXPORTS so this does not flip the ~308 incomplete
  # ICCPROFLIB_API annotations and change how every class and function exports.
  #
  # PRIVATE for the export side; INTERFACE for the import side so anything
  # linking THIS shared target -- in-tree tools and find_package() consumers
  # alike -- sees __declspec(dllimport). The static target deliberately gets
  # neither, since a static consumer must keep a plain extern.
  target_compile_definitions(${TARGET_NAME}
    PRIVATE   ICCPROFLIBDLL_DATA_EXPORTS
    INTERFACE ICCPROFLIBDLL_DATA_IMPORTS)
  if(ICC_USE_ZLIB)
    target_compile_definitions(${TARGET_NAME} PUBLIC ICC_USE_ZLIB=1)
  endif()

  if(MSVC)
    set_target_properties(${TARGET_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()

  # NOTE: -fvisibility=hidden is intentionally NOT used here.
  # IccProfLib has incomplete ICCPROFLIB_API annotations (~308 partial uses
  # across 43 headers), and IccXML has zero export annotations. Hidden
  # visibility causes undefined references when linking tools against the
  # shared libraries. Until complete export annotations are added, all
  # symbols remain at default visibility for shared builds.

  # Enforce C++17 standard per-target
  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)
  if(ICCDEV_AVX2_ENABLED)
    target_compile_definitions(${TARGET_NAME} PRIVATE ICC_USE_AVX2=1)
  endif()
  if(ICC_AVX2_CLUT_DEBUG)
    target_compile_definitions(${TARGET_NAME} PRIVATE ICC_AVX2_CLUT_DEBUG=1)
  endif()
  if(ICCDEV_ENABLE_PERF_MONITORING)
    target_compile_definitions(${TARGET_NAME} PRIVATE ICC_PERF_MONITORING=1)
  endif()
  if(ICCDEV_AVX512_ENABLED)
    target_compile_definitions(${TARGET_NAME} PRIVATE ICC_USE_AVX512=1)
  endif()

  # PUBLIC include dirs - propagate to consumers via target_link_libraries()
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
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_RUNTIME_COMPONENT}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT})
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
  if(ICCDEV_AVX2_ENABLED)
    target_compile_definitions(${TARGET_NAME}-static PRIVATE ICC_USE_AVX2=1)
  endif()
  if(ICC_AVX2_CLUT_DEBUG)
    target_compile_definitions(${TARGET_NAME}-static PRIVATE ICC_AVX2_CLUT_DEBUG=1)
  endif()
  if(ICCDEV_ENABLE_PERF_MONITORING)
    target_compile_definitions(${TARGET_NAME}-static PRIVATE ICC_PERF_MONITORING=1)
  endif()
  if(ICCDEV_AVX512_ENABLED)
    target_compile_definitions(${TARGET_NAME}-static PRIVATE ICC_USE_AVX512=1)
  endif()
  if(ICC_USE_ZLIB)
    target_compile_definitions(${TARGET_NAME}-static PUBLIC ICC_USE_ZLIB=1)
  endif()

  # PUBLIC include dirs - build dir FIRST for generated version header
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
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${ICCDEV_DEVELOPMENT_COMPONENT})
  ENDIF()

  # Create ALIAS target when no shared library (STATIC_ONLY configuration)
  # This ensures ${TARGET_NAME} always exists for target_link_libraries
  IF(NOT ENABLE_SHARED_LIBS)
    ADD_LIBRARY(${TARGET_NAME} ALIAS ${TARGET_NAME}-static)
  ENDIF()

ENDIF()

if(ICC_USE_ZLIB)
  if(ENABLE_SHARED_LIBS AND TARGET ${TARGET_NAME})
    target_compile_definitions(${TARGET_NAME} PUBLIC ICC_USE_ZLIB=1)
    target_link_libraries(${TARGET_NAME} PUBLIC ZLIB::ZLIB)
  endif()
  if(ENABLE_STATIC_LIBS AND TARGET ${TARGET_NAME}-static)
    target_compile_definitions(${TARGET_NAME}-static PUBLIC ICC_USE_ZLIB=1)
    target_link_libraries(${TARGET_NAME}-static PUBLIC ZLIB::ZLIB)
  endif()
endif()

# Target selection logic for parent use
IF(ENABLE_SHARED_LIBS)
  SET(TARGET_LIB ${TARGET_NAME})
ELSE()
  SET(TARGET_LIB ${TARGET_NAME}-static)
ENDIF()

SET(TARGET_LIB ${TARGET_LIB} PARENT_SCOPE)
