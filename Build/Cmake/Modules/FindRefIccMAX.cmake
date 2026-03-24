###############################################################
#
# Copyright (©) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# FindRefIccMAX.cmake — Find module for RefIccMAX / iccDEV libraries
#
# Copy this File to CMAKE_MODULE_PATH and call:
#
#   find_package(RefIccMAX REQUIRED)
#   target_link_libraries(myapp PRIVATE RefIccMAX::IccProfLib2)
#
#   1. Try CONFIG mode first (uses installed RefIccMAXConfig.cmake)
#   2. Fall back to manual header/library search
#
# Imported targets created on success:
#   RefIccMAX::IccProfLib2  — Core ICC profile library
#   RefIccMAX::IccXML2      — XML serialization (links IccProfLib2 transitively)
#
# Legacy variables set on success:
#   REFICCMAX_FOUND         — TRUE
#   REFICCMAX_INCLUDE_DIRS  — Header search paths
#   REFICCMAX_LIBRARIES     — Libraries to link
#   REFICCMAX_VERSION       — Version string
#
###############################################################

# ----- Phase 1: Try CONFIG mode (preferred) -----
find_package(RefIccMAX CONFIG QUIET)

if(RefIccMAX_FOUND)
  # CONFIG already created targets and set variables — nothing more to do
  return()
endif()

# ----- Phase 2: Manual search fallback -----

# Allow user hints via REFICCMAX_ROOT or RefIccMAX_DIR
set(_hints
  ${REFICCMAX_ROOT}
  ${RefIccMAX_DIR}
  $ENV{REFICCMAX_ROOT}
  $ENV{RefIccMAX_DIR}
)

# Find IccProfLib2 header
find_path(REFICCMAX_ICCPROFLIB_INCLUDE_DIR
  NAMES IccProfile.h
  HINTS ${_hints}
  PATH_SUFFIXES
    include/RefIccMAX/IccProfLib2
    include/IccProfLib2
    include/iccDEV/IccProfLib2
    IccProfLib
)

# Find IccXML2 header
find_path(REFICCMAX_ICCXML_INCLUDE_DIR
  NAMES IccProfileXml.h
  HINTS ${_hints}
  PATH_SUFFIXES
    include/RefIccMAX/IccXML2
    include/IccXML2
    include/iccDEV/IccXML2
    IccXML/IccLibXML
)

