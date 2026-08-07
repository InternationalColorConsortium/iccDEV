# CTest entry points for the existing iccDEV script-based test suites.
#
# The tests intentionally keep the current script bodies as the source of truth
# while exposing standard CMake/CTest labels, timeouts, fixtures, and a check
# target for local and CI use.

find_program(BASH_EXECUTABLE bash)

get_filename_component(ICCDEV_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(ICCDEV_TOOLS_DIR "${CMAKE_BINARY_DIR}/Tools")
set(ICCDEV_TESTING_DIR "${ICCDEV_REPO_ROOT}/Testing")
set(ICCDEV_TEST_OUTDIR "${CMAKE_BINARY_DIR}/Testing/ctest-output")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")

get_property(ICCDEV_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(ICCDEV_IS_MULTI_CONFIG)
  set(ICCDEV_CTEST_CONFIG_ARGS -C "$<CONFIG>")
else()
  set(ICCDEV_CTEST_CONFIG_ARGS)
endif()

if(WIN32)
  include("${CMAKE_CURRENT_LIST_DIR}/WindowsRuntimePaths.cmake")
  iccdev_collect_cache_runtime_path_entries(
    ICCDEV_WINDOWS_RUNTIME_PATHS
    "${CMAKE_BINARY_DIR}")
endif()

add_custom_target(check
  COMMAND "${CMAKE_CTEST_COMMAND}"
          ${ICCDEV_CTEST_CONFIG_ARGS}
          --output-on-failure
          --no-tests=error
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Run iccDEV CTest suite"
  VERBATIM
)

add_custom_target(build-test-binaries
  COMMENT "Build iccDEV CTest regression helper binaries"
)
add_dependencies(check build-test-binaries)
if(TARGET iccdev-stage-windows-runtime)
  add_dependencies(check iccdev-stage-windows-runtime)
endif()

function(iccdev_add_regression_executable TARGET_NAME)
  add_executable("${TARGET_NAME}" EXCLUDE_FROM_ALL ${ARGN})
  add_dependencies(build-test-binaries "${TARGET_NAME}")
endfunction()

# Link target for regression executables that reference an exported IccProfLib
# global *variable* rather than only its functions (#1888).
#
# On a Windows shared build the library is IccProfLib2.dll, exported by CMake's
# WINDOWS_EXPORT_ALL_SYMBOLS rather than by __declspec annotations: MSVC is
# deliberately excluded from the ICCPROFLIBDLL_EXPORTS definition in
# Build/Cmake/IccProfLib/CMakeLists.txt, a decision made in #764 and recorded in
# the comment there. WINDOWS_EXPORT_ALL_SYMBOLS auto-exports functions; global
# data still needs explicit dllexport/dllimport, which IccProfLib does not carry.
# A consumer compiling with ICCPROFLIB_API empty therefore emits a direct data
# reference with no __imp_ indirection and fails to link (LNK2019/LNK2001), while
# every function in the same header thunks normally. Linux and macOS are immune,
# so a green local pre-flight says nothing about this.
#
# The three tools with the same problem already resolve it this way -- see
# Build/Cmake/Tools/{IccDumpProfile,IccProfilePlot,wxProfileDump}/CMakeLists.txt.
#
# USE THIS ONLY FOR TESTS THAT LINK IccProfLib ALONE. IccXML2 and IccJson link
# ${TARGET_LIB_ICCPROFLIB} PUBLIC, so a test that links one of those *and* the
# static IccProfLib pulls the library into the process twice -- once inside the
# DLL, once in the executable -- giving two copies of its globals. Tests that
# link IccXML/IccJson must keep using ${TARGET_LIB_ICCPROFLIB} and must not
# reference exported library data at all; assert on literal values instead, as
# .github/ci/regression/{json-sparsematrix-huaf,xml-writer-graceful-degrade,
# json-bcd-version-parse}.cpp do -- those three are the tests that actually link
# one of the two libraries. mpe-empty-identity.cpp and pawg-q4-xyz-pcs-decode.cpp
# also hard-code literals, but they link IccProfLib alone and simply predate this
# variable; they are eligible for it, not barred from it.
if(WIN32 AND ENABLE_SHARED_LIBS AND TARGET IccProfLib2-static)
  set(ICCDEV_TEST_LIB_ICCPROFLIB IccProfLib2-static)
else()
  set(ICCDEV_TEST_LIB_ICCPROFLIB ${TARGET_LIB_ICCPROFLIB})
endif()
message(STATUS "ICCDEV_TEST_LIB_ICCPROFLIB  = ${ICCDEV_TEST_LIB_ICCPROFLIB}")

add_custom_target(check-fast
  COMMAND "${CMAKE_CTEST_COMMAND}"
          ${ICCDEV_CTEST_CONFIG_ARGS}
          --label-exclude slow
          --output-on-failure
          --no-tests=error
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Run iccDEV CTest suite excluding slow tests"
  VERBATIM
)
add_dependencies(check-fast build-test-binaries)
if(TARGET iccdev-stage-windows-runtime)
  add_dependencies(check-fast iccdev-stage-windows-runtime)
endif()

function(iccdev_add_check_dependencies)
  foreach(_target
      iccApplyNamedCmm
      iccApplyProfiles
      iccApplySearch
      iccApplyToLink
      iccDescribeSinkTest
      iccDumpProfile
      iccPawgReport
      iccFromCube
      iccFromJson
      iccFromXml
      iccJpegDump
      iccPngDump
      iccRoundTrip
      iccSpecSepToTiff
      iccTiffDump
      iccToJson
      iccToXml
      iccV5DspObsToV4Dsp
      iccProfileVisualize
  )
    if(TARGET "${_target}")
      add_dependencies(check "${_target}")
      add_dependencies(check-fast "${_target}")
    endif()
  endforeach()
endfunction()

iccdev_add_check_dependencies()
if(TARGET iccDescribeSinkTest)
  add_dependencies(build-test-binaries iccDescribeSinkTest)
endif()

function(iccdev_add_iccconnect_thread_test)
  if(NOT DEFINED TARGET_LIB_ICCCONNECT OR NOT TARGET "${TARGET_LIB_ICCCONNECT}")
    return()
  endif()

  iccdev_add_regression_executable(iccConnectThreadTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccconnect-threaded-cmm.cpp"
  )
  target_compile_features(iccConnectThreadTest PRIVATE cxx_std_17)
  target_link_libraries(iccConnectThreadTest PRIVATE ${TARGET_LIB_ICCCONNECT})

  add_dependencies(check iccConnectThreadTest)

  if(WIN32)
    add_test(
      NAME iccdev.iccconnect-threaded-cmm
      COMMAND
        "$<TARGET_FILE:iccConnectThreadTest>"
        "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
    )
  else()
    add_test(
      NAME iccdev.iccconnect-threaded-cmm
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccConnectThreadTest>"
        "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
    )
  endif()
  set_tests_properties(iccdev.iccconnect-threaded-cmm PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccconnect;threaded;regression"
  )
  if(WIN32)
    set(_iccconnect_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccConnectThreadTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCCONNECT}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccconnect_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccconnect-threaded-cmm PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccconnect_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_iccconnect_config_parser_test)
  if(NOT DEFINED TARGET_LIB_ICCCONNECT OR NOT TARGET "${TARGET_LIB_ICCCONNECT}")
    return()
  endif()

  iccdev_add_regression_executable(iccConnectConfigParserTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccconnect-config-parser.cpp"
  )
  target_compile_features(iccConnectConfigParserTest PRIVATE cxx_std_17)
  target_link_libraries(iccConnectConfigParserTest PRIVATE ${TARGET_LIB_ICCCONNECT})

  add_dependencies(check iccConnectConfigParserTest)

  if(WIN32)
    add_test(
      NAME iccdev.iccconnect-config-parser
      COMMAND "$<TARGET_FILE:iccConnectConfigParserTest>"
    )
  else()
    add_test(
      NAME iccdev.iccconnect-config-parser
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccConnectConfigParserTest>"
    )
  endif()
  set_tests_properties(iccdev.iccconnect-config-parser PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccconnect;config;cli;json;regression"
  )
  if(WIN32)
    set(_iccconnect_config_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccConnectConfigParserTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCCONNECT}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccconnect_config_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccconnect-config-parser PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccconnect_config_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_cmmsearch_init_pcc_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCmmSearchInitPccTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/cmmsearch-init-pcc.cpp"
  )
  target_link_libraries(iccCmmSearchInitPccTest PRIVATE ${TARGET_LIB_ICCPROFLIB})

  add_dependencies(check iccCmmSearchInitPccTest)

  # sRGB_D65_MAT.icc is the regression case: a tracked v5 profile carrying an
  # 'svcn' tag that declares D65, so a PCC that fails to resolve falls back to
  # the D50 default and a spurious adaptation is spliced in (issue #1860).
  # sRGB_v4_ICC_preference.icc is the control: it has no 'svcn', both ends of the
  # chain default to D50 either way, and its results must not move.
  set(_cmmsearch_init_pcc_cases
    "d65-svcn=${ICCDEV_TESTING_DIR}/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
    "v4-no-svcn=${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )

  foreach(_case IN LISTS _cmmsearch_init_pcc_cases)
    string(REGEX REPLACE "=.*$" "" _case_name "${_case}")
    string(REGEX REPLACE "^[^=]*=" "" _case_profile "${_case}")

    if(WIN32)
      add_test(
        NAME iccdev.cmmsearch-init-pcc.${_case_name}
        COMMAND "$<TARGET_FILE:iccCmmSearchInitPccTest>" "${_case_profile}"
      )
    else()
      add_test(
        NAME iccdev.cmmsearch-init-pcc.${_case_name}
        COMMAND
          "${CMAKE_COMMAND}" -E env
          ${ICCDEV_TEST_ENV}
          "$<TARGET_FILE:iccCmmSearchInitPccTest>"
          "${_case_profile}"
      )
    endif()
    set_tests_properties(iccdev.cmmsearch-init-pcc.${_case_name} PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
      TIMEOUT 120
      LABELS "iccdev;iccprofLib;cmmsearch;pcc;regression"
    )
    if(WIN32)
      set(_cmmsearch_init_pcc_windows_env_mods
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCmmSearchInitPccTest>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      )
      foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
        list(APPEND _cmmsearch_init_pcc_windows_env_mods
          "PATH=path_list_prepend:${_runtime_path}")
      endforeach()
      set_tests_properties(iccdev.cmmsearch-init-pcc.${_case_name} PROPERTIES
        ENVIRONMENT_MODIFICATION "${_cmmsearch_init_pcc_windows_env_mods}"
      )
    endif()
  endforeach()
endfunction()

function(iccdev_add_embedio_read8_bounds_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccEmbedIoRead8BoundsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/embedio-read8-bounds.cpp"
  )
  target_link_libraries(iccEmbedIoRead8BoundsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccEmbedIoRead8BoundsTest)

  add_test(
    NAME iccdev.embedio-read8-bounds
    COMMAND "$<TARGET_FILE:iccEmbedIoRead8BoundsTest>"
  )
  set_tests_properties(iccdev.embedio-read8-bounds PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;embedio;regression"
  )
  if(WIN32)
    set(_embedio_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccEmbedIoRead8BoundsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _embedio_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.embedio-read8-bounds PROPERTIES
      ENVIRONMENT_MODIFICATION "${_embedio_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_tag_layout_shared_data_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccTagLayoutSharedDataTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tag-layout-shared-data.cpp"
  )
  target_link_libraries(iccTagLayoutSharedDataTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccTagLayoutSharedDataTest)

  add_test(
    NAME iccdev.tag-layout-shared-data
    COMMAND "$<TARGET_FILE:iccTagLayoutSharedDataTest>"
  )
  set_tests_properties(iccdev.tag-layout-shared-data PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;validation;layout;regression"
  )
  if(WIN32)
    set(_tag_layout_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccTagLayoutSharedDataTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _tag_layout_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.tag-layout-shared-data PROPERTIES
      ENVIRONMENT_MODIFICATION "${_tag_layout_windows_env_mods}"
    )
  endif()
endfunction()

# Guards iccPawgReport's internal-PCS-XYZ decode in the quality metrics (#1454):
# iccquality::pcs_to_lab must un-scale an XYZ-PCS sample (a CMM output is
# internal PCS XYZ, ~0.5x actual) with icXyzFromPcs before icXYZtoLab, exactly as
# IccEval.cpp does -- without it an XYZ-PCS white decoded to L* ~= 76 and every
# Q1/Q3/Q4 dE was computed in a distorted Lab space.  The test decodes an
# internal-PCS D50 white (must be L*=100, not ~76) and runs Q4 on an in-memory
# XYZ-PCS matrix profile against an absolute-colorimetric target (must reproduce
# to ~0 dE).  Header-only metrics, so it just needs the IccPawgReport include dir
# + IccProfLib.
function(iccdev_add_pawg_q4_xyz_pcs_decode_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPawgQ4XyzPcsDecodeTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pawg-q4-xyz-pcs-decode.cpp"
  )
  target_include_directories(iccPawgQ4XyzPcsDecodeTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport"
  )
  target_link_libraries(iccPawgQ4XyzPcsDecodeTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPawgQ4XyzPcsDecodeTest)

  add_test(
    NAME iccdev.pawg-q4-xyz-pcs-decode
    COMMAND "$<TARGET_FILE:iccPawgQ4XyzPcsDecodeTest>"
  )
  set_tests_properties(iccdev.pawg-q4-xyz-pcs-decode PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;pawg;quality;characterization;regression"
  )
  if(WIN32)
    set(_pawg_q4_xyz_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPawgQ4XyzPcsDecodeTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pawg_q4_xyz_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pawg-q4-xyz-pcs-decode PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pawg_q4_xyz_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_tointernal_fixedint_roundtrip_test)
  if(NOT TARGET "${ICCDEV_TEST_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccToInternalFixedIntRoundtripTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tointernal-fixedint-roundtrip.cpp"
  )
  target_link_libraries(iccToInternalFixedIntRoundtripTest PRIVATE ${ICCDEV_TEST_LIB_ICCPROFLIB})
  add_dependencies(check iccToInternalFixedIntRoundtripTest)

  add_test(
    NAME iccdev.tointernal-fixedint-roundtrip
    COMMAND "$<TARGET_FILE:iccToInternalFixedIntRoundtripTest>"
  )
  set_tests_properties(iccdev.tointernal-fixedint-roundtrip PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;encoding;regression"
  )
  if(WIN32)
    set(_tointernal_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccToInternalFixedIntRoundtripTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${ICCDEV_TEST_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _tointernal_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.tointernal-fixedint-roundtrip PROPERTIES
      ENVIRONMENT_MODIFICATION "${_tointernal_env_mods}"
    )
  endif()
endfunction()

# Guards the CMM-signature validation allow-list catch-up landed for #1724 in
# CIccProfile::CheckHeader (IccProfLib/IccProfile.cpp).  PR #1473 named the live-
# registry CMMs 'repr' and 'iccd' but left them out of CheckHeader's accept-list,
# so Validate() still warned "Unregistered CMM signature" for them.  This test
# drives CheckHeader directly and pins: 'repr'/'iccd' (and the already-listed
# 'WCS '/'RIMX') validate without a CMM warning, while 'MSFT' (a platform sig,
# not a registered CMM) and unregistered junk still warn.  Core IccProfLib only,
# so no extra include dirs are needed.
function(iccdev_add_cmm_registry_allowlist_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCmmRegistryAllowlistTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/cmm-registry-allowlist.cpp"
  )
  target_link_libraries(iccCmmRegistryAllowlistTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCmmRegistryAllowlistTest)

  add_test(
    NAME iccdev.cmm-registry-allowlist
    COMMAND "$<TARGET_FILE:iccCmmRegistryAllowlistTest>"
  )
  set_tests_properties(iccdev.cmm-registry-allowlist PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;pawg;cmm;registry;regression"
  )
  if(WIN32)
    set(_cmm_allowlist_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCmmRegistryAllowlistTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _cmm_allowlist_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.cmm-registry-allowlist PROPERTIES
      ENVIRONMENT_MODIFICATION "${_cmm_allowlist_windows_env_mods}"
    )
  endif()
endfunction()

# Guards iccPawgReport's Q2 curve-invertibility scope (#1458): the round-trip
# inverse-error metric must apply to tone-reproduction curves (rTRC/gTRC/bTRC/
# kTRC) only, not to LUT A/B/M shaper curves.  A B2A output shaper curve
# legitimately has a non-invertible dark-end clamping plateau, so thresholding
# its inverse error spuriously WARNed correct CMYK output profiles (APTEC 0.0977,
# CRPC6 0.1406).  The test builds a B2A whose output curves carry that plateau
# and asserts the shaper inverse error is no longer thresholded, while a tone
# curve carrying the same plateau is still flagged.  Header-only metrics, so it
# just needs the IccPawgReport include dir + IccProfLib.
function(iccdev_add_pawg_q2_tonecurves_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPawgQ2ToneCurvesTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pawg-q2-tonecurves-only.cpp"
  )
  target_include_directories(iccPawgQ2ToneCurvesTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport"
  )
  target_link_libraries(iccPawgQ2ToneCurvesTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPawgQ2ToneCurvesTest)

  add_test(
    NAME iccdev.pawg-q2-tonecurves-only
    COMMAND "$<TARGET_FILE:iccPawgQ2ToneCurvesTest>"
  )
  set_tests_properties(iccdev.pawg-q2-tonecurves-only PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;pawg;quality;invertibility;regression"
  )
  if(WIN32)
    set(_pawg_q2_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPawgQ2ToneCurvesTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pawg_q2_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pawg-q2-tonecurves-only PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pawg_q2_windows_env_mods}"
    )
  endif()
endfunction()

# Guards iccPawgReport's Q4 characterization round-trip
# (iccquality::evaluate_characterization): it must (1) compare predicted output
# against the embedded measurement data in ABSOLUTE colorimetric -- the only
# intent defined to match measured colorimetry -- and (2) keep every CGATS data
# row including those with a text SAMPLE_NAME column.  The pre-fix code used the
# perceptual A2B0 table (~7 dE on accurate profiles) and dropped any row with a
# non-numeric cell (1617-patch targets collapsed to ~49).  Header-only metrics,
# so the test just needs the IccPawgReport include dir + IccProfLib.
function(iccdev_add_pawg_q4_characterization_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPawgQ4CharacterizationTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pawg-q4-characterization.cpp"
  )
  target_include_directories(iccPawgQ4CharacterizationTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport"
  )
  target_link_libraries(iccPawgQ4CharacterizationTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPawgQ4CharacterizationTest)

  add_test(
    NAME iccdev.pawg-q4-characterization
    COMMAND "$<TARGET_FILE:iccPawgQ4CharacterizationTest>"
  )
  set_tests_properties(iccdev.pawg-q4-characterization PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;pawg;quality;characterization;regression"
  )
  if(WIN32)
    set(_pawg_q4_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPawgQ4CharacterizationTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pawg_q4_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pawg-q4-characterization PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pawg_q4_windows_env_mods}"
    )
  endif()
endfunction()

