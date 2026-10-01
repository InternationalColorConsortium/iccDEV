# Verify opt-in CLUT counters with a chain that performs two 3D lookups per pixel.
foreach(_name IN ITEMS ICCDEV_FROM_XML ICCDEV_BENCH_APPLY ICCDEV_RGB_XML
    ICCDEV_CMYK_XML ICCDEV_TEST_OUTDIR)
  if(NOT DEFINED ${_name} OR "${${_name}}" STREQUAL "")
    message(FATAL_ERROR "${_name} is required")
  endif()
endforeach()

file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_rgb "${ICCDEV_TEST_OUTDIR}/rgb.icc")
set(_cmyk "${ICCDEV_TEST_OUTDIR}/cmyk.icc")
foreach(_fixture IN ITEMS rgb cmyk)
  if(_fixture STREQUAL "rgb")
    set(_fixture_xml "${ICCDEV_RGB_XML}")
    set(_fixture_icc "${_rgb}")
  else()
    set(_fixture_xml "${ICCDEV_CMYK_XML}")
    set(_fixture_icc "${_cmyk}")
  endif()
  execute_process(
    COMMAND "${ICCDEV_FROM_XML}" "${_fixture_xml}" "${_fixture_icc}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "iccFromXml ${_fixture} failed: ${_stdout}${_stderr}")
  endif()
endforeach()

foreach(_mode IN ITEMS linear tetra)
  if(_mode STREQUAL "linear")
    set(_interpolation 0)
    set(_kind 3d_linear)
  else()
    set(_interpolation 1)
    set(_kind 3d_tetra)
  endif()
  foreach(_stride IN ITEMS 1 16)
    set(_report "${ICCDEV_TEST_OUTDIR}/${_mode}-${_stride}.perf.txt")
    file(REMOVE "${_report}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "ICC_PERF_STATS_FILE=${_report}"
        "ICC_PERF_CLUT_TIMING_STRIDE=${_stride}"
        "${ICCDEV_BENCH_APPLY}" -pixels 1024 -repeats 1 -csv
        "${_interpolation}" "${_rgb}" 1 "${_cmyk}" 1
      RESULT_VARIABLE _result
      OUTPUT_VARIABLE _stdout
      ERROR_VARIABLE _stderr)
    if(NOT _result EQUAL 0 OR NOT "${_stdout}${_stderr}" MATCHES ",ok")
      message(FATAL_ERROR "iccBenchApply ${_mode} failed: ${_stdout}${_stderr}")
    endif()
    if(NOT EXISTS "${_report}")
      message(FATAL_ERROR "${_mode} performance report was not written")
    endif()
    file(READ "${_report}" _content)
    math(EXPR _samples "4096 / ${_stride}")
    foreach(_expected IN ITEMS
        "format=iccdev-perf-v1"
        "clut_timing_stride=${_stride}"
        "clut_all_calls_total=4096"
        "clut_all_calls_${_kind}=4096"
        "clut_all_timed_samples_${_kind}=${_samples}"
        "clut_all_calls_outputs_3=2048"
        "clut_all_calls_outputs_4=2048")
      string(FIND "${_content}" "${_expected}\n" _position)
      if(_position EQUAL -1)
        message(FATAL_ERROR "${_mode} report missing ${_expected}\n${_content}")
      endif()
    endforeach()
    if(NOT _content MATCHES "clut_all_elapsed_ns_${_kind}=[1-9][0-9]*")
      message(FATAL_ERROR "${_mode} elapsed counter is absent or zero")
    endif()
    if(_mode STREQUAL "tetra" AND NOT _content MATCHES "clut_elapsed_ns=0\n")
      message(FATAL_ERROR "tetra changed the legacy 3D linear elapsed counter")
    endif()
  endforeach()
endforeach()

message(STATUS "[PASS] CLUT performance counters")
