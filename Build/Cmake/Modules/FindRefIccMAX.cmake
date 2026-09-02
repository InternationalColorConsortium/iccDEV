###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# FindRefIccMAX.cmake - Find module for RefIccMAX / iccDEV libraries
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
#   RefIccMAX::IccProfLib2  - Core ICC profile library
#   RefIccMAX::IccXML2      - XML serialization (links IccProfLib2 transitively)
#
# Legacy variables set on success:
#   REFICCMAX_FOUND         - TRUE
#   REFICCMAX_INCLUDE_DIRS  - Header search paths
#   REFICCMAX_LIBRARIES     - Libraries to link
#   REFICCMAX_VERSION       - Version string
#
###############################################################

# ----- Phase 1: Try CONFIG mode (preferred) -----
find_package(RefIccMAX CONFIG QUIET)

# Accept CONFIG mode only if it actually produced the imported target this
# module documents. RefIccMAX_FOUND on its own is not that proof: CMake sets it
# for any RefIccMAXConfig.cmake it loads, and every iccDEV before #712
# (2026-03-24) shipped a config that predates the target-based package -- it
# sets REFICCMAX_* variables and defines no RefIccMAX::* targets at all. A 2.3.1
# install of exactly that shape is what turned this up. One of those left on the search path
# (a stale system or package-manager prefix) wins Phase 1 over a correct newer
# install, and returning on the flag alone then hands the caller a "found"
# package whose every documented target_link_libraries(RefIccMAX::IccProfLib2)
# line fails at configure time. Fall through to the manual search instead,
# which builds the targets itself. examples/hello-iccdev/CMakeLists.txt already
# works around this downstream; the module has to do it too (#2154).
if(RefIccMAX_FOUND AND (TARGET RefIccMAX::IccProfLib2 OR
                        TARGET RefIccMAX::IccProfLib2-static))
  # CONFIG already created targets and set variables - nothing more to do
  return()
endif()

if(RefIccMAX_FOUND)
  message(STATUS
    "FindRefIccMAX: ignoring CONFIG package at ${RefIccMAX_DIR} -- it defines "
    "no RefIccMAX::IccProfLib2 target; falling back to manual discovery")
  # RefIccMAX_DIR is deliberately left alone. Clearing it would not blacklist
  # anything -- the next find_package(... CONFIG) just re-runs the search and
  # finds the same config again -- while it WOULD discard a hint the user set
  # with -DRefIccMAX_DIR=..., which Phase 2 reads below and which would then be
  # gone from the cache on every later configure.
  set(RefIccMAX_FOUND FALSE)
  set(REFICCMAX_FOUND FALSE)
  # The rejected config's variables are still live in this scope. REFICCMAX_
  # INCLUDE_DIRS and REFICCMAX_LIBRARIES are rebuilt unconditionally further
  # down, but REFICCMAX_VERSION is only overwritten if IccProfLibVer.h is found
  # and parses -- otherwise find_package_handle_standard_args would report the
  # DISCARDED package's version for the manually discovered install, and a
  # find_package(RefIccMAX 2.4 MODULE) would fail against a stale 2.3.1.
  unset(REFICCMAX_VERSION)
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

