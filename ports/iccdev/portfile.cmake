###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# portfile.cmake — vcpkg port for iccDEV (RefIccMAX)
#
# Builds IccProfLib2, IccXML2, and core CLI tools from source.
# Image-dependent tools (tiff/png/jpeg) and wxProfileDump GUI
# are excluded — this port focuses on the ICC profile library
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
         PATTERN ".git" EXCLUDE)
    message(STATUS "Using local source from $ENV{VCPKG_ICCDEV_SOURCE}")
else()
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO InternationalColorConsortium/iccDEV
        REF "v${VERSION}"
        SHA512 0  # Placeholder — update after tagging release
        HEAD_REF ci-vcpkg-ports
    )
endif()

# ── Source patches ────────────────────────────────────────────

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

# 2. Disable IccDEVCmm (Windows CMM DLL — PCH issues under Ninja)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccDEVCmm)"
    "# IccDEVCmm disabled in vcpkg port (Windows CMM DLL, not a library)"
)

# 3. Make TIFF/PNG/JPEG optional (only needed for image tools, excluded here)
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

# 4. Disable IccJpegDump (image tool mixed in with core tools)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccJpegDump)"
    "# IccJpegDump disabled in vcpkg port (requires JPEG, image tools excluded)"
)

# 5. Disable TIFF block FATAL_ERROR → skip
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR "TIFF library not found. Please install libtiff-dev.")]=]
    [=[message(STATUS "TIFF not found — skipping TIFF-dependent tools (vcpkg port: image tools excluded)")]=]
)

# 6. Disable PNG block FATAL_ERROR (line ~439 in deps section)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR " PNG not found. Please install libpng-dev or install via vcpkg.")]=]
    [=[message(STATUS "PNG not found — skipping (vcpkg port: image tools excluded)")]=]
)

# 7. Disable IccPngDump subdirectory and its FATAL_ERROR guard
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    "ADD_SUBDIRECTORY(Tools/IccPngDump)"
    "# IccPngDump disabled in vcpkg port (requires PNG, image tools excluded)"
)
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR "PNG not found. Please install libpng-dev or install via vcpkg.")]=]
    [=[message(STATUS "PNG not found — skipping IccPngDump (vcpkg port)")]=]
)

# 8. Disable JPEG FATAL_ERROR
vcpkg_replace_string("${SOURCE_PATH}/Build/Cmake/CMakeLists.txt"
    [=[message(FATAL_ERROR " JPEG not found. Please install libjpeg-dev or install via vcpkg.")]=]
    [=[message(STATUS "JPEG not found — skipping (vcpkg port: image tools excluded)")]=]
)

# Feature flags → CMake options
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
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_build()
vcpkg_cmake_install()

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

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