# Find shared libraries
find_library(REFICCMAX_ICCPROFLIB_LIBRARY
  NAMES IccProfLib2
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

find_library(REFICCMAX_ICCXML_LIBRARY
  NAMES IccXML2
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

# Find static libraries
find_library(REFICCMAX_ICCPROFLIB_STATIC_LIBRARY
  NAMES IccProfLib2-static
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

find_library(REFICCMAX_ICCXML_STATIC_LIBRARY
  NAMES IccXML2-static
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

# Try to extract version from IccProfLibVer.h
if(REFICCMAX_ICCPROFLIB_INCLUDE_DIR)
  set(_ver_file "${REFICCMAX_ICCPROFLIB_INCLUDE_DIR}/IccProfLibVer.h")
  if(EXISTS "${_ver_file}")
    file(STRINGS "${_ver_file}" _ver_line REGEX "^#define[ \t]+ICCPROFLIBVER_VERSION_STRING")
    if(_ver_line)
      string(REGEX REPLACE ".*\"([0-9]+\\.[0-9]+\\.[0-9]+[.0-9]*)\".*" "\\1" REFICCMAX_VERSION "${_ver_line}")
    endif()
  endif()
endif()

# Standard find_package handling
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RefIccMAX
  REQUIRED_VARS REFICCMAX_ICCPROFLIB_INCLUDE_DIR REFICCMAX_ICCPROFLIB_LIBRARY
  VERSION_VAR REFICCMAX_VERSION
)

if(RefIccMAX_FOUND)
  set(REFICCMAX_FOUND TRUE)
  set(REFICCMAX_INCLUDE_DIRS
    ${REFICCMAX_ICCPROFLIB_INCLUDE_DIR}
    ${REFICCMAX_ICCXML_INCLUDE_DIR}
  )

  # IccXML2 needs libxml2 for transitive linking
  find_package(LibXml2 QUIET)

  # ---- Create IMPORTED targets ----

  # IccProfLib2 (shared)
  if(REFICCMAX_ICCPROFLIB_LIBRARY AND NOT TARGET RefIccMAX::IccProfLib2)
    add_library(RefIccMAX::IccProfLib2 UNKNOWN IMPORTED)
    set_target_properties(RefIccMAX::IccProfLib2 PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCPROFLIB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${REFICCMAX_ICCPROFLIB_INCLUDE_DIR}"
      INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
  endif()

  # IccProfLib2-static
  if(REFICCMAX_ICCPROFLIB_STATIC_LIBRARY AND NOT TARGET RefIccMAX::IccProfLib2-static)
    add_library(RefIccMAX::IccProfLib2-static STATIC IMPORTED)
    set_target_properties(RefIccMAX::IccProfLib2-static PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCPROFLIB_STATIC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${REFICCMAX_ICCPROFLIB_INCLUDE_DIR}"
      INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
  endif()

  # IccXML2 (shared)
  if(REFICCMAX_ICCXML_LIBRARY AND NOT TARGET RefIccMAX::IccXML2)
    add_library(RefIccMAX::IccXML2 UNKNOWN IMPORTED)
    set(_xml_deps "")
    if(TARGET RefIccMAX::IccProfLib2)
      list(APPEND _xml_deps RefIccMAX::IccProfLib2)
    endif()
    if(LIBXML2_LIBRARIES)
      list(APPEND _xml_deps ${LIBXML2_LIBRARIES})
    endif()
    set_target_properties(RefIccMAX::IccXML2 PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCXML_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${REFICCMAX_ICCXML_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${_xml_deps}"
      INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
  endif()

  # IccXML2-static
  if(REFICCMAX_ICCXML_STATIC_LIBRARY AND NOT TARGET RefIccMAX::IccXML2-static)
    add_library(RefIccMAX::IccXML2-static STATIC IMPORTED)
    set(_xml_static_deps "")
    if(TARGET RefIccMAX::IccProfLib2-static)
      list(APPEND _xml_static_deps RefIccMAX::IccProfLib2-static)
    elseif(TARGET RefIccMAX::IccProfLib2)
      list(APPEND _xml_static_deps RefIccMAX::IccProfLib2)
    endif()
    if(LIBXML2_LIBRARIES)
      list(APPEND _xml_static_deps ${LIBXML2_LIBRARIES})
    endif()
    set_target_properties(RefIccMAX::IccXML2-static PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCXML_STATIC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${REFICCMAX_ICCXML_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${_xml_static_deps}"
      INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
  endif()

  # Non-namespaced aliases for convenience
  if(TARGET RefIccMAX::IccProfLib2 AND NOT TARGET IccProfLib2)
    add_library(IccProfLib2 ALIAS RefIccMAX::IccProfLib2)
  endif()
  if(TARGET RefIccMAX::IccXML2 AND NOT TARGET IccXML2)
    add_library(IccXML2 ALIAS RefIccMAX::IccXML2)
  endif()

  # Legacy library list
  set(REFICCMAX_LIBRARIES "")
  if(TARGET RefIccMAX::IccProfLib2)
    list(APPEND REFICCMAX_LIBRARIES RefIccMAX::IccProfLib2)
  endif()
  if(TARGET RefIccMAX::IccXML2)
    list(APPEND REFICCMAX_LIBRARIES RefIccMAX::IccXML2)
  endif()

  mark_as_advanced(
    REFICCMAX_ICCPROFLIB_INCLUDE_DIR
    REFICCMAX_ICCXML_INCLUDE_DIR
    REFICCMAX_ICCPROFLIB_LIBRARY
    REFICCMAX_ICCXML_LIBRARY
    REFICCMAX_ICCPROFLIB_STATIC_LIBRARY
    REFICCMAX_ICCXML_STATIC_LIBRARY
  )
endif()
