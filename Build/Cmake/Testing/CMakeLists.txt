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
    endif()
  endforeach()
endfunction()

iccdev_add_check_dependencies()

function(iccdev_add_iccconnect_thread_test)
  if(NOT TARGET "${TARGET_LIB_ICCCONNECT}")
    return()
  endif()

  add_executable(iccConnectThreadTest
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

function(iccdev_add_embedio_read8_bounds_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  add_executable(iccEmbedIoRead8BoundsTest
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

function(iccdev_add_fileio_getlength_position_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  add_executable(iccFileIoGetLengthPositionTest
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

  add_executable(iccFileIoSeekTellTest
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

  add_executable(iccProfileWriteFailureTest
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

function(iccdev_add_parser_restore_calls_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  add_executable(iccParserRestoreCallsTest
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

  iccdev_add_iccconnect_thread_test()
  iccdev_add_embedio_read8_bounds_test()
  iccdev_add_fileio_getlength_position_test()
  iccdev_add_profile_write_failure_test()
  iccdev_add_fileio_seek_tell_test()
  iccdev_add_parser_restore_calls_test()

  return()
endif()

if(NOT BASH_EXECUTABLE)
  message(WARNING "bash not found; iccDEV CTest script tests are disabled.")
  return()
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

list(JOIN ICCDEV_TOOL_PATHS ":" ICCDEV_TOOL_PATH)
set(ICCDEV_SHARED_LIBRARY_PATH
    "${CMAKE_BINARY_DIR}/IccProfLib:${CMAKE_BINARY_DIR}/IccXML:${CMAKE_BINARY_DIR}/IccJSON:${CMAKE_BINARY_DIR}/IccConnect")

set(ICCDEV_TEST_ENV
  "PATH=${ICCDEV_TOOL_PATH}:$ENV{PATH}"
  "LD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
  "DYLD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}"
  "CC=${CMAKE_C_COMPILER}"
  "CXX=${CMAKE_CXX_COMPILER}"
  "ASAN_OPTIONS=halt_on_error=0,detect_leaks=0"
  "UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1"
  "LLVM_PROFILE_FILE=/dev/null"
  "ICCDEV_TOOLS_DIR=${ICCDEV_TOOLS_DIR}"
  "ICCDEV_TESTING_DIR=${ICCDEV_TESTING_DIR}"
  "ICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "ICCDEV_REQUIRE_JSON_ROUNDTRIP=1"
)

iccdev_add_iccconnect_thread_test()
iccdev_add_embedio_read8_bounds_test()
iccdev_add_fileio_getlength_position_test()
iccdev_add_profile_write_failure_test()
iccdev_add_fileio_seek_tell_test()
iccdev_add_parser_restore_calls_test()

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

iccdev_add_script_test(
  iccdev.specsep-tiff-geometry-regression
  120
  "iccdev;specsep;tiff;geometry;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-specsep-tiff-geometry-regression-tests.sh"
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
  iccdev.issue-1121-json-envvars-regression
  60
  "iccdev;json;cfg;envvars;issue-1121;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1121-json-envvars-regression.sh"
)

iccdev_add_script_test(
  iccdev.issue-1166-cam-hyperbolicinv-regression
  60
  "iccdev;json;cfg;cam;issue-1166;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1166-cam-hyperbolicinv-regression.sh"
)

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

iccdev_add_script_test(
  iccdev.issue-1079-fromjson-soft-failure
  60
  "iccdev;json;fromjson;issue-1079;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1079-fromjson-soft-failure.sh"
)

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

iccdev_add_script_test(
  iccdev.cam-degenerate-regressions
  300
  "iccdev;cam;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-cam-degenerate-regression-tests.sh"
)

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
  iccdev.issue-1151-pngdump-icc-injection
  60
  "iccdev;tools;pngdump;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1151-pngdump-icc-injection.sh"
)

iccdev_add_script_test(
  iccdev.issue-1167-jpegdump-byte-regression
  120
  "iccdev;tools;jpegdump;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-issue-1167-jpegdump-byte-regression.sh"
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
  iccdev.calculator-regressions
  300
  "iccdev;calculator;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-calculator-regression-tests.sh"
)
set_tests_properties(iccdev.calculator-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)

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
