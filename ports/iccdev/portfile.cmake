###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Last Updated: 2026-04-02 01:27:42 UTC by David Hoyt
#               Add SHA Pin
#               Fixups following BeyondRGB PR-418
# Remark:       vcpkg testing macOS, WSL-2 & Win11              
#
# portfile.cmake - vcpkg port for iccDEV (RefIccMAX)
#
# Builds IccProfLib2, IccXML2, and core CLI tools from source.
# Image-dependent tools (tiff/png/jpeg) and wxProfileDump GUI
# are excluded; this port focuses on the ICC profile library
# and core command-line utilities.
#
###############################################################

# CI mode: use local checkout to avoid GitHub API auth issues.
# Set VCPKG_ICCDEV_SOURCE env var to the repo root.
# Requires VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE to pass through vcpkg sandbox.
if(DEFINED ENV{VCPKG_ICCDEV_SOURCE} AND EXISTS "$ENV{VCPKG_ICCDEV_SOURCE}/Build/Cmake/CMakeLists.txt")
    set(SOURCE_PATH "${CURRENT_BUILDTREES_DIR}/src/local")
    file(REMOVE_RECURSE "${SOURCE_PATH}")
    file(COPY "$ENV{VCPKG_ICCDEV_SOURCE}/" DESTINATION "${SOURCE_PATH}"
         PATTERN ".git" EXCLUDE
         REGEX "/\\.vs($|/)" EXCLUDE
         REGEX "/out($|/)" EXCLUDE
         REGEX "/deps-extract($|/)" EXCLUDE
         REGEX "/deps\\.zip$" EXCLUDE
         REGEX "/iccDEV-sdk-[^/]+($|/)" EXCLUDE
         REGEX "/Build/Cmake/build[^/]*($|/)" EXCLUDE
         REGEX "/examples/hello-iccdev/build[^/]*($|/)" EXCLUDE
         REGEX "/examples/hello-iccdev/vcpkg_installed($|/)" EXCLUDE
         REGEX "/vcpkg_installed($|/)" EXCLUDE)
    message(STATUS "Using local source from $ENV{VCPKG_ICCDEV_SOURCE}")
else()
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO InternationalColorConsortium/iccDEV
        REF "v${VERSION}"
        SHA512 df284c3d39c969283b8d616a00970d228edf6b3ec8710206e9959c173dee7f63bf1cbbe6786c3f98763bd99bef924736e79c7f8e998cbe38d8539fec2d48c8ea
        HEAD_REF master
    )
endif()

# -- Source patches ------------------------------------------------

# 1. Guard IccXML subdirectory behind ENABLE_ICCXML
#    (upstream adds it unconditionally).
set(_iccxml_old [=[#
# XML library
#
if(VERBOSE_CONFIG)
  message(STATUS "Adding subdirectory for IccXML.")
endif()
add_subdirectory(IccXML)

# Set default link target for IccXML (matches build configuration)
IF(ENABLE_SHARED_LIBS)
  set(TARGET_LIB_ICCXML IccXML2 CACHE INTERNAL "Link target for IccXML2")
ELSE()
  set(TARGET_LIB_ICCXML IccXML2-static CACHE INTERNAL "Link target for IccXML2")
ENDIF()]=])

set(_iccxml_new [=[#
# XML library (guarded by ENABLE_ICCXML)
#
if(ENABLE_ICCXML)
  if(VERBOSE_CONFIG)
    message(STATUS "Adding subdirectory for IccXML.")
  endif()
  add_subdirectory(IccXML)

  # Set default link target for IccXML (matches build configuration)
  IF(ENABLE_SHARED_LIBS)
    set(TARGET_LIB_ICCXML IccXML2 CACHE INTERNAL "Link target for IccXML2")
  ELSE()
    set(TARGET_LIB_ICCXML IccXML2-static CACHE INTERNAL "Link target for IccXML2")
  ENDIF()
endif()]=])

vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "${_iccxml_old}" "${_iccxml_new}"
)

# 2. Disable IccDEVCmm (Windows CMM DLL; PCH issues under Ninja)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccDEVCmm)"
    "# IccDEVCmm disabled in vcpkg port (Windows CMM DLL, not a library)"
)

# 3. Disable IccIisIsapi (Windows IIS ISAPI DLL; requires IIS SDK, not a library)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccIisIsapi)"
    "# IccIisIsapi disabled in vcpkg port (Windows IIS ISAPI DLL, not a library)"
)

# 4. Make TIFF/PNG/JPEG optional (only needed for image tools, excluded here)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "find_package(TIFF REQUIRED)"
    "find_package(TIFF QUIET)"
)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "find_package(PNG REQUIRED)"
    "find_package(PNG QUIET)"
)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "find_package(JPEG REQUIRED)"
    "find_package(JPEG QUIET)"
)

