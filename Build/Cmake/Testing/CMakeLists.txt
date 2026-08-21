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
          --label-exclude known-red
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

# Regression executables link ${TARGET_LIB_ICCPROFLIB}, functions and exported
# global data alike. There used to be a second link target here,
# ${ICCDEV_TEST_LIB_ICCPROFLIB}, which resolved to IccProfLib2-static on Windows
# shared builds because WINDOWS_EXPORT_ALL_SYMBOLS auto-exports functions but not
# data (#1888): a test referencing icMsgValidateWarning or icD50XYZ emitted a
# direct data reference with no __imp_ indirection and failed with LNK2019. #2219
# gave those eight globals real dllexport/dllimport via ICCPROFLIB_DATA_API, so
# #2154 retired the fallback here and in the three tools that carried the same
# workaround. See docs/build.md, "Exported global data".

add_custom_target(check-fast
  COMMAND "${CMAKE_CTEST_COMMAND}"
          ${ICCDEV_CTEST_CONFIG_ARGS}
          --label-exclude slow
          --label-exclude known-red
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

function(iccdev_add_pcs_unitfloat_source_encoding_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPcsUnitFloatSourceEncodingTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcs-unitfloat-source-encoding.cpp"
  )
  target_link_libraries(iccPcsUnitFloatSourceEncodingTest PRIVATE ${TARGET_LIB_ICCPROFLIB})

  add_dependencies(check iccPcsUnitFloatSourceEncodingTest)

  # No fixture and no profile: the assertions call the two static encoding
  # converters directly, so nothing is read from disk and the whole test is a
  # few dozen scalar conversions. The 120 timeout matches its siblings rather
  # than reflecting any measured need.
  if(WIN32)
    add_test(
      NAME iccdev.pcs-unitfloat-source-encoding
      COMMAND "$<TARGET_FILE:iccPcsUnitFloatSourceEncodingTest>"
    )
  else()
    add_test(
      NAME iccdev.pcs-unitfloat-source-encoding
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccPcsUnitFloatSourceEncodingTest>"
    )
  endif()
  set_tests_properties(iccdev.pcs-unitfloat-source-encoding PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 120
    LABELS "iccdev;iccprofLib;cmm;encoding;issue-2146;regression"
  )
  if(WIN32)
    set(_pcs_unitfloat_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPcsUnitFloatSourceEncodingTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcs_unitfloat_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcs-unitfloat-source-encoding PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcs_unitfloat_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_ndlut_input_channel_mismatch_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccNDLutInputChannelMismatchTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/ndlut-input-channel-mismatch.cpp"
  )
  target_link_libraries(iccNDLutInputChannelMismatchTest PRIVATE ${TARGET_LIB_ICCPROFLIB})

  add_dependencies(check iccNDLutInputChannelMismatchTest)

  # No fixture argument: the test builds all three profiles in memory.  The AFL
  # input that reported #2119 is a 164 KB 13-channel profile with one mutated
  # header byte, which is both larger than the CFL/AFL smoke seed caps would keep
  # and far more profile than the assertion needs.
  if(WIN32)
    add_test(
      NAME iccdev.ndlut-input-channel-mismatch
      COMMAND "$<TARGET_FILE:iccNDLutInputChannelMismatchTest>"
    )
  else()
    add_test(
      NAME iccdev.ndlut-input-channel-mismatch
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccNDLutInputChannelMismatchTest>"
    )
  endif()
  set_tests_properties(iccdev.ndlut-input-channel-mismatch PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 120
    LABELS "iccdev;iccprofLib;cmm;regression"
  )
  if(WIN32)
    set(_ndlut_input_channel_mismatch_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccNDLutInputChannelMismatchTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _ndlut_input_channel_mismatch_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.ndlut-input-channel-mismatch PROPERTIES
      ENVIRONMENT_MODIFICATION "${_ndlut_input_channel_mismatch_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_cmmsearch_rebegin_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCmmSearchRebeginTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/cmmsearch-rebegin.cpp"
  )
  target_link_libraries(iccCmmSearchRebeginTest PRIVATE ${TARGET_LIB_ICCPROFLIB})

  add_dependencies(check iccCmmSearchRebeginTest)

  # Both profiles drive the same assertion about Begin(), so this is not a
  # regression/control pair the way cmmsearch-init-pcc is: the v5 profile carries
  # an 'svcn' and takes the PCC-resolution path through Begin(), the v4 one does
  # not, and the idempotency guard has to hold on both.
  set(_cmmsearch_rebegin_cases
    "d65-svcn=${ICCDEV_TESTING_DIR}/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
    "v4-no-svcn=${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
  )

  foreach(_case IN LISTS _cmmsearch_rebegin_cases)
    string(REGEX REPLACE "=.*$" "" _case_name "${_case}")
    string(REGEX REPLACE "^[^=]*=" "" _case_profile "${_case}")

    if(WIN32)
      add_test(
        NAME iccdev.cmmsearch-rebegin.${_case_name}
        COMMAND "$<TARGET_FILE:iccCmmSearchRebeginTest>" "${_case_profile}"
      )
    else()
      add_test(
        NAME iccdev.cmmsearch-rebegin.${_case_name}
        COMMAND
          "${CMAKE_COMMAND}" -E env
          ${ICCDEV_TEST_ENV}
          "$<TARGET_FILE:iccCmmSearchRebeginTest>"
          "${_case_profile}"
      )
    endif()
    set_tests_properties(iccdev.cmmsearch-rebegin.${_case_name} PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
      TIMEOUT 120
      LABELS "iccdev;iccprofLib;cmmsearch;regression"
    )
    if(WIN32)
      set(_cmmsearch_rebegin_windows_env_mods
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCmmSearchRebeginTest>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      )
      foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
        list(APPEND _cmmsearch_rebegin_windows_env_mods
          "PATH=path_list_prepend:${_runtime_path}")
      endforeach()
      set_tests_properties(iccdev.cmmsearch-rebegin.${_case_name} PROPERTIES
        ENVIRONMENT_MODIFICATION "${_cmmsearch_rebegin_windows_env_mods}"
      )
    endif()
  endforeach()

  # The three-profile branch of Begin() fails quietly: a second call survives,
  # returns icCmmStatOk and leaves the results alone while leaking the previous
  # CIccApplyCmm, so the cases above cannot see it.  Only LeakSanitizer can, and
  # both the CI tool-test job and ICCDEV_TEST_ENV export
  # ASAN_OPTIONS=...detect_leaks=0, so run the same binary once more with
  # detect_leaks=1 forced for this process alone -- the same approach
  # iccdev.cmmsearch-namedcolor-ownership uses, including its MSVC carve-out.
  if(MSVC)
    message(STATUS
      "Skipping iccdev.cmmsearch-rebegin-leak: MSVC AddressSanitizer "
      "does not support LeakSanitizer detect_leaks=1")
    return()
  endif()

  add_test(
    NAME iccdev.cmmsearch-rebegin-leak
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "ASAN_OPTIONS=detect_leaks=1"
      "$<TARGET_FILE:iccCmmSearchRebeginTest>"
      "${ICCDEV_TESTING_DIR}/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc"
      "--three-profile-only"
  )
  set_tests_properties(iccdev.cmmsearch-rebegin-leak PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 120
    LABELS "iccdev;iccprofLib;cmmsearch;leak;regression"
  )
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

# Guards the one-level per-tag load that wxProfileDump's "open embedded
# profile in its own window" feature depends on (FindTag per tag rather than
# ReadAll(), a populated copy after, the copy outliving the profile it was
# copied from, the load never descending past one level, and the nLoaded==0
# condition that drives the second-level dump fallback). See the long comment
# at the top of .github/ci/regression/embedded-profile-onelevel-load.cpp for
# the full story. This test links IccProfLib alone and calls functions only.
#
# Most of what this test checks is an in-memory nested profile built and
# saved to a scratch file at runtime (path passed as argv[1]), so it needs no
# corpus fixture and always runs. A smaller set of checks additionally uses
# the fixture Testing/hybrid/ICC/CMYK_Hybrid_Profile.icc, generated by
# Testing/hybrid/BuildAndTest.sh from a tracked XML source; it is not
# produced on every CI leg (macOS runs "CreateAllProfiles only", Windows has
# no hybrid step, and even the Linux hybrid step tolerates its own failure),
# so the test exits 77 and prints SKIP when the fixture is simply absent --
# SKIP_RETURN_CODE below turns that into a ctest Skipped result instead of a
# false pass or fail. The in-memory checks always run before that fixture
# probe, so their pass/fail is never affected by the fixture's presence.
function(iccdev_add_embedded_profile_onelevel_load_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccEmbeddedProfileOneLevelLoadTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/embedded-profile-onelevel-load.cpp"
  )
  target_link_libraries(iccEmbeddedProfileOneLevelLoadTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccEmbeddedProfileOneLevelLoadTest)

  add_test(
    NAME iccdev.embedded-profile-onelevel-load
    COMMAND
      "$<TARGET_FILE:iccEmbeddedProfileOneLevelLoadTest>"
      "${ICCDEV_TEST_OUTDIR}/embedded-profile-onelevel-nested-scratch.icc"
  )
  set_tests_properties(iccdev.embedded-profile-onelevel-load PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;embedded-profile;wxProfileDump;regression"
    SKIP_RETURN_CODE 77
  )
  if(WIN32)
    set(_embedded_profile_onelevel_load_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccEmbeddedProfileOneLevelLoadTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _embedded_profile_onelevel_load_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.embedded-profile-onelevel-load PROPERTIES
      ENVIRONMENT_MODIFICATION "${_embedded_profile_onelevel_load_windows_env_mods}"
    )
  endif()
endfunction()

# Guards CIccTagEmbeddedProfile::DetachIO() (IccProfLib/IccTagEmbedIcc.h/.cpp).
# CIccProfile::Detach() (IccProfLib/IccProfile.cpp) calls pTag->DetachIO() on
# every one of its loaded tags before freeing its own IO, but the base
# CIccTag::DetachIO() (IccProfLib/IccTagBasic.h) is an empty virtual that
# nothing overrode -- CIccTagEmbeddedProfile included. An embedded profile's
# inner CIccProfile can hold a CIccEmbedIO wrapping its PARENT's IO
# (CIccTagEmbeddedProfile::Read, IccProfLib/IccTagEmbedIcc.cpp), so detaching
# the outer profile left the inner one still reporting HasIO()==true over IO
# the outer had just deleted -- any later tag load on it read through freed
# memory. The test builds a 2-level nested embedded profile in memory
# (pTop -> pMid -> pLeaf), loads pMid's copyrightTag and its own
# embeddedV5ProfileTag (reaching pLeaf) before detaching pTop, and asserts:
# pMid and pLeaf both report HasIO()==false afterwards (the fix, and its
# 2-level cascade); a still-unloaded tag on either (pMid's luminanceTag,
# pLeaf's copyrightTag) comes back NULL/false rather than succeeding through
# freed memory; and pMid's already-loaded copyrightTag survives the detach
# with its original text intact. No corpus fixture -- entirely in-memory --
# so this cannot be silently disabled by a missing fixture.
function(iccdev_add_embedded_profile_detachio_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccEmbeddedProfileDetachIoTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/embedded-profile-detachio.cpp"
  )
  target_link_libraries(iccEmbeddedProfileDetachIoTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccEmbeddedProfileDetachIoTest)

  add_test(
    NAME iccdev.embedded-profile-detachio
    COMMAND
      "$<TARGET_FILE:iccEmbeddedProfileDetachIoTest>"
      "${ICCDEV_TEST_OUTDIR}/embedded-profile-detachio-scratch.icc"
  )
  set_tests_properties(iccdev.embedded-profile-detachio PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;embedded-profile;detachio;regression"
  )
  if(WIN32)
    set(_embedded_profile_detachio_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccEmbeddedProfileDetachIoTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _embedded_profile_detachio_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.embedded-profile-detachio PROPERTIES
      ENVIRONMENT_MODIFICATION "${_embedded_profile_detachio_windows_env_mods}"
    )
  endif()
endfunction()

# Issue #2128: the two CIccProfile release paths that skipped the DetachIO()
# notification the sibling test above depends on. CIccProfile::Detach()
# (IccProfLib/IccProfile.cpp) notified tags only in its owning branch, and
# CIccProfile::CopyAttach(nullptr) notified nothing at all -- so an embedded
# profile loaded through a borrowed IO was left reporting HasIO()==true over
# an IO its parent had released. CopyAttach() is the path the library actually
# exercises (IccProfLib/IccCmm.cpp borrows the caller's IO with
# CopyAttach(&Profile, true) and returns it with CopyAttach(nullptr)), so a fix
# confined to Detach() would not have reached it. Cleanup() also left
# m_bSharedIO set after clearing m_pAttachIO, leaving a reused profile where
# Detach() took the shared branch and returned true having done nothing.
# The test drives both release paths against an in-memory fixture and asserts:
# the inner profile reports HasIO()==false afterwards (the fix, on each path
# independently); a tag loaded before the release keeps its data; a tag never
# loaded comes back NULL rather than reading through the released IO; the
# LENDER still has usable IO afterwards (borrowed IO is dropped, never deleted
# -- this is what catches the shared branch being folded into the owning one);
# and Detach() on a reused, formerly-shared profile now returns false. No
# corpus fixture -- entirely in-memory -- so this cannot be silently disabled
# by a missing fixture.
function(iccdev_add_profile_shared_io_detach_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccProfileSharedIoDetachTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/profile-shared-io-detach.cpp"
  )
  target_link_libraries(iccProfileSharedIoDetachTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccProfileSharedIoDetachTest)

  add_test(
    NAME iccdev.profile-shared-io-detach
    COMMAND "$<TARGET_FILE:iccProfileSharedIoDetachTest>"
  )
  set_tests_properties(iccdev.profile-shared-io-detach PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;embedded-profile;shared-io;issue-2128;regression"
  )
  if(WIN32)
    set(_profile_shared_io_detach_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccProfileSharedIoDetachTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _profile_shared_io_detach_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.profile-shared-io-detach PROPERTIES
      ENVIRONMENT_MODIFICATION "${_profile_shared_io_detach_windows_env_mods}"
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
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccToInternalFixedIntRoundtripTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tointernal-fixedint-roundtrip.cpp"
  )
  target_link_libraries(iccToInternalFixedIntRoundtripTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
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
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
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

# #2094: CIccTagChromaticity::Validate() reported a channel count other than
# three as a critical error and then walked into the colorant-encoding switch
# anyway, comparing m_xy[0..2] against three fixed xy pairs on a tag whose array
# is shorter -- a heap out-of-bounds read (CWE-125) reachable from all three
# readers, since Read() sizes the array from the tag's declared size, the XML
# parser from the number of <Channel> elements and the JSON parser from the
# length of the "channels" array. No chromaticityTag exists anywhere in the
# corpus in any format, so no path had coverage. Also pins
# the constructor invariant (the count was clamped to three while the raw nSize
# was allocated) and the colorant encoding surviving a copy, which neither the
# copy constructor nor operator= carried across.
#
# #2106 adds the read path the same tag reaches that state through. Read() cross
# -checked the declared channel count against one derived from the tag-table
# size and then sized the tag from the derived figure, discarding the count it
# had just validated against -- so a three-channel tag inside a 92-byte element
# became ten channels, and Write() re-emitted ten, persisting the rewrite for
# anything that round-tripped the profile. CIccTagNamedColor2::Read resolves the
# same two-source situation the other way, using its size-derived count only as
# a capacity guard. Header + IccProfLib only.
function(iccdev_add_chromaticity_validate_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccChromaticityValidateTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/chromaticity-validate-oob.cpp"
  )
  target_link_libraries(iccChromaticityValidateTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccChromaticityValidateTest)

  add_test(
    NAME iccdev.chromaticity-validate-oob
    COMMAND "$<TARGET_FILE:iccChromaticityValidateTest>"
  )
  set_tests_properties(iccdev.chromaticity-validate-oob PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;tag;chromaticity;security;issue-2094;issue-2106;regression"
  )
  if(WIN32)
    set(_chromaticity_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccChromaticityValidateTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _chromaticity_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.chromaticity-validate-oob PROPERTIES
      ENVIRONMENT_MODIFICATION "${_chromaticity_windows_env_mods}"
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

function(iccdev_add_pcs_xyzconvert_matrix_channels_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccPcsXyzConvertMatrixChannelsTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/pcs-xyzconvert-matrix-channels.cpp"
  )
  target_link_libraries(iccPcsXyzConvertMatrixChannelsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccPcsXyzConvertMatrixChannelsTest)

  add_test(
    NAME iccdev.pcs-xyzconvert-matrix-channels
    COMMAND "$<TARGET_FILE:iccPcsXyzConvertMatrixChannelsTest>"
  )
  set_tests_properties(iccdev.pcs-xyzconvert-matrix-channels PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;cmm;pcc;security;issue-2175;regression"
  )
  if(WIN32)
    set(_pcsxyz_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccPcsXyzConvertMatrixChannelsTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _pcsxyz_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.pcs-xyzconvert-matrix-channels PROPERTIES
      ENVIRONMENT_MODIFICATION "${_pcsxyz_windows_env_mods}"
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
  )
  target_compile_features(iccVizMetricsTest PRIVATE cxx_std_17)
  # IccVizEngine supplies IccVizModel.cpp and the engine include directory, so
  # this stays a genuine second translation unit against the public header
  # while naming neither path by hand (#1975).
  target_link_libraries(iccVizMetricsTest PRIVATE ${TARGET_LIB_ICCPROFLIB} IccVizEngine)
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
  # Headers only, deliberately: principalStdDevs is inline in IccVizMath.hpp, so
  # this test must NOT pull in IccVizModel.cpp.  IccVizEngineHeaders is the
  # engine include directory without the engine source (#1975).
  target_link_libraries(iccVizDegenerateTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB} IccVizEngineHeaders)
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

# Guards the Mini{PDF,SVG,TIFF} serialization seam added for #2116.
#
# The three iccviz tests above all stop at the data layer or drive the whole CLI;
# nothing exercised the byte-generating layer in isolation, and nothing could,
# because the only entry to it was a CLI function that derives every output path
# from the input path.  #2116 split out PDFWriter::Serialize(ostream&),
# SVGOut::Serialize(ostream&) and a WriteTIFF overload taking an already-open
# FILE*; this test asserts the split is behaviour-preserving (each writer emits
# identical bytes through the stream path and the file path), and drives the
# TIFF writer with the real engine's rasters so its IFD layout and offset
# arithmetic execute -- under the sanitizer lanes, for the first time.
#
# It also pins the raster geometry contract that keeps that arithmetic in range.
# MiniTIFF.cpp does check its stream-derived quantities (ftell failure, strip
# offsets past UINT32_MAX, a short fwrite), but not the geometry handed to it:
# width/height/channels/depth are guarded only by assert()s that a default
# Release build compiles out.  The guards that make them safe live in
# buildClutRaster (IccVizModel.cpp) instead, so the invariant spans two files
# and was written down in neither.
#
# The Mini* writers are not in any library: they are listed directly in the
# iccProfileVisualizePlot executable, so this test compiles the same three
# translation units.  Deliberately the Tools/CmdLine/IccProfilePlot copy -- there
# is a second, diverged copy under Tools/CmdLine/IccProfileVisualize that also
# ships, and this test says nothing about it (#2116 records that split).
# IccVizEngine supplies IccVizModel.cpp plus the engine include directory (#1975).
function(iccdev_add_iccviz_writer_serialization_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()
  if(NOT TARGET IccVizEngine)
    return()
  endif()

  set(_viz_writer_dir "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccProfilePlot")
  iccdev_add_regression_executable(iccVizWriterSerializationTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/iccviz-writer-serialization.cpp"
    "${_viz_writer_dir}/MiniPDF.cpp"
    "${_viz_writer_dir}/MiniSVG.cpp"
    "${_viz_writer_dir}/MiniTIFF.cpp"
  )
  target_compile_features(iccVizWriterSerializationTest PRIVATE cxx_std_17)
  # Name the writer directory explicitly rather than relying on IccVizEngine's
  # interface include path to supply Mini*.hpp.  The two happen to be the same
  # directory today, but the sources above are pinned to this copy by absolute
  # path, and the other copy has no Serialize declaration -- so if the engine's
  # include dir ever moved, the headers and the sources could silently come from
  # different copies.
  target_include_directories(iccVizWriterSerializationTest PRIVATE "${_viz_writer_dir}")
  target_link_libraries(iccVizWriterSerializationTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB} IccVizEngine)
  add_dependencies(check iccVizWriterSerializationTest)

  add_test(
    NAME iccdev.iccviz-writer-serialization
    COMMAND "$<TARGET_FILE:iccVizWriterSerializationTest>"
      "${ICCDEV_TESTING_DIR}/sRGB_v4_ICC_preference.icc"
      "${ICCDEV_TEST_OUTDIR}/iccviz-writer-serialization"
  )
  set_tests_properties(iccdev.iccviz-writer-serialization PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 120
    LABELS "iccdev;iccprofileplot;viz;pdf;svg;tiff;issue-2116;regression"
  )
  if(WIN32)
    set(_iccviz_writer_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccVizWriterSerializationTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _iccviz_writer_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.iccviz-writer-serialization PROPERTIES
      ENVIRONMENT_MODIFICATION "${_iccviz_writer_windows_env_mods}"
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

# Guards the reopen half of #1940. CIccFileIO::Open fclose()s the stream it is
# already holding without nulling m_fFile, which only stayed safe while the
# fopen() assignment was the next statement. #1204 put the POSIX regular-file
# check in between, so a refused write-mode reopen returns with a freed FILE*
# still attached: plain builds segfault in the next fread(), sanitizer builds
# reach the following Close() and report a double-free. That check is compiled
# out on Windows, so the test exits 77 there and SKIP_RETURN_CODE turns it into
# a ctest Skipped instead of a false pass. Core IccProfLib only.
function(iccdev_add_fileio_reopen_nonregular_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccFileIoReopenNonRegularTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/fileio-reopen-nonregular.cpp"
  )
  target_link_libraries(iccFileIoReopenNonRegularTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccFileIoReopenNonRegularTest)

  add_test(
    NAME iccdev.fileio-reopen-nonregular
    COMMAND
      "$<TARGET_FILE:iccFileIoReopenNonRegularTest>"
  )
  set_tests_properties(iccdev.fileio-reopen-nonregular PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    SKIP_RETURN_CODE 77
    LABELS "iccdev;fileio;issue-1940;regression"
  )
  if(WIN32)
    set(_fileio_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccFileIoReopenNonRegularTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _fileio_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.fileio-reopen-nonregular PROPERTIES
      ENVIRONMENT_MODIFICATION "${_fileio_windows_env_mods}"
    )
  else()
    set_tests_properties(iccdev.fileio-reopen-nonregular PROPERTIES
      ENVIRONMENT "${ICCDEV_TEST_ENV}"
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

function(iccdev_add_ziputf8_read_inflate_contract_test)
  if(NOT ICC_USE_ZLIB)
    return()
  endif()
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccZipUtf8ReadInflateContractTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/ziputf8-read-inflate-contract.cpp"
  )
  target_compile_features(iccZipUtf8ReadInflateContractTest PRIVATE cxx_std_17)
  # Same linkage note as iccdev_add_ziputf8_settext_contract_test above: IccProfLib
  # links ZLIB::ZLIB and defines ICC_USE_ZLIB=1 PUBLIC, so both propagate here; the
  # explicit zlib link is belt-and-braces.
  target_link_libraries(iccZipUtf8ReadInflateContractTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  if(TARGET ZLIB::ZLIB)
    target_link_libraries(iccZipUtf8ReadInflateContractTest PRIVATE ZLIB::ZLIB)
  endif()
  add_dependencies(check iccZipUtf8ReadInflateContractTest)

  add_test(
    NAME iccdev.ziputf8-read-inflate-contract
    COMMAND "$<TARGET_FILE:iccZipUtf8ReadInflateContractTest>"
  )
  set_tests_properties(iccdev.ziputf8-read-inflate-contract PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;tag;zlib;compression;issue-1853;regression"
  )
  if(WIN32)
    set(_ziputf8_readinflate_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccZipUtf8ReadInflateContractTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _ziputf8_readinflate_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.ziputf8-read-inflate-contract PROPERTIES
      ENVIRONMENT_MODIFICATION "${_ziputf8_readinflate_windows_env_mods}"
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

# #2006 part 4a: CIccTagCurve::SetSize() answered an over-cap request the same
# way it answered SetSize(0) -- free the table, m_nSize = 0, return true -- so
# the return value could not distinguish "emptied" (a success, and ICC.1 10.6's
# identity encoding) from "refused". This pins the split contract: an over-cap
# request now reports failure and leaves any existing table intact, and
# CIccTagCurve::Read() consequently rejects a curveType whose declared count
# exceeds the cap instead of loading it as a silent identity curve.
#
# Pure library test -- no fixtures on disk and no tool invocation -- so it needs
# only IccProfLib and no scratch directory.
function(iccdev_add_curve_setsize_contract_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCurveSetSizeContractTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/curve-setsize-contract.cpp"
  )
  target_compile_features(iccCurveSetSizeContractTest PRIVATE cxx_std_17)
  target_include_directories(iccCurveSetSizeContractTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccCurveSetSizeContractTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCurveSetSizeContractTest)

  if(WIN32)
    add_test(
      NAME iccdev.curve-setsize-contract
      COMMAND "$<TARGET_FILE:iccCurveSetSizeContractTest>"
    )
  else()
    add_test(
      NAME iccdev.curve-setsize-contract
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccCurveSetSizeContractTest>"
    )
  endif()
  set_tests_properties(iccdev.curve-setsize-contract PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;curve;setsize;regression"
  )
  if(WIN32)
    set(_curve_contract_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCurveSetSizeContractTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _curve_contract_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.curve-setsize-contract PROPERTIES
      ENVIRONMENT_MODIFICATION "${_curve_contract_windows_env_mods}"
    )
  endif()
endfunction()

# #1811: spectralViewingConditions illuminantXYZ/surroundXYZ carry absolute
# luminance in cd/m^2, and CIccInfo::CheckLuminance warns when Y is 1 because that
# is what chromaticity normalization leaves behind. The check cannot be exact --
# Testing/Display holds the same profile at Y = 1, 300 and 500, and the 300 cd/m^2
# fixture scaled to 1 cd/m^2 *is* the normalized fixture -- so this pins that the
# diagnostic still fires on Y = 1, stays a Warning rather than escalating to an
# assertion, and leaves the physical values #1811 names untouched.
#
# Pure library test -- no fixtures on disk and no tool invocation -- so it needs
# only IccProfLib and no scratch directory. Exercising CheckLuminance directly
# rather than CIccTagSpectralViewingConditions::Validate keeps the missing-observer
# critical error from masking the status under test.
# Restores the u8Fixed8Number gamma coverage that shipped with #808 and was dropped
# in 35f5c5e6 (#968) when ci-iccdev-tool-tests.yml / ci-tool-tests.yml lost their
# inline bodies: steps R12/R13/R8 checked all four gamma-*.icc fixtures and none of
# it was carried over to CTest, leaving three of the four fixtures unreferenced
# anywhere in the tree. A one-entry curveType is a u8Fixed8 exponent, so consumers
# must undo ReadUInt16Float's /65535 with `* 65535.0 / 256.0` rather than `* 256.0`.
# The test reads each fixture through CIccProfileJson with the JSON factories pushed
# -- iccToJson's own path -- and pins both Describe()'s exact text and the emitted
# JSON gamma, the latter against a derived gamma*2^-24 bound rather than the flat
# 0.01 the old R13 used (#2044). Needs IccJSON + IccProfLib; skipped where IccJSON
# is not built.
function(iccdev_add_curve_gamma_u8fixed8_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCJSON}")
    return()
  endif()

  iccdev_add_regression_executable(iccCurveGammaU8Fixed8Test
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/curve-gamma-u8fixed8.cpp"
  )
  target_compile_features(iccCurveGammaU8Fixed8Test PRIVATE cxx_std_17)
  target_include_directories(iccCurveGammaU8Fixed8Test PRIVATE
    "${ICCDEV_REPO_ROOT}/IccJSON/IccLibJSON"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccCurveGammaU8Fixed8Test PRIVATE
    ${TARGET_LIB_ICCJSON} ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCurveGammaU8Fixed8Test)

  if(WIN32)
    add_test(
      NAME iccdev.curve-gamma-u8fixed8
      COMMAND "$<TARGET_FILE:iccCurveGammaU8Fixed8Test>"
        "${ICCDEV_REPO_ROOT}/.github/ci/regression"
    )
  else()
    add_test(
      NAME iccdev.curve-gamma-u8fixed8
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccCurveGammaU8Fixed8Test>"
        "${ICCDEV_REPO_ROOT}/.github/ci/regression"
    )
  endif()
  set_tests_properties(iccdev.curve-gamma-u8fixed8 PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;json;curve;gamma;issue-815;issue-2044;regression"
  )
  if(WIN32)
    set(_curve_gamma_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCurveGammaU8Fixed8Test>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCJSON}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _curve_gamma_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.curve-gamma-u8fixed8 PROPERTIES
      ENVIRONMENT_MODIFICATION "${_curve_gamma_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_luminance_normalization_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccLuminanceNormalizationTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/luminance-normalization-diagnostic.cpp"
  )
  target_compile_features(iccLuminanceNormalizationTest PRIVATE cxx_std_17)
  target_include_directories(iccLuminanceNormalizationTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccLuminanceNormalizationTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccLuminanceNormalizationTest)

  if(WIN32)
    add_test(
      NAME iccdev.luminance-normalization
      COMMAND "$<TARGET_FILE:iccLuminanceNormalizationTest>"
    )
  else()
    add_test(
      NAME iccdev.luminance-normalization
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccLuminanceNormalizationTest>"
    )
  endif()
  set_tests_properties(iccdev.luminance-normalization PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;luminance;validation;issue-1811;regression"
  )
  if(WIN32)
    set(_luminance_norm_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccLuminanceNormalizationTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _luminance_norm_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.luminance-normalization PROPERTIES
      ENVIRONMENT_MODIFICATION "${_luminance_norm_windows_env_mods}"
    )
  endif()
endfunction()

# #2012/#2013/#2014: g_icTagNameTable published ten tag names matching neither
# the signature nor their siblings. The table drives the XML element name and the
# JSON key in both directions, so the corrected name must be the one emitted while
# the legacy name keeps resolving through the read-only g_icAltTagNameTable.
# Links IccProfLib only -- the contract is the factory mapping, not a round-trip.
function(iccdev_add_tag_name_spelling_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccTagNameSpellingTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tag-name-spelling.cpp"
  )
  target_compile_features(iccTagNameSpellingTest PRIVATE cxx_std_17)
  target_include_directories(iccTagNameSpellingTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccTagNameSpellingTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccTagNameSpellingTest)

  if(WIN32)
    add_test(
      NAME iccdev.tag-name-spelling
      COMMAND "$<TARGET_FILE:iccTagNameSpellingTest>"
    )
  else()
    add_test(
      NAME iccdev.tag-name-spelling
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccTagNameSpellingTest>"
    )
  endif()
  set_tests_properties(iccdev.tag-name-spelling PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;tagfactory;naming;regression"
  )
  if(WIN32)
    set(_tag_name_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccTagNameSpellingTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _tag_name_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.tag-name-spelling PROPERTIES
      ENVIRONMENT_MODIFICATION "${_tag_name_windows_env_mods}"
    )
  endif()
endfunction()

# #2028: g_icStructNames is the struct-name analogue of the tag table above and was
# not covered by that sweep. It published brdfTransfromStructure and
# colorantInfoStruct, each of which had a second entry for the same signature that
# GetStructSigName's first-match could never reach. For brdf that broke JSON
# round-tripping outright: CIccTagJsonStruct::ToJson writes GetDisplayName(), which
# has always returned the correct brdfTransformStructure, and ParseJson reads back
# through this table, so the struct type was silently discarded on read. The
# corrected name must be the one emitted while both legacy spellings keep resolving
# through the read-only g_icAltStructNames, and -- the check nothing else makes --
# the factory table and GetDisplayName must agree for every struct signature.
# Links IccProfLib only; the contract is the factory mapping, not a round-trip.
function(iccdev_add_struct_name_spelling_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccStructNameSpellingTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/struct-name-spelling.cpp"
  )
  target_compile_features(iccStructNameSpellingTest PRIVATE cxx_std_17)
  target_include_directories(iccStructNameSpellingTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccStructNameSpellingTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccStructNameSpellingTest)

  if(WIN32)
    add_test(
      NAME iccdev.struct-name-spelling
      COMMAND "$<TARGET_FILE:iccStructNameSpellingTest>"
    )
  else()
    add_test(
      NAME iccdev.struct-name-spelling
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccStructNameSpellingTest>"
    )
  endif()
  set_tests_properties(iccdev.struct-name-spelling PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;structfactory;naming;regression"
  )
  if(WIN32)
    set(_struct_name_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccStructNameSpellingTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _struct_name_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.struct-name-spelling PROPERTIES
      ENVIRONMENT_MODIFICATION "${_struct_name_windows_env_mods}"
    )
  endif()
endfunction()

# #1853 harvest: icSigSrcMCSChannelData encodes its channel count in the signature the
# same way icSigNChannelData and the spectral families do, and icNumColorSpaceChannels
# already decoded it -- but icGetColorSig, icGetColorSigStr and GetColorSpaceSigName
# left it out of their case lists, so MCS spaces reached a generic fallback and printed
# as 'mc??' = 6D630004 / 6D630004h / 0x4ColorData. icGetColorSigStr is the XML and JSON
# <MCS> writer, so the fallback was reaching serialized profiles. No value was lost
# (icGetSigVal parses the hex escape back), which is why the test asserts the round trip
# as well as the spelling: the fix changes which icGetSigVal branch a real profile takes.
# Links IccProfLib only; the contract is the naming tables, not a profile round-trip.
# #1853 harvest: IccSignatureUtils.h had no test and no consumer -- nothing in the tree
# included it. That is how it came to sit with an unguarded `#include <execinfo.h>` behind a
# bare __linux__ (fixed in #2072). This test is deliberately the header's first consumer, so
# CI compiles it from now on rather than only reading it. Coverage-wise the point is that
# ColorSpaceSignatureToStr, IsValidColorSpaceSignature and DescribeColorSpaceSignature each
# carry their own copy of the same signature knowledge -- Describe() re-implements the
# validity switch inline -- so they can drift apart silently; the test asserts they agree.
# Adapted with credit from @xsscx's signature-utils.cpp on ci-qa-flags, which is what first
# surfaced the MCS naming gap. Links IccProfLib only; header-inline contract, no fixture.
# #1853 harvest: CTiffImg computes bytes-per-line in both directions and only checked one.
# Open() routes it through calcBytesPerLine() (TiffImg.cpp:471,:489), which multiplies in 64
# bits and refuses a result that will not fit in unsigned int; Create() did the same sums raw,
# and additionally narrowed TIFFStripSize()'s signed 64-bit tmsize_t with a C cast. Reachable
# because Create() runs before any pixel data is read (iccApplyProfiles.cpp:508), so a source
# TIFF need only DECLARE a large ImageWidth -- and the big strip allocation that looks like it
# would fail first is granted under Linux overcommit, so Create() returned true carrying a
# bytes-per-line smaller than one row, which the caller then uses to size its row buffer
# (iccApplyProfiles.cpp:539).
#
# This is the only regression test that compiles a Tools translation unit rather than linking
# IccProfLib alone: CTiffImg lives in the tool, so the contract cannot be reached any other
# way. It therefore needs libtiff, and is skipped when TIFF is unavailable.
function(iccdev_add_tiff_create_overflow_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()
  if(NOT TARGET TIFF::TIFF AND NOT TIFF_LIBRARY)
    find_package(TIFF QUIET)
  endif()
  if(TARGET TIFF::TIFF)
    set(_tiff_lib TIFF::TIFF)
  elseif(TIFF_LIBRARY)
    set(_tiff_lib ${TIFF_LIBRARY})
  else()
    message(STATUS "iccdev.tiff-create-overflow: libtiff not found, test skipped")
    return()
  endif()

  iccdev_add_regression_executable(iccTiffCreateOverflowTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tiff-create-bytesperline-overflow.cpp"
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccApplyProfiles/TiffImg.cpp"
  )
  target_compile_features(iccTiffCreateOverflowTest PRIVATE cxx_std_17)
  target_include_directories(iccTiffCreateOverflowTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
    "${ICCDEV_REPO_ROOT}/Tools/CmdLine/IccApplyProfiles"
  )
  target_link_libraries(iccTiffCreateOverflowTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB} ${_tiff_lib})
  add_dependencies(check iccTiffCreateOverflowTest)

  if(WIN32)
    add_test(
      NAME iccdev.tiff-create-overflow
      COMMAND "$<TARGET_FILE:iccTiffCreateOverflowTest>"
    )
  else()
    add_test(
      NAME iccdev.tiff-create-overflow
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccTiffCreateOverflowTest>"
    )
  endif()
  set_tests_properties(iccdev.tiff-create-overflow PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 120
    LABELS "iccdev;tools;tiff;overflow;regression"
  )
  if(WIN32)
    set(_tiffcreate_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccTiffCreateOverflowTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _tiffcreate_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.tiff-create-overflow PROPERTIES
      ENVIRONMENT_MODIFICATION "${_tiffcreate_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_signature_utils_contract_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccSignatureUtilsContractTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/signature-utils-contract.cpp"
  )
  target_compile_features(iccSignatureUtilsContractTest PRIVATE cxx_std_17)
  target_include_directories(iccSignatureUtilsContractTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccSignatureUtilsContractTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccSignatureUtilsContractTest)

  if(WIN32)
    add_test(
      NAME iccdev.signature-utils-contract
      COMMAND "$<TARGET_FILE:iccSignatureUtilsContractTest>"
    )
  else()
    add_test(
      NAME iccdev.signature-utils-contract
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccSignatureUtilsContractTest>"
    )
  endif()
  set_tests_properties(iccdev.signature-utils-contract PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;iccprofLib;signature;naming;regression"
  )
  if(WIN32)
    set(_sigutils_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccSignatureUtilsContractTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _sigutils_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.signature-utils-contract PROPERTIES
      ENVIRONMENT_MODIFICATION "${_sigutils_windows_env_mods}"
    )
  endif()
endfunction()

function(iccdev_add_mcs_colorspace_name_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccMcsColorSpaceNameTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/mcs-colorspace-name.cpp"
  )
  target_compile_features(iccMcsColorSpaceNameTest PRIVATE cxx_std_17)
  target_include_directories(iccMcsColorSpaceNameTest PRIVATE
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccMcsColorSpaceNameTest PRIVATE
    ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccMcsColorSpaceNameTest)

  if(WIN32)
    add_test(
      NAME iccdev.mcs-colorspace-name
      COMMAND "$<TARGET_FILE:iccMcsColorSpaceNameTest>"
    )
  else()
    add_test(
      NAME iccdev.mcs-colorspace-name
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccMcsColorSpaceNameTest>"
    )
  endif()
  set_tests_properties(iccdev.mcs-colorspace-name PROPERTIES
    TIMEOUT 120
    LABELS "iccdev;iccprofLib;naming;mcs;regression"
  )
  if(WIN32)
    set(_mcs_name_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccMcsColorSpaceNameTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _mcs_name_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.mcs-colorspace-name PROPERTIES
      ENVIRONMENT_MODIFICATION "${_mcs_name_windows_env_mods}"
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

function(iccdev_add_calc_envsig_sign_extension_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccCalcEnvSigSignExtensionTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/calc-envsig-sign-extension.cpp"
  )
  target_link_libraries(iccCalcEnvSigSignExtensionTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccCalcEnvSigSignExtensionTest)

  add_test(
    NAME iccdev.calc-envsig-sign-extension
    COMMAND "$<TARGET_FILE:iccCalcEnvSigSignExtensionTest>"
  )
  set_tests_properties(iccdev.calc-envsig-sign-extension PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;calculator;issue-2196;regression"
  )
  if(WIN32)
    set(_calc_envsig_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccCalcEnvSigSignExtensionTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _calc_envsig_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.calc-envsig-sign-extension PROPERTIES
      ENVIRONMENT_MODIFICATION "${_calc_envsig_windows_env_mods}"
    )
  endif()
endfunction()

# #1888: proves a regression executable can link an exported IccProfLib global
# *variable*. WINDOWS_EXPORT_ALL_SYMBOLS exports functions but not data, so until
# #2219 gave the eight globals ICCPROFLIB_DATA_API this linked on a Windows shared
# build only by falling back to IccProfLib2-static. That fallback is retired
# (#2154): the test now links the DLL like every other, and if the annotation is
# lost the Windows leg fails at BUILD time here -- while
# iccdev.proflib-exported-data-dll-linkage, which builds its consumer out of tree,
# reports the same loss as a test failure. Also pins the values that tests which
# read no library data hard-code instead.
function(iccdev_add_proflib_exported_data_linkage_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
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
    ${TARGET_LIB_ICCPROFLIB})
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
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
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

# #2085: CIccTagJsonFactory registered neither u16Fixed16ArrayType ('uf32') nor
# spectralRangeType ('srng'), both of which IccProfLib and IccTagXmlFactory carry.
# Because the JSON factory's default arm returns CIccTagJsonUnknown rather than
# NULL, a missing case is a silent downgrade to an opaque blob, not a fall-back to
# the right tag. This pins registration and a value-preserving JSON round trip for
# both, including the upper uf32 values the signed icFtoD/icDtoF helpers -- which the
# FixedNum template used unconditionally -- cannot carry: the writer misreads from
# 32768 up, the reader truncates from 32767.0 up. It also pins the srng reader's
# rejection of malformed start/end/steps and the writer's handling of a bi-spectral
# range. Note "value-preserving" is bounded by icUFtoD returning icFloatNumber
# (float), so uf32 words are exact only below 256.0 -- a limit shared with s15Fixed16
# and the XML path, noted in #2095. Needs IccJSON + IccProfLib; skipped where IccJSON
# is not built.
function(iccdev_add_json_srng_uf32_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}" OR NOT TARGET "${TARGET_LIB_ICCJSON}")
    return()
  endif()

  iccdev_add_regression_executable(iccJsonSrngUf32Test
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/json-srng-uf32-roundtrip.cpp"
  )
  target_compile_features(iccJsonSrngUf32Test PRIVATE cxx_std_17)
  target_include_directories(iccJsonSrngUf32Test PRIVATE
    "${ICCDEV_REPO_ROOT}/IccJSON/IccLibJSON"
    "${ICCDEV_REPO_ROOT}/IccProfLib"
  )
  target_link_libraries(iccJsonSrngUf32Test PRIVATE
    ${TARGET_LIB_ICCJSON} ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccJsonSrngUf32Test)

  add_test(
    NAME iccdev.json-srng-uf32-roundtrip
    COMMAND "$<TARGET_FILE:iccJsonSrngUf32Test>"
  )
  set_tests_properties(iccdev.json-srng-uf32-roundtrip PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
    TIMEOUT 60
    LABELS "iccdev;json;icc2;v5;issue-2085;regression"
  )
  if(WIN32)
    set(_json_srng_uf32_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccJsonSrngUf32Test>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCJSON}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _json_srng_uf32_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.json-srng-uf32-roundtrip PROPERTIES
      ENVIRONMENT_MODIFICATION "${_json_srng_uf32_windows_env_mods}"
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
# IccProfLib alone and references no exported library data.
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
  target_link_libraries(iccV2LegacyPcsTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
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
# Needs IccXML (CIccProfileXml) + IccProfLib; skipped where IccXML is not built.
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

# Pins CIccCLUT's grid-coordinate clamp semantics, in particular that +Inf
# saturates to the top of the grid on both the default and the former NoClip
# path. Those two disagreed about +Inf before the clamp was inlined -- one
# saturated high, the other collapsed to zero.
#
# This needs its own test because the change is invisible to everything else:
# the throughput harness reported no checksum movement on any case even with
# +Inf on every channel, since the only corpus profiles that install the NoClip
# path are three-channel and the only wider profile has no CLUT element.
if(TARGET ${TARGET_LIB_ICCPROFLIB})
  iccdev_add_regression_executable(iccClutGridClampTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/clut-grid-clamp-inf.cpp"
  )
  target_link_libraries(iccClutGridClampTest PRIVATE ${TARGET_LIB_ICCPROFLIB})
  add_dependencies(check iccClutGridClampTest)

  add_test(
    NAME iccdev.clut-grid-clamp-inf
    COMMAND "$<TARGET_FILE:iccClutGridClampTest>"
  )
  set_tests_properties(iccdev.clut-grid-clamp-inf PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;iccprofLib;clut;clamp;regression"
  )
  if(WIN32)
    set(_clutclamp_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccClutGridClampTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _clutclamp_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.clut-grid-clamp-inf PROPERTIES
      ENVIRONMENT_MODIFICATION "${_clutclamp_windows_env_mods}"
    )
  endif()
endif()


# Throughput harness for the IccProfLib apply paths (iccBenchApply).
#
# NOTE: this must stay ABOVE the if(WIN32) branch immediately below. That branch
# registers the Windows test set and then return()s, so anything placed after it
# never runs on Windows. This block handles WIN32 itself.
#
# Records; never asserts a timing threshold. Shared CI runners vary 20-30%
# between runs on the same commit, so a threshold would flake without catching
# anything a local run would not. The test passes when every case either
# measured or reported SKIP, and fails only on a real defect: a checksum that
# moved between thread counts, or a second Begin() that changed the output.
#
# The 'slow' label puts it on the existing opt-out path rather than inventing
# one: check-fast excludes 'slow' while check excludes only 'known-red', so the
# full suite runs it and the fast loop does not.
#
# ENVIRONMENT hands the tool both profile roots. iccdev.create-profiles writes
# generated profiles into the source tree while iccdev.windows-create-profiles
# writes them into the build tree, and the tool resolves against both rather
# than assuming a platform.
if(TARGET iccBenchApply)
  add_dependencies(build-test-binaries iccBenchApply)

  if(WIN32)
    add_test(
      NAME iccdev.apply-throughput
      COMMAND "$<TARGET_FILE:iccBenchApply>" -suite -csv -pixels 65536 -repeats 3
    )
  else()
    add_test(
      NAME iccdev.apply-throughput
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccBenchApply>" -suite -csv -pixels 65536 -repeats 3
    )
  endif()

  set_tests_properties(iccdev.apply-throughput PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 300
    LABELS "iccdev;iccprofLib;benchmark;throughput;slow"
    FIXTURES_REQUIRED iccdev_profiles
    ENVIRONMENT "ICCDEV_BENCH_SOURCE_ROOT=${ICCDEV_REPO_ROOT};ICCDEV_BENCH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
  )

  if(WIN32)
    set(_bench_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccBenchApply>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _bench_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.apply-throughput PROPERTIES
      ENVIRONMENT_MODIFICATION "${_bench_windows_env_mods}"
    )
  endif()
endif()


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
      # 135 since Testing/V2/v2GrayTRC{,Lab}.xml were added: they are the only
      # sources in the tree that produce a CIccXformMonochrome, one per PCS
      # encoding. Bump this with any change to the number of profiles
      # CreateAllProfiles.bat generates.
      EXPECTED_GENERATED_PROFILE_COUNT 135
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

  # The two shared-library checks below configure consumers outside this CMake
  # tree. Propagate the parent ASan mode and CRT selection so those executables
  # load an instrumented IccProfLib2.dll with the same runtime contract.
  set(_iccdev_windows_consumer_asan OFF)
  if(ENABLE_ASAN OR ENABLE_SANITIZERS)
    set(_iccdev_windows_consumer_asan ON)
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
        "-DICCDEV_ENABLE_ASAN=${_iccdev_windows_consumer_asan}"
        "-DICCDEV_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
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

  # #2154: the DLL-side counterpart to iccdev.proflib-exported-data-linkage.
  # Both now link the DLL, but they fail differently, which is why both stay: an
  # in-tree target that cannot link takes the BUILD down, so it can never be
  # expressed as a red test. This one configures and compiles its consumer out of
  # tree at test time, so a lost ICCPROFLIB_DATA_API annotation surfaces as a
  # single failing test with the LNK2019 in its log.
  if(ENABLE_SHARED_LIBS AND TARGET IccProfLib2)
    add_test(
      NAME iccdev.proflib-exported-data-dll-linkage
      COMMAND
        "${CMAKE_COMMAND}"
        "-DICCDEV_TEST_NAME=iccdev.proflib-exported-data-dll-linkage"
        "-DICCDEV_TEST_OUTDIR=${ICCDEV_TEST_OUTDIR}/iccdev-proflib-exported-data-dll-linkage"
        "-DICCDEV_REPO_ROOT=${ICCDEV_REPO_ROOT}"
        "-DICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DICCDEV_CONFIG=$<CONFIG>"
        "-DICCDEV_ICCPROFLIB_DLL=$<TARGET_FILE:IccProfLib2>"
        "-DICCDEV_ICCPROFLIB_IMPLIB=$<TARGET_LINKER_FILE:IccProfLib2>"
        "-DICCDEV_GENERATOR=${CMAKE_GENERATOR}"
        "-DICCDEV_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DICCDEV_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DICCDEV_ENABLE_ASAN=${_iccdev_windows_consumer_asan}"
        "-DICCDEV_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunWindowsExportedDataLinkageTest.cmake"
    )
    set_tests_properties(iccdev.proflib-exported-data-dll-linkage PROPERTIES
      WORKING_DIRECTORY "${ICCDEV_TEST_OUTDIR}"
      TIMEOUT 180
      LABELS "iccdev;windows;shared;linkage;issue-1888;issue-2154;regression"
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
  iccdev_add_cmmsearch_rebegin_test()
  iccdev_add_ndlut_input_channel_mismatch_test()
  iccdev_add_pcs_unitfloat_source_encoding_test()
  iccdev_add_embedio_read8_bounds_test()
  iccdev_add_embedded_profile_onelevel_load_test()
  iccdev_add_embedded_profile_detachio_test()
  iccdev_add_profile_shared_io_detach_test()
  iccdev_add_fileio_getlength_position_test()
  iccdev_add_profile_write_failure_test()
  iccdev_add_fileio_seek_tell_test()
  iccdev_add_fileio_reopen_nonregular_test()
  iccdev_add_iccio_write_float_aliasing_test()
  iccdev_add_tagdata_zlib_roundtrip_test()
  iccdev_add_ziputf8_settext_contract_test()
  iccdev_add_ziputf8_read_inflate_contract_test()
  iccdev_add_compression_describe_labels_test()
  iccdev_add_pawg_compression_paths_test()
  iccdev_add_json_sparsematrix_huaf_test()
  iccdev_add_xml_writer_graceful_degrade_test()
  iccdev_add_xml_relaxng_validation_leak_test()
  iccdev_add_xml_curve_oversize_file_test()
  iccdev_add_curve_setsize_contract_test()
  iccdev_add_curve_gamma_u8fixed8_test()
  iccdev_add_luminance_normalization_test()
  iccdev_add_tag_name_spelling_test()
  iccdev_add_struct_name_spelling_test()
  iccdev_add_mcs_colorspace_name_test()
  iccdev_add_signature_utils_contract_test()
  iccdev_add_tiff_create_overflow_test()
  iccdev_add_mpexml_unknown_reserved_test()
  iccdev_add_calc_envsig_sign_extension_test()
  iccdev_add_proflib_exported_data_linkage_test()
  iccdev_add_proflib_exported_global_definitions_test()
  iccdev_add_json_bcd_version_test()
  iccdev_add_json_writer_move_test()
  iccdev_add_json_fixednum_size_cap_test()
  iccdev_add_json_srng_uf32_test()
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
  iccdev_add_chromaticity_validate_test()
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
  iccdev_add_pcs_xyzconvert_matrix_channels_test()
  iccdev_add_iccviz_metrics_test()
  iccdev_add_iccviz_degenerate_test()
  iccdev_add_iccviz_pdf_axis_test()
  iccdev_add_iccviz_writer_serialization_test()
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
iccdev_add_cmmsearch_rebegin_test()
iccdev_add_ndlut_input_channel_mismatch_test()
iccdev_add_pcs_unitfloat_source_encoding_test()
iccdev_add_embedio_read8_bounds_test()
iccdev_add_embedded_profile_onelevel_load_test()
iccdev_add_embedded_profile_detachio_test()
iccdev_add_profile_shared_io_detach_test()
iccdev_add_fileio_getlength_position_test()
iccdev_add_profile_write_failure_test()
iccdev_add_fileio_seek_tell_test()
iccdev_add_fileio_reopen_nonregular_test()
iccdev_add_iccio_write_float_aliasing_test()
iccdev_add_tagdata_zlib_roundtrip_test()
iccdev_add_ziputf8_settext_contract_test()
iccdev_add_ziputf8_read_inflate_contract_test()
iccdev_add_compression_describe_labels_test()
iccdev_add_pawg_compression_paths_test()
iccdev_add_json_sparsematrix_huaf_test()
iccdev_add_xml_writer_graceful_degrade_test()
iccdev_add_xml_relaxng_validation_leak_test()
iccdev_add_xml_curve_oversize_file_test()
iccdev_add_curve_setsize_contract_test()
iccdev_add_curve_gamma_u8fixed8_test()
iccdev_add_luminance_normalization_test()
iccdev_add_tag_name_spelling_test()
iccdev_add_struct_name_spelling_test()
iccdev_add_mcs_colorspace_name_test()
iccdev_add_signature_utils_contract_test()
iccdev_add_tiff_create_overflow_test()
iccdev_add_mpexml_unknown_reserved_test()
iccdev_add_calc_envsig_sign_extension_test()
iccdev_add_proflib_exported_data_linkage_test()
iccdev_add_proflib_exported_global_definitions_test()
iccdev_add_json_bcd_version_test()
iccdev_add_json_writer_move_test()
iccdev_add_json_fixednum_size_cap_test()
iccdev_add_json_srng_uf32_test()
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
iccdev_add_chromaticity_validate_test()
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
iccdev_add_pcs_xyzconvert_matrix_channels_test()
iccdev_add_iccviz_metrics_test()
iccdev_add_iccviz_degenerate_test()
iccdev_add_iccviz_pdf_axis_test()
iccdev_add_iccviz_writer_serialization_test()
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
    "iccdev;tools;hybrid;asan;regression;slow;pr-extended"
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

# TIFFTAG_RESOLUTIONUNIT lives in shared CTiffImg code, so it is covered per caller:
# iccSpecSepToTiff and iccApplyProfiles both write through the same Create(), and
# iccTiffDump reads the same member back (#2220).  Guarded on all three targets
# because the cases are not independent of any one of them.
if(TARGET iccSpecSepToTiff AND TARGET iccApplyProfiles AND TARGET iccTiffDump)
  iccdev_add_script_test(
    iccdev.tiff-resolution-unit-regression
    120
    "iccdev;tools;tiff;resolution;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-tiff-resolution-unit-regression-tests.sh"
  )
endif()

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
    "iccdev;json;sort;regression;issue-1920;asan;slow;pr-extended"
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
#
# The known-red label is deliberately absent (#2160): it was added while the
# large-CLUT round trip could not pass against libxml2 >= 2.13, and the writer
# now bounds each text node, so the quarantine no longer describes anything
# true. Where that matters is ci-iccdev-tool-tests.yml run with ctest_mode=full
# and the local check target; both exclude known-red and neither excludes slow.
#
# It does NOT put this test on the pull_request path: ci-pr-action.yml pins
# ctest_mode=fast, which drops it on the slow label, and the Windows legs never
# register it at all because the if(WIN32) branch above returns before the bash
# script tests. Measured on dispatched run 31852372579 -- the case does not
# execute on any PR leg. Widening that is a separate question from this fix.
if(TARGET iccFromXml AND TARGET iccToXml AND TARGET iccApplyToLink)
  iccdev_add_script_test(
    iccdev.xml-parser-regressions
    600
    "iccdev;xml;parser;regression;issue-1856;issue-2108;issue-2160;asan;slow"
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
  iccdev.issue-2162-jpegdump-read-error
  60
  "iccdev;tools;jpegdump;issue-2162;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-2162-jpegdump-read-error.sh"
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

# Functional correctness for the tJab/fJab calculator operators of ICC.2:2023
# Table 94 (#2089). Kept separate from iccdev.calculator-regressions above so a
# Jab break is named as such rather than surfacing as one failed verdict out of
# the 86 that srgbCalcTest.xml aggregates. It builds its own profiles, so
# unlike its neighbour it needs no iccdev_profiles fixture.
if(TARGET iccApplyNamedCmm AND TARGET iccFromXml)
  iccdev_add_script_test(
    iccdev.issue-2089-jab-operators
    120
    "iccdev;calculator;jab;cam;issue-2089;regression;asan"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-2089-jab-operators-regression.sh"
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

# #2097: the same three tests above all concern a signature that is zero in all
# four bytes, which the call sites can (and now do) special-case.  This one is the
# case no call-site guard reaches: a signature whose zeros are only trailing.
# icGetSigStr() demanded its "%08Xh" escape for a zero *followed* by a non-zero
# byte, so 'DUP\0' took the four-character path and the zero terminated the C
# string -- "DUP", which icGetSigVal() pads back to 'DUP '.  Six registered tag
# signatures were rewritten by every XML or JSON round trip.  Nothing in the
# validator notices, since the corrupted value is unregistered rather than
# malformed, so the assertions are byte comparisons plus one anti-overfit check
# that printable signatures are still written as readable text.
# Plain functional test -- skips cleanly if the tools/fixtures are absent.
iccdev_add_script_test(
  iccdev.issue-2097-nul-signature-roundtrip
  90
  "iccdev;xml;json;signature;roundtrip;issue-2097;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-2097-nul-signature-roundtrip-regression.sh"
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
  #
  # #2124 adds the encoding-contract half: final_data_encoding is validated per
  # colour space, so the script pins a Lab and an XYZ destination against each
  # other. Both profiles it uses are tracked, so this still needs no fixture
  # dependency and the timeout is unchanged -- the added cases run the same
  # 16-row rgb8bit.txt transform the existing ones already do.
  iccdev_add_script_test(
    iccdev.applynamedcmm-cli-args
    120
    "iccdev;cmm;config;cli;security;issue-1674;issue-2124;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-applynamedcmm-cli-args-regression.sh"
   )
endif()

# The iccApplySearch half of the same #1674 argument-contract family. It publishes
# the identical "-cfg <path>" usage but never received the guard added above, so
# "iccApplySearch -cfg cfg.json anything-else" ignored the tail and still exited 0.
# Registered separately from the iccApplyNamedCmm block because the two tools are
# built by independent targets and either can be configured out on its own.
if(TARGET iccApplySearch)
  iccdev_add_script_test(
    iccdev.applysearch-cli-args
    120
    "iccdev;cmm;config;cli;security;issue-1674;issue-1853;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-applysearch-cli-args-regression.sh"
   )
endif()

# The iccApplyProfiles third of the same family (#2126). Both of its usages
# dropped trailing arguments and still exited 0, but by different routes: "-cfg"
# ignored any tail, while the positional usage ignored only an odd leftover token
# because CIccCfgProfileSequence::fromArgs() consumes the profile group in pairs.
# Timeout matches the two siblings: although every case here runs a real TIFF
# transform rather than a text data file, the fixture is an 8x8 image and the
# whole script measured under 1s against an ASAN+UBSAN build, so it needs no
# larger budget than they do.
if(TARGET iccApplyProfiles)
  iccdev_add_script_test(
    iccdev.applyprofiles-cli-args
    120
    "iccdev;cmm;config;cli;security;issue-1674;issue-2126;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-applyprofiles-cli-args-regression.sh"
   )
endif()

# #2150 - both iccApply* tools remap a PCS source signature to icSigDevXYZData /
# icSigDevLabData so PCS numbers are not reinterpreted as device data, then
# cleared the resulting 0.0-1.0 clip for icEncodeFloat only. Three encodings
# carry a PCS range wider than that, not one: icEncodeUnitFloat (an exact
# synonym of icEncodeFloat in both PCS arms since #2146) and icEncodePercent
# (the same range x100) were both left clipping, so the selector alone decided
# whether the data survived -- the tool refused to read back what it had just
# written, under both.
#
# Distinct from the *-cli-args siblings above, which assert whether arguments
# are accepted. This asserts what the accepted arguments then compute, so it
# cannot be folded into them -- and it covers both tools in one script because
# they carry independent copies of the remap block rather than sharing a helper.
#
# Needs the profile fixture: the branch is only reached when the first profile
# header carries a PCS colour space, and no such .icc is tracked -- PCC/
# XYZ_float-D50_2deg.icc and PCC/Lab_float-D50_2deg.icc are generated by
# CreateAllProfiles.sh from tracked XML. The device control uses a tracked
# profile but sits in the same script, so the whole test takes the dependency.
#
# Timeout matches the siblings: thirteen single-row text transforms, measured
# under an ASAN+UBSAN build at well under 1s for the test itself -- a margin of
# two orders of magnitude on the 120 budget.
if(TARGET iccApplyNamedCmm OR TARGET iccApplySearch)
  iccdev_add_script_test(
    iccdev.issue-2150-pcs-unitfloat-clip
    120
    "iccdev;cmm;tools;cli;encoding;issue-2146;issue-2150;regression"
    "${ICCDEV_REPO_ROOT}"
    "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-2150-pcs-unitfloat-clip-regression.sh"
   )
  set_tests_properties(iccdev.issue-2150-pcs-unitfloat-clip PROPERTIES
    FIXTURES_REQUIRED iccdev_profiles
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
  "iccdev;tools;applytolink;security;issue-1781;regression;slow;pr-extended"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1781-applytolink-qa-matrix.sh"
)

# #2011: the BT.2100 fixtures have to agree with themselves.  All four
# Testing/HDR/BT2100PQ*.xml declared cicp TransferCharacteristics="18" -- ARIB
# STD-B67 (HLG) per ITU-T H.273 Table 3 -- while carrying the SMPTE ST 2084
# constants, which is 16; three of them also described themselves as
# "BT.2100 HLG".  Both claims came from the HLG siblings they were copied from.
# Nothing caught it: CIccTagCicp::Validate reads MatrixCoefficients against the
# colour space and never inspects TransferCharacteristics at all, so every one
# of these validated clean before the correction and validates clean after.
# This test supplies the consistency check the library does not have, for the
# corpus, keyed on the curve constants rather than the file name so a renamed or
# copied fixture is still judged by what it encodes.  Two controls reintroduce
# each half of the defect and must be caught, so a grep that stopped matching
# cannot leave this passing on nothing.
# Runs without the tools built; the XML -> ICC -> XML half is skipped instead.
iccdev_add_script_test(
  iccdev.issue-2011-hdr-cicp-transfer-regression
  120
  "iccdev;xml;fixtures;hdr;cicp;issue-2011;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-2011-hdr-cicp-transfer-regression.sh"
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
