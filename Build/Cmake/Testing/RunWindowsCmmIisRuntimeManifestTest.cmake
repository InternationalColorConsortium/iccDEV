# Verify that the CMM and IIS runtime installers share one manifest entry for
# a vcpkg DLL installed by both paths, then replay the runtime uninstaller.
foreach(_required IN ITEMS
    ICCDEV_CMM_IIS_BUILD_DIR
    ICCDEV_CMM_IIS_BINDIR
    ICCDEV_CMM_IIS_ZLIB_DLL)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

string(TIMESTAMP _stamp "%Y%m%d%H%M%S")
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef _nonce)
set(CMAKE_INSTALL_PREFIX
  "${ICCDEV_CMM_IIS_BUILD_DIR}/Testing/cmm-iis-runtime-${_stamp}-${_nonce}")
set(CMAKE_INSTALL_CONFIG_NAME Release)
set(CMAKE_INSTALL_COMPONENT runtime)
set(_bin "${CMAKE_INSTALL_PREFIX}/${ICCDEV_CMM_IIS_BINDIR}")
set(_zlib "${_bin}/${ICCDEV_CMM_IIS_ZLIB_DLL}")

include("${ICCDEV_CMM_IIS_BUILD_DIR}/Tools/IccDEVCmm/cmake_install.cmake")
include("${ICCDEV_CMM_IIS_BUILD_DIR}/Tools/IccIisIsapi/install-iis-runtime-deps.cmake")

if(NOT EXISTS "${_zlib}")
  message(FATAL_ERROR "Combined CMM/IIS install is missing ${_zlib}")
endif()

set(_zlib_count 0)
foreach(_installed IN LISTS CMAKE_INSTALL_MANIFEST_FILES)
  string(FIND "${_installed}" "${CMAKE_INSTALL_PREFIX}/" _prefix_index)
  if(NOT _prefix_index EQUAL 0)
    message(FATAL_ERROR "Runtime manifest escaped the test prefix: ${_installed}")
  endif()
  if(_installed STREQUAL "${_zlib}")
    math(EXPR _zlib_count "${_zlib_count} + 1")
  endif()
endforeach()

set(_manifest "${ICCDEV_CMM_IIS_BUILD_DIR}/Testing/cmm-iis-runtime-manifest-${_stamp}-${_nonce}.txt")
string(REPLACE ";" "\n" _manifest_content "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "${_manifest}" "${_manifest_content}\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DICCDEV_UNINSTALL_MANIFEST=${_manifest}"
    -P "${ICCDEV_CMM_IIS_BUILD_DIR}/RefIccMAXUninstall.cmake"
  RESULT_VARIABLE _uninstall_result
  OUTPUT_VARIABLE _uninstall_output
  ERROR_VARIABLE _uninstall_error)
if(NOT _uninstall_result EQUAL 0 OR
   _uninstall_output MATCHES "does not exist\\." OR
   EXISTS "${_zlib}" OR
   NOT _zlib_count EQUAL 1)
  message(FATAL_ERROR
    "Combined CMM/IIS runtime manifest is invalid (zlib entries: ${_zlib_count}, "
    "uninstall result: ${_uninstall_result}):\n${_uninstall_output}\n${_uninstall_error}")
endif()
message(STATUS "Combined CMM/IIS runtime manifest and uninstall passed")