# 5. Disable IccJpegDump (image tool mixed in with core tools)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccJpegDump)"
    "# IccJpegDump disabled in vcpkg port (requires JPEG, image tools excluded)"
)

# 6. Disable TIFF block FATAL_ERROR -> skip
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR "TIFF library not found. Please install libtiff-dev.")]=]
    [=[message(STATUS "TIFF not found; skipping TIFF-dependent tools (vcpkg port: image tools excluded)")]=]
)

# 7. Disable PNG block FATAL_ERROR (line ~439 in deps section)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR " PNG not found. Please install libpng-dev or install via vcpkg.")]=]
    [=[message(STATUS "PNG not found; skipping (vcpkg port: image tools excluded)")]=]
)

# 8. Disable IccPngDump subdirectory and its FATAL_ERROR guard
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccPngDump)"
    "# IccPngDump disabled in vcpkg port (requires PNG, image tools excluded)"
)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR "PNG not found. Please install libpng-dev or install via vcpkg.")]=]
    [=[message(STATUS "PNG not found; skipping IccPngDump (vcpkg port)")]=]
)

# 9. Disable JPEG FATAL_ERROR
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR " JPEG not found. Please install libjpeg-dev or install via vcpkg.")]=]
    [=[message(STATUS "JPEG not found; skipping (vcpkg port: image tools excluded)")]=]
)

# 10. Make LibXml2 conditional on ENABLE_ICCXML (upstream has it unconditional)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[find_package(LibXml2 REQUIRED)]=]
    [=[if(ENABLE_ICCXML)
  find_package(LibXml2 REQUIRED)
endif()]=]
)

# Feature flags -> CMake options
vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        tools   ENABLE_TOOLS
        xml     ENABLE_ICCXML
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/Build/Cmake"
    OPTIONS
        -DENABLE_INSTALL_RIM=ON
        -DENABLE_TESTS=OFF
        -DENABLE_SANITIZERS=OFF
        -DENABLE_COVERAGE=OFF
        -DENABLE_FUZZING=OFF
        -DENABLE_SHARED_LIBS=OFF
        -DENABLE_STATIC_LIBS=ON
        -DENABLE_WXWIDGETS=OFF
        # Keep static archives linkable from non-Clang consumers by disabling
        # Release IPO/LTO in the packaged build.
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_build()
vcpkg_cmake_install()

# Create IccProfLib/ compatibility include directory
# Upstream installs headers under include/RefIccMAX/IccProfLib2/. Consumers using
# submodule-style includes (#include <IccProfLib/IccProfile.h>) need this alias.
file(GLOB _proflib_headers "${CURRENT_PACKAGES_DIR}/include/RefIccMAX/IccProfLib2/*.h")
file(COPY ${_proflib_headers} DESTINATION "${CURRENT_PACKAGES_DIR}/include/IccProfLib")

# Fix up CMake config files location
# Upstream installs to lib/cmake/reficcmax (lowercase)
vcpkg_cmake_config_fixup(
    PACKAGE_NAME RefIccMAX
    CONFIG_PATH lib/cmake/reficcmax
)

# Move core tools into tools/iccdev/
if("tools" IN_LIST FEATURES)
    set(_core_tools
        iccDumpProfile
        iccApplyNamedCmm
        iccRoundTrip
        iccFromCube
        iccApplyToLink
        iccApplySearch
        iccV5DspObsToV4Dsp
    )
    vcpkg_copy_tools(TOOL_NAMES ${_core_tools} AUTO_CLEAN)

    # XML tools only when xml feature enabled
    if("xml" IN_LIST FEATURES)
        vcpkg_copy_tools(
            TOOL_NAMES
                iccFromXml
                iccToXml
            AUTO_CLEAN
        )
    endif()
endif()

# Clean up
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# Remove stray dependency DLLs that cmake installs alongside tools.
# These are already provided by their own vcpkg ports (libiconv, libxml2, zlib).
file(GLOB _stray_dlls
    "${CURRENT_PACKAGES_DIR}/bin/*.dll"
    "${CURRENT_PACKAGES_DIR}/debug/bin/*.dll"
)
if(_stray_dlls)
    file(REMOVE ${_stray_dlls})
endif()
# Remove empty bin directories left after DLL cleanup
foreach(_bindir "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
    if(IS_DIRECTORY "${_bindir}")
        file(GLOB _remaining "${_bindir}/*")
        if(NOT _remaining)
            file(REMOVE_RECURSE "${_bindir}")
        endif()
    endif()
endforeach()

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