# Guards iccPawgReport's Q1 (round-trip) and Q3 (smoothness) intent (#1455):
# iccquality::begin_profile_cmm must default to RELATIVE colorimetric, not the
# profile header rendering intent.  The pre-fix `AddXform(*pIcc)` used the header
# intent -- Perceptual for most output profiles -- and round-tripping the
# perceptual A2B/B2A pair (which compresses gamut) reported spuriously large dE
# (CMYK-3DLUTs.icc Q1 max 22.6 -> a false FAIL; CRPC6 max ~21 vs ~2 relative).
# The test builds a profile whose A2B0 (perceptual) and A2B1 (relative) tables
# differ and asserts the default forward matches the relative one.  Header-only
# metrics, so it just needs the IccPawgReport include dir + IccProfLib.
function(iccdev_add_pawg_q1q3_relative_intent_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPawgQ1Q3RelativeIntentTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pawg-q1q3-relative-colorimetric.cpp"
  )
  target_include_directories(iccPawgQ1Q3RelativeIntentTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport"
  )
  target_link_libraries(iccPawgQ1Q3RelativeIntentTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPawgQ1Q3RelativeIntentTest)

  add_test(
    NAME iccdev.pawg-q1q3-relative-colorimetric
    COMMAND "$<TARGET_FILE:iccPawgQ1Q3RelativeIntentTest>"
  )
  set_tests_properties(iccdev.pawg-q1q3-relative-colorimetric PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;pawg;quality;roundtrip;regression"
  )
  if(WIN32)
    set(_pawg_q1q3_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPawgQ1Q3RelativeIntentTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pawg_q1q3_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pawg-q1q3-relative-colorimetric PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pawg_q1q3_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the calc-element matrix-size integer overflow (#1447 Solve / #1448
# Transpose): CIccOpDefTranspose/Solve::Exec computed `int nSize = r*c` with r,c
# each up to 65536, overflowing int and slipping past the size guard into a bad
# resize / OOB stack index.  The test drives the two minimized h0rk1p PoC
# profiles (embedded) through a CMM; the signed-integer-overflow sanitizer traps
# the pre-fix multiply.  Most meaningful under the ASAN+UBSAN tool job, but also
# catches the regression in a plain build (the wrapped size crashes resize()).
function(iccdev_add_mpecalc_matrix_overflow_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccMpeCalcMatrixOverflowTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/mpecalc-matrix-size-overflow.cpp"
  )
  target_link_libraries(iccMpeCalcMatrixOverflowTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccMpeCalcMatrixOverflowTest)

  add_test(
    NAME iccdev.mpecalc-matrix-size-overflow
    COMMAND "$<TARGET_FILE:iccMpeCalcMatrixOverflowTest>"
  )
  # With the fix the oversized op is rejected in microseconds; pre-fix a plain
  # (non-sanitizer) build instead runs the op's ~2.1e9-iteration loop, so a short
  # timeout is itself a regression signal there.  Under the ASAN+UBSAN job the
  # overflow traps instantly -- that is the authoritative guard.
  set_tests_properties(iccdev.mpecalc-matrix-size-overflow PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 25
    LABELS "iccdev;mpe;calc;overflow;regression"
  )
  if(WIN32)
    set(_mpecalc_overflow_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccMpeCalcMatrixOverflowTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _mpecalc_overflow_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.mpecalc-matrix-size-overflow PROPERTIES
      ENVIRONMENT_MODIFICATION "${_mpecalc_overflow_windows_env_mods}"
    )
  endif()
endfunction()

# Guards CIccSparseMatrixUInt8::set / CIccSparseMatrixUInt16::set in IccSparseMatrix.h
# (alerts #1061, #2260): (1) the clamp is NaN-safe -- `!(value > 0.0) ? 0` routes NaN
# and value<=0 to 0 before the float->int cast, where the old `value < 0.0` test let a
# NaN reach the undefined cast; (2) the UInt16 entry casts to (icUInt16Number), not the
# truncating (icUInt8Number) that stored e.g. 0.5 as 0 into the 16-bit slot. The set()/
# get() methods are inline, so the test exercises the shipped code; reverting either fix
# fails an assertion.
function(iccdev_add_sparsematrix_set_roundtrip_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccSparseMatrixSetRoundtripTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccsparsematrix-set-roundtrip.cpp"
  )
  target_link_libraries(iccSparseMatrixSetRoundtripTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccSparseMatrixSetRoundtripTest)

  add_test(
    NAME iccdev.iccsparsematrix-set-roundtrip
    COMMAND "$<TARGET_FILE:iccSparseMatrixSetRoundtripTest>"
  )
  set_tests_properties(iccdev.iccsparsematrix-set-roundtrip PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 25
    LABELS "iccdev;sparsematrix;nan;roundtrip;regression"
  )
  if(WIN32)
    set(_sparsematrix_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccSparseMatrixSetRoundtripTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _sparsematrix_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccsparsematrix-set-roundtrip PROPERTIES
      ENVIRONMENT_MODIFICATION "${_sparsematrix_windows_env_mods}"
    )
  endif()
endfunction()

# Guards CIccCmmSearch::AddXform profile ownership (#1332): the search CMM
# rejects a NamedColor profile (no search LUT) and, like the base AddXform
# contract (#1327), must free it.  The helper feeds a NamedColor profile through
# the inherited filename overload, which opens it internally, so a profile the
# override fails to free is unreachable and reported by the ASan/LSan CI job.
function(iccdev_add_cmmsearch_namedcolor_ownership_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()
  if(MSVC)
    message(STATUS
      "Skipping iccdev.cmmsearch-namedcolor-ownership: MSVC AddressSanitizer "
      "does not support LeakSanitizer detect_leaks=1")
    return()
  endif()

  iccdev_add_regression_executable(iccCmmSearchNamedColorOwnershipTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/cmmsearch-namedcolor-ownership.cpp"
  )
  target_link_libraries(iccCmmSearchNamedColorOwnershipTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCmmSearchNamedColorOwnershipTest)

  # This regression's pass/fail signal IS the LeakSanitizer at-exit scan, but the
  # CI tool-test job and ICCDEV_TEST_ENV both export ASAN_OPTIONS=...detect_leaks=0
  # to silence unrelated leaks in the smoke-tested tools.  Inheriting that would
  # disable LSan here and let a profile leak pass undetected.  Launch through
  # `cmake -E env ASAN_OPTIONS=detect_leaks=1` to force leak detection on for
  # this test's own process.  Set ONLY detect_leaks=1: appending halt_on_error=0
  # makes the runtime print the leak but exit 0, and a ':' option separator is
  # mis-parsed and silently disables the check.
  add_test(
    NAME iccdev.cmmsearch-namedcolor-ownership
    COMMAND "${CMAKE_COMMAND}" -E env
      "ASAN_OPTIONS=detect_leaks=1"
      "$<TARGET_FILE:iccCmmSearchNamedColorOwnershipTest>"
      "${ICCDEV_REPO_ROOT}/.github/ci/test-data/fuzz-nmcl-9c09d1e6.icc"
  )
  set_tests_properties(iccdev.cmmsearch-namedcolor-ownership PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccconnect;search;leak;issue-1332;regression"
  )
  if(WIN32)
    set(_cmmsearch_namedcolor_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCmmSearchNamedColorOwnershipTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _cmmsearch_namedcolor_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.cmmsearch-namedcolor-ownership PROPERTIES
      ENVIRONMENT_MODIFICATION "${_cmmsearch_namedcolor_windows_env_mods}"
    )
  endif()
endfunction()

# Guards CIccXform::Create(CIccProfile*, CIccTag*) profile ownership (#1308):
# AddXform hands its owned profile to the tag-based Create overload, which must
# free it when no xform can be built.  The helper feeds a valid profile plus a
# NULL tag, so Create takes its failure path; a profile it fails to free is
# unreachable and reported by the ASan/LSan CI job.
function(iccdev_add_xform_create_tag_ownership_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()
  if(MSVC)
    message(STATUS
      "Skipping iccdev.xform-create-tag-ownership: MSVC AddressSanitizer "
      "does not support LeakSanitizer detect_leaks=1")
    return()
  endif()

  iccdev_add_regression_executable(iccXformCreateTagOwnershipTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/xform-create-tag-ownership.cpp"
  )
  target_link_libraries(iccXformCreateTagOwnershipTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccXformCreateTagOwnershipTest)

  # This regression's pass/fail signal IS the LeakSanitizer at-exit scan, but the
  # CI tool-test job and ICCDEV_TEST_ENV both export ASAN_OPTIONS=detect_leaks=0
  # to silence unrelated leaks in the smoke-tested tools.  Inheriting that would
  # disable LSan here and make the test a false pass even when the profile leaks.
  # Force leak detection back on for this test's own process by launching it
  # through `cmake -E env` (the same override mechanism the script tests use; a
  # CTest ENVIRONMENT property is NOT reliable for ASAN_OPTIONS here).  Set ONLY
  # detect_leaks=1: do not append halt_on_error=0 -- with halt_on_error=0 the
  # runtime still prints the leak but exits 0, so ctest would pass on a real
  # leak.  Plain detect_leaks=1 makes a detected leak exit non-zero (the failure
  # signal ctest needs).
  add_test(
    NAME iccdev.xform-create-tag-ownership
    COMMAND "${CMAKE_COMMAND}" -E env
      "ASAN_OPTIONS=detect_leaks=1"
      "$<TARGET_FILE:iccXformCreateTagOwnershipTest>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )
  set_tests_properties(iccdev.xform-create-tag-ownership PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;cmm;xform;leak;issue-1308;regression"
  )
  if(WIN32)
    set(_xform_create_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccXformCreateTagOwnershipTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _xform_create_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.xform-create-tag-ownership PROPERTIES
      ENVIRONMENT_MODIFICATION "${_xform_create_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_xform_abstorel_adjust_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccXformAbsToRelAdjustTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/xform-abstorel-adjust.cpp"
  )
  target_link_libraries(iccXformAbsToRelAdjustTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccXformAbsToRelAdjustTest)

  add_test(
    NAME iccdev.xform-abstorel-adjust
    COMMAND "$<TARGET_FILE:iccXformAbsToRelAdjustTest>"
  )
  set_tests_properties(iccdev.xform-abstorel-adjust PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;cmm;xform;issue-1662;regression"
  )
  if(WIN32)
    set(_xform_abstorel_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccXformAbsToRelAdjustTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _xform_abstorel_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.xform-abstorel-adjust PROPERTIES
      ENVIRONMENT_MODIFICATION "${_xform_abstorel_windows_env_mods}"
    )
  endif()
endfunction()

# Guards #1671: reflectance observer setup must allocate temporary observer
# matrices with the applied illuminant range, not the element reflectance range.
function(iccdev_add_reflectance_observer_illum_range_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccReflectanceObserverIllumRangeTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/reflectance-observer-illum-range.cpp"
  )
  target_link_libraries(iccReflectanceObserverIllumRangeTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccReflectanceObserverIllumRangeTest)

  add_test(
    NAME iccdev.reflectance-observer-illum-range
    COMMAND "$<TARGET_FILE:iccReflectanceObserverIllumRangeTest>"
  )
  set_tests_properties(iccdev.reflectance-observer-illum-range PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;mpe;spectral;pcc;issue-1671;regression;asan"
  )
  if(WIN32)
    set(_reflectance_illum_range_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccReflectanceObserverIllumRangeTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _reflectance_illum_range_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.reflectance-observer-illum-range PROPERTIES
      ENVIRONMENT_MODIFICATION "${_reflectance_illum_range_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the IccColorimetry spectral->XYZ reduction module (#1475): the registry-
# aligned reduction methods (icXYZCalcDirectSum / icXYZCalcWeighting /
# icXYZCalcSpragueTo1nm) and their range-reconciliation primitives
# (icSpectralResample, icComputeWeightingTable, icApplyWeightingTable) must satisfy
# their method-independent numeric contract: exact resampling of constants and
# linear ramps (incl. Sprague interior + linear extrapolation), relative-colorimetry
# normalization (perfect diffuser -> Y = 1), operator linearity, same-grid
# equivalence of the three methods, and free-function/calculator weighting parity.
# The test feeds smooth synthetic observer/illuminant data -- no profile file and no
# (still-provisional) registry constants -- so it pins the algorithm, not the
# numbers; any sign/index/normalization/Sprague-coefficient break trips it.  The
# module is header + IccProfLib only.
function(iccdev_add_colorimetry_methods_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccColorimetryMethodsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/colorimetry-methods.cpp"
  )
  target_compile_features(iccColorimetryMethodsTest PRIVATE cxx_std_17)
  target_link_libraries(iccColorimetryMethodsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccColorimetryMethodsTest)

  add_test(
    NAME iccdev.colorimetry-methods
    COMMAND "$<TARGET_FILE:iccColorimetryMethodsTest>"
  )
  set_tests_properties(iccdev.colorimetry-methods PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;colorimetry;spectral;issue-1475;regression"
  )
  if(WIN32)
    set(_colorimetry_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccColorimetryMethodsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _colorimetry_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.colorimetry-methods PROPERTIES
      ENVIRONMENT_MODIFICATION "${_colorimetry_windows_env_mods}"
    )
  endif()
endfunction()

# #1553: CIccTag*NumArray::GetValues must bound the source start offset (nStart), not
# only the destination length, or m_Num[i+nStart] over-reads (CWE-125). Pins the
# corrected bound on the FloatNum (the repro'd defect), FixedNum and Num variants; each
# assertion is red-green against the matching guard. Header + IccProfLib only.
function(iccdev_add_getvalues_nstart_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccGetValuesNStartTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/getvalues-nstart-bound.cpp"
  )
  target_link_libraries(iccGetValuesNStartTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccGetValuesNStartTest)

  add_test(
    NAME iccdev.getvalues-nstart-bound
    COMMAND "$<TARGET_FILE:iccGetValuesNStartTest>"
  )
  set_tests_properties(iccdev.getvalues-nstart-bound PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;tag;issue-1553;regression"
  )
  if(WIN32)
    set(_getvalues_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccGetValuesNStartTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _getvalues_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.getvalues-nstart-bound PROPERTIES
      ENVIRONMENT_MODIFICATION "${_getvalues_windows_env_mods}"
    )
  endif()
endfunction()

# #2000: CIccTagCicp's copy constructor had an empty body and a commented-out
# parameter (/* ITXYZ */, copy-paste residue from CIccTagXYZ), so the four
# icUInt8Number members -- which have no in-class initialiser -- were left
# uninitialized on every copy (CWE-457). NewCopy() *is* that constructor, and
# CIccProfile's copy constructor clones each tag through NewCopy(), which is the
# path CIccApplyBPC takes for intents 40/41/42. Pins all three levels. The
# fixture values are non-zero and distinct because the pre-fix reads differ per
# level -- measured 0/0/0/0 for the stack copy and 0xBE (ASan's malloc fill) for
# both heap copies -- so a zero fixture would leave the stack assertion toothless.
# Header + IccProfLib only.
function(iccdev_add_cicp_copy_ctor_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCicpCopyCtorTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/cicp-copy-ctor-uninitialized.cpp"
  )
  target_link_libraries(iccCicpCopyCtorTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCicpCopyCtorTest)

  add_test(
    NAME iccdev.cicp-copy-ctor-uninitialized
    COMMAND "$<TARGET_FILE:iccCicpCopyCtorTest>"
  )
  set_tests_properties(iccdev.cicp-copy-ctor-uninitialized PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;tag;cicp;security;issue-2000;regression"
  )
  if(WIN32)
    set(_cicp_copy_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCicpCopyCtorTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _cicp_copy_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.cicp-copy-ctor-uninitialized PROPERTIES
      ENVIRONMENT_MODIFICATION "${_cicp_copy_windows_env_mods}"
    )
  endif()
endfunction()

# #2001: iccPawgReport check C5 warned on every profile carrying a cicpTag,
# because icSigCicpTag was missing from kCommonOptional. The tag is recognised by
# IsSpecTag() (CIccTagCreator resolves 'cicp' to "cicpTag"), so it is never
# bucketed as a private tag and falls straight through IsAllowedForClass() to the
# warning. Compiles PawgReport.cpp into the test the same way the #1775
# compression test does, then drives DumpPawgReport and reads the C5 line back:
# C5's verdict is not exposed through PawgReport.h, and the emitted text is what
# the issue was reported against. A/B against one base profile -- the tracked
# plain-SDR sRGB v4 preference profile as the control, and the same
# profile plus a BT.709 cicpTag as the subject -- which is also what shows the
# defect is not HDR-specific. Needs IccProfLib only.
function(iccdev_add_pawg_c5_cicp_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPawgC5CicpTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pawg-c5-cicp-optional.cpp"
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport/PawgReport.cpp"
  )
  target_compile_features(iccPawgC5CicpTest PRIVATE cxx_std_17)
  target_include_directories(iccPawgC5CicpTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport"
  )
  target_link_libraries(iccPawgC5CicpTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPawgC5CicpTest)

  add_test(
    NAME iccdev.pawg-c5-cicp-optional
    COMMAND "$<TARGET_FILE:iccPawgC5CicpTest>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )
  # Runs in the ctest output dir because it writes the subject profile and the
  # two captured reports beside itself.
  set_tests_properties(iccdev.pawg-c5-cicp-optional PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;pawg;cicp;issue-2001;regression"
  )
  if(WIN32)
    set(_pawg_c5_cicp_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPawgC5CicpTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pawg_c5_cicp_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pawg-c5-cicp-optional PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pawg_c5_cicp_windows_env_mods}"
    )
  endif()
endfunction()

# #1976: two defects on the gamt path. CIccXform::Create() forces bInput false to walk
# the B-to-A shaped gamut tag, but m_bInput also drove GetDstSpace()/GetNumDstSamples(),
# so the xform advertised the profile's device space while CIccCmm::AddXform() had
# recorded PCS -> icSigGamutData; Begin() rejected every gamt-bearing profile in Testing/
# with icCmmStatBadSpaceLink. Separately CIccCmm::IsInGamut() applied an 8-bit 1/255
# threshold to a tag that may be lut16Type, so 16-bit out-of-gamut codes below 258 read
# as in gamut. Pins the corrected connection metadata and the exact-zero classification;
# builds its gamt profile in memory because the corpus gamt profiles are generated from
# XML rather than tracked. Header + IccProfLib only.
function(iccdev_add_gamut_xform_semantics_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccGamutXformSemanticsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/gamut-xform-semantics.cpp"
  )
  target_link_libraries(iccGamutXformSemanticsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccGamutXformSemanticsTest)

  add_test(
    NAME iccdev.gamut-xform-semantics
    COMMAND "$<TARGET_FILE:iccGamutXformSemanticsTest>"
  )
  set_tests_properties(iccdev.gamut-xform-semantics PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;cmm;gamut;issue-1976;regression"
  )
  if(WIN32)
    set(_gamut_xform_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccGamutXformSemanticsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _gamut_xform_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.gamut-xform-semantics PROPERTIES
      ENVIRONMENT_MODIFICATION "${_gamut_xform_windows_env_mods}"
    )
  endif()
endfunction()

# #1853: CIccMatrixMath::rangeMap() discarded SetRange()'s verdict and returned a matrix
# whose coefficients were never written, so a rejected spectral range pair fed
# uninitialized heap into the colour math (CWE-908). Pins the corrected NULL return, the
# pFailed overload that keeps callers able to tell "no matrix needed" from "no matrix
# possible", and the observer row-layout guard in CIccTagSpectralViewingConditions.
# Header + IccProfLib only.
function(iccdev_add_rangemap_uninitialized_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccRangeMapUninitializedTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/rangemap-uninitialized.cpp"
  )
  target_link_libraries(iccRangeMapUninitializedTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccRangeMapUninitializedTest)

  add_test(
    NAME iccdev.rangemap-uninitialized
    COMMAND "$<TARGET_FILE:iccRangeMapUninitializedTest>"
  )
  set_tests_properties(iccdev.rangemap-uninitialized PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;spectral;issue-1853;regression"
  )
  if(WIN32)
    set(_rangemap_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccRangeMapUninitializedTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _rangemap_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.rangemap-uninitialized PROPERTIES
      ENVIRONMENT_MODIFICATION "${_rangemap_windows_env_mods}"
    )
  endif()
endfunction()

# #1677: the bi-spectral PCS connection steps treated CIccPcsXform::rangeMap()'s NULL as
# "the ranges are identical" when it also means "a conversion was needed but SetRange()
# refused it". pushBiRef2Rad() then copied illuminantRange.steps floats into a buffer sized
# to biSpectralRange.steps -- a 324-byte write into a 1-byte region for the reported
# reproducer (CWE-787), reached from CIccCmm::Begin() before any pixel is applied.
# pushBiRef2Ref() has the mirror fault and overruns the illuminant on read instead.
#
# Also pins the controls: only one bi-spectral profile exists in Testing/ and it is not
# usable as a colour xform, so nothing else in the suite would notice a guard here that
# rejected conformant spectral profiles. Header + IccProfLib only, no fixture.
function(iccdev_add_pcs_birefl_illuminant_range_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPcsBiReflIlluminantRangeTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcs-birefl-illuminant-range.cpp"
  )
  target_link_libraries(iccPcsBiReflIlluminantRangeTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPcsBiReflIlluminantRangeTest)

  add_test(
    NAME iccdev.pcs-birefl-illuminant-range
    COMMAND "$<TARGET_FILE:iccPcsBiReflIlluminantRangeTest>"
  )
  set_tests_properties(iccdev.pcs-birefl-illuminant-range PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;spectral;issue-1677;regression"
  )
  if(WIN32)
    set(_pcs_birefl_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPcsBiReflIlluminantRangeTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcs_birefl_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcs-birefl-illuminant-range PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcs_birefl_windows_env_mods}"
    )
  endif()
endfunction()

# #1853 (ci-qa-flags bucket 4): the PCS step chain could be left disagreeing with itself,
# and the temporaries it runs through were sized assuming it never is.
#
# pushApplyIllum() held the last two of the six CIccPcsXform::rangeMap() call sites still
# reading the ambiguous NULL as "no conversion needed" when, inside the else of an
# icSameSpectralRange() test, it can only mean SetRange() refused the pair. It dropped the
# step and returned icCmmStatOk, leaving a scale running at illuminantRange.steps over a
# spectralRange.steps-wide vector -- and when the dropped step is the return leg, that
# width is what the caller reads back.
#
# CIccPcsXform::MaxChannels() then failed to contain it: it took GetDstChannels() from the
# first step only, so no later step had the width it writes into m_temp1/m_temp2 counted.
#
# Header + IccProfLib only, no fixture. Also pins the controls -- no spectral profile in
# Testing/ exercises a refused illuminant map, and none would notice a MaxChannels() that
# started answering smaller.
function(iccdev_add_pcs_applyillum_chain_sizing_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPcsApplyIllumChainSizingTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcs-applyillum-chain-sizing.cpp"
  )
  target_link_libraries(iccPcsApplyIllumChainSizingTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPcsApplyIllumChainSizingTest)

  add_test(
    NAME iccdev.pcs-applyillum-chain-sizing
    COMMAND "$<TARGET_FILE:iccPcsApplyIllumChainSizingTest>"
  )
  set_tests_properties(iccdev.pcs-applyillum-chain-sizing PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;spectral;issue-1853;regression"
  )
  if(WIN32)
    set(_pcs_applyillum_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPcsApplyIllumChainSizingTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcs_applyillum_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcs-applyillum-chain-sizing PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcs_applyillum_windows_env_mods}"
    )
  endif()
endfunction()

# #1932: CIccPcsXform::Connect() sized the spectral PCS steps it pushes from the header's
# spectralRange while the pixel buffers those steps run in are sized from the channel
# count in the spectralPCS signature, and never checked that the two agreed. A header
# declaring "rs" + 36 channels alongside spectralRange.steps == 184 produced a 3x184
# observer matrix reading a 36-float pixel (CWE-125 heap over-read in
# CIccSparseMatrix::MultiplyVector). CIccProfile::Validate() already calls that a critical
# error, but nothing on the apply path calls Validate().
#
# Drives Connect() directly through stub xforms, so the assertions are on returned status
# and need no sanitizer. Pins the deliberate sparse-matrix exclusion and the conformant
# shapes too: the check fronts every spectral branch, so a guard that rejected too much
# would break spectral colour management wholesale. Header + IccProfLib only, no fixture.
function(iccdev_add_spectral_pcs_range_consistency_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccSpectralPcsRangeConsistencyTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/spectral-pcs-range-consistency.cpp"
  )
  target_link_libraries(iccSpectralPcsRangeConsistencyTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccSpectralPcsRangeConsistencyTest)

  add_test(
    NAME iccdev.spectral-pcs-range-consistency
    COMMAND "$<TARGET_FILE:iccSpectralPcsRangeConsistencyTest>"
  )
  set_tests_properties(iccdev.spectral-pcs-range-consistency PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;spectral;issue-1932;regression"
  )
  if(WIN32)
    set(_spectral_pcs_range_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccSpectralPcsRangeConsistencyTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _spectral_pcs_range_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.spectral-pcs-range-consistency PROPERTIES
      ENVIRONMENT_MODIFICATION "${_spectral_pcs_range_windows_env_mods}"
    )
  endif()
endfunction()

# #1853: CIccProfile::calcMediaWhiteXYZ() read the spectral white point tag with
# GetValues()'s default nVectorSize of 1, so all but the first sample of the white vector
# were uninitialized heap by the time they reached the returned XYZ. Builds its profiles
# in memory and checks the XYZ against a closed-form expectation, which is what
# iccdev.rangemap-uninitialized cannot see from the matrix helper alone.
#
# No FIXTURES_REQUIRED and no Testing/ paths on purpose: the generated PCC profiles this
# first used are written into the source tree by iccdev.create-profiles but into the build
# tree by iccdev.windows-create-profiles, so a path-based test satisfied the fixture and
# still found nothing on Windows. Header + IccProfLib only.
function(iccdev_add_spectral_media_white_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccSpectralMediaWhiteTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/spectral-media-white.cpp"
  )
  target_link_libraries(iccSpectralMediaWhiteTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccSpectralMediaWhiteTest)

  add_test(
    NAME iccdev.spectral-media-white
    COMMAND "$<TARGET_FILE:iccSpectralMediaWhiteTest>"
  )
  set_tests_properties(iccdev.spectral-media-white PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;spectral;pcc;issue-1853;regression"
  )
  if(WIN32)
    set(_spectralwhite_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccSpectralMediaWhiteTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _spectralwhite_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.spectral-media-white PROPERTIES
      ENVIRONMENT_MODIFICATION "${_spectralwhite_windows_env_mods}"
    )
  endif()
endfunction()

# #1581: CIccTagGamutBoundaryDesc::Read must reject a gbd tag with 0 PCS channels,
# which otherwise collapses the vertex-count size bound and lets a tiny tag declare a
# near-2^31 vertex count (CWE-400 hang in Describe()). The test exercises Read() over a
# crafted CIccMemIO payload and never calls Describe(), so it is fast either way.
function(iccdev_add_gbd_zero_pcs_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccGbdZeroPcsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/gbd-zero-pcs-channels.cpp"
  )
  target_link_libraries(iccGbdZeroPcsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccGbdZeroPcsTest)

  add_test(
    NAME iccdev.gbd-zero-pcs-channels
    COMMAND "$<TARGET_FILE:iccGbdZeroPcsTest>"
  )
  set_tests_properties(iccdev.gbd-zero-pcs-channels PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;security;tag;issue-1581;regression"
  )
  if(WIN32)
    set(_gbd_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccGbdZeroPcsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _gbd_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.gbd-zero-pcs-channels PROPERTIES
      ENVIRONMENT_MODIFICATION "${_gbd_windows_env_mods}"
    )
  endif()
endfunction()

# #1853: getReflectanceObserver() scaled the combined observer/range-map matrix by the
# illuminant instead of the observer matrix. The combined matrix is rangeRef.steps wide
# while the illuminant SPD holds illumRange.steps samples, so a reflectance range finer
# than the illuminant read past the end of the SPD, and every mapped range weighted the
# reflectance axis rather than the wavelength axis. Mult() also returns a new matrix
# without consuming its operands, so the observer matrix leaked. The test pins the
# physical invariant -- resampling a flat reflectance over the same span must not move
# the white point -- which fails on value comparison alone, while the sanitizer legs
# additionally catch the overflow and the leak. Links IccProfLib only.
function(iccdev_add_pcc_reflectance_observer_range_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPccReflectanceObserverRangeTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcc-reflectance-observer-range.cpp"
  )
  target_link_libraries(iccPccReflectanceObserverRangeTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPccReflectanceObserverRangeTest)

  add_test(
    NAME iccdev.pcc-reflectance-observer-range
    COMMAND "$<TARGET_FILE:iccPccReflectanceObserverRangeTest>"
  )
  set_tests_properties(iccdev.pcc-reflectance-observer-range PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;pcc;spectral;issue-1853;regression"
  )
  if(WIN32)
    set(_pcc_refl_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPccReflectanceObserverRangeTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcc_refl_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcc-reflectance-observer-range PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcc_refl_windows_env_mods}"
    )
  endif()
endfunction()

# #1809: a multiProcessElementType tag with zero processing elements and equal input and
# output channel counts is the identity transform -- Read()/Write()/Begin()/Apply() all
# treat it as a defined state, and Apply() memcpy()s source to destination. Validate()
# alone reported it at warning level, putting 79 reference profiles (148 messages, mostly
# the PCC, ICS and mcs fixtures that author DToB3/BToD3 as identity on purpose) into the
# warning cohort. The test pins the information-level reporting, the unchanged critical
# error for unequal channel counts, and the runtime behaviour that justifies the split.
function(iccdev_add_mpe_empty_identity_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccMpeEmptyIdentityTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/mpe-empty-identity.cpp"
  )
  target_link_libraries(iccMpeEmptyIdentityTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccMpeEmptyIdentityTest)

  add_test(
    NAME iccdev.mpe-empty-identity
    COMMAND "$<TARGET_FILE:iccMpeEmptyIdentityTest>"
  )
  set_tests_properties(iccdev.mpe-empty-identity PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;validation;tag;issue-1809;regression"
  )
  if(WIN32)
    set(_mpe_empty_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccMpeEmptyIdentityTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _mpe_empty_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.mpe-empty-identity PROPERTIES
      ENVIRONMENT_MODIFICATION "${_mpe_empty_windows_env_mods}"
    )
  endif()
endfunction()

# #1748: CIccPcsXform::ConnectFirst()/ConnectLast() build the PCS transforms at the two
# outer edges of a CMM chain, and CheckPCSConnections() pushes them onto the xform list
# like any other xform. Unlike Connect(), they left m_srcSpace/m_nSrcSamples/m_dstSpace/
# m_nDstSamples at the CIccXform defaults, so the guard in CIccCmm::Begin() that compares
# the last xform's output count against GetDestSamples() read 0 and rejected the chain
# with icCmmStatBadSpaceLink. The test pins the four fields on both entry points and then
# drives whole CMMs whose PCS edge space differs from the fixture's own PCS, checking that
# the XYZ route agrees numerically with the Lab route. It uses a tracked fixture and no
# LUT interpolation of any size, so it is fast.
function(iccdev_add_pcs_edge_metadata_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPcsEdgeMetadataTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcs-edge-metadata.cpp"
  )
  target_link_libraries(iccPcsEdgeMetadataTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPcsEdgeMetadataTest)

  add_test(
    NAME iccdev.pcs-edge-metadata
    COMMAND "$<TARGET_FILE:iccPcsEdgeMetadataTest>"
  )
  set_tests_properties(iccdev.pcs-edge-metadata PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;cmm;pcs;issue-1748;regression"
  )
  if(WIN32)
    set(_pcs_edge_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPcsEdgeMetadataTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcs_edge_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcs-edge-metadata PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcs_edge_windows_env_mods}"
    )
  endif()
endfunction()

# #1808: a negative Z in an XYZNumber is legal (it arises from chromatic adaptation,
# e.g. the custom-observer -> D50 PCC in iccV5DspObsToV4Dsp) and must validate as a
# Warning, while a negative X or Y stays NonCompliant. The test drives
# CIccInfo::CheckData directly over both the fixed-point and float overloads, so it
# needs no profile fixture and pins the asymmetry in both directions.
function(iccdev_add_xyz_negative_z_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccXyzNegativeZTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/xyznumber-negative-z-legal.cpp"
  )
  target_link_libraries(iccXyzNegativeZTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccXyzNegativeZTest)

  add_test(
    NAME iccdev.xyznumber-negative-z-legal
    COMMAND "$<TARGET_FILE:iccXyzNegativeZTest>"
  )
  set_tests_properties(iccdev.xyznumber-negative-z-legal PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;validation;issue-1808;regression"
  )
  if(WIN32)
    set(_xyz_negz_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccXyzNegativeZTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _xyz_negz_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.xyznumber-negative-z-legal PROPERTIES
      ENVIRONMENT_MODIFICATION "${_xyz_negz_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_utf8_textarray_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccUtf8TextArrayTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/utf8-textarray-handler.cpp"
  )
  target_link_libraries(iccUtf8TextArrayTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccUtf8TextArrayTest)

  add_test(
    NAME iccdev.utf8-textarray-handler
    COMMAND "$<TARGET_FILE:iccUtf8TextArrayTest>"
  )
  set_tests_properties(iccdev.utf8-textarray-handler PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;tag;array;issue-1726;regression"
  )
  if(WIN32)
    set(_utf8_array_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccUtf8TextArrayTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _utf8_array_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.utf8-textarray-handler PROPERTIES
      ENVIRONMENT_MODIFICATION "${_utf8_array_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_extended_device_colorspace_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccExtendedDeviceColorSpaceTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/extended-device-colorspace.cpp"
  )
  target_link_libraries(iccExtendedDeviceColorSpaceTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccExtendedDeviceColorSpaceTest)

  add_test(
    NAME iccdev.extended-device-colorspace
    COMMAND "$<TARGET_FILE:iccExtendedDeviceColorSpaceTest>"
  )
  set_tests_properties(iccdev.extended-device-colorspace PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;spec;tag;issue-626;regression"
  )
  if(WIN32)
    set(_extdevcs_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccExtendedDeviceColorSpaceTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _extdevcs_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.extended-device-colorspace PROPERTIES
      ENVIRONMENT_MODIFICATION "${_extdevcs_windows_env_mods}"
    )
  endif()
endfunction()

# #1843: the round-trip and PRMG evaluators accept only the four device classes that
# carry a device<->PCS transform pair, and used to refuse every other class with
# icCmmStatInvalidProfile -- so iccRoundTrip called a cleanly-validating abstract or
# namedColor profile "Invalid profile", and no caller could tell a damaged profile from
# an inapplicable one. Both now return icCmmStatUnsupportedProfileClass. The test drives
# the CIccProfile* overloads directly, which needs no fixture and is the only way to
# reach CIccPRMG's copy of the class test (every tool runs it after the round-trip
# evaluation has already succeeded). Header + IccProfLib only.
function(iccdev_add_unsupported_profile_class_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccUnsupportedProfileClassTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/unsupported-profile-class-status.cpp"
  )
  target_link_libraries(iccUnsupportedProfileClassTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccUnsupportedProfileClassTest)

  add_test(
    NAME iccdev.unsupported-profile-class-status
    COMMAND "$<TARGET_FILE:iccUnsupportedProfileClassTest>"
  )
  set_tests_properties(iccdev.unsupported-profile-class-status PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;cmm;status;issue-1843;regression"
  )
  if(WIN32)
    set(_unsupclass_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccUnsupportedProfileClassTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _unsupclass_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.unsupported-profile-class-status PROPERTIES
      ENVIRONMENT_MODIFICATION "${_unsupclass_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_dpcc_pcc_replacement_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccDpccPccReplacementTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/dpcc-pcc-replacement.cpp"
  )
  target_link_libraries(iccDpccPccReplacementTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccDpccPccReplacementTest)

  add_test(
    NAME iccdev.dpcc-pcc-replacement
    COMMAND "$<TARGET_FILE:iccDpccPccReplacementTest>"
  )
  set_tests_properties(iccdev.dpcc-pcc-replacement PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;spec;cmm;issue-1559;regression"
  )
  if(WIN32)
    set(_dpcc_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccDpccPccReplacementTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _dpcc_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.dpcc-pcc-replacement PROPERTIES
      ENVIRONMENT_MODIFICATION "${_dpcc_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the two scalar visualization metrics in the iccviz engine
# (Tools/CmdLine/IccProfilePlot/IccVizModel): GamutVolume and RoundTripDE (#1712).
# Both build CIccXform objects through CIccXform::Create(bOwnsProfile=false) +
# ShareProfile(), the borrowed-profile ownership path touched by the review, so
# driving them end-to-end on the stock sRGB v4 LUT profile guards the metric numbers
# (positive 3-D gamut volume, not degenerate; small in-gamut round-trip dE) and keeps
# that construction path under test.  The metrics live in tool sources, not
# IccProfLib, so the test compiles IccVizModel.cpp directly and pulls the shared
# engine headers from the IccProfilePlot dir.
function(iccdev_add_iccviz_metrics_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccVizMetricsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccviz-gamut-roundtrip-metrics.cpp"
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccProfilePlot/IccVizModel.cpp"
  )
  target_compile_features(iccVizMetricsTest PRIVATE cxx_std_17)
  target_include_directories(iccVizMetricsTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccProfilePlot"
  )
  target_link_libraries(iccVizMetricsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccVizMetricsTest)

  add_test(
    NAME iccdev.iccviz-gamut-roundtrip-metrics
    COMMAND "$<TARGET_FILE:iccVizMetricsTest>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )
  set_tests_properties(iccdev.iccviz-gamut-roundtrip-metrics PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 120
    LABELS "iccdev;iccprofileplot;viz;gamut;roundtrip;issue-1712;regression"
  )
  if(WIN32)
    set(_iccviz_metrics_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccVizMetricsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccviz_metrics_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccviz-gamut-roundtrip-metrics PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccviz_metrics_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the gamut-degeneracy heuristic behind GamutVolumeResult.degenerate (#1712):
# iccvizmath::principalStdDevs() (closed-form symmetric-3x3 eigenvalues) plus the
# "flat = s3 < 0.02*s1" coplanarity predicate that flags a boundary cloud collapsed
# to a plane/line, which the morphological-closing volume would otherwise report as
# a non-zero sheet/tube artifact.  A header-only unit test over synthetic blob/plane/
# line clouds; it calibrates the 0.02 constant (ratio 0.01 must flag, 0.05 must not).
# principalStdDevs is inline in IccVizMath.hpp, so this links IccProfLib only for the
# ICC scalar types the header pulls in via IccDefs.h.
function(iccdev_add_iccviz_degenerate_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccVizDegenerateTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccviz-degenerate-detection.cpp"
  )
  target_compile_features(iccVizDegenerateTest PRIVATE cxx_std_17)
  target_include_directories(iccVizDegenerateTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccProfilePlot"
  )
  target_link_libraries(iccVizDegenerateTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccVizDegenerateTest)

  add_test(
    NAME iccdev.iccviz-degenerate-detection
    COMMAND "$<TARGET_FILE:iccVizDegenerateTest>"
  )
  set_tests_properties(iccdev.iccviz-degenerate-detection PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofileplot;viz;gamut;degenerate;issue-1712;regression"
  )
  if(WIN32)
    set(_iccviz_degen_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccVizDegenerateTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccviz_degen_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccviz-degenerate-detection PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccviz_degen_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the DrawAxisPDF axis-label parameterization in iccProfileVisualizePlot
# (#1712): the #1711 sync parameterized the PDF axis drawing (a drawIdentity toggle
# plus custom start/mid/end tick labels), and the review asserted the Input/Output
# plots stayed byte-identical.  The driver runs the tool on the stock sRGB v4 profile
# and asserts the emitted LUT PDF carries both axis-label paths: the default identity
# axis (Input/Output) and the parameterized custom-tick axis (L* (100 to 0) / % ink).
# It is a content pin, not a byte-for-byte golden PDF, so a legitimate LUT
# curve-sampling change across IccProfLib versions will not false-fail it.  Drives the
# real tool binary (the change lives in tool sources), so it is gated on the target.
function(iccdev_add_iccviz_pdf_axis_test)
  if(NOT TARGET iccProfileVisualizePlot)
    return()
  endif()
  add_dependencies(check iccProfileVisualizePlot)

  add_test(
    NAME iccdev.iccviz-pdf-axis-labels
    COMMAND "${CMAKE_COMMAND}"
      "-DVIZ_TOOL=$<TARGET_FILE:iccProfileVisualizePlot>"
      "-DPROFILE=${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
      "-DWORKDIR=${ICCDEV_TEST_OUTDIR}/iccviz-pdf-axis"
      -P "${CMAKE_CURRENT_LIST_DIR}/RunIccVizPdfAxisTest.cmake"
  )
  set_tests_properties(iccdev.iccviz-pdf-axis-labels PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 120
    LABELS "iccdev;iccprofileplot;viz;pdf;axis;issue-1712;regression"
  )
  if(WIN32)
    set(_iccviz_pdf_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfileVisualizePlot>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccviz_pdf_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccviz-pdf-axis-labels PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccviz_pdf_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the IccVizModel CLUT->raster lattice walk against heap-buffer-overflow
# reads (#1548): buildClutRaster() derived its tile-packing geometry
# (tiles/tileWidth/tileHeight strides) from the grid arrangement, NOT from the
# CLUT's actual sample array, so a malformed / non-square CLUT drove the computed
# input index past the end of clut->GetData() -- a 4-byte heap OOB read ASan
# trapped both while Enumerate() probed the tag and on an explicit `raster`
# render.  The test renders the A2B0 CLUT of a minimized fuzz profile through the
# built iccProfilePlot CLI; the read is now bounded by the true element count, so
# it exits 0, whereas the pre-fix code aborts under ASan.  This drives the real
# tool binary (the bug lives in tool sources, not IccProfLib), so it is most
# meaningful under the ASAN+UBSAN tool job but also a plain smoke test elsewhere.
function(iccdev_add_iccprofileplot_clut_oob_test)
  if(NOT TARGET iccProfilePlot)
    return()
  endif()
  add_dependencies(check iccProfilePlot)

  # Invoke the tool directly (no ICCDEV_TEST_ENV wrapper): ASan's default
  # abort-on-error already yields a non-zero exit on the OOB read, which is the
  # failure signal ctest needs; on a non-sanitizer build the bounded walk simply
  # renders and exits 0.
  add_test(
    NAME iccdev.iccprofileplot-clut-oob
    COMMAND "$<TARGET_FILE:iccProfilePlot>"
      "${ICCDEV_REPO_ROOT}/.github/ci/test-data/fuzz-clut-hbo-69197729.icc"
      raster clut:A2B0
  )
  set_tests_properties(iccdev.iccprofileplot-clut-oob PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;iccprofileplot;viz;clut;oob;issue-1548;regression"
  )
  if(WIN32)
    set(_clut_oob_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfilePlot>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _clut_oob_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccprofileplot-clut-oob PROPERTIES
      ENVIRONMENT_MODIFICATION "${_clut_oob_windows_env_mods}"
    )
  endif()
endfunction()

# Guards PDFWriter object ownership in the IccProfilePlot report writer (#1547):
# PDFWriter::CloseFile() cleared its owning m_objects vector on the success path
# (and early-returned on the write-failure path) BEFORE the unified delete loop,
# leaking every PDFObject -- including the per-page PDFPage allocations from
# AddPage() (the 3456-byte / 36-object direct leak in the CI report).  The test
# runs the built iccProfileVisualizePlot CLI over the stock sRGB v4 profile, which
# emits a multi-page LUT PDF and so exercises AddPage(), under forced
# LeakSanitizer; with cleanup routed through the single delete loop it exits 0,
# whereas the pre-fix writer leaks (non-zero exit).
function(iccdev_add_iccprofileplot_pdf_leak_test)
  if(NOT TARGET iccProfileVisualizePlot)
    return()
  endif()
  if(MSVC)
    message(STATUS
      "Skipping iccdev.iccprofileplot-pdf-leak: MSVC AddressSanitizer "
      "does not support LeakSanitizer detect_leaks=1")
    return()
  endif()
  add_dependencies(check iccProfileVisualizePlot)

  # The pass/fail signal IS the LeakSanitizer at-exit scan, but the CI tool job
  # and ICCDEV_TEST_ENV export ASAN_OPTIONS=...detect_leaks=0 to silence unrelated
  # tool leaks.  Inheriting that would make this a false pass, so launch through
  # `cmake -E env ASAN_OPTIONS=detect_leaks=1` to force leak detection on for this
  # process only (same mechanism as the cmmsearch/xform ownership tests).  The
  # tool writes its PDF/TIFF beside the CWD, so run inside the ctest output dir.
  add_test(
    NAME iccdev.iccprofileplot-pdf-leak
    COMMAND "${CMAKE_COMMAND}" -E env
      "ASAN_OPTIONS=detect_leaks=1"
      "$<TARGET_FILE:iccProfileVisualizePlot>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )
  set_tests_properties(iccdev.iccprofileplot-pdf-leak PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;iccprofileplot;viz;pdf;leak;issue-1547;regression"
  )
  if(WIN32)
    set(_pdf_leak_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfileVisualizePlot>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pdf_leak_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccprofileplot-pdf-leak PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pdf_leak_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the process exit codes of the IccProfilePlot tools (#1550 QA findings):
# both tools previously returned 0 even when the work failed, so scripts and CI
# would treat a bad run as success.  Two WILL_FAIL tests assert the deliberate
# nonzero exit:
#   * Finding 1 -- iccProfileVisualizePlot on a non-ICC input must exit nonzero
#     (it parses nothing and produces no output).  We feed it the tool's own
#     Readme.md, a guaranteed-present file that is clearly not a profile.
#   * Finding 2 -- iccProfilePlot raster with an unwritable raw-output path must
#     exit nonzero.  We point the optional out.raw at the ctest output DIRECTORY:
#     icOpenRegularWriteBinaryFile rejects it (not a regular file), reproducing
#     the "could not open output file" path portably, without an OS-specific
#     "/no/such/dir".  The render itself still succeeds, isolating the exit-code
#     contract.  WILL_FAIL passes iff the process returns nonzero.
function(iccdev_add_iccprofileplot_exitcode_test)
  if(TARGET iccProfileVisualizePlot)
    add_dependencies(check iccProfileVisualizePlot)
    add_test(
      NAME iccdev.iccprofileplot-viz-badinput-exitcode
      COMMAND "$<TARGET_FILE:iccProfileVisualizePlot>"
        "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccProfilePlot/Readme.md"
    )
    set_tests_properties(iccdev.iccprofileplot-viz-badinput-exitcode PROPERTIES
      WILL_FAIL TRUE
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 60
      LABELS "iccdev;iccprofileplot;viz;exitcode;issue-1550;regression"
    )
  endif()

  if(TARGET iccProfilePlot)
    add_dependencies(check iccProfilePlot)
    add_test(
      NAME iccdev.iccprofileplot-raster-badoutput-exitcode
      COMMAND "$<TARGET_FILE:iccProfilePlot>"
        "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
        raster clut:A2B0 "${ICCDEV_TEST_OUTDIR}"
    )
    set_tests_properties(iccdev.iccprofileplot-raster-badoutput-exitcode PROPERTIES
      WILL_FAIL TRUE
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 60
      LABELS "iccdev;iccprofileplot;raster;exitcode;issue-1550;regression"
    )
  endif()

  if(WIN32)
    set(_exitcode_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _exitcode_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    if(TARGET iccProfileVisualizePlot)
      set_tests_properties(iccdev.iccprofileplot-viz-badinput-exitcode PROPERTIES
        ENVIRONMENT_MODIFICATION
          "${_exitcode_windows_env_mods};PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfileVisualizePlot>"
      )
    endif()
    if(TARGET iccProfilePlot)
      set_tests_properties(iccdev.iccprofileplot-raster-badoutput-exitcode PROPERTIES
        ENVIRONMENT_MODIFICATION
          "${_exitcode_windows_env_mods};PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfilePlot>"
      )
    endif()
  endif()
endfunction()

function(iccdev_add_applytolink_invalid_arg_test)
  if(NOT TARGET iccApplyToLink)
    return()
  endif()

  add_dependencies(check iccApplyToLink)
  add_dependencies(check-fast iccApplyToLink)

  add_test(
    NAME iccdev.applytolink-invalid-decoded-intent
    COMMAND "$<TARGET_FILE:iccApplyToLink>"
      "${ICCDEV_TEST_OUTDIR}/applytolink-invalid-intent.icc"
      0 2 1 InvalidIntent 0 1 0 0
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc" 9
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc" 0
  )
  set_tests_properties(iccdev.applytolink-invalid-decoded-intent PROPERTIES
    WILL_FAIL TRUE
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;tools;applytolink;cli;exitcode;regression"
    FIXTURES_REQUIRED iccdev_profiles
  )

  if(WIN32)
    set(_applytolink_invalid_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccApplyToLink>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _applytolink_invalid_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(
      iccdev.applytolink-invalid-decoded-intent
      PROPERTIES
        ENVIRONMENT_MODIFICATION "${_applytolink_invalid_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_applytolink_v4_missing_desc_test)
  if(NOT TARGET iccApplyToLink)
    return()
  endif()

  add_dependencies(check iccApplyToLink)
  add_dependencies(check-fast iccApplyToLink)

  add_test(
    NAME iccdev.applytolink-v4-missing-device-descriptions
    COMMAND "$<TARGET_FILE:iccApplyToLink>"
      "${ICCDEV_TEST_OUTDIR}/applytolink-v4-missing-device-descriptions.icc"
      0 2 0 MissingDeviceDescriptions 0 1 1 0
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc" 12
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc" 12
  )
  set_tests_properties(iccdev.applytolink-v4-missing-device-descriptions PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;tools;applytolink;pseq;regression"
    FIXTURES_REQUIRED iccdev_profiles
  )

  if(WIN32)
    set(_applytolink_v4_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccApplyToLink>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _applytolink_v4_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.applytolink-v4-missing-device-descriptions PROPERTIES
      ENVIRONMENT_MODIFICATION "${_applytolink_v4_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_fileio_getlength_position_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccFileIoGetLengthPositionTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/fileio-getlength-position.cpp"
  )
  target_link_libraries(iccFileIoGetLengthPositionTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccFileIoGetLengthPositionTest)

  add_test(
    NAME iccdev.fileio-getlength-preserves-position
    COMMAND
      "$<TARGET_FILE:iccFileIoGetLengthPositionTest>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
      "${ICCDEV_TEST_OUTDIR}/fileio-getlength-roundtrip.icc"
  )
  set_tests_properties(iccdev.fileio-getlength-preserves-position PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;fileio;issue-987;regression"
  )
  if(WIN32)
    set(_fileio_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccFileIoGetLengthPositionTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _fileio_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.fileio-getlength-preserves-position PROPERTIES
      ENVIRONMENT_MODIFICATION "${_fileio_windows_env_mods}"
    )
  else()
    set_tests_properties(iccdev.fileio-getlength-preserves-position PROPERTIES
      ENVIRONMENT "${ICCDEV_TEST_ENV}"
    )
  endif()
endfunction()

function(iccdev_add_fileio_seek_tell_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccFileIoSeekTellTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/fileio-seek-tell.cpp"
  )
  target_link_libraries(iccFileIoSeekTellTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccFileIoSeekTellTest)

  add_test(
    NAME iccdev.fileio-seek-tell
    COMMAND
      "$<TARGET_FILE:iccFileIoSeekTellTest>"
  )
  set_tests_properties(iccdev.fileio-seek-tell PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;fileio;regression"
  )
  if(WIN32)
    set(_fileio_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccFileIoSeekTellTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _fileio_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.fileio-seek-tell PROPERTIES
      ENVIRONMENT_MODIFICATION "${_fileio_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the strict-aliasing fix in CIccIO::Write16/Write32/Write64 (#1948).
# Those writers used to load the caller's buffer through the wide integer type,
# which is undefined behaviour when the buffer holds floats -- as it does on
# every WriteFloat32Float() call. Under -flto (ENABLE_LTO defaults to ON) clang
# at -O3 dropped the caller's store, so segmented-curve breakpoints reached the
# file as 0.0 and the resulting curve sets could not be applied. The test drives
# CIccSegmentedCurve::Write and inspects the emitted bytes. Core IccProfLib
# only, so no extra include dirs are needed.
function(iccdev_add_iccio_write_float_aliasing_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccIoWriteFloatAliasingTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccio-write-float-aliasing.cpp"
  )
  target_link_libraries(iccIoWriteFloatAliasingTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccIoWriteFloatAliasingTest)

  add_test(
    NAME iccdev.iccio-write-float-aliasing
    COMMAND
      "$<TARGET_FILE:iccIoWriteFloatAliasingTest>"
  )
  set_tests_properties(iccdev.iccio-write-float-aliasing PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;fileio;curves;issue-1948;regression"
  )
  if(WIN32)
    set(_iccio_aliasing_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccIoWriteFloatAliasingTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccio_aliasing_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccio-write-float-aliasing PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccio_aliasing_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_profile_write_failure_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccProfileWriteFailureTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/profile-write-failure.cpp"
  )
  target_link_libraries(iccProfileWriteFailureTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccProfileWriteFailureTest)

  add_test(
    NAME iccdev.profile-write-failure
    COMMAND "$<TARGET_FILE:iccProfileWriteFailureTest>"
  )
  set_tests_properties(iccdev.profile-write-failure PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;profile;write;regression"
  )
  if(WIN32)
    set(_profile_write_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfileWriteFailureTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _profile_write_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.profile-write-failure PROPERTIES
      ENVIRONMENT_MODIFICATION "${_profile_write_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_tagdata_zlib_roundtrip_test)
  # Compressed dataType read/write only works when the library is built with
  # zlib (#1530 ICC_USE_ZLIB option); skip otherwise so the deflate/inflate
  # invariants this test asserts are never checked against a no-zlib build.
  if(NOT ICC_USE_ZLIB)
    return()
  endif()
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccTagDataZlibRoundtripTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tagdata-zlib-roundtrip.cpp"
  )
  target_compile_features(iccTagDataZlibRoundtripTest PRIVATE cxx_std_17)
  # IccProfLib links ZLIB::ZLIB and defines ICC_USE_ZLIB=1 PUBLIC, so both
  # propagate to this executable; link zlib explicitly as belt-and-braces.
  target_link_libraries(iccTagDataZlibRoundtripTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  if(TARGET ZLIB::ZLIB)
    target_link_libraries(iccTagDataZlibRoundtripTest PRIVATE ZLIB::ZLIB)
  endif()
  add_dependencies(check iccTagDataZlibRoundtripTest)

  add_test(
    NAME iccdev.tagdata-zlib-roundtrip
    COMMAND "$<TARGET_FILE:iccTagDataZlibRoundtripTest>"
  )
  set_tests_properties(iccdev.tagdata-zlib-roundtrip PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;tag;zlib;compression;issue-1521;regression"
  )
  if(WIN32)
    set(_tagdata_zlib_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccTagDataZlibRoundtripTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _tagdata_zlib_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.tagdata-zlib-roundtrip PROPERTIES
      ENVIRONMENT_MODIFICATION "${_tagdata_zlib_windows_env_mods}"
    )
  endif()
endfunction()

# CIccTagZipUtf8Text::SetText() success/failure contract (#1743). SetText() used to
# return true even when AllocBuffer() failed, so a caller could not tell "text
# stored" from "nothing stored"; the copy was also bounded by (int)m_nBufSize,
# narrowing an icUInt32Number to int. Pins the invariant a true return implies a
# populated buffer, and guards the bulk copy that replaced the loop against a wrong
# length. Same zlib gating as the #1521 test above: without ICC_USE_ZLIB, SetText()
# always returns false and none of these invariants hold.
function(iccdev_add_ziputf8_settext_contract_test)
  if(NOT ICC_USE_ZLIB)
    return()
  endif()
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccZipUtf8SetTextContractTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/ziputf8-settext-contract.cpp"
  )
  target_compile_features(iccZipUtf8SetTextContractTest PRIVATE cxx_std_17)
  # IccProfLib links ZLIB::ZLIB and defines ICC_USE_ZLIB=1 PUBLIC, so both
  # propagate to this executable; link zlib explicitly as belt-and-braces.
  target_link_libraries(iccZipUtf8SetTextContractTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  if(TARGET ZLIB::ZLIB)
    target_link_libraries(iccZipUtf8SetTextContractTest PRIVATE ZLIB::ZLIB)
  endif()
  add_dependencies(check iccZipUtf8SetTextContractTest)

  add_test(
    NAME iccdev.ziputf8-settext-contract
    COMMAND "$<TARGET_FILE:iccZipUtf8SetTextContractTest>"
  )
  set_tests_properties(iccdev.ziputf8-settext-contract PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;tag;zlib;compression;issue-1743;regression"
  )
  if(WIN32)
    set(_ziputf8_settext_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccZipUtf8SetTextContractTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _ziputf8_settext_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.ziputf8-settext-contract PROPERTIES
      ENVIRONMENT_MODIFICATION "${_ziputf8_settext_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_parser_restore_calls_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccParserRestoreCallsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/parser-restore-calls.cpp"
  )
  target_compile_features(iccParserRestoreCallsTest PRIVATE cxx_std_17)
  target_link_libraries(iccParserRestoreCallsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})

  add_dependencies(check iccParserRestoreCallsTest)

  if(WIN32)
    add_test(
      NAME iccdev.parser-restore-calls
      COMMAND "$<TARGET_FILE:iccParserRestoreCallsTest>"
    )
  else()
    add_test(
      NAME iccdev.parser-restore-calls
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccParserRestoreCallsTest>"
    )
  endif()
  set_tests_properties(iccdev.parser-restore-calls PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;profile;parser;io;regression"
  )
  if(WIN32)
    set(_parser_restore_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccParserRestoreCallsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _parser_restore_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.parser-restore-calls PROPERTIES
      ENVIRONMENT_MODIFICATION "${_parser_restore_windows_env_mods}"
    )
  endif()
endfunction()

# Compressed-tag Describe() labelling (#1723 Compression Paths). Two output-only
# defects: CIccTagData::Describe() dropped the "Compressed " marker (a following
# `=` assignment overwrote it), and the no-zlib CIccTagZipUtf8Text::Describe()
# marker was misspelled "COMPESSED" with an unbalanced delimiter. Unlike the
# #1521/#1743 zlib tests this needs no deflate/inflate -- the tags are built in
# memory and only their dump strings are inspected -- so it is NOT gated on
# ICC_USE_ZLIB and runs in every configuration (the defect-2 assertion inside the
# test self-gates to no-zlib builds).
function(iccdev_add_compression_describe_labels_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCompressionDescribeLabelsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/compression-describe-labels.cpp"
  )
  target_compile_features(iccCompressionDescribeLabelsTest PRIVATE cxx_std_17)
  target_link_libraries(iccCompressionDescribeLabelsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCompressionDescribeLabelsTest)

  add_test(
    NAME iccdev.compression-describe-labels
    COMMAND "$<TARGET_FILE:iccCompressionDescribeLabelsTest>"
  )
  set_tests_properties(iccdev.compression-describe-labels PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;tag;compression;issue-1723;regression"
  )
  if(WIN32)
    set(_compression_describe_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCompressionDescribeLabelsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _compression_describe_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.compression-describe-labels PROPERTIES
      ENVIRONMENT_MODIFICATION "${_compression_describe_windows_env_mods}"
    )
  endif()
endfunction()

# Guards IccPawgReport's compression-path measurement (S14, issue #1775): the
# report classified tags by signature and never noticed a tag whose *type* was
# DEFLATE-compressed (zut8/zxml/compressed data), the gap #1774 flagged. The test
# drives PawgCompressionVerdict() -- the in-memory, output-free entry the fuzzing
# harness uses -- over hand-built minimal profile images and pins: zut8/zxml and a
# flagged 'data' tag are measured as compressed (Ok with zlib, Gap without), while
# uncompressed controls read as the empty case. Detection is build-independent, so
# it runs in every configuration; only the verdict value self-gates on
# ICC_USE_ZLIB inside the test. Links PawgReport.cpp in (the entry lives in the
# tool, not a library) plus the IccPawgReport include dir and IccProfLib.
function(iccdev_add_pawg_compression_paths_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPawgCompressionPathsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pawg-compression-paths.cpp"
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport/PawgReport.cpp"
  )
  target_compile_features(iccPawgCompressionPathsTest PRIVATE cxx_std_17)
  target_include_directories(iccPawgCompressionPathsTest PRIVATE
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccPawgReport"
  )
  target_link_libraries(iccPawgCompressionPathsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPawgCompressionPathsTest)

  add_test(
    NAME iccdev.pawg-compression-paths
    COMMAND "$<TARGET_FILE:iccPawgCompressionPathsTest>"
  )
  set_tests_properties(iccdev.pawg-compression-paths PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;pawg;compression;issue-1775;regression"
  )
  if(WIN32)
    set(_pawg_compression_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPawgCompressionPathsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pawg_compression_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pawg-compression-paths PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pawg_compression_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the heap-use-after-free in CIccTagJsonSparseMatrixArray::ToJson (#1791):
# the JSON sparse-matrix serializer called CIccSparseMatrix::Reset() without
# checking the result and then walked the matrix through m_RowStart-derived
# pointers (GetColumnsForRow/GetRowOffset/getPtr), so a corrupt sparse-matrix tag
# dereferenced wild pointers (heap-use-after-free under ASAN; garbage otherwise).
# The XML serializer already refuses such a tag; the fix mirrors its
# `!Reset() || GetNumEntries()>GetMaxEntries() || !IsValid()` guard. The test
# Read()s the fuzz PoC from memory into a CIccProfileJson and asserts the corrupt
# sparseMatrixArrayType tag is refused (absent from the output), not walked. Needs
# IccJSON (CIccProfileJson) + IccProfLib; skipped where IccJSON is not built.
function(iccdev_add_json_sparsematrix_huaf_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCJSON}")
    return()
  endif()

  iccdev_add_regression_executable(iccJsonSparseMatrixHuafTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/json-sparsematrix-huaf.cpp"
  )
  target_compile_features(iccJsonSparseMatrixHuafTest PRIVATE cxx_std_17)
  target_include_directories(iccJsonSparseMatrixHuafTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccJSON/IccLibJSON"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccJsonSparseMatrixHuafTest PRIVATE
    ${TARGET_LIB_ICCJSON} ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccJsonSparseMatrixHuafTest)

  add_test(
    NAME iccdev.json-sparsematrix-huaf
    COMMAND "$<TARGET_FILE:iccJsonSparseMatrixHuafTest>"
  )
  set_tests_properties(iccdev.json-sparsematrix-huaf PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;json;sparsematrix;issue-1791;regression"
  )
  if(WIN32)
    set(_json_huaf_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccJsonSparseMatrixHuafTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCJSON}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _json_huaf_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.json-sparsematrix-huaf PROPERTIES
      ENVIRONMENT_MODIFICATION "${_json_huaf_windows_env_mods}"
    )
  endif()
endfunction()

# Guards the XML writer's failure policy (#1779): a single tag whose serialize
# returned false made CIccProfileXml::ToXml abandon the ENTIRE document, so
# iccToXml wrote no file at all where iccToJson -- which skips the offending tag
# and keeps going -- produced one. Top-level tags and struct members are now
# skipped with an XML comment in their place; array elements and the MPE tint
# array stay strict, because an array position cannot be left empty in this
# format and the JSON writer is equally strict there. The test also pins a
# pre-existing markup defect the fix exposed: privateStruct/privateArray were
# written self-closed while still emitting a matching end tag, which is why the
# assertions parse the output back with libxml2 rather than only checking that a
# document appeared. Needs IccXML (CIccProfileXml) + IccProfLib; skipped where
# IccXML is not built.
function(iccdev_add_xml_writer_graceful_degrade_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCXML}")
    return()
  endif()

  iccdev_add_regression_executable(iccXmlWriterGracefulDegradeTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/xml-writer-graceful-degrade.cpp"
  )
  target_compile_features(iccXmlWriterGracefulDegradeTest PRIVATE cxx_std_17)
  target_include_directories(iccXmlWriterGracefulDegradeTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccXML/IccLibXML"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    ${LIBXML2_INCLUDE_DIR}
  )
  target_link_libraries(iccXmlWriterGracefulDegradeTest PRIVATE
    ${TARGET_LIB_ICCXML} ${TARGET_LIB_ICCPROFLIB} ${LIBXML2_LIBRARIES})
  add_dependencies(check iccXmlWriterGracefulDegradeTest)

  add_test(
    NAME iccdev.xml-writer-graceful-degrade
    COMMAND "$<TARGET_FILE:iccXmlWriterGracefulDegradeTest>"
  )
  set_tests_properties(iccdev.xml-writer-graceful-degrade PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;xml;writer;issue-1779;regression"
  )
  if(WIN32)
    set(_xml_degrade_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccXmlWriterGracefulDegradeTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCXML}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _xml_degrade_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.xml-writer-graceful-degrade PROPERTIES
      ENVIRONMENT_MODIFICATION "${_xml_degrade_windows_env_mods}"
    )
  endif()
endfunction()

# #1999: CIccProfileXml::LoadXml's RelaxNG validation block (iccFromXml -v) created an
# xmlRelaxNGParserCtxt, an xmlRelaxNG and an xmlRelaxNGValidCtxt and freed none
# of them on any path -- four bare `return false` exits and a fall-through, so
# the leak happened whether validation passed or failed. The same exits also
# returned without the xmlFreeDoc that LoadXml owes the document, and a sibling
# in CIccMpeXmlCalculator::ParseImport dropped the DOM of an <Import> whose root
# element is not IccCalcImport.
#
# The leak signal is the LeakSanitizer at-exit scan, so this must run with
# detect_leaks=1 -- see the `cmake -E env` note below. The test also asserts the
# accept/reject outcome at each exit, which is meaningful in non-ASan builds too.
# Needs IccXML (CIccProfileXml) + IccProfLib; skipped where IccXML is not built.
function(iccdev_add_xml_relaxng_validation_leak_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCXML}")
    return()
  endif()

  iccdev_add_regression_executable(iccXmlRelaxNGValidationLeakTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/xml-relaxng-validation-leak.cpp"
  )
  target_include_directories(iccXmlRelaxNGValidationLeakTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccXML/IccLibXML"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    ${LIBXML2_INCLUDE_DIR}
  )
  target_link_libraries(iccXmlRelaxNGValidationLeakTest PRIVATE
    ${TARGET_LIB_ICCXML} ${TARGET_LIB_ICCPROFLIB} ${LIBXML2_LIBRARIES})
  add_dependencies(check iccXmlRelaxNGValidationLeakTest)

  # The CI tool-test job and ICCDEV_TEST_ENV both export
  # ASAN_OPTIONS=...detect_leaks=0 to silence unrelated leaks in the smoke-tested
  # tools. Inheriting that here would disable LSan and make this a false pass on
  # a real leak. Force it back on for this test's own process via `cmake -E env`
  # (a CTest ENVIRONMENT property is not reliable for ASAN_OPTIONS). Set ONLY
  # detect_leaks=1: appending halt_on_error=0 makes the runtime print the leak
  # and still exit 0, which ctest would read as a pass.
  #
  # The three existing leak regressions (cmmsearch-namedcolor-ownership,
  # xform-create-tag-ownership, iccprofileplot-pdf-leak) skip themselves entirely
  # on MSVC, whose AddressSanitizer has no LeakSanitizer and would be handed an
  # option it does not implement. This test is not skipped there, because its
  # LoadXml return-value assertions pin the accept/reject behaviour at each of the
  # four RelaxNG exits and are worth running on Windows even with no leak check;
  # only the detect_leaks=1 wrapper is dropped.
  set(_relaxng_leak_args
    "${ICCDEV_TESTING_DIR}/Display/sRGB_D65_MAT.xml"
    "${ICCDEV_REPO_ROOT}/.github/ci/test-data/relaxng-accept-any.rng"
    "${ICCDEV_REPO_ROOT}/.github/ci/test-data/relaxng-reject-all.rng"
    "${ICCDEV_REPO_ROOT}/.github/ci/test-data/relaxng-malformed.rng"
    "relaxng-calc-import-badroot-main.xml"
  )
  if(MSVC)
    add_test(
      NAME iccdev.xml-relaxng-validation-leak
      COMMAND "$<TARGET_FILE:iccXmlRelaxNGValidationLeakTest>" ${_relaxng_leak_args}
    )
  else()
    add_test(
      NAME iccdev.xml-relaxng-validation-leak
      COMMAND "${CMAKE_COMMAND}" -E env
        "ASAN_OPTIONS=detect_leaks=1"
        "$<TARGET_FILE:iccXmlRelaxNGValidationLeakTest>" ${_relaxng_leak_args}
    )
  endif()
  # WORKING_DIRECTORY is the fixture directory rather than the usual output dir
  # because <Import Filename="..."> is resolved relative to the process working
  # directory, and IccXmlIsPathSafe rejects the absolute or "../" path that any
  # other choice would need.
  set_tests_properties(iccdev.xml-relaxng-validation-leak PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}/.github/ci/test-data"
    TIMEOUT 60
    LABELS "iccdev;xml;relaxng;leak;issue-1999;regression"
  )
  if(WIN32)
    set(_relaxng_leak_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccXmlRelaxNGValidationLeakTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCXML}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _relaxng_leak_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.xml-relaxng-validation-leak PROPERTIES
      ENVIRONMENT_MODIFICATION "${_relaxng_leak_windows_env_mods}"
    )
  endif()
endfunction()

# #2006: CIccTagCurve::SetSize() reports a request above its 65536-entry cap as success
# after freeing the table, and the three File=/Format="text" branches of
# CIccTagXmlCurve::ParseXml neither checked the return nor compared the resulting
# size before writing through GetData(0). A hand-authored profile with a 65537
# entry curve table crashed iccFromXml. The equivalent inline <Curve> branches
# have guarded this since #1158, so the test pins the contrast as well as the
# 65536 boundary.
function(iccdev_add_xml_curve_oversize_file_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCXML}")
    return()
  endif()

  iccdev_add_regression_executable(iccXmlCurveOversizeFileTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/xml-curve-oversize-file.cpp"
  )
  target_compile_features(iccXmlCurveOversizeFileTest PRIVATE cxx_std_17)
  target_include_directories(iccXmlCurveOversizeFileTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccXML/IccLibXML"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    ${LIBXML2_INCLUDE_DIR}
  )
  target_link_libraries(iccXmlCurveOversizeFileTest PRIVATE
    ${TARGET_LIB_ICCXML} ${TARGET_LIB_ICCPROFLIB} ${LIBXML2_LIBRARIES})
  add_dependencies(check iccXmlCurveOversizeFileTest)

  # The helper generates its own fixtures and chdir()s into this directory: the
  # File= attribute is resolved relative to the process working directory and
  # IccXmlIsPathSafe rejects absolute paths, so the table has to sit beside the
  # document. Keeping it under the build tree stops the 65537-entry table being
  # written into the source tree the way the hybrid pipeline does.
  set(_curve_oversize_scratch "${CMAKE_CURRENT_BINARY_DIR}/ctest-output/xml-curve-oversize-file")

  if(WIN32)
    add_test(
      NAME iccdev.xml-curve-oversize-file
      COMMAND "$<TARGET_FILE:iccXmlCurveOversizeFileTest>" "${_curve_oversize_scratch}"
    )
  else()
    add_test(
      NAME iccdev.xml-curve-oversize-file
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccXmlCurveOversizeFileTest>" "${_curve_oversize_scratch}"
    )
  endif()
  set_tests_properties(iccdev.xml-curve-oversize-file PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;xml;curve;setsize;regression"
  )
  if(WIN32)
    set(_curve_oversize_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccXmlCurveOversizeFileTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCXML}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _curve_oversize_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.xml-curve-oversize-file PROPERTIES
      ENVIRONMENT_MODIFICATION "${_curve_oversize_windows_env_mods}"
    )
  endif()
endfunction()

# #1886: CIccMpeXmlUnknown::ToXml formatted the Reserved attribute into `line`
# and then appended `buf`, which still held the element's type signature from the
# icGetSigStr call above it. The value was dropped and the signature was injected
# into the attribute list before the start tag was closed, so the document did
# not parse at all. The reader had never parsed Reserved either -- invisible
# while the writer could not emit it -- so both directions are pinned here. The
# assertions parse the writer's output with libxml2 because the defect produced
# text containing every substring a grep would look for while being malformed
# overall. Needs IccXML (CIccMpeXmlUnknown) + IccProfLib; skipped where IccXML is
# not built.
function(iccdev_add_mpexml_unknown_reserved_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCXML}")
    return()
  endif()

  iccdev_add_regression_executable(iccMpeXmlUnknownReservedTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/mpexml-unknown-reserved.cpp"
  )
  target_compile_features(iccMpeXmlUnknownReservedTest PRIVATE cxx_std_17)
  target_include_directories(iccMpeXmlUnknownReservedTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccXML/IccLibXML"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    ${LIBXML2_INCLUDE_DIR}
  )
  target_link_libraries(iccMpeXmlUnknownReservedTest PRIVATE
    ${TARGET_LIB_ICCXML} ${TARGET_LIB_ICCPROFLIB} ${LIBXML2_LIBRARIES})
  add_dependencies(check iccMpeXmlUnknownReservedTest)

  add_test(
    NAME iccdev.mpexml-unknown-reserved
    COMMAND "$<TARGET_FILE:iccMpeXmlUnknownReservedTest>"
  )
  set_tests_properties(iccdev.mpexml-unknown-reserved PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;xml;writer;issue-1886;regression"
  )
  if(WIN32)
    set(_mpexml_reserved_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccMpeXmlUnknownReservedTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCXML}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _mpexml_reserved_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.mpexml-unknown-reserved PROPERTIES
      ENVIRONMENT_MODIFICATION "${_mpexml_reserved_windows_env_mods}"
    )
  endif()
endfunction()

# #1888: proves a regression executable can link an exported IccProfLib global
# *variable*. WINDOWS_EXPORT_ALL_SYMBOLS exports functions but not data, so on a
# Windows shared build this only links via ${ICCDEV_TEST_LIB_ICCPROFLIB}; if that
# fallback is dropped the Windows leg fails here rather than in whichever new
# test next reaches for library data. Also pins the values that the tests which
# cannot link these symbols -- anything co-linking IccXML -- hard-code instead.
# Links IccProfLib only, by design: co-linking IccXML would load IccProfLib twice.
function(iccdev_add_proflib_exported_data_linkage_test)
  if(NOT TARGET "${ICCDEV_TEST_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccProfLibExportedDataLinkageTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/proflib-exported-data-linkage.cpp"
  )
  target_compile_features(iccProfLibExportedDataLinkageTest PRIVATE cxx_std_17)
  target_include_directories(iccProfLibExportedDataLinkageTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccProfLibExportedDataLinkageTest PRIVATE
    ${ICCDEV_TEST_LIB_ICCPROFLIB})
  add_dependencies(check iccProfLibExportedDataLinkageTest)

  add_test(
    NAME iccdev.proflib-exported-data-linkage
    COMMAND "$<TARGET_FILE:iccProfLibExportedDataLinkageTest>"
  )
  set_tests_properties(iccdev.proflib-exported-data-linkage PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;build;linkage;issue-1888;regression"
  )
  if(WIN32)
    set(_exported_data_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfLibExportedDataLinkageTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${ICCDEV_TEST_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _exported_data_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.proflib-exported-data-linkage PROPERTIES
      ENVIRONMENT_MODIFICATION "${_exported_data_windows_env_mods}"
    )
  endif()
endfunction()

# #1897: audits every ICCPROFLIB_API extern global declared in the public headers
# against the symbol table of the library those headers describe, and fails naming
# any that is declared but defined nowhere. icInfo (IccUtil.h) was exactly that
# from the 2015 import until #1897 removed it: nothing in-tree referenced it, so
# no build ever tried to resolve it and CI stayed green while the declaration
# promised a symbol no consumer could ever link.
#
# Deliberately NOT a C++ executable like its iccdev.proflib-exported-data-linkage
# sibling above. Referencing an undefined global does not fail an assertion, it
# fails the link -- such a test would stop BUILDING rather than report a defect,
# which is a far worse failure to diagnose. Reading the symbol table sidesteps
# that, and lets the list of globals come from the headers rather than from a
# hand-maintained copy that would drift the same way icInfo did.
#
# Reads the symbol table with nm, so it is restricted to the ELF and Mach-O
# legs. Windows is skipped explicitly rather than by letting find_program come
# up empty: a Windows runner may well have an nm on PATH from a MinGW or
# Git-for-Windows install, and GNU nm cannot list the export table of an
# MSVC-produced DLL -- it would find none of the globals, trip the control-symbol
# check and report the audit as broken on every Windows run. Skipping costs
# nothing in coverage, because a missing definition is missing on every platform:
# the Linux and macOS legs catch it. The Windows-specific half of this area is
# already covered by iccdev.proflib-exported-data-linkage, which exists precisely
# to fail there (#1888).
function(iccdev_add_proflib_exported_global_definitions_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  if(WIN32 OR MSVC)
    message(STATUS
      "Windows - skipping iccdev.proflib-exported-global-definitions "
      "(nm cannot read a DLL export table; see #1897)")
    return()
  endif()

  find_program(ICCDEV_NM_EXECUTABLE NAMES nm llvm-nm)
  if(NOT ICCDEV_NM_EXECUTABLE)
    message(STATUS
      "nm not found - skipping iccdev.proflib-exported-global-definitions")
    return()
  endif()

  add_dependencies(check ${TARGET_LIB_ICCPROFLIB})

  add_test(
    NAME iccdev.proflib-exported-global-definitions
    COMMAND "${CMAKE_COMMAND}"
      "-DHEADERS=${ICCDEV_REPO_ROOT}/IccProfLib/IccUtil.h;${ICCDEV_REPO_ROOT}/IccProfLib/IccSolve.h"
      "-DLIBRARY=$<TARGET_FILE:${TARGET_LIB_ICCPROFLIB}>"
      "-DNM_TOOL=${ICCDEV_NM_EXECUTABLE}"
      -P "${CMAKE_CURRENT_LIST_DIR}/RunProfLibExportedGlobalDefinitionsTest.cmake"
  )
  set_tests_properties(iccdev.proflib-exported-global-definitions PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;build;linkage;api;issue-1897;regression"
  )
endfunction()

# #1830: the JSON BCD version parser used atoi() per component, which accepts a
# leading '-'; "-1" produced 0xFFFFFFFF and wrapped both the "<< 8" in
# icJsonParseBCDVersionStr and the caller's "<< 16" (UBSan: "left shift of
# 4294967295 by 8 places"). Each component must now be one or two digits, so the
# packed pair is at most 0x9999 and neither shift can overflow. The test drives the
# public CIccProfileJson::ParseJson, so it needs IccJSON + IccProfLib and is skipped
# where IccJSON is not built.
function(iccdev_add_json_bcd_version_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCJSON}")
    return()
  endif()

  iccdev_add_regression_executable(iccJsonBcdVersionTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/json-bcd-version-parse.cpp"
  )
  target_compile_features(iccJsonBcdVersionTest PRIVATE cxx_std_17)
  target_include_directories(iccJsonBcdVersionTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccJSON/IccLibJSON"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccJsonBcdVersionTest PRIVATE
    ${TARGET_LIB_ICCJSON} ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccJsonBcdVersionTest)

  add_test(
    NAME iccdev.json-bcd-version-parse
    COMMAND "$<TARGET_FILE:iccJsonBcdVersionTest>"
  )
  set_tests_properties(iccdev.json-bcd-version-parse PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;json;header;issue-1830;regression"
  )
  if(WIN32)
    set(_json_bcd_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccJsonBcdVersionTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCJSON}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _json_bcd_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.json-bcd-version-parse PROPERTIES
      ENVIRONMENT_MODIFICATION "${_json_bcd_windows_env_mods}"
    )
  endif()
endfunction()

# #1911: the JSON writers handed every payload up to the document root by value, so a
# CLUT-bearing tag's samples were deep-copied and destroyed once per level of the
# chain (data -> clut -> tagData -> tagObj -> entry -> Tags -> IccProfile). Those
# hand-offs are now std::move, and the sample arrays reserve() their known size. The
# risk that introduces is a misplaced move: nlohmann leaves a moved-from value as
# null, which a writer that only reports success cannot detect. This test walks the
# emitted document and asserts the payload actually arrived -- element counts, values
# and no null where data belongs. Header + IccProfLib/IccJSON only, no fixture.
function(iccdev_add_json_writer_move_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCJSON}")
    return()
  endif()

  iccdev_add_regression_executable(iccJsonWriterMoveTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/json-writer-move-integrity.cpp"
  )
  target_compile_features(iccJsonWriterMoveTest PRIVATE cxx_std_17)
  target_include_directories(iccJsonWriterMoveTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccJSON/IccLibJSON"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccJsonWriterMoveTest PRIVATE
    ${TARGET_LIB_ICCJSON} ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccJsonWriterMoveTest)

  add_test(
    NAME iccdev.json-writer-move-integrity
    COMMAND "$<TARGET_FILE:iccJsonWriterMoveTest>"
  )
  set_tests_properties(iccdev.json-writer-move-integrity PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;json;performance;issue-1911;regression"
  )
  if(WIN32)
    set(_json_move_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccJsonWriterMoveTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCJSON}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _json_move_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.json-writer-move-integrity PROPERTIES
      ENVIRONMENT_MODIFICATION "${_json_move_windows_env_mods}"
    )
  endif()
endfunction()

# CodeQL #2343: CIccTagJsonFixedNum::ToJson was the only m_nSize-driven walk in the
# JSON writer without an explicit upper bound -- its Num/FloatNum siblings and its XML
# mirror CIccTagXmlFixedNum::ToXml all cap at 0xffffff. This pins the cap in both
# directions: the largest legal count still serializes in full, and one past it is
# rejected. The boundary case builds a 64 MB tag, so allow more time than the 60s the
# lighter regressions use.
function(iccdev_add_json_fixednum_size_cap_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCJSON}")
    return()
  endif()

  iccdev_add_regression_executable(iccJsonFixedNumSizeCapTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/json-fixednum-size-cap.cpp"
  )
  target_compile_features(iccJsonFixedNumSizeCapTest PRIVATE cxx_std_17)
  target_include_directories(iccJsonFixedNumSizeCapTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccJSON/IccLibJSON"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccJsonFixedNumSizeCapTest PRIVATE
    ${TARGET_LIB_ICCJSON} ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccJsonFixedNumSizeCapTest)

  add_test(
    NAME iccdev.json-fixednum-size-cap
    COMMAND "$<TARGET_FILE:iccJsonFixedNumSizeCapTest>"
  )
  set_tests_properties(iccdev.json-fixednum-size-cap PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 180
    LABELS "iccdev;json;codeql;alert-2343;regression;slow"
  )
  if(WIN32)
    set(_json_fixednum_cap_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccJsonFixedNumSizeCapTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCJSON}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _json_fixednum_cap_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.json-fixednum-size-cap PROPERTIES
      ENVIRONMENT_MODIFICATION "${_json_fixednum_cap_windows_env_mods}"
    )
  endif()
endfunction()

# #1883: the Testing corpus carried no ICC v2 profile at all -- a clean checkout's
# 210 profiles are 208 v5 and 2 v4, with 0 'mft2' and 0 'mft1' tags -- so every
# UseLegacyPCS()==true branch was dead code as far as CI was concerned. That
# predicate selects the v2 16-bit Lab encoding (0..0xff00 rather than 0..0xffff)
# in CIccXform::AdjustPCS and in the PCS step CIccCmm::Begin splices onto a
# legacy transform. This test builds both a v2 'mft2' and a v4 'mAB ' profile
# over identical CLUT content in memory and asserts the two disagree by exactly
# the 65535/65280 factor, so it cannot be disabled by a corpus change. Links
# IccProfLib alone and references no exported library data, so it is eligible for
# ${ICCDEV_TEST_LIB_ICCPROFLIB}.
function(iccdev_add_v2_legacy_pcs_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccV2LegacyPcsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/v2-legacy-pcs.cpp"
  )
  target_compile_features(iccV2LegacyPcsTest PRIVATE cxx_std_17)
  target_include_directories(iccV2LegacyPcsTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccV2LegacyPcsTest PRIVATE ${ICCDEV_TEST_LIB_ICCPROFLIB})
  add_dependencies(check iccV2LegacyPcsTest)

  if(WIN32)
    add_test(
      NAME iccdev.v2-legacy-pcs
      COMMAND "$<TARGET_FILE:iccV2LegacyPcsTest>"
    )
  else()
    add_test(
      NAME iccdev.v2-legacy-pcs
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccV2LegacyPcsTest>"
    )
  endif()
  set_tests_properties(iccdev.v2-legacy-pcs PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;v2;legacy-pcs;cmm;issue-1883;regression"
  )
  if(WIN32)
    set(_v2_legacy_pcs_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccV2LegacyPcsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _v2_legacy_pcs_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.v2-legacy-pcs PROPERTIES
      ENVIRONMENT_MODIFICATION "${_v2_legacy_pcs_windows_env_mods}"
    )
  endif()
endfunction()

# #1883, second half: pin the tracked v2 XML fixtures themselves. Testing/**/*.icc
# is gitignored, so Testing/V2/*.xml is the tracked artifact and the profile is
# generated from it by CreateAllProfiles.sh. Each case asserts the fixture still
# parses through the same CIccProfileXml::LoadXml path iccFromXml uses, still
# declares a 2.x header, still carries the LUT type it exists to provide, and
# still validates without a critical error -- a fixture that silently drifted to
# v4 would otherwise restore the original coverage gap while looking healthy.
# Needs IccXML (CIccProfileXml) + IccProfLib; skipped where IccXML is not built,
# and barred from ${ICCDEV_TEST_LIB_ICCPROFLIB} because it co-links IccXML.
function(iccdev_add_v2_xml_fixtures_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCXML}")
    return()
  endif()

  iccdev_add_regression_executable(iccV2XmlFixturesTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/v2-xml-fixtures.cpp"
  )
  target_compile_features(iccV2XmlFixturesTest PRIVATE cxx_std_17)
  target_include_directories(iccV2XmlFixturesTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccXML/IccLibXML"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    ${LIBXML2_INCLUDE_DIR}
  )
  target_link_libraries(iccV2XmlFixturesTest PRIVATE
    ${TARGET_LIB_ICCXML} ${TARGET_LIB_ICCPROFLIB} ${LIBXML2_LIBRARIES})
  add_dependencies(check iccV2XmlFixturesTest)

  # <case name>=<fixture>|<expected AToB0 tag type, or none>
  set(_v2_xml_fixture_cases
    "cmyk-lut16=${ICCDEV_TESTING_DIR}/V2/v2CmykLut16.xml|mft2"
    "rgb-lut8=${ICCDEV_TESTING_DIR}/V2/v2RgbLut8.xml|mft1"
    "rgb-matrix-trc=${ICCDEV_TESTING_DIR}/V2/v2RgbMatrixTRC.xml|none"
  )

  foreach(_case IN LISTS _v2_xml_fixture_cases)
    string(REGEX REPLACE "=.*$" "" _case_name "${_case}")
    string(REGEX REPLACE "^[^=]*=" "" _case_rest "${_case}")
    string(REGEX REPLACE "\\|.*$" "" _case_xml "${_case_rest}")
    string(REGEX REPLACE "^[^|]*\\|" "" _case_sig "${_case_rest}")

    if(WIN32)
      add_test(
        NAME iccdev.v2-xml-fixtures.${_case_name}
        COMMAND "$<TARGET_FILE:iccV2XmlFixturesTest>" "${_case_xml}" "${_case_sig}"
      )
    else()
      add_test(
        NAME iccdev.v2-xml-fixtures.${_case_name}
        COMMAND
          "${CMAKE_COMMAND}" -E env
          ${ICCDEV_TEST_ENV}
          "$<TARGET_FILE:iccV2XmlFixturesTest>"
          "${_case_xml}" "${_case_sig}"
      )
    endif()
    set_tests_properties(iccdev.v2-xml-fixtures.${_case_name} PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
      TIMEOUT 60
      LABELS "iccdev;xml;v2;fixtures;issue-1883;regression"
    )
    if(WIN32)
      set(_v2_xml_windows_env_mods
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccV2XmlFixturesTest>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCXML}>"
      )
      foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
        list(APPEND _v2_xml_windows_env_mods
          "PATH=path_list_prepend:${_runtime_path}")
      endforeach()
      set_tests_properties(iccdev.v2-xml-fixtures.${_case_name} PROPERTIES
        ENVIRONMENT_MODIFICATION "${_v2_xml_windows_env_mods}"
      )
    endif()
  endforeach()
endfunction()

# #1831: icGetDateTimeValue discarded its sscanf conversion count and assigned every
# field into the icUInt16Number members of icDateTimeNumber without a range check, so
# "%u" turned "-1" into 4294967295 and a year of 590359881 was truncated to 11593
# (UBSan implicit-conversion). The parse is now explicit and a malformed string
# yields the all-zero "date unknown" value. Pure IccProfLib, no fixture needed.
function(iccdev_add_datetime_parse_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccDateTimeParseTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/datetime-parse-strict.cpp"
  )
  target_link_libraries(iccDateTimeParseTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccDateTimeParseTest)

  add_test(
    NAME iccdev.datetime-parse-strict
    COMMAND "$<TARGET_FILE:iccDateTimeParseTest>"
  )
  set_tests_properties(iccdev.datetime-parse-strict PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;validation;issue-1831;regression"
  )
  if(WIN32)
    set(_datetime_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccDateTimeParseTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _datetime_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.datetime-parse-strict PROPERTIES
      ENVIRONMENT_MODIFICATION "${_datetime_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_encoding_surround_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccEncodingSurroundTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/encoding-surround-ratio-divzero.cpp"
  )
  target_link_libraries(iccEncodingSurroundTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccEncodingSurroundTest)

  add_test(
    NAME iccdev.encoding-surround-ratio-divzero
    COMMAND "$<TARGET_FILE:iccEncodingSurroundTest>"
  )
  set_tests_properties(iccdev.encoding-surround-ratio-divzero PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;encoding;issue-1817;issue-1980;regression"
  )
  if(WIN32)
    set(_encsurround_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccEncodingSurroundTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _encsurround_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.encoding-surround-ratio-divzero PROPERTIES
      ENVIRONMENT_MODIFICATION "${_encsurround_windows_env_mods}"
    )
  endif()
endfunction()

# CIccDefaultEncProfileConverter::ConvertFromParams deleted the luma/chroma
# matrix element it had only borrowed from the caller's colorEncodingParams
# struct via FindElemOfType, leaving the entry in the struct's element list. The
# owner's later teardown walked that list and wrote through, then freed, the
# dangling pointer -- a heap use-after-free write followed by a virtual-destructor
# dispatch on freed memory and a second free, all on profile-controlled input.
# The regression asserts the borrowed element survives the call intact on both
# the succeeding and the failing return path (#1985).
function(iccdev_add_encoding_lumamtx_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccEncodingLumaMtxTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/encoding-lumamtx-double-free.cpp"
  )
  target_link_libraries(iccEncodingLumaMtxTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccEncodingLumaMtxTest)

  add_test(
    NAME iccdev.encoding-lumamtx-double-free
    COMMAND "$<TARGET_FILE:iccEncodingLumaMtxTest>"
  )
  set_tests_properties(iccdev.encoding-lumamtx-double-free PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;encoding;issue-1985;regression"
  )
  if(WIN32)
    set(_enclumamtx_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccEncodingLumaMtxTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _enclumamtx_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.encoding-lumamtx-double-free PROPERTIES
      ENVIRONMENT_MODIFICATION "${_enclumamtx_windows_env_mods}"
    )
  endif()
endfunction()

# icConvertEncodingProfile layered a colour-encoding profile's local overrides by
# calling DeleteElem then AttachElem on pStruct -- the struct it was iterating --
# instead of on pParams, the copy of the base encoding profile's params. DeleteElem
# erases the list node and deletes the tag, so the loop read back an entry it had
# just freed (heap use-after-free), advanced an erased iterator (a non-terminating
# walk), and never wrote pParams at all, leaving the override silently discarded.
# The regression asserts the override reaches the converter and that the profile's
# own struct is left untouched (#1991). Against the unfixed library the call faults
# before those assertions are reached -- heap-use-after-free under ASAN, exit 139
# unsanitized -- and the timeout guards the erased-iterator walk, which does not
# terminate when the read lands on still-mapped memory.
function(iccdev_add_encoding_override_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccEncodingOverrideTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/encoding-override-uaf.cpp"
  )
  target_link_libraries(iccEncodingOverrideTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccEncodingOverrideTest)

  add_test(
    NAME iccdev.encoding-override-uaf
    COMMAND "$<TARGET_FILE:iccEncodingOverrideTest>"
  )
  set_tests_properties(iccdev.encoding-override-uaf PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;encoding;issue-1991;regression"
  )
  if(WIN32)
    set(_encoverride_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccEncodingOverrideTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _encoverride_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.encoding-override-uaf PROPERTIES
      ENVIRONMENT_MODIFICATION "${_encoverride_windows_env_mods}"
    )
  endif()
endfunction()

# CIccPRMG::GetChroma normalized an arbitrary hue angle into [0, 360) with
# "while (h < 0.0) h += 360.0;" / "while (h >= 360.0) h -= 360.0;". Those loops do
# not terminate once |h| is large enough that 360 falls below the float's ULP
# (around 1e20): the addition rounds back to the original value and the condition
# never changes. The isfinite() guard at the top of the function does not catch it
# because such values are finite. fmod performs the same reduction exactly and in
# one step. The test drives the public GetChroma with hues that hang the old
# loops, so the unfixed code fails by TIMEOUT; the value checks additionally
# confirm the reduction matches an independently computed fmod, and that the
# NaN/infinity/L-range rejections are unchanged. IccProfLib only.
function(iccdev_add_prmg_hue_normalization_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPrmgHueNormalizationTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/prmg-hue-normalization.cpp"
  )
  target_compile_features(iccPrmgHueNormalizationTest PRIVATE cxx_std_17)
  target_include_directories(iccPrmgHueNormalizationTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccPrmgHueNormalizationTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPrmgHueNormalizationTest)

  add_test(
    NAME iccdev.prmg-hue-normalization
    COMMAND "$<TARGET_FILE:iccPrmgHueNormalizationTest>"
  )
  set_tests_properties(iccdev.prmg-hue-normalization PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;prmg;regression"
  )
  if(WIN32)
    set(_prmghue_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPrmgHueNormalizationTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _prmghue_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.prmg-hue-normalization PROPERTIES
      ENVIRONMENT_MODIFICATION "${_prmghue_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_windows_iccdevcmm_smoke_test)
  if(NOT WIN32 OR NOT TARGET RefIccMAX)
    return()
  endif()

  iccdev_add_regression_executable(iccDEVCmmSmokeTest
    "${ICCDEV_REPO_ROOT}/Tools/Winnt/IccDEVCmm/tests/IccDEVCmmSmoke.cpp"
  )
  target_compile_features(iccDEVCmmSmokeTest PRIVATE cxx_std_17)
  target_compile_definitions(iccDEVCmmSmokeTest PRIVATE UNICODE _UNICODE)
  target_link_libraries(iccDEVCmmSmokeTest PRIVATE Mscms.lib)

  add_dependencies(check iccDEVCmmSmokeTest RefIccMAX)

  add_test(
    NAME iccdev.windows-iccdevcmm-smoke
    COMMAND
      "$<TARGET_FILE:iccDEVCmmSmokeTest>"
      "$<TARGET_FILE:RefIccMAX>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )
  set_tests_properties(iccdev.windows-iccdevcmm-smoke PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;windows;cmm;icm;dll"
  )

  set(_iccdevcmm_windows_env_mods
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccDEVCmmSmokeTest>"
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:RefIccMAX>"
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
  )
  foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
    list(APPEND _iccdevcmm_windows_env_mods
      "PATH=path_list_prepend:${_runtime_path}")
  endforeach()
  set_tests_properties(iccdev.windows-iccdevcmm-smoke PROPERTIES
    ENVIRONMENT_MODIFICATION "${_iccdevcmm_windows_env_mods}"
  )
endfunction()


function(iccdev_add_zip_utf8_text_zlib_test)
  if(NOT ICC_USE_ZLIB OR NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccZipUtf8TextZlibTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/zip-utf8-text-zlib.cpp"
  )
  target_compile_features(iccZipUtf8TextZlibTest PRIVATE cxx_std_17)
  target_link_libraries(iccZipUtf8TextZlibTest PRIVATE ${TARGET_LIB_ICCPROFLIB})

  add_dependencies(check iccZipUtf8TextZlibTest)

  if(WIN32)
    add_test(
      NAME iccdev.zip-utf8-text-zlib
      COMMAND "$<TARGET_FILE:iccZipUtf8TextZlibTest>"
    )
  else()
    add_test(
      NAME iccdev.zip-utf8-text-zlib
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccZipUtf8TextZlibTest>"
    )
  endif()
  set_tests_properties(iccdev.zip-utf8-text-zlib PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;zlib;ziputf8;regression"
  )
  if(WIN32)
    set(_zip_utf8_text_zlib_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccZipUtf8TextZlibTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _zip_utf8_text_zlib_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.zip-utf8-text-zlib PROPERTIES
      ENVIRONMENT_MODIFICATION "${_zip_utf8_text_zlib_windows_env_mods}"
    )
  endif()
endfunction()

if(WIN32)
  function(iccdev_add_windows_batch_test NAME TIMEOUT LABELS SCRIPT)
    set(_one_value_args
      EXPECTED_PROFILE_PARSE_COUNT
      EXPECTED_GENERATED_PROFILE_COUNT
      EXPECTED_OUTPUT
      FORBIDDEN_REGEX
    )
    cmake_parse_arguments(ICCDEV_WIN_TEST "" "${_one_value_args}" "" ${ARGN})

    if(NOT EXISTS "${SCRIPT}")
      message(STATUS "Skipping ${NAME}: ${SCRIPT} not found")
      return()
    endif()

    string(REPLACE "." "-" _out_name "${NAME}")
    set(_test_args
      "-DICCDEV_TEST_NAME=${NAME}"
      "-DICCDEV_TESTING_DIR=${ICCDEV_TESTING_DIR}"
      "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/${_out_name}"
      "-DICCDEV_BATCH_SCRIPT=${SCRIPT}"
      "-DICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DICCDEV_WINDOWS_WORK_DIR=${ICCDEV_TEST_OUTDIR}/windows-testing"
      "-DICCDEV_CONFIG=$<CONFIG>"
    )

    if(DEFINED ICCDEV_WIN_TEST_EXPECTED_PROFILE_PARSE_COUNT)
      list(APPEND _test_args
        "-DICCDEV_EXPECTED_PROFILE_PARSE_COUNT=${ICCDEV_WIN_TEST_EXPECTED_PROFILE_PARSE_COUNT}")
    endif()

    if(DEFINED ICCDEV_WIN_TEST_EXPECTED_GENERATED_PROFILE_COUNT)
      list(APPEND _test_args
        "-DICCDEV_EXPECTED_GENERATED_PROFILE_COUNT=${ICCDEV_WIN_TEST_EXPECTED_GENERATED_PROFILE_COUNT}")
    endif()

    if(DEFINED ICCDEV_WIN_TEST_EXPECTED_OUTPUT)
      list(APPEND _test_args
        "-DICCDEV_EXPECTED_OUTPUT=${ICCDEV_WIN_TEST_EXPECTED_OUTPUT}")
    endif()

    if(DEFINED ICCDEV_WIN_TEST_FORBIDDEN_REGEX)
      list(APPEND _test_args
        "-DICCDEV_FORBIDDEN_REGEX=${ICCDEV_WIN_TEST_FORBIDDEN_REGEX}")
    endif()

    add_test(
      NAME "${NAME}"
      COMMAND
        "${CMAKE_COMMAND}"
        ${_test_args}
        -P "${CMAKE_CURRENT_LIST_DIR}/RunWindowsBatchTest.cmake"
    )
    set_tests_properties("${NAME}" PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT "${TIMEOUT}"
      LABELS "${LABELS}"
    )
  endfunction()

  if(TARGET iccFromXml)
    iccdev_add_windows_batch_test(
      iccdev.windows-create-profiles
      300
      "iccdev;windows;setup;profiles"
      "${ICCDEV_TESTING_DIR}/CreateAllProfiles.bat"
      # Validate the disposable Windows Testing tree by artifact count. The
      # batch file echoes commands through cmd.exe, and tool stdout can be lost
      # in that layer even when profiles are generated successfully.
      EXPECTED_GENERATED_PROFILE_COUNT 133
    )
    set_tests_properties(iccdev.windows-create-profiles PROPERTIES
      FIXTURES_SETUP iccdev_profiles
    )
  endif()

  if(TARGET iccFromXml AND TARGET iccApplyNamedCmm AND TARGET iccToJson AND TARGET iccFromJson)
    iccdev_add_windows_batch_test(
      iccdev.windows-legacy-run-tests
      300
      "iccdev;windows;legacy;profiles"
      "${ICCDEV_TESTING_DIR}/RunTests.bat"
      EXPECTED_OUTPUT "JSON round-trip: PASS"
      FORBIDDEN_REGEX "JSON round-trip: FAIL|not found -- skipping JSON tests"
    )
    set_tests_properties(iccdev.windows-legacy-run-tests PROPERTIES
      FIXTURES_REQUIRED iccdev_profiles
    )
  endif()

  if(TARGET iccDumpProfile)
    add_test(
      NAME iccdev.windows-icc-dump-profile-smoke
      COMMAND
        "${CMAKE_COMMAND}"
        "-DICCDEV_TEST_NAME=iccdev.windows-icc-dump-profile-smoke"
        "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/iccdev-windows-icc-dump-profile-smoke"
        "-DICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DICCDEV_ICCDUMP_EXE=$<TARGET_FILE:iccDumpProfile>"
        "-DICCDEV_PROFILE=${ICCDEV_TESTING_DIR}/CalcTest/calcUnderStack_add.icc"
        "-DICCDEV_EXPECTED_PROFILE_SIZE=3932"
        "-DICCDEV_EXPECTED_TAG_COUNT=8"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunWindowsDumpProfileSmokeTest.cmake"
    )
    set_tests_properties(iccdev.windows-icc-dump-profile-smoke PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 60
      LABELS "iccdev;windows;tools;profile"
    )
    add_dependencies(check iccDumpProfile)
  endif()

  if(TARGET iccPawgReport)
    add_test(
      NAME iccdev.windows-pawg-report-smoke
      COMMAND
        "${CMAKE_COMMAND}"
        "-DICCDEV_TEST_NAME=iccdev.windows-pawg-report-smoke"
        "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/iccdev-windows-pawg-report-smoke"
        "-DICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DICCDEV_PAWG_EXE=$<TARGET_FILE:iccPawgReport>"
        "-DICCDEV_PROFILE=${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
        "-DICCDEV_EXPECTED_ITEM_COUNT=32"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunWindowsPawgReportSmokeTest.cmake"
    )
    set_tests_properties(iccdev.windows-pawg-report-smoke PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 60
      LABELS "iccdev;windows;tools;pawg;report;profile"
    )
    add_dependencies(check iccPawgReport)
  endif()

  if(ENABLE_SHARED_LIBS AND TARGET IccProfLib2)
    add_test(
      NAME iccdev.issue-987-shared-mpe-export
      COMMAND
        "${CMAKE_COMMAND}"
        "-DICCDEV_TEST_NAME=iccdev.issue-987-shared-mpe-export"
        "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/iccdev-issue-987-shared-mpe-export"
        "-DICCDEV_REPO_ROOT=${ICCDEV_REPO_ROOT}"
        "-DICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DICCDEV_CONFIG=$<CONFIG>"
        "-DICCDEV_ICCPROFLIB_DLL=$<TARGET_FILE:IccProfLib2>"
        "-DICCDEV_ICCPROFLIB_IMPLIB=$<TARGET_LINKER_FILE:IccProfLib2>"
        "-DICCDEV_GENERATOR=${CMAKE_GENERATOR}"
        "-DICCDEV_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DICCDEV_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DICCDEV_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DICCDEV_OBJDUMP=${CMAKE_OBJDUMP}"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunWindowsSharedExportTest.cmake"
    )
    set_tests_properties(iccdev.issue-987-shared-mpe-export PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 180
      LABELS "iccdev;windows;shared;issue-987"
    )
    add_dependencies(check IccProfLib2)
  endif()

  if(ENABLE_SHARED_LIBS AND TARGET IccJSON2)
    add_test(
      NAME iccdev.issue-1009-iccjson-profilejson-export
      COMMAND
        "${CMAKE_COMMAND}"
        "-DICCDEV_TEST_NAME=iccdev.issue-1009-iccjson-profilejson-export"
        "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/iccdev-issue-1009-iccjson-profilejson-export"
        "-DICCDEV_ICCJSON_DLL=$<TARGET_FILE:IccJSON2>"
        "-DICCDEV_ICCJSON_IMPLIB=$<TARGET_LINKER_FILE:IccJSON2>"
        "-DICCDEV_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DICCDEV_OBJDUMP=${CMAKE_OBJDUMP}"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunWindowsIccJsonExportTest.cmake"
    )
    set_tests_properties(iccdev.issue-1009-iccjson-profilejson-export PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 60
      LABELS "iccdev;windows;shared;issue-1009"
    )
    add_dependencies(check IccJSON2)
  endif()

  iccdev_add_iccconnect_thread_test()
  iccdev_add_iccconnect_config_parser_test()
  iccdev_add_cmmsearch_init_pcc_test()
  iccdev_add_embedio_read8_bounds_test()
  iccdev_add_fileio_getlength_position_test()
  iccdev_add_profile_write_failure_test()
  iccdev_add_fileio_seek_tell_test()
  iccdev_add_iccio_write_float_aliasing_test()
  iccdev_add_tagdata_zlib_roundtrip_test()
  iccdev_add_ziputf8_settext_contract_test()
  iccdev_add_compression_describe_labels_test()
  iccdev_add_pawg_compression_paths_test()
  iccdev_add_json_sparsematrix_huaf_test()
  iccdev_add_xml_writer_graceful_degrade_test()
  iccdev_add_xml_relaxng_validation_leak_test()
  iccdev_add_xml_curve_oversize_file_test()
  iccdev_add_mpexml_unknown_reserved_test()
  iccdev_add_proflib_exported_data_linkage_test()
  iccdev_add_proflib_exported_global_definitions_test()
  iccdev_add_json_bcd_version_test()
  iccdev_add_json_writer_move_test()
  iccdev_add_json_fixednum_size_cap_test()
  iccdev_add_v2_legacy_pcs_test()
  iccdev_add_v2_xml_fixtures_test()
  iccdev_add_datetime_parse_test()
  iccdev_add_encoding_surround_test()
  iccdev_add_encoding_lumamtx_test()
  iccdev_add_encoding_override_test()
  iccdev_add_parser_restore_calls_test()
  iccdev_add_tag_layout_shared_data_test()
  iccdev_add_pawg_q2_tonecurves_test()
  iccdev_add_pawg_q4_characterization_test()
  iccdev_add_pawg_q4_xyz_pcs_decode_test()
  iccdev_add_tointernal_fixedint_roundtrip_test()
  iccdev_add_pawg_q1q3_relative_intent_test()
  iccdev_add_cmm_registry_allowlist_test()
  iccdev_add_mpecalc_matrix_overflow_test()
  iccdev_add_sparsematrix_set_roundtrip_test()
  iccdev_add_cmmsearch_namedcolor_ownership_test()
  iccdev_add_xform_create_tag_ownership_test()
  iccdev_add_xform_abstorel_adjust_test()
  iccdev_add_reflectance_observer_illum_range_test()
  iccdev_add_colorimetry_methods_test()
  iccdev_add_getvalues_nstart_test()
  iccdev_add_cicp_copy_ctor_test()
  iccdev_add_pawg_c5_cicp_test()
  iccdev_add_gamut_xform_semantics_test()
  iccdev_add_rangemap_uninitialized_test()
  iccdev_add_pcs_birefl_illuminant_range_test()
  iccdev_add_pcs_applyillum_chain_sizing_test()
  iccdev_add_spectral_pcs_range_consistency_test()
  iccdev_add_spectral_media_white_test()
  iccdev_add_gbd_zero_pcs_test()
  iccdev_add_mpe_empty_identity_test()
  iccdev_add_pcc_reflectance_observer_range_test()
  iccdev_add_pcs_edge_metadata_test()
  iccdev_add_xyz_negative_z_test()
  iccdev_add_utf8_textarray_test()
  iccdev_add_extended_device_colorspace_test()
  iccdev_add_unsupported_profile_class_test()
  iccdev_add_dpcc_pcc_replacement_test()
  iccdev_add_iccviz_metrics_test()
  iccdev_add_iccviz_degenerate_test()
  iccdev_add_iccviz_pdf_axis_test()
  iccdev_add_iccprofileplot_clut_oob_test()
  iccdev_add_iccprofileplot_pdf_leak_test()
  iccdev_add_iccprofileplot_exitcode_test()
  iccdev_add_applytolink_invalid_arg_test()
  iccdev_add_applytolink_v4_missing_desc_test()
  iccdev_add_prmg_hue_normalization_test()
  iccdev_add_windows_iccdevcmm_smoke_test()
  iccdev_add_zip_utf8_text_zlib_test()

  return()
endif()

if(NOT BASH_EXECUTABLE)
  message(WARNING "bash not found; iccDEV CTest script tests are disabled.")
  return()
endif()

iccdev_add_applytolink_v4_missing_desc_test()

set(ICCDEV_TIMEOUT_COMPAT_DIR "")
if(NOT WIN32)
  find_program(ICCDEV_TIMEOUT_EXECUTABLE timeout)
  if(NOT ICCDEV_TIMEOUT_EXECUTABLE)
    find_program(ICCDEV_GTIMEOUT_EXECUTABLE gtimeout)
    set(ICCDEV_TIMEOUT_COMPAT_DIR "${CMAKE_BINARY_DIR}/Testing/timeout-bin")
    set(_ICCDEV_TIMEOUT_COMPAT "${ICCDEV_TIMEOUT_COMPAT_DIR}/timeout")
    file(MAKE_DIRECTORY "${ICCDEV_TIMEOUT_COMPAT_DIR}")
    if(ICCDEV_GTIMEOUT_EXECUTABLE)
      file(WRITE "${_ICCDEV_TIMEOUT_COMPAT}"
        "#!/bin/sh\nexec \"${ICCDEV_GTIMEOUT_EXECUTABLE}\" \"$@\"\n")
    else()
      file(WRITE "${_ICCDEV_TIMEOUT_COMPAT}" [=[#!/usr/bin/env bash
set -u

if [ "$#" -lt 2 ]; then
  echo "usage: timeout SECONDS COMMAND [ARG ...]" >&2
  exit 125
fi

duration=$1
shift

case "$duration" in
  ''|*[!0-9.]*)
    echo "timeout: invalid duration '$duration'" >&2
    exit 125
    ;;
esac

timed_out=$(mktemp "${TMPDIR:-/tmp}/iccdev-timeout.XXXXXX") || exit 125
rm -f "$timed_out"

"$@" &
pid=$!

(
  sleep "$duration"
  if kill -0 "$pid" >/dev/null 2>&1; then
    : > "$timed_out"
    kill -TERM "$pid" >/dev/null 2>&1 || true
    sleep 2
    kill -KILL "$pid" >/dev/null 2>&1 || true
  fi
) &
watcher=$!

wait "$pid"
status=$?

kill "$watcher" >/dev/null 2>&1 || true
wait "$watcher" 2>/dev/null || true

if [ -e "$timed_out" ]; then
  rm -f "$timed_out"
  exit 124
fi

exit "$status"
]=])
    endif()
    execute_process(
      COMMAND chmod +x "${_ICCDEV_TIMEOUT_COMPAT}"
      RESULT_VARIABLE _ICCDEV_TIMEOUT_CHMOD_RESULT
    )
    if(NOT _ICCDEV_TIMEOUT_CHMOD_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to make ${_ICCDEV_TIMEOUT_COMPAT} executable")
    endif()
  endif()
endif()

set(ICCDEV_TOOL_PATHS
  "${ICCDEV_TOOLS_DIR}/IccApplyNamedCmm"
  "${ICCDEV_TOOLS_DIR}/IccApplyProfiles"
  "${ICCDEV_TOOLS_DIR}/IccApplySearch"
  "${ICCDEV_TOOLS_DIR}/IccApplyToLink"
  "${ICCDEV_TOOLS_DIR}/IccDumpProfile"
  "${ICCDEV_TOOLS_DIR}/IccFromCube"
  "${ICCDEV_TOOLS_DIR}/IccFromJson"
  "${ICCDEV_TOOLS_DIR}/IccFromXml"
  "${ICCDEV_TOOLS_DIR}/IccJpegDump"
  "${ICCDEV_TOOLS_DIR}/IccPawgReport"
  "${ICCDEV_TOOLS_DIR}/IccPngDump"
  "${ICCDEV_TOOLS_DIR}/IccRoundTrip"
  "${ICCDEV_TOOLS_DIR}/IccSpecSepToTiff"
  "${ICCDEV_TOOLS_DIR}/IccTiffDump"
  "${ICCDEV_TOOLS_DIR}/IccToJson"
  "${ICCDEV_TOOLS_DIR}/IccToXml"
  "${ICCDEV_TOOLS_DIR}/IccV5DspObsToV4Dsp"
  "${ICCDEV_TOOLS_DIR}/IccProfileVisualize"
)
if(ICCDEV_TIMEOUT_COMPAT_DIR)
  list(PREPEND ICCDEV_TOOL_PATHS "${ICCDEV_TIMEOUT_COMPAT_DIR}")
endif()

list(JOIN ICCDEV_TOOL_PATHS ":" ICCDEV_TOOL_PATH)
set(ICCDEV_SHARED_LIBRARY_PATH
    "${CMAKE_BINARY_DIR}/IccProfLib:${CMAKE_BINARY_DIR}/IccXML:${CMAKE_BINARY_DIR}/IccJSON:${CMAKE_BINARY_DIR}/IccConnect")

set(ICCDEV_REQUIRE_JSON_ROUNDTRIP 0)
if(TARGET iccToJson AND TARGET iccFromJson)
  set(ICCDEV_REQUIRE_JSON_ROUNDTRIP 1)
endif()

set(ICCDEV_HAS_ICCCONNECT FALSE)
if(DEFINED TARGET_LIB_ICCCONNECT AND TARGET "${TARGET_LIB_ICCCONNECT}")
  set(ICCDEV_HAS_ICCCONNECT TRUE)
endif()

set(ICCDEV_HAS_JSON_TOOLS FALSE)
if(TARGET iccToJson AND TARGET iccFromJson)
  set(ICCDEV_HAS_JSON_TOOLS TRUE)
endif()

set(ICCDEV_HAS_JSON_RUNTIME_CONFIG_TOOLS FALSE)
if(TARGET iccApplyNamedCmm AND TARGET iccApplyProfiles AND TARGET iccApplySearch)
  set(ICCDEV_HAS_JSON_RUNTIME_CONFIG_TOOLS TRUE)
endif()

set(ICCDEV_MACOS_SDK_ENV)
if(APPLE)
  set(ICCDEV_MACOS_SDKROOT "${CMAKE_OSX_SYSROOT}")
  if(NOT ICCDEV_MACOS_SDKROOT)
    find_program(ICCDEV_XCRUN_EXECUTABLE xcrun)
    if(ICCDEV_XCRUN_EXECUTABLE)
      execute_process(
        COMMAND "${ICCDEV_XCRUN_EXECUTABLE}" --show-sdk-path
        OUTPUT_VARIABLE ICCDEV_MACOS_SDKROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
  endif()
  if(ICCDEV_MACOS_SDKROOT)
    list(APPEND ICCDEV_MACOS_SDK_ENV "SDKROOT=${ICCDEV_MACOS_SDKROOT}")
  endif()
endif()

set(ICCDEV_TEST_INCLUDE_ENV)
find_path(ICCDEV_NLOHMANN_INCLUDE_DIR nlohmann/json.hpp)
if(ICCDEV_NLOHMANN_INCLUDE_DIR)
  set(ICCDEV_TEST_INCLUDE_ENV "CPATH=${ICCDEV_NLOHMANN_INCLUDE_DIR}:$ENV{CPATH}")
endif()

set(ICCDEV_TEST_ENV
  "PATH=${ICCDEV_TOOL_PATH}:$ENV{PATH}"
  "LD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
  "DYLD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}"
  ${ICCDEV_MACOS_SDK_ENV}
  ${ICCDEV_TEST_INCLUDE_ENV}
  "CC=${CMAKE_C_COMPILER}"
  "CXX=${CMAKE_CXX_COMPILER}"
  "ASAN_OPTIONS=halt_on_error=0,detect_leaks=0"
  "UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1"
  # print_stacktrace=1 above makes every sanitizer report symbolize its stack.
  # llvm-symbolizer consults DEBUGINFOD_URLS while doing so, and the regression
  # container inherits Ubuntu's default (https://debuginfod.ubuntu.com) because
  # ci-docker.yml runs ctest under a login shell, which sources /etc/profile.d.
  # That host resolves from the container but never completes a connection, so
  # each report blocks on the fetch for roughly 90 seconds - time attributed to
  # whichever test happened to trip the report, not to the network. That is
  # what timed out
  # iccdev.json-sort-regression (issue #1928): a single benign libstdc++
  # unsigned-overflow report inside std::sort turned a ~4s run into a >300s one.
  # Emptying the variable keeps stack traces fully symbolized - our binaries
  # carry their own -g DWARF, and debuginfod only ever supplied *system*
  # library debug info - while removing the network dependency from every test.
  "DEBUGINFOD_URLS="
  "LLVM_PROFILE_FILE=/dev/null"
  "ICCDEV_TOOLS_DIR=${ICCDEV_TOOLS_DIR}"
  "ICCDEV_TESTING_DIR=${ICCDEV_TESTING_DIR}"
  "ICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "ICCDEV_REQUIRE_JSON_ROUNDTRIP=${ICCDEV_REQUIRE_JSON_ROUNDTRIP}"
  # Issue #1856. The JSON size bound is configurable, so a test that asserts on
  # a document size has to know which limit the readers were actually built
  # with rather than assuming the default.
  "ICC_JSON_MAX_FILE_MB=${ICC_JSON_MAX_FILE_MB}"
)

iccdev_add_iccconnect_thread_test()
iccdev_add_iccconnect_config_parser_test()
iccdev_add_cmmsearch_init_pcc_test()
iccdev_add_embedio_read8_bounds_test()
iccdev_add_fileio_getlength_position_test()
iccdev_add_profile_write_failure_test()
iccdev_add_fileio_seek_tell_test()
iccdev_add_iccio_write_float_aliasing_test()
iccdev_add_tagdata_zlib_roundtrip_test()
iccdev_add_ziputf8_settext_contract_test()
iccdev_add_compression_describe_labels_test()
iccdev_add_pawg_compression_paths_test()
iccdev_add_json_sparsematrix_huaf_test()
iccdev_add_xml_writer_graceful_degrade_test()
iccdev_add_xml_relaxng_validation_leak_test()
iccdev_add_xml_curve_oversize_file_test()
iccdev_add_mpexml_unknown_reserved_test()
iccdev_add_proflib_exported_data_linkage_test()
iccdev_add_proflib_exported_global_definitions_test()
iccdev_add_json_bcd_version_test()
iccdev_add_json_writer_move_test()
iccdev_add_json_fixednum_size_cap_test()
iccdev_add_v2_legacy_pcs_test()
iccdev_add_v2_xml_fixtures_test()
iccdev_add_datetime_parse_test()
iccdev_add_encoding_surround_test()
iccdev_add_encoding_lumamtx_test()
iccdev_add_encoding_override_test()
iccdev_add_parser_restore_calls_test()
iccdev_add_tag_layout_shared_data_test()
iccdev_add_pawg_q2_tonecurves_test()
iccdev_add_pawg_q4_characterization_test()
iccdev_add_pawg_q4_xyz_pcs_decode_test()
iccdev_add_pawg_q1q3_relative_intent_test()
iccdev_add_cmm_registry_allowlist_test()
iccdev_add_mpecalc_matrix_overflow_test()
iccdev_add_sparsematrix_set_roundtrip_test()
iccdev_add_cmmsearch_namedcolor_ownership_test()
iccdev_add_xform_create_tag_ownership_test()
iccdev_add_xform_abstorel_adjust_test()
iccdev_add_reflectance_observer_illum_range_test()
iccdev_add_colorimetry_methods_test()
iccdev_add_getvalues_nstart_test()
iccdev_add_cicp_copy_ctor_test()
iccdev_add_pawg_c5_cicp_test()
iccdev_add_gamut_xform_semantics_test()
iccdev_add_rangemap_uninitialized_test()
iccdev_add_pcs_birefl_illuminant_range_test()
iccdev_add_pcs_applyillum_chain_sizing_test()
iccdev_add_spectral_pcs_range_consistency_test()
iccdev_add_spectral_media_white_test()
iccdev_add_gbd_zero_pcs_test()
iccdev_add_mpe_empty_identity_test()
iccdev_add_pcc_reflectance_observer_range_test()
iccdev_add_pcs_edge_metadata_test()
iccdev_add_xyz_negative_z_test()
iccdev_add_utf8_textarray_test()
iccdev_add_extended_device_colorspace_test()
iccdev_add_unsupported_profile_class_test()
iccdev_add_dpcc_pcc_replacement_test()
iccdev_add_iccviz_metrics_test()
iccdev_add_iccviz_degenerate_test()
iccdev_add_iccviz_pdf_axis_test()
iccdev_add_iccprofileplot_clut_oob_test()
iccdev_add_iccprofileplot_pdf_leak_test()
iccdev_add_iccprofileplot_exitcode_test()
iccdev_add_prmg_hue_normalization_test()
iccdev_add_zip_utf8_text_zlib_test()

function(iccdev_add_script_test NAME TIMEOUT LABELS WORKING_DIRECTORY SCRIPT)
  if(NOT EXISTS "${SCRIPT}")
    message(STATUS "Skipping ${NAME}: ${SCRIPT} not found")
    return()
  endif()

  string(REPLACE "." "-" _out_name "${NAME}")
  add_test(
    NAME "${NAME}"
    COMMAND
      "${CMAKE_COMMAND}" -E env
      ${ICCDEV_TEST_ENV}
      "ICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/${_out_name}"
      "${BASH_EXECUTABLE}" "${SCRIPT}" ${ARGN}
  )
  set_tests_properties("${NAME}" PROPERTIES
    WORKING_DIRECTORY "${WORKING_DIRECTORY}"
    TIMEOUT "${TIMEOUT}"
    LABELS "${LABELS}"
  )
endfunction()

iccdev_add_script_test(
  iccdev.create-profiles
  300
  "iccdev;setup;profiles"
  "${ICCDEV_TESTING_DIR}"
  "${ICCDEV_TESTING_DIR}/CreateAllProfiles.sh"
)
set_tests_properties(iccdev.create-profiles PROPERTIES
  FIXTURES_SETUP iccdev_profiles
)

iccdev_add_script_test(
  iccdev.legacy-run-tests
  300
  "iccdev;legacy;profiles"
  "${ICCDEV_TESTING_DIR}"
  "${ICCDEV_TESTING_DIR}/RunTests.sh"
)
set_tests_properties(iccdev.legacy-run-tests PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

# #1809: the corpus mixes conformant examples, profiles carrying a baselined
# warning, and fixtures that are malformed on purpose. A sweep that only asks
# whether iccDumpProfile exited non-zero cannot tell a deliberate rejection from a
# regression, so the 78 negative fixtures either read as unexplained failures or,
# if the sweep tolerates them, mask a conformant profile that has newly turned
# critical. Testing/qa-profile-manifest.tsv records the expected verdict per
# profile and this test enforces it. It runs after iccdev.create-profiles because
# 133 of the 213 profiles do not exist until that fixture generates them.
# The manifest is expressed in terms of what iccDumpProfile -v reports, so the test
# is only meaningful where that tool was built.
if(TARGET iccDumpProfile)
  iccdev_add_script_test(
    iccdev.qa-profile-manifest
    300
    "iccdev;qa;manifest;profiles;validation;issue-1809;regression;slow"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-qa-profile-manifest.sh"
    check
  )
  set_tests_properties(iccdev.qa-profile-manifest PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

if(ICCDEV_HAS_JSON_RUNTIME_CONFIG_TOOLS AND ICCDEV_HAS_JSON_TOOLS)
  iccdev_add_script_test(
    iccdev.tool-coverage
    300
    "iccdev;tools;asan;regression;slow"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-tool-coverage-baseline.sh"
    --asan
    --skip-hybrid
  )
  set_tests_properties(iccdev.tool-coverage PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )

  iccdev_add_script_test(
    iccdev.hybrid-pipeline
    900
    "iccdev;tools;hybrid;asan;regression;slow"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-hybrid-pipeline-tests.sh"
  )
  set_tests_properties(iccdev.hybrid-pipeline PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

if(TARGET iccFromXml AND TARGET iccApplyNamedCmm AND TARGET iccApplySearch)
  iccdev_add_script_test(
    iccdev.searchvec-uio-regression
    120
    "iccdev;tools;hybrid;search;ub;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-searchvec-uio-regression.sh"
  )
endif()

iccdev_add_script_test(
  iccdev.specsep-tiff-geometry-regression
  120
  "iccdev;specsep;tiff;geometry;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-specsep-tiff-geometry-regression-tests.sh"
)

iccdev_add_script_test(
  iccdev.specsep-usage-exit-code-regression
  120
  "iccdev;specsep;tiff;usage;exitcode;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1514-specsep-usage-exit-code-regression-tests.sh"
)

iccdev_add_script_test(
  iccdev.specsep-cli-args-regression
  120
  "iccdev;specsep;tiff;cli;args;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-specsep-cli-args-regression-tests.sh"
)

iccdev_add_script_test(
  iccdev.dump-profile-header-regression
  120
  "iccdev;dump;profile;header;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-dump-profile-header-regression-tests.sh"
)
set_tests_properties(iccdev.dump-profile-header-regression PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

iccdev_add_script_test(
  iccdev.tiffdump-output-hardening
  120
  "iccdev;tiffdump;output;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-tiffdump-output-hardening-tests.sh"
)
set_tests_properties(iccdev.tiffdump-output-hardening PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

iccdev_add_script_test(
  iccdev.spectral-tiff-preview
  120
  "iccdev;tiff;spectral;preview;regression;slow"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-spectral-tiff-preview-tests.sh"
)

iccdev_add_script_test(
  iccdev.basic-string-regressions
  120
  "iccdev;xml;basic-string;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-basic-string-regression-tests.sh"
)

iccdev_add_script_test(
  iccdev.pawg-report-regressions
  120
  "iccdev;pawg;report;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-pawg-report-regression-tests.sh"
)

if(TARGET iccApplyNamedCmm AND TARGET iccApplyProfiles AND TARGET iccApplySearch)
  iccdev_add_script_test(
    iccdev.json-cfg
    300
    "iccdev;json;cfg;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-json-cfg-tests.sh"
  )
  set_tests_properties(iccdev.json-cfg PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )

  iccdev_add_script_test(
    iccdev.issue-1230-json-threads-regression
    60
    "iccdev;json;cfg;applyprofiles;issue-1230;regression;ubsan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1230-json-threads-regression.sh"
  )

  iccdev_add_script_test(
    iccdev.issue-1121-json-envvars-regression
    60
    "iccdev;json;cfg;envvars;issue-1121;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1121-json-envvars-regression.sh"
  )
endif()

# Guards iccApplyToLink -PCC profile ownership on its error-exit paths (#1336):
# drives the AddXform BadSpaceLink path with tracked profiles under LeakSanitizer
# and fails if a -PCC profile is stranded.  Uses committed test-data profiles, so
# no generated-profile fixture is required.
iccdev_add_script_test(
  iccdev.issue-1336-applytolink-pcc-regression
  60
  "iccdev;tools;applytolink;leak;issue-1336;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1336-applytolink-pcc-regression.sh"
)

# Guards CIccCmm::CheckPCSConnections CIccPcsXform ownership on its Connect
# error path (#1337).  Generates two profiles from committed XML, chains them
# into an unsupported PCS link under LeakSanitizer, and fails on a leak.  No
# -PCC, so this is independent of the iccApplyToLink #1336 fix.
iccdev_add_script_test(
  iccdev.issue-1337-checkpcsconnections-regression
  120
  "iccdev;tools;cmm;leak;issue-1337;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1337-checkpcsconnections-regression.sh"
)

# CI debug helper for the NaN/SixChanInputRef iccApplyToLink PoC.  It preserves
# the original master heap-buffer-overflow replay with extra signature/register
# logging and now expects the NaN range to fail closed before CMM/LUT generation.
iccdev_add_script_test(
  iccdev.applytolink-nan-sixchan-debug
  120
  "iccdev;tools;cmm;applytolink;debug;asan;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-applytolink-nan-sixchan-debug.sh"
)

if(ICC_USE_ZLIB)
  set(_hextext_compression_mode "ON")
else()
  set(_hextext_compression_mode "OFF")
endif()

iccdev_add_script_test(
  iccdev.hextext-compression-ab-regression
  120
  "iccdev;xml;hextext;zlib;compression;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-hextext-compression-ab-regression.sh"
  "${_hextext_compression_mode}"
)

if(TARGET iccApplyProfiles)
  iccdev_add_script_test(
    iccdev.issue-1166-cam-hyperbolicinv-regression
    60
    "iccdev;json;cfg;cam;issue-1166;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1166-cam-hyperbolicinv-regression.sh"
  )
endif()

if(ICCDEV_HAS_JSON_TOOLS)
  iccdev_add_script_test(
    iccdev.json-cli-exercise
    300
    "iccdev;json;cli;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/json-cli-exercise.sh"
  )
  set_tests_properties(iccdev.json-cli-exercise PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )

  iccdev_add_script_test(
    iccdev.json-parser-regressions
    300
    "iccdev;json;parser;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-json-parser-regression-tests.sh"
  )
  set_tests_properties(iccdev.json-parser-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )

  # Issue #1920. iccToJson -sort had no coverage at all, so nothing objected when
  # its scalar path became the most expensive part of the conversion. This pins
  # the properties the sort pass must preserve - ascending keys, content equal to
  # the unsorted document, deterministic and still readable by iccFromJson - so
  # that the scalar copy it performs can be changed without silently altering data.
  iccdev_add_script_test(
    iccdev.json-sort-regression
    300
    "iccdev;json;sort;regression;issue-1920;asan;slow"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-json-sort-regression.sh"
  )
  set_tests_properties(iccdev.json-sort-regression PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

iccdev_add_script_test(
  iccdev.cam-impact-surround-regression
  300
  "iccdev;cam;xml;validation;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-cam-impact-surround-regression.sh"
)
set_tests_properties(iccdev.cam-impact-surround-regression PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

if(TARGET iccFromJson)
  iccdev_add_script_test(
    iccdev.issue-1079-fromjson-soft-failure
    60
    "iccdev;json;fromjson;issue-1079;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1079-fromjson-soft-failure.sh"
  )
endif()

if(ICCDEV_HAS_JSON_TOOLS AND TARGET iccFromXml AND TARGET iccDumpProfile)
  iccdev_add_script_test(
    iccdev.stdobserver-regressions
    300
    "iccdev;xml;json;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-stdobserver-regression-tests.sh"
  )
  set_tests_properties(iccdev.stdobserver-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

iccdev_add_script_test(
  iccdev.mluc-setter-regressions
  300
  "iccdev;mluc;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-mluc-setter-regression-tests.sh"
)
set_tests_properties(iccdev.mluc-setter-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

if(ICCDEV_HAS_JSON_TOOLS AND TARGET iccFromXml AND TARGET iccToXml AND TARGET iccDumpProfile)
  iccdev_add_script_test(
    iccdev.mluc-read-utf16-regressions
    300
    "iccdev;mluc;utf16;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-mluc-read-utf16-regression-tests.sh"
  )
  set_tests_properties(iccdev.mluc-read-utf16-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

iccdev_add_script_test(
  iccdev.mluc-iso-code-regressions
  300
  "iccdev;mluc;iso;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-mluc-iso-code-regression-tests.sh"
)
set_tests_properties(iccdev.mluc-iso-code-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

iccdev_add_script_test(
  iccdev.pcc-zero-illuminant-regressions
  300
  "iccdev;pcc;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-pcc-zero-illuminant-regression-tests.sh"
)
set_tests_properties(iccdev.pcc-zero-illuminant-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

# Issue #1856. Pins both halves of the XML reader's size behaviour at once: a
# profile whose CLUT exceeds libxml2's per-text-node limit must still round
# trip, and the depth, name-length and entity-amplification caps must stay
# armed. The second half exists so that a future attempt to solve a size
# problem with XML_PARSE_HUGE turns this test red instead of quietly widening
# what the parser accepts. Builds the fixture link with iccApplyToLink, so it
# needs the generated profiles.
if(TARGET iccFromXml AND TARGET iccToXml AND TARGET iccApplyToLink)
  iccdev_add_script_test(
    iccdev.xml-parser-regressions
    600
    "iccdev;xml;parser;regression;issue-1856;asan;slow"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-xml-parser-regression-tests.sh"
  )
  set_tests_properties(iccdev.xml-parser-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

if(ICCDEV_HAS_ICCCONNECT)
  iccdev_add_script_test(
    iccdev.iccconnect-search-cost-regressions
    300
    "iccdev;iccconnect;search;cost;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-iccconnect-search-cost-regression-tests.sh"
  )
  set_tests_properties(iccdev.iccconnect-search-cost-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

iccdev_add_script_test(
  iccdev.cam-degenerate-regressions
  300
  "iccdev;cam;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-cam-degenerate-regression-tests.sh"
)
# The #1950 divide-by-zero case needs -fsanitize=float-divide-by-zero from the
# compiler driving the test. A toolchain without it cannot run that case, and the
# script exits 77 so this reports as Skipped instead of a Passed run that checked
# less than its name claims.
set_tests_properties(iccdev.cam-degenerate-regressions PROPERTIES
  SKIP_RETURN_CODE 77
)

if(TARGET iccApplyProfiles)
  iccdev_add_script_test(
    iccdev.issue-1097-applyprofiles-hbo
    120
    "iccdev;tools;applyprofiles;asan;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1097-applyprofiles-hbo.sh"
  )
  set_tests_properties(iccdev.issue-1097-applyprofiles-hbo PROPERTIES
    SKIP_RETURN_CODE 77
  )

  iccdev_add_script_test(
    iccdev.applyprofiles-observer-hbo-regression
    120
    "iccdev;tools;applyprofiles;asan;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-applyprofiles-observer-hbo-regression.sh"
  )
  set_tests_properties(iccdev.applyprofiles-observer-hbo-regression PROPERTIES
    SKIP_RETURN_CODE 77
  )

endif()

iccdev_add_script_test(
  iccdev.issue-1151-pngdump-icc-injection
  60
  "iccdev;tools;pngdump;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1151-pngdump-icc-injection.sh"
)

iccdev_add_script_test(
  iccdev.issue-1383-pngdump-palette-injection
  60
  "iccdev;tools;pngdump;palette;issue-1383;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1383-pngdump-palette-injection.sh"
)

iccdev_add_script_test(
  iccdev.issue-1167-jpegdump-byte-regression
  120
  "iccdev;tools;jpegdump;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1167-jpegdump-byte-regression.sh"
)

iccdev_add_script_test(
  iccdev.issue-1382-jpegdump-app2-reassembly
  60
  "iccdev;tools;jpegdump;app2;issue-1382;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1382-jpegdump-app2-reassembly.sh"
)

iccdev_add_script_test(
  iccdev.issue-1168-pawg-calculator-hbo
  120
  "iccdev;tools;pawg;calculator;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1168-pawg-calculator-hbo.sh"
)

iccdev_add_script_test(
  iccdev.issue-1148-writer-device-regression
  120
  "iccdev;tools;fromcube;fromxml;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1148-writer-device-regression.sh"
)
set_tests_properties(iccdev.issue-1148-writer-device-regression PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

iccdev_add_script_test(
  iccdev.issue-1150-output-failure-regression
  120
  "iccdev;tools;output;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1150-output-failure-regression.sh"
)
set_tests_properties(iccdev.issue-1150-output-failure-regression PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
  SKIP_RETURN_CODE 77
)

iccdev_add_script_test(
  iccdev.issue-1178-fromcube-devnull
  60
  "iccdev;tools;fromcube;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1178-fromcube-devnull-regression.sh"
)

iccdev_add_script_test(
  iccdev.issue-1179-fromcube-regression
  120
  "iccdev;tools;fromcube;mluc;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1179-fromcube-regression.sh"
)

iccdev_add_script_test(
  iccdev.issue-1379-fromcube-conformance
  60
  "iccdev;tools;fromcube;validation;issue-1379;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1379-fromcube-conformance-regression.sh"
)

iccdev_add_script_test(
  iccdev.fromcube-cli-args
  60
  "iccdev;tools;fromcube;cli;domain;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-fromcube-cli-args-regression.sh"
)

iccdev_add_script_test(
  iccdev.issue-1843-fromcube-clut-order
  60
  "iccdev;tools;fromcube;clut;issue-1843;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1843-fromcube-clut-order-regression.sh"
)

if(TARGET iccApplyNamedCmm AND TARGET iccDumpProfile)
  iccdev_add_script_test(
    iccdev.calculator-regressions
    300
    "iccdev;calculator;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-calculator-regression-tests.sh"
  )
  set_tests_properties(iccdev.calculator-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

iccdev_add_script_test(
  iccdev.lut16-zero-curve-regressions
  300
  "iccdev;lut16;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-lut16-zero-curve-regression-tests.sh"
)
set_tests_properties(iccdev.lut16-zero-curve-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

iccdev_add_script_test(
  iccdev.namedcolor-apply-regressions
  300
  "iccdev;namedcolor;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-namedcolor-apply-regression-tests.sh"
)
set_tests_properties(iccdev.namedcolor-apply-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

if(TARGET iccApplyNamedCmm AND TARGET iccRoundTrip)
  iccdev_add_script_test(
    iccdev.v5-namedcmm-regressions
    300
    "iccdev;v5;namedcmm;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-v5-namedcmm-regression-tests.sh"
  )
  set_tests_properties(iccdev.v5-namedcmm-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

if(TARGET iccApplyNamedCmm)
  iccdev_add_script_test(
    iccdev.namedcolor-overprint-regressions
    300
    "iccdev;namedcolor;overprint;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-namedcolor-overprint-regression-tests.sh"
  )
  set_tests_properties(iccdev.namedcolor-overprint-regressions PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
  )
endif()

iccdev_add_script_test(
  iccdev.version-bcd-regressions
  300
  "iccdev;version;bcd;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-version-bcd-regression-tests.sh"
)
set_tests_properties(iccdev.version-bcd-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

iccdev_add_script_test(
  iccdev.profile-visualize-regressions
  300
  "iccdev;profile;visualize;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-profile-visualize-tests.sh"
)
set_tests_properties(iccdev.profile-visualize-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

# #1346: signed atoi() -> unsigned wrap in IccTagXml ParseXml handlers.
# Replays a PoC profile with a negative cicpFields ColorPrimaries attribute
# through iccFromXml; under the integer/implicit-conversion sanitizer the test
# FAILS if UBSan's implicit-integer-sign-change diagnostic reappears, and SKIPS
# cleanly on a plain (non-sanitizer) build.  Uses a committed PoC, so it needs
# no generated-profile fixture.
iccdev_add_script_test(
  iccdev.issue-1346-imageparse-atoi-unsigned-regression
  60
  "iccdev;xml;imageparse;issue-1346;regression;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1346-imageparse-atoi-unsigned-regression.sh"
)

# Replays the #1342 / #1343 fuzz PoCs (negative SeamlessIndicator) through
# iccFromXml and fails if the signed->unsigned implicit conversion in the
# Embedded Height/Normal image ParseXml() handlers reappears.  Meaningful only
# under the integer/implicit-conversion sanitizer build (the script skips
# cleanly otherwise), so it carries the ubsan label.  No profile fixture needed
# -- the trigger XML files are committed test data.
iccdev_add_script_test(
  iccdev.issue-1342-1343-imageparse-signed-unsigned-regression
  120
  "iccdev;imageparse;signed-unsigned;regression;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1342-1343-imageparse-signed-unsigned-regression.sh"
)

# #1845: negative <ProfileVersion> component in the XML importer.  parseVersion()
# used atoi(), so "-12" produced the non-BCD version byte 0xEE, and iccFromXml
# reported "parsed and saved correctly" while writing it.  Unlike the #1342/#1346
# tests this one carries a functional assertion as well as the sanitizer one --
# the saved major-version byte must be a valid BCD pair -- so it is meaningful on
# every build, not only an integer/implicit-conversion one.
iccdev_add_script_test(
  iccdev.issue-1845-profileversion-bcd-regression
  60
  "iccdev;xml;header;issue-1845;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1845-profileversion-bcd-regression.sh"
)

# #1846/#1847: unvalidated CountOfDeviceCoords in the XML namedColor2 reader.
# Case A replays the reported out-of-int-range count and needs the
# integer/implicit-conversion sanitizer.  Case B replays a large POSITIVE count,
# which is the memory-unsafe half the issues do not describe: SetSize() failed,
# its result was discarded, and the entry loop then wrote past the constructor's
# single-entry buffer.  Case B needs no sanitizer at all to be meaningful -- with
# ASan it reports heap-buffer-overflow, without it the corrupted heap metadata
# SEGVs inside malloc -- so this test has teeth in the ordinary CI jobs.
iccdev_add_script_test(
  iccdev.issue-1846-1847-namedcolor-devicecoords-regression
  60
  "iccdev;xml;namedcolor;issue-1846;issue-1847;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1846-namedcolor-devicecoords-regression.sh"
)

# #1851: unvalidated ParametricCurve FunctionType in the XML parametricCurveType
# reader.  atoi() fed SetFunctionType(icUInt16Number), and the surrounding
# `if (!SetFunctionType(...))` could never reject anything because that function
# always returns true.  Case A is the half that has teeth in the ordinary CI
# jobs: FunctionType="65540" narrows to the legal type 4, so the malformed
# attribute was laundered into a valid one and iccFromXml wrote a profile
# byte-identical to the FunctionType="4" control.  Case B replays the reported
# breadcrumb (96948242 -> 20498), which the parameter-count check already
# rejected pre-fix, so it is gated on an integer/implicit-conversion build
# rather than passing vacuously.  A control profile guards against over-
# rejection of legitimate type-4 curves.
iccdev_add_script_test(
  iccdev.issue-1851-parametriccurve-functiontype-regression
  60
  "iccdev;xml;parametriccurve;issue-1851;regression;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1851-parametriccurve-functiontype-regression.sh"
)

# #1898/#1899/#1900/#1901/#1902/#1903: unbounded MPE channel and spectral-step
# counts in the XML and JSON readers.  Six issues, one defect -- the counts are
# u16 and every reader parsed them into an int behind a "!nIn || !nOut" guard
# that rejects zero and nothing else, so 360200 reached storage as 32520.  Case A
# has teeth in the ordinary CI jobs: four documents converted with exit 0 and the
# written profile re-serializes with a channel count the source never contained,
# on the XML side and the JSON side both.  Case B covers the reported breadcrumbs
# whose narrowed value was already caught by a later constraint (the CLUT and the
# two observer elements), so it is gated on an integer/implicit-conversion build
# rather than passing vacuously.  Two controls: a legal count, and 32520 itself --
# the second is what shows the fix refuses unrepresentable counts rather than
# having lowered the channel ceiling.
iccdev_add_script_test(
  iccdev.issue-1898-mpe-channel-narrowing-regression
  120
  "iccdev;xml;json;mpe;issue-1898;issue-1901;issue-1902;regression;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1898-mpe-channel-narrowing-regression.sh"
)

# #1909: the same narrowing at the tag-level readers #1908 left behind -- cicp
# code points, CLUT grid granularity, tag spectral step counts, SparseMatrix
# dimensions and GamutBoundaryDesc channel counts.  Here the cast is explicit at
# every site, so UBSan never saw it and the tell is entirely functional: six
# documents converted with exit 0 and re-serialize carrying a value the source
# never contained (265 -> 9, 65937 -> 401, 65539 -> 3, 65567 -> 31, 66304 -> 768),
# and GridGranularity="258" produced a profile byte-identical to "2".  Each is
# paired with the wrapped value itself as a control, which is what shows the fix
# refuses values that do not fit the field rather than having lowered a ceiling.
# Two further cases are not laundering at all, both from the same unchecked
# CIccSparseMatrix::Init() return: an over-large <SparseMatrix> reached the
# row-start fill with a NULL pointer (that one keys on the exit status, since a
# rejection and a segfault both leave no profile behind), and an array whose
# outputChannels cannot hold its own declared row count used to convert with
# exit 0 while the tool printed "Profile is invalid, but saved correctly".
# The latter is pinned because honouring Init() changes acceptance on purpose.
# Fixtures are mutated from tracked Testing/ XML
# at run time and the script fails if a mutation did not apply, so a donor
# edited upstream cannot turn this into a silent no-op.
iccdev_add_script_test(
  iccdev.issue-1909-xml-narrowing-regression
  120
  "iccdev;xml;tags;issue-1909;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1909-xml-narrowing-regression.sh"
)

# #1954/#1931: the same narrowing at the MPE element readers, which is the
# surface #1908 and #1909 did not cover.  The reported breadcrumbs are negatives
# (FunctionType="-143" -> 65393, "-1" -> 65535), but those wrap to a function
# type the switch already refuses, so the cases with teeth in an ordinary job
# are the overflows that land on a value it accepts: FunctionType="65536" is
# stored and re-serialized as 0, a supported function, and Reserved2="-1" as
# 65535.  Flags="-1" on the spectral CLUT was accepted outright, since
# 0xFFFFFFFF is a value that 32-bit field is willing to hold.
# One case covers the writer, which had to change in the same commit: it cast
# m_flags to unsigned and then printed it with "%d", so a legal Flags of
# 0x80000002 was written back out as "-2147483646".  That round-tripped only
# because the reader's atoi() wrapped the negative straight back, so tightening
# the reader alone would have stopped those profiles loading -- the case asserts
# the attribute is emitted unsigned and survives XML -> ICC -> XML -> ICC.
# Controls pair every rejection with the value it wrapped to (0, 65535, 0) plus
# Flags="2147483650" itself, which must still be accepted: that is what shows
# the fix refuses unrepresentable values rather than having lowered a ceiling.
# Fixtures are mutated from tracked Testing/ XML at run time and the script
# fails if a mutation did not apply, so a donor edited upstream cannot turn
# this into a silent no-op.
iccdev_add_script_test(
  iccdev.issue-1954-mpe-attr-narrowing-regression
  120
  "iccdev;xml;mpe;issue-1954;issue-1931;regression;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1954-mpe-attr-narrowing-regression.sh"
)

# #1954/#1931 follow-up: the same narrowing at the atoi() sites OUTSIDE
# IccMpeXml.cpp, which the test above deliberately left for a separate change.
# As there, the cases with teeth are the overflows rather than the reported
# negatives, because they land on values the rest of the library accepts:
# SpectralRange steps="65567" is stored and re-serialized as 31, BiSpectralRange
# steps="65577" as 41, and SparseMatrixArray matrixType="65540" as 4 --
# icSparseMatrixFloat32, a fully supported type -- with iccFromXml reporting
# "Profile parsed and saved correctly" in every case.
# Two cases cover the writers, which had to change in the same commit. The
# embedded-image tags print two unsigned 32-bit fields with "%d": for
# EncodingFormat that cancels against the reader, so master accepts
# EncodingFormat="-12623518" and re-emits it verbatim, while for
# SeamlessIndicator it does not, so the legal value 2147483650 round-trips to 0
# and is destroyed. A third asserts the ObserverFuncs "end" wavelength keeps its
# fraction, since that reader used atoi() where its own sibling uses atof().
# A fourth pins a writer typo found beside them: the BiSpectralRange format
# string ended "/>\n)" and emitted a stray ')' text node into every profile
# carrying a bi-spectral range.
# Controls pair every rejection with the value it wrapped to (31, 41, 4), so a
# fix that simply lowered a ceiling would fail here.
iccdev_add_script_test(
  iccdev.issue-1954-xml-attr-narrowing-followup-regression
  120
  "iccdev;xml;header;sparsematrix;issue-1954;issue-1931;regression;ubsan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1954-xml-attr-narrowing-followup-regression.sh"
)

# #1810: 'RICC' in the CMM and platform header fields of 15 Testing fixtures.
# 'RICC' was never registered -- icSigRefIccMAX itself carried 0x52494343
# ('RICC') against a comment and a registry that both said 'RIMX', until #1473
# corrected the enum.  Fixtures authored in the meantime kept the bad value, so
# 13 reported "Unregistered CMM signature" and 2 reported "Unknown platform
# signature".  Case A is a text guard against reintroduction and needs no build,
# so it still asserts in jobs that do not produce the tools.  Case B converts
# every corrected fixture and uses the validator itself as the oracle, so the
# registered-signature allow-list is not duplicated in shell.  Because nothing
# should warn after the fix, a control first injects an unregistered 'ZZZZ' and
# requires the finding to appear -- otherwise a validator that had stopped
# checking the field would make case B a vacuous pass.
iccdev_add_script_test(
  iccdev.issue-1810-ricc-header-signatures
  120
  "iccdev;xml;header;signatures;issue-1810;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1810-ricc-header-signatures.sh"
)

# #1853: plain <TextData> silently dropped by the compressed-text XML readers.
# CIccTagXmlZipUtf8Text::ParseXml and CIccTagXmlZipXml::ParseXml scanned the
# sibling list for <HexCompressedData> and, on falling out of that loop, passed
# the now-NULL walking pointer to the plain-text reader.  icXmlParseTextString()
# starts with its own `while (pNode)`, so it did not crash -- it returned true
# having read nothing, and the tag became SetText("").  Fail-open data loss:
# iccFromXml reported success and wrote an empty tag.  Case A covers zut8, case
# B the separate copy of the same code in zxml, and case C closes the loop
# through iccToXml, which re-emits both in the hex form.  A HexCompressedData
# control runs first so over-rejection or a no-zlib build is reported as a skip
# rather than as a #1853 regression.  No sanitizer required.
iccdev_add_script_test(
  iccdev.issue-1853-zip-text-plaintext-regression
  60
  "iccdev;xml;compression;roundtrip;issue-1853;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1853-zip-text-plaintext-regression.sh"
)

# #1356: XML serializer corrupts a zero (NoData) header colour-space signature
# on round-trip.  iccToXml wrote the literal "NULL" for a 0x00000000 data
# colour space / PCS; iccFromXml then reparsed "NULL" to 0x4E554C4C, so the
# value did not survive a round-trip (iccFromJson kept it as NoData because the
# JSON writer guards the zero case).  Round-trips a committed NoData profile
# through iccToXml -> iccFromXml and fails if the corrupted 0x4E554C4C value
# reappears.  Plain functional test -- skips cleanly if the tools are absent.
iccdev_add_script_test(
  iccdev.issue-1356-xml-colorspace-null-roundtrip-regression
  60
  "iccdev;xml;colourspace;roundtrip;issue-1356;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1356-xml-colorspace-null-roundtrip-regression.sh"
)

# #1362: iccV5DspObsToV4Dsp must emit the rXYZ/gXYZ/bXYZ matrix-column tags as
# XYZType ('XYZ '), not s15Fixed16ArrayType ('sf32').  Runs the producer on the
# committed V5 display + observer inputs and fails if any of the three tags is
# still 'sf32' on disk.  Plain functional test -- skips cleanly if the tool or
# fixtures are absent.
iccdev_add_script_test(
  iccdev.issue-1362-v5dspobs-xyztype-regression
  60
  "iccdev;tools;v5dspobs;xyztype;issue-1362;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1362-v5dspobs-xyztype-regression.sh"
)

# #1361: generalises #1356 to the remaining header signature fields that the
# JSON serializer guards but the XML serializer did not -- PreferredCMMType
# (cmmId), ProfileDeviceClass (deviceClass) and ProfileCreator (creator).
# Zeroes those three fields in a real profile and fails if any is corrupted to
# 0x4E554C4C ("NULL") on an iccToXml -> iccFromXml round-trip. Plain
# functional test -- skips cleanly if the tools/fixture are absent.
iccdev_add_script_test(
  iccdev.issue-1361-header-sig-roundtrip-regression
  60
  "iccdev;xml;signature;roundtrip;issue-1361;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1361-header-sig-roundtrip-regression.sh"
)

# #1843: carries #1356/#1361 past the profile header into the tag and element
# writers that had the same unguarded icGetSig*(0) -> "NULL" -> 0x4E554C4C hole
# on BOTH the XML and JSON sides -- profileSequenceDescType's manufacturer/model/
# technology, signatureType tags, and BAcs/EAcs elements.  The pseq case is what
# made #1843 look like an iccFromCube defect: iccFromCube leaves the technology
# as icSigUndefined, so every profile it wrote came back NonCompliant with
# "Unknown 'NULL' = 4E554C4C".  The signatureType case is silent -- four payload
# bytes change and the profile still validates -- so that section asserts a
# byte-exact round trip rather than a clean validation report.  Note the two
# earlier tests only had to cover XML: for those header fields JSON already
# guarded the zero case, which is exactly the asymmetry the companion CodeQL
# query keys off.  Here neither serializer guarded, so both are exercised.
# Plain functional test -- skips cleanly if the tools/fixtures are absent.
iccdev_add_script_test(
  iccdev.issue-1843-zero-signature-roundtrip
  90
  "iccdev;xml;json;signature;roundtrip;issue-1843;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1843-zero-signature-roundtrip-regression.sh"
)

# #1355 - matrix/TRC (PCSXYZ) profiles must round-trip near-exactly. Guards the
# two CMM/eval fixes (ConnectFirst actual->internal XYZ rescale; native-PCS
# round-trip connection): iccRoundTrip RT1 mean dE was ~20 before, ~0 after.
# Skips cleanly if iccRoundTrip is not built.
iccdev_add_script_test(
  iccdev.issue-1355-roundtrip-mattrc-invertibility-regression
  120
  "iccdev;cmm;roundtrip;issue-1355;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1355-roundtrip-mattrc-invertibility-regression.sh"
)

# #1395 - CIccInfo::GetProfileClassSigName appended a glued "Class" suffix to its
# display labels ("DisplayClass", "OutputClass", ...), which is not an ICC.1/ICC.2
# spec identifier and is redundant in every (display-only) caller. The suffix was
# dropped; this asserts iccDumpProfile now prints the de-suffixed name ("Display",
# "Output", ...) for one committed profile per deviceClass, with no trailing "Class".
iccdev_add_script_test(
  iccdev.issue-1395-profileclass-signame
  120
  "iccdev;tools;dump;signame;issue-1395;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1395-profileclass-signame.sh"
)

# #1729 - Testing/Display/LCDDisplay.xml and LaserProjector.xml stored their
# spectralDataInfoType tag under the unregistered signature 'smwi' (0x736D7769),
# so a fromXml -> dump round-trip reported "Unknown 'smwi'" and never populated
# the registered 'sdin' (spectralDataInfoTag) slot. The fixtures were corrected to
# 'sdin'; this rebuilds each with iccFromXml and asserts iccDumpProfile recognizes
# the tag as spectralDataInfoTag and no longer emits the "Unknown 'smwi'" line.
iccdev_add_script_test(
  iccdev.issue-1729-spectral-data-info-sig
  120
  "iccdev;tools;dump;fromxml;signame;issue-1729;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1729-spectral-data-info-sig.sh"
)

# #1730 - when iccApplyToLink built a V4 device link, the deviceModelDesc block
# copied the model text into psd.m_deviceMfgDesc (a copy/paste of the
# manufacturer block), clobbering the pseq manufacturer description and leaving
# the model description at its default. The tool still exits 0 in that state and
# the sibling applytolink-v4-missing-device-descriptions test uses a source with
# no dmnd/dmdd, so neither can observe it. This derives a fixture carrying
# distinct dmnd/dmdd, builds a V4 link, and asserts each description lands in
# its own profileSequenceDesc slot.
iccdev_add_script_test(
  iccdev.issue-1730-applytolink-pseq-desc
  180
  "iccdev;tools;applytolink;pseq;issue-1730;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1730-applytolink-pseq-desc.sh"
)

# #1394 - CIccTagXmlNum::ToXml walked m_nSize without a bound, so a malformed
# ui08/ui16/ui32/ui64 array drove an unbounded serialization loop (CWE-400/834):
# a ~16 MB ui08 tag ballooned to ~80 MB of XML. Asserts the 0xffffff cap refuses
# an over-cap array while an in-bounds array still serializes. python3-gated.
iccdev_add_script_test(
  iccdev.issue-1394-xml-num-array-dos
  120
  "iccdev;xml;security;dos;issue-1394;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1394-xml-num-array-dos.sh"
)

# #1406 - CIccPcsXform::pushXyzLumToXyz computed 1.0/XYZLum[1] (illuminant Y) for
# the inverse-luminance PCS step. A profile with a zero svcn illuminant luminance
# made that a divide by zero (UBSan float-divide-by-zero, IccCmm.cpp:3048),
# yielding an infinite scale that corrupts every PCS sample. Replays the fuzz PoC
# through iccApplyProfiles with a +1000 luminance intent and fails if the
# divide-by-zero reappears. Only meaningful under a float-divide-by-zero
# regression image; SKIPS otherwise.
if(TARGET iccApplyProfiles)
  iccdev_add_script_test(
    iccdev.issue-1406-roundtrip-xyzlum-dbz
    120
    "iccdev;cmm;security;ub;issue-1406;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1406-roundtrip-xyzlum-dbz.sh"
   )
endif()

# #1410 - icYxy2XYZVector (IccEncoding.cpp) converts Yxy white/media/surround
# chromaticities to XYZ as Y*x/y and Y*(1-x-y)/y. Loading a colorEncodingParams
# ("cenc") profile whose chromaticity y coordinate is zero made that a divide by
# zero (UBSan float-divide-by-zero, IccEncoding.cpp:138/140), poisoning the header
# illuminant and derived XYZ tags with inf/NaN. Adds the fuzz PoC as a transform
# profile through iccApplyToLink and fails if the divide-by-zero reappears. Only
# meaningful under a float-divide-by-zero regression image; SKIPS otherwise.
iccdev_add_script_test(
  iccdev.issue-1410-encoding-yxy-xyz-dbz
  120
  "iccdev;encoding;security;ub;issue-1410;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1410-encoding-yxy-xyz-dbz.sh"
 )

# #1985 - CIccDefaultEncProfileConverter::ConvertFromParams (IccEncoding.cpp)
# deleted the luma/chroma matrix element it had only borrowed from the profile's
# colorEncodingParams ("cept") struct via FindElemOfType, leaving the entry in the
# element list. icConvertEncodingProfile then destroyed the struct, and
# CIccTagStruct::Cleanup wrote a null parent into the freed chunk before deleting
# it a second time -- ASAN heap-use-after-free, SCARINESS 52
# (8-byte-write-heap-use-after-free). The API-level regression is the
# iccdev.encoding-lumamtx-double-free CTest; this one covers reachability, adding
# a 'cenc' PoC profile as a transform profile through iccApplyToLink so
# CIccXform::Create routes it into the converter. It asserts on two signals so it
# is meaningful on every lane: a crash (measured 3/3 as exit 139 on an
# unsanitized clang Debug build, against 255 for the fixed library's ordinary
# rejection) and an ASAN report naming the converter. The ASAN lanes are the
# authoritative ones -- the crash depends on the allocator recycling the chunk,
# which is also what defeats glibc's tcache double-free check.
iccdev_add_script_test(
  iccdev.issue-1985-encoding-lumamtx-uaf
  120
  "iccdev;encoding;security;asan;issue-1985;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1985-encoding-lumamtx-uaf-regression-tests.sh"
 )

# #1421 - CIccCfgProfileSequence::fromArgs decodes a packed rendering-intent code
# whose tens field selects the xform LUT type (intent 65535 -> nType 53), and
# CIccCfgProfile::toJson reads it back via icGetTransformName. icXformLutType only
# defines 0x0..0xD, so at the bisected commit (b90ac39) toJson loaded value 53 as
# an icXformLutType -- UBSan -fsanitize=enum undefined behaviour. #1400 (b087101)
# pins the decoded type to valid range before storing it; this test replays the
# 65535 repro through iccApplyProfiles -exportcfg and fails if the invalid-enum
# load reappears. -exportcfg runs the toJson load before any file is opened, so
# placeholder names suffice. Only meaningful under a -fsanitize=undefined build
# (ENABLE_UBSAN/ENABLE_SANITIZERS); SKIPS otherwise.
if(TARGET iccApplyProfiles)
  iccdev_add_script_test(
    iccdev.issue-1421-cfg-transform-enum-load
    120
    "iccdev;cmm;config;security;ub;issue-1421;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1421-cfg-transform-enum-load.sh"
   )
endif()

# #1422 - CIccCfgDataApply::fromArgs (iccApplyNamedCmm -exportcfganddata) narrowed
# the destination encoding selector straight from the command line to an
# icFloatColorEncoding and then read it in icIsJsonColorEncoding. The enum only
# defines 0..icEncodeUnknown, so an out-of-range selector (the fuzz repro passes 9)
# was loaded as an invalid enum -- UBSan -fsanitize=enum undefined behaviour at
# IccCmmConfig.cpp:345. The fix range-checks the raw int before narrowing. The
# selector is parsed before any data/profile file is opened, so this test drives
# the exact "9" argument with placeholder names and fails if the invalid-enum load
# reappears. Only meaningful under a -fsanitize=undefined build
# (ENABLE_UBSAN/ENABLE_SANITIZERS); SKIPS otherwise.
if(TARGET iccApplyNamedCmm)
  iccdev_add_script_test(
    iccdev.issue-1422-cfg-encoding-enum-load
    120
    "iccdev;cmm;config;security;ub;issue-1422;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1422-cfg-encoding-enum-load.sh"
   )

  # #1674: iccApplyNamedCmm accepted argument lists it did not honour -- extra
  # arguments after "-cfg <path>", and anything trailing the profile/intent pairs,
  # were parsed past and the transform ran as though they had not been supplied,
  # exiting 0. Pins rejection of both, alongside the encoding, interpolation,
  # intent and -ENV forms that were already strict, so a later relaxation of any
  # of them is caught here.
  iccdev_add_script_test(
    iccdev.applynamedcmm-cli-args
    120
    "iccdev;cmm;config;cli;security;issue-1674;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-applynamedcmm-cli-args-regression.sh"
   )
endif()

# Legacy v2 namedColor2Type stores each rootName in a fixed 32-byte field. A
# malformed profile can fill all 32 bytes without a NUL terminator; the reader
# must force termination before iccApplyNamedCmm echoes the selected name. The
# test uses an all-ASCII generated fixture so the regression signal is readable
# and deterministic instead of terminal-dependent replacement characters.
if(TARGET iccApplyNamedCmm)
  iccdev_add_script_test(
    iccdev.ncl2-nonul-ascii-regression
    120
    "iccdev;cmm;namedcolor;security;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-ncl2-nonul-ascii-regression.sh"
   )
endif()

# #1405 - CIccEvalCompare::EvaluateProfile walked an nGran^ndim device-sample
# grid with no bound, so a 6-channel profile at the default granularity 33 ran
# 33^6 (~1.3 billion) samples for over an hour (CWE-400). Regenerates the 6CLR
# SixChanCameraRef fixture from its tracked XML and round-trips it under a short
# timeout: pre-fix the tool never terminates, post-fix it reports "Too many
# samples used"; a 3-channel control still round-trips. iccFromXml-gated. The
# CTest ceiling sits above the script's internal 45s DoS timeout.
iccdev_add_script_test(
  iccdev.issue-1405-roundtrip-sample-cap
  180
  "iccdev;roundtrip;security;dos;issue-1405;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1405-roundtrip-sample-cap.sh"
)

# #1781 - the iccApplyToLink QA matrix, recalibrated to run in CI, plus the
# three defects working through it exposed. CIccCLUT::Init limits both the node
# count and the node count times the output channel count, but the tool's own
# cap omitted the output-channel factor -- so a CMYK source at the documented
# maximum lut_size of 255 passed the cap and was then refused by Init. Init
# leaves m_pData NULL while m_nNumPoints is already committed and NewCLUT
# discards its result, so the writer got a NULL data pointer with a non-zero
# countdown and the first node memcpy'd to address zero: SIGSEGV on both the v4
# and v5 paths. Also pins the two range defects -- a zero-width or reversed
# range wrote a file and exited 0 (QA cases 19/20), and a v4 link took a
# restricted range it has nowhere to record, declaring [0,1] while sampling
# something else (QA case 04). Argument rejections 13-18 are pinned so the new
# range checks cannot disturb them. Builds its own CMYK source from the tracked
# CMYK-3DLUTs.xml, so no Testing/ profile generation is required.
# iccFromXml-gated.
iccdev_add_script_test(
  iccdev.issue-1781-applytolink-qa-matrix
  300
  "iccdev;tools;applytolink;security;issue-1781;regression;slow"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1781-applytolink-qa-matrix.sh"
)

# Describe sink + options API smoke test. Verifies byte-equivalence
# between the legacy std::string Describe() and the new
# Describe(IDescribeSink&, const DescribeOptions&) virtual, plus
# truncation/suppression honouring of the structured options.
# Uses the always-shipped sRGB v4 profile from Testing/.
if(TARGET iccDescribeSinkTest)
  add_test(
    NAME iccdev.describe-sink-api
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "LD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
      "DYLD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}"
      $<TARGET_FILE:iccDescribeSinkTest>
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )
  set_tests_properties(iccdev.describe-sink-api PROPERTIES
    TIMEOUT 60
    LABELS  "iccdev;describe;api;sink;options"
  )
endif()
