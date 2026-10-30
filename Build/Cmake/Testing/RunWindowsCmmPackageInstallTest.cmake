# Exercise the CMM runtime install without the optional IIS install rules.
foreach(_required IN ITEMS
    ICCDEV_CMM_PACKAGE_BUILD_DIR
    ICCDEV_CMM_PACKAGE_SMOKE
    ICCDEV_CMM_PACKAGE_REPO_ROOT
    ICCDEV_CMM_PACKAGE_BINDIR
    ICCDEV_CMM_PACKAGE_DOCDIR
    ICCDEV_CMM_PACKAGE_COMPONENT
    ICCDEV_CMM_PACKAGE_ZLIB_DLL)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

string(TIMESTAMP _stamp "%Y%m%d%H%M%S")
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef _nonce)
set(_prefix
  "${ICCDEV_CMM_PACKAGE_BUILD_DIR}/Testing/cmm-runtime-install-${_stamp}-${_nonce}")
set(_bin "${_prefix}/${ICCDEV_CMM_PACKAGE_BINDIR}")

foreach(_install_script IN ITEMS
    "${ICCDEV_CMM_PACKAGE_BUILD_DIR}/IccProfLib/cmake_install.cmake"
    "${ICCDEV_CMM_PACKAGE_BUILD_DIR}/Tools/IccDEVCmm/cmake_install.cmake")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DCMAKE_INSTALL_PREFIX=${_prefix}"
      "-DCMAKE_INSTALL_CONFIG_NAME=Release"
      "-DCMAKE_INSTALL_COMPONENT=${ICCDEV_CMM_PACKAGE_COMPONENT}"
      -P "${_install_script}"
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_output
    ERROR_VARIABLE _install_error)
  if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR
      "CMM component install failed (${_install_script}):\n${_install_output}\n${_install_error}")
  endif()
endforeach()

foreach(_required_file IN ITEMS
    "${_bin}/IccDEVCmm.dll"
    "${_bin}/IccProfLib2.dll"
    "${_bin}/${ICCDEV_CMM_PACKAGE_ZLIB_DLL}"
    "${_prefix}/${ICCDEV_CMM_PACKAGE_DOCDIR}/licenses/zlib.txt")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "CMM runtime package is missing ${_required_file}")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${ICCDEV_CMM_PACKAGE_SMOKE}" "${_bin}/iccDEVCmmSmokeTest.exe"
  RESULT_VARIABLE _copy_result)
if(NOT _copy_result EQUAL 0)
  message(FATAL_ERROR "Could not stage CMM smoke executable")
endif()

set(_saved_path "$ENV{PATH}")
set(ENV{PATH} "${_bin};$ENV{SystemRoot}/System32;$ENV{SystemRoot}")
execute_process(
  COMMAND "${_bin}/iccDEVCmmSmokeTest.exe"
    "${_bin}/IccDEVCmm.dll"
    "${ICCDEV_CMM_PACKAGE_REPO_ROOT}/Testing/sRGB_v4_ICC_preference.icc"
    "${ICCDEV_CMM_PACKAGE_REPO_ROOT}/.github/ci/test-data/cmm-namedcolor-v4.icc"
  RESULT_VARIABLE _smoke_result
  OUTPUT_VARIABLE _smoke_output
  ERROR_VARIABLE _smoke_error)
set(ENV{PATH} "${_saved_path}")
if(NOT _smoke_result EQUAL 0)
  message(FATAL_ERROR
    "CMM isolated runtime smoke failed (${_smoke_result}):\n${_smoke_output}\n${_smoke_error}")
endif()
message(STATUS "CMM isolated runtime install and smoke passed: ${_prefix}")