# Find shared libraries.
#
# The "d"-suffixed spellings are the same libraries from a Debug install:
# Build/Cmake/CMakeLists.txt sets CMAKE_DEBUG_POSTFIX "d", so a Debug build
# installs IccProfLib2d.lib / libIccProfLib2d.so and this module found NOTHING
# against it -- on every platform, not just Windows. Release names are listed
# first so a prefix carrying both still resolves to the release library.
find_library(REFICCMAX_ICCPROFLIB_LIBRARY
  NAMES IccProfLib2 IccProfLib2d
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

find_library(REFICCMAX_ICCXML_LIBRARY
  NAMES IccXML2 IccXML2d
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

# Find static libraries
find_library(REFICCMAX_ICCPROFLIB_STATIC_LIBRARY
  NAMES IccProfLib2-static IccProfLib2-staticd
  HINTS ${_hints}
  PATH_SUFFIXES lib lib64
)

find_library(REFICCMAX_ICCXML_STATIC_LIBRARY
  NAMES IccXML2-static IccXML2-staticd
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

# Standard find_package handling.
#
# REFICCMAX_ICCPROFLIB_LIBRARY is the SHARED library on purpose: this module
# reports not-found for a static-only install rather than half-supporting one.
# A static archive exports none of its own dependencies, so the consumer has to
# repeat them -- ICC_USE_ZLIB decides whether zlib is among them, ENABLE_ICCXML
# decides libxml2, and a hand-written find module cannot see which options the
# install was built with. Accepting the static archive here gets as far as a
# "DSO missing from command line" link failure on zlib instead of a clean
# not-found. Consume a static-only install through CONFIG mode, whose exported
# targets are generated from the build that produced them and carry exactly the
# right dependencies; that is the path ports/iccdev takes (#176, #2154).
#
# The -static targets below are still created whenever the archives sit next to
# the shared libraries. It is only the "Static-only installs still need generic
# target names" fallback that this makes unreachable: reaching it requires
# RefIccMAX::IccProfLib2 to be absent, and FPHSA has already failed by then.
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
  find_package(Threads REQUIRED)

  # Prefer the imported target over ${LIBXML2_LIBRARIES}. The installed
  # IccProfileXml.h includes <libxml/parser.h>, so the dependency has to carry
  # libxml2's INCLUDE directories as well as its link line -- and a raw library
  # path carries none. LibXml2::LibXml2 does (FindLibXml2 has defined it since
  # CMake 3.12); without it every consumer reaching IccXML2 through this module
  # fails to compile on the first IccXML header, while the CONFIG-mode package
  # builds fine because its exported target links LibXml2::LibXml2 (#2154).
  set(_reficcmax_libxml2 "")
  if(TARGET LibXml2::LibXml2)
    set(_reficcmax_libxml2 LibXml2::LibXml2)
  elseif(LIBXML2_LIBRARIES)
    set(_reficcmax_libxml2 ${LIBXML2_LIBRARIES})
  endif()

  # ---- Create IMPORTED targets ----

  # IccProfLib2 (shared)
  #
  # ICCPROFLIBDLL_DATA_IMPORTS matches what the CONFIG-mode package carries: the
  # shared IccProfLib target sets it INTERFACE, so a consumer sees __declspec(
  # dllimport) on the eight ICCPROFLIB_DATA_API globals (#2154, from #1888).
  # Without it a Windows consumer that reaches iccDEV through THIS module rather
  # than through install(EXPORT) compiles with the macro empty, emits a direct
  # data reference with no __imp_ indirection, and still gets LNK2019 -- the one
  # remaining way to hit the defect #2219 fixed. Deliberately not set on the
  # -static target below, which must keep a plain extern. Inert off Windows:
  # IccProfLibConf.h's non-PC branch does not read this macro.
  if(REFICCMAX_ICCPROFLIB_LIBRARY AND NOT TARGET RefIccMAX::IccProfLib2)
    add_library(RefIccMAX::IccProfLib2 UNKNOWN IMPORTED)
    set_target_properties(RefIccMAX::IccProfLib2 PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCPROFLIB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${REFICCMAX_ICCPROFLIB_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "Threads::Threads"
      INTERFACE_COMPILE_DEFINITIONS "ICCPROFLIBDLL_DATA_IMPORTS"
      INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
  endif()

  # IccProfLib2-static
  if(REFICCMAX_ICCPROFLIB_STATIC_LIBRARY AND NOT TARGET RefIccMAX::IccProfLib2-static)
    add_library(RefIccMAX::IccProfLib2-static STATIC IMPORTED)
    set_target_properties(RefIccMAX::IccProfLib2-static PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCPROFLIB_STATIC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${REFICCMAX_ICCPROFLIB_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "Threads::Threads"
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
    if(_reficcmax_libxml2)
      list(APPEND _xml_deps ${_reficcmax_libxml2})
    endif()
    set(_xml_incs "${REFICCMAX_ICCXML_INCLUDE_DIR}")
    if(NOT TARGET LibXml2::LibXml2 AND LIBXML2_INCLUDE_DIR)
      list(APPEND _xml_incs "${LIBXML2_INCLUDE_DIR}")
    endif()
    set_target_properties(RefIccMAX::IccXML2 PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCXML_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${_xml_incs}"
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
    if(_reficcmax_libxml2)
      list(APPEND _xml_static_deps ${_reficcmax_libxml2})
    endif()
    set(_xml_static_incs "${REFICCMAX_ICCXML_INCLUDE_DIR}")
    if(NOT TARGET LibXml2::LibXml2 AND LIBXML2_INCLUDE_DIR)
      list(APPEND _xml_static_incs "${LIBXML2_INCLUDE_DIR}")
    endif()
    set_target_properties(RefIccMAX::IccXML2-static PROPERTIES
      IMPORTED_LOCATION "${REFICCMAX_ICCXML_STATIC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${_xml_static_incs}"
      INTERFACE_LINK_LIBRARIES "${_xml_static_deps}"
      INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
  endif()

  # Static-only installs still need generic target names for consumers.
  if(NOT TARGET RefIccMAX::IccProfLib2 AND TARGET RefIccMAX::IccProfLib2-static)
    add_library(RefIccMAX::IccProfLib2 INTERFACE IMPORTED)
    set_target_properties(RefIccMAX::IccProfLib2 PROPERTIES
      INTERFACE_LINK_LIBRARIES "RefIccMAX::IccProfLib2-static"
    )
  endif()
  if(NOT TARGET RefIccMAX::IccXML2 AND TARGET RefIccMAX::IccXML2-static)
    add_library(RefIccMAX::IccXML2 INTERFACE IMPORTED)
    set_target_properties(RefIccMAX::IccXML2 PROPERTIES
      INTERFACE_LINK_LIBRARIES "RefIccMAX::IccXML2-static"
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
