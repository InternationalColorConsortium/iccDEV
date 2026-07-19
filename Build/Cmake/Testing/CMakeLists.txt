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

# Guards CIccCmmSearch::AddXform profile ownership (#1332): the search CMM
# rejects a NamedColor profile (no search LUT) and, like the base AddXform
# contract (#1327), must free it.  The helper feeds a NamedColor profile through
# the inherited filename overload, which opens it internally, so a profile the
# override fails to free is unreachable and reported by the ASan/LSan CI job.
function(iccdev_add_cmmsearch_namedcolor_ownership_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
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

if(WIN32)
  function(iccdev_add_windows_batch_test NAME TIMEOUT LABELS SCRIPT)
    set(_one_value_args EXPECTED_PROFILE_PARSE_COUNT EXPECTED_OUTPUT FORBIDDEN_REGEX)
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
      EXPECTED_PROFILE_PARSE_COUNT 130
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
        "-DICCDEV_EXPECTED_ITEM_COUNT=31"
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
  iccdev_add_embedio_read8_bounds_test()
  iccdev_add_fileio_getlength_position_test()
  iccdev_add_profile_write_failure_test()
  iccdev_add_fileio_seek_tell_test()
  iccdev_add_tagdata_zlib_roundtrip_test()
  iccdev_add_parser_restore_calls_test()
  iccdev_add_tag_layout_shared_data_test()
  iccdev_add_pawg_q2_tonecurves_test()
  iccdev_add_pawg_q4_characterization_test()
  iccdev_add_pawg_q4_xyz_pcs_decode_test()
  iccdev_add_pawg_q1q3_relative_intent_test()
  iccdev_add_mpecalc_matrix_overflow_test()
  iccdev_add_cmmsearch_namedcolor_ownership_test()
  iccdev_add_xform_create_tag_ownership_test()
  iccdev_add_xform_abstorel_adjust_test()
  iccdev_add_colorimetry_methods_test()
  iccdev_add_getvalues_nstart_test()
  iccdev_add_gbd_zero_pcs_test()
  iccdev_add_utf8_textarray_test()
  iccdev_add_extended_device_colorspace_test()
  iccdev_add_dpcc_pcc_replacement_test()
  iccdev_add_iccviz_metrics_test()
  iccdev_add_iccviz_degenerate_test()
  iccdev_add_iccviz_pdf_axis_test()
  iccdev_add_iccprofileplot_clut_oob_test()
  iccdev_add_iccprofileplot_pdf_leak_test()
  iccdev_add_iccprofileplot_exitcode_test()
  iccdev_add_applytolink_invalid_arg_test()
  iccdev_add_windows_iccdevcmm_smoke_test()

  return()
endif()

if(NOT BASH_EXECUTABLE)
  message(WARNING "bash not found; iccDEV CTest script tests are disabled.")
  return()
endif()

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
  "LLVM_PROFILE_FILE=/dev/null"
  "ICCDEV_TOOLS_DIR=${ICCDEV_TOOLS_DIR}"
  "ICCDEV_TESTING_DIR=${ICCDEV_TESTING_DIR}"
  "ICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "ICCDEV_REQUIRE_JSON_ROUNDTRIP=${ICCDEV_REQUIRE_JSON_ROUNDTRIP}"
)

iccdev_add_iccconnect_thread_test()
iccdev_add_iccconnect_config_parser_test()
iccdev_add_embedio_read8_bounds_test()
iccdev_add_fileio_getlength_position_test()
iccdev_add_profile_write_failure_test()
iccdev_add_fileio_seek_tell_test()
iccdev_add_tagdata_zlib_roundtrip_test()
iccdev_add_parser_restore_calls_test()
iccdev_add_tag_layout_shared_data_test()
iccdev_add_pawg_q2_tonecurves_test()
iccdev_add_pawg_q4_characterization_test()
iccdev_add_pawg_q4_xyz_pcs_decode_test()
iccdev_add_pawg_q1q3_relative_intent_test()
iccdev_add_mpecalc_matrix_overflow_test()
iccdev_add_cmmsearch_namedcolor_ownership_test()
iccdev_add_xform_create_tag_ownership_test()
iccdev_add_xform_abstorel_adjust_test()
iccdev_add_colorimetry_methods_test()
iccdev_add_getvalues_nstart_test()
iccdev_add_gbd_zero_pcs_test()
iccdev_add_utf8_textarray_test()
iccdev_add_extended_device_colorspace_test()
iccdev_add_dpcc_pcc_replacement_test()
iccdev_add_iccviz_metrics_test()
iccdev_add_iccviz_degenerate_test()
iccdev_add_iccviz_pdf_axis_test()
iccdev_add_iccprofileplot_clut_oob_test()
iccdev_add_iccprofileplot_pdf_leak_test()
iccdev_add_iccprofileplot_exitcode_test()

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

if(ICCDEV_HAS_JSON_RUNTIME_CONFIG_TOOLS AND ICCDEV_HAS_JSON_TOOLS)
  iccdev_add_script_test(
    iccdev.tool-coverage
    300
    "iccdev;tools;asan;regression"
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
  "iccdev;tiff;spectral;preview;regression"
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
# 0x4E554C4C ("NULL") on an iccToXml -> iccFromXml round-trip.  Companion to the
# static CodeQL query signature-serialized-without-zero-guard.ql.  Plain
# functional test -- skips cleanly if the tools/fixture are absent.
iccdev_add_script_test(
  iccdev.issue-1361-header-sig-roundtrip-regression
  60
  "iccdev;xml;signature;roundtrip;issue-1361;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1361-header-sig-roundtrip-regression.sh"
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
