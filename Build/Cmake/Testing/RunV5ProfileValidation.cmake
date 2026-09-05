if(NOT DEFINED ICCDEV_FROM_XML OR NOT EXISTS "${ICCDEV_FROM_XML}")
  message(FATAL_ERROR "ICCDEV_FROM_XML is not an executable path")
endif()
if(NOT DEFINED ICCDEV_DUMP_PROFILE OR NOT EXISTS "${ICCDEV_DUMP_PROFILE}")
  message(FATAL_ERROR "ICCDEV_DUMP_PROFILE is not an executable path")
endif()
if(NOT DEFINED ICCDEV_FIXTURE_DIR OR NOT IS_DIRECTORY "${ICCDEV_FIXTURE_DIR}")
  message(FATAL_ERROR "ICCDEV_FIXTURE_DIR is not a directory")
endif()
if(NOT DEFINED ICCDEV_TEST_OUTDIR)
  message(FATAL_ERROR "ICCDEV_TEST_OUTDIR is required")
endif()

file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")

function(iccdev_run_v5_profile_case NAME EXPECT_VALID EXPECTED_TEXT)
  set(_xml "${ICCDEV_FIXTURE_DIR}/${NAME}.xml")
  set(_icc "${ICCDEV_TEST_OUTDIR}/${NAME}.icc")

  if(NOT EXISTS "${_xml}")
    message(FATAL_ERROR "Missing fixture: ${_xml}")
  endif()

  execute_process(
    COMMAND "${ICCDEV_FROM_XML}" "${_xml}" "${_icc}"
    RESULT_VARIABLE _from_result
    OUTPUT_VARIABLE _from_stdout
    ERROR_VARIABLE _from_stderr
  )
  set(_from_combined "${_from_stdout}${_from_stderr}")
  if(NOT "${_from_result}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "${NAME}: iccFromXml terminated abnormally (${_from_result})\n"
      "${_from_combined}")
  endif()
  if(EXPECT_VALID AND NOT _from_result EQUAL 0)
    message(FATAL_ERROR
      "${NAME}: expected iccFromXml success, got ${_from_result}\n"
      "${_from_combined}")
  endif()
  if(NOT EXPECT_VALID AND _from_result EQUAL 0)
    message(FATAL_ERROR
      "${NAME}: expected iccFromXml rejection, got success\n"
      "${_from_combined}")
  endif()
  if(NOT EXISTS "${_icc}")
    message(FATAL_ERROR "${NAME}: iccFromXml did not preserve the profile artifact")
  endif()
  file(SIZE "${_icc}" _icc_size)
  if(_icc_size EQUAL 0)
    message(FATAL_ERROR "${NAME}: iccFromXml wrote an empty profile artifact")
  endif()

  execute_process(
    COMMAND "${ICCDEV_DUMP_PROFILE}" -v 100 "${_icc}"
    RESULT_VARIABLE _dump_result
    OUTPUT_VARIABLE _dump_stdout
    ERROR_VARIABLE _dump_stderr
  )
  set(_dump_combined "${_dump_stdout}${_dump_stderr}")
  set(_combined "${_from_combined}${_dump_combined}")

  if(NOT "${_dump_result}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "${NAME}: iccDumpProfile terminated abnormally (${_dump_result})\n"
      "${_combined}")
  endif()

  if(EXPECT_VALID AND NOT _dump_result EQUAL 0)
    message(FATAL_ERROR
      "${NAME}: expected successful validation, got ${_dump_result}\n${_combined}")
  endif()
  if(NOT EXPECT_VALID AND _dump_result EQUAL 0)
    message(FATAL_ERROR
      "${NAME}: expected validation failure, got success\n${_combined}")
  endif()

  string(FIND "${_combined}" "${EXPECTED_TEXT}" _expected_at)
  if(_expected_at EQUAL -1)
    message(FATAL_ERROR
      "${NAME}: expected text not found: ${EXPECTED_TEXT}\n${_combined}")
  endif()
  if(NOT EXPECT_VALID)
    string(FIND "${_from_combined}" "${EXPECTED_TEXT}" _from_expected_at)
    string(FIND "${_dump_combined}" "${EXPECTED_TEXT}" _dump_expected_at)
    if(_from_expected_at EQUAL -1 OR _dump_expected_at EQUAL -1)
      message(FATAL_ERROR
        "${NAME}: expected rejection from both tools: ${EXPECTED_TEXT}\n${_combined}")
    endif()
  endif()

  if(_combined MATCHES
      "AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer|DEADLYSIGNAL")
    message(FATAL_ERROR "${NAME}: sanitizer diagnostic found\n${_combined}")
  endif()

  message(STATUS "PASS: ${NAME}")
endfunction()

iccdev_run_v5_profile_case(
  v5-sref-two-step-boundary TRUE "Profile is valid for version 5.10")
iccdev_run_v5_profile_case(
  v5-sref-direct-36-channel TRUE "Profile is valid for version 5.10")
iccdev_run_v5_profile_case(
  v5-sref-direct-81-channel TRUE "Profile is valid for version 5.10")
iccdev_run_v5_profile_case(
  v5-sref-spectral-range-mismatch FALSE
  "Number of channels defined for spectral PCS do not match spectral range definition.")
iccdev_run_v5_profile_case(
  v5-sref-one-step-review FALSE
  "spectral wavelength steps!")
iccdev_run_v5_profile_case(
  v4-illegal-nchannel-space FALSE
  "Invalid data colour space (0x6E630006) for a v2/v4 profile; only iccMAX (v5) permits this!")
