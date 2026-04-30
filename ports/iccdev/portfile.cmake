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

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

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

# Feature flags -> CMake options
vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        json    ENABLE_ICCJSON
        tools   ENABLE_TOOLS
        xml     ENABLE_ICCXML
)

set(_iccdev_msvc_runtime_options_release)
set(_iccdev_msvc_runtime_options_debug)
if(VCPKG_TARGET_IS_WINDOWS)
    if(VCPKG_CRT_LINKAGE STREQUAL "static")
        list(APPEND _iccdev_msvc_runtime_options_release
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
        )
        list(APPEND _iccdev_msvc_runtime_options_debug
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug"
        )
    else()
        list(APPEND _iccdev_msvc_runtime_options_release
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
        )
        list(APPEND _iccdev_msvc_runtime_options_debug
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL"
        )
    endif()
endif()

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
        -DENABLE_IMAGE_TOOLS=OFF
        -DENABLE_CMM_TOOLS=OFF
        -DENABLE_IIS_TOOLS=OFF
        # Keep static archives linkable from non-Clang consumers by disabling
        # Release IPO/LTO in the packaged build.
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
        ${FEATURE_OPTIONS}
    OPTIONS_RELEASE
        ${_iccdev_msvc_runtime_options_release}
    OPTIONS_DEBUG
        ${_iccdev_msvc_runtime_options_debug}
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
    if("json" IN_LIST FEATURES)
        vcpkg_copy_tools(
            TOOL_NAMES
                iccFromJson
                iccToJson
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

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
