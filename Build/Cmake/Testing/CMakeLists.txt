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
      iccDumpProfile
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
  )
    if(TARGET "${_target}")
      add_dependencies(check "${_target}")
    endif()
  endforeach()
endfunction()

iccdev_add_check_dependencies()

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

  iccdev_add_windows_batch_test(
    iccdev.windows-create-profiles
    300
    "iccdev;windows;setup;profiles"
    "${ICCDEV_TESTING_DIR}/CreateAllProfiles.bat"
    EXPECTED_PROFILE_PARSE_COUNT 129
  )
  set_tests_properties(iccdev.windows-create-profiles PROPERTIES
    FIXTURES_SETUP iccdev_profiles
  )

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
  "${ICCDEV_TOOLS_DIR}/IccPngDump"
  "${ICCDEV_TOOLS_DIR}/IccRoundTrip"
  "${ICCDEV_TOOLS_DIR}/IccSpecSepToTiff"
  "${ICCDEV_TOOLS_DIR}/IccTiffDump"
  "${ICCDEV_TOOLS_DIR}/IccToJson"
  "${ICCDEV_TOOLS_DIR}/IccToXml"
  "${ICCDEV_TOOLS_DIR}/IccV5DspObsToV4Dsp"
)

list(JOIN ICCDEV_TOOL_PATHS ":" ICCDEV_TOOL_PATH)
set(ICCDEV_SHARED_LIBRARY_PATH
    "${CMAKE_BINARY_DIR}/IccProfLib:${CMAKE_BINARY_DIR}/IccXML:${CMAKE_BINARY_DIR}/IccJSON")

set(ICCDEV_TEST_ENV
  "PATH=${ICCDEV_TOOL_PATH}:$ENV{PATH}"
  "LD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
  "DYLD_LIBRARY_PATH=${ICCDEV_SHARED_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}"
  "ASAN_OPTIONS=halt_on_error=0,detect_leaks=0"
  "UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1"
  "LLVM_PROFILE_FILE=/dev/null"
  "ICCDEV_TOOLS_DIR=${ICCDEV_TOOLS_DIR}"
  "ICCDEV_TESTING_DIR=${ICCDEV_TESTING_DIR}"
  "ICCDEV_BUILD_DIR=${CMAKE_BINARY_DIR}"
  "ICCDEV_REQUIRE_JSON_ROUNDTRIP=1"
)

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
  900
  "iccdev;tools;asan;regression"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-tool-coverage-baseline.sh"
  --asan
)
set_tests_properties(iccdev.tool-coverage PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
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
  iccdev.version-bcd-regressions
  300
  "iccdev;version;bcd;regression;asan"
  "${ICCDEV_REPO_ROOT}"
  "${ICCDEV_REPO_ROOT}/.github/scripts/iccdev-version-bcd-regression-tests.sh"
)
set_tests_properties(iccdev.version-bcd-regressions PROPERTIES
  FIXTURES_REQUIRED iccdev_profiles
)
