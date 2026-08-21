#################################################################################
# Native high-output 3D CLUT regression
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################

set(_required_vars
  ICCDEV_TEST_OUTDIR
  ICCDEV_FROM_XML
  ICCDEV_DUMP_PROFILE
  ICCDEV_APPLY_NAMED_CMM
  ICCDEV_FIXTURE_XML_8
  ICCDEV_FIXTURE_XML_9
  ICCDEV_FIXTURE_XML_11
  ICCDEV_FIXTURE_XML_14
  ICCDEV_FIXTURE_XML_15
  ICCDEV_FIXTURE_INPUT
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

foreach(_required_file IN ITEMS
    "${ICCDEV_FROM_XML}"
    "${ICCDEV_DUMP_PROFILE}"
    "${ICCDEV_APPLY_NAMED_CMM}"
    "${ICCDEV_FIXTURE_XML_8}"
    "${ICCDEV_FIXTURE_XML_9}"
    "${ICCDEV_FIXTURE_XML_11}"
    "${ICCDEV_FIXTURE_XML_14}"
    "${ICCDEV_FIXTURE_XML_15}"
    "${ICCDEV_FIXTURE_INPUT}")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "required file not found: ${_required_file}")
  endif()
endforeach()

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")

function(iccdev_run_clut_case NAME FIXTURE_XML EXPECTED_OUTPUT)
  set(_fixture_icc "${ICCDEV_TEST_OUTDIR}/${NAME}.icc")

  execute_process(
    COMMAND "${ICCDEV_FROM_XML}" "${FIXTURE_XML}" "${_fixture_icc}"
    RESULT_VARIABLE _from_xml_result
    OUTPUT_VARIABLE _from_xml_stdout
    ERROR_VARIABLE _from_xml_stderr
  )
  if(NOT _from_xml_result EQUAL 0)
    message(FATAL_ERROR
      "iccFromXml failed for ${NAME} (${_from_xml_result})\n${_from_xml_stdout}${_from_xml_stderr}")
  endif()

  execute_process(
    COMMAND "${ICCDEV_DUMP_PROFILE}" -v 100 "${_fixture_icc}"
    RESULT_VARIABLE _dump_result
    OUTPUT_VARIABLE _dump_stdout
    ERROR_VARIABLE _dump_stderr
  )
  if(NOT _dump_result EQUAL 0)
    message(FATAL_ERROR
      "iccDumpProfile failed for ${NAME} (${_dump_result})\n${_dump_stdout}${_dump_stderr}")
  endif()
  if(NOT "${_dump_stdout}${_dump_stderr}" MATCHES "Profile is valid for version 5\\.10")
    message(FATAL_ERROR
      "converted ${NAME} fixture is not ICC-valid\n${_dump_stdout}${_dump_stderr}")
  endif()

  execute_process(
    COMMAND
      "${ICCDEV_APPLY_NAMED_CMM}"
      "${ICCDEV_FIXTURE_INPUT}" 3 0 "${_fixture_icc}" 0
    RESULT_VARIABLE _apply_result
    OUTPUT_VARIABLE _apply_stdout
    ERROR_VARIABLE _apply_stderr
  )
  if(NOT _apply_result EQUAL 0)
    message(FATAL_ERROR
      "iccApplyNamedCmm failed for ${NAME} (${_apply_result})\n${_apply_stdout}${_apply_stderr}")
  endif()

  set(_apply_output "${_apply_stdout}${_apply_stderr}")
  if(NOT _apply_output MATCHES "${EXPECTED_OUTPUT}")
    message(FATAL_ERROR
      "${NAME} CLUT output differs from the expected vector\n${_apply_output}")
  endif()
endfunction()

iccdev_run_clut_case(
  avx2-3d-8-output
  "${ICCDEV_FIXTURE_XML_8}"
  "0\\.2200[ \t]+0\\.2300[ \t]+0\\.2400[ \t]+0\\.2500[ \t]+0\\.2600[ \t]+0\\.2700[ \t]+0\\.2800[ \t]+0\\.2900"
)
iccdev_run_clut_case(
  avx2-3d-9-output
  "${ICCDEV_FIXTURE_XML_9}"
  "0\\.2200[ \t]+0\\.2300[ \t]+0\\.2400[ \t]+0\\.2500[ \t]+0\\.2600[ \t]+0\\.2700[ \t]+0\\.2800[ \t]+0\\.2900[ \t]+0\\.3000"
)
iccdev_run_clut_case(
  avx2-3d-11-output
  "${ICCDEV_FIXTURE_XML_11}"
  "0\\.2200[ \t]+0\\.2300[ \t]+0\\.2400[ \t]+0\\.2500[ \t]+0\\.2600[ \t]+0\\.2700[ \t]+0\\.2800[ \t]+0\\.2900[ \t]+0\\.3000[ \t]+0\\.3100[ \t]+0\\.3200"
)
iccdev_run_clut_case(
  avx2-3d-14-output
  "${ICCDEV_FIXTURE_XML_14}"
  "0\\.2200[ \t]+0\\.2300[ \t]+0\\.2400[ \t]+0\\.2500[ \t]+0\\.2600[ \t]+0\\.2700[ \t]+0\\.2800[ \t]+0\\.2900[ \t]+0\\.3000[ \t]+0\\.3100[ \t]+0\\.3200[ \t]+0\\.3300[ \t]+0\\.3400[ \t]+0\\.3500"
)
iccdev_run_clut_case(
  avx2-3d-15-output
  "${ICCDEV_FIXTURE_XML_15}"
  "0\\.2200[ \t]+0\\.2300[ \t]+0\\.2400[ \t]+0\\.2500[ \t]+0\\.2600[ \t]+0\\.2700[ \t]+0\\.2800[ \t]+0\\.2900[ \t]+0\\.3000[ \t]+0\\.3100[ \t]+0\\.3200[ \t]+0\\.3300[ \t]+0\\.3400[ \t]+0\\.3500[ \t]+0\\.3600"
)

message(STATUS "[PASS] clut-eight-output-regression")
