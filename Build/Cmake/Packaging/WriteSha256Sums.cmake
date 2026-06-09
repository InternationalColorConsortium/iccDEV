set(_sha_files)

if(DEFINED ICCDEV_SHA256_FILES AND NOT ICCDEV_SHA256_FILES STREQUAL "")
  list(APPEND _sha_files ${ICCDEV_SHA256_FILES})
endif()

if(DEFINED ICCDEV_SHA256_GLOB_DIR AND DEFINED ICCDEV_SHA256_PREFIX)
  file(GLOB _globbed LIST_DIRECTORIES false "${ICCDEV_SHA256_GLOB_DIR}/${ICCDEV_SHA256_PREFIX}*")
  list(APPEND _sha_files ${_globbed})
endif()

if(NOT _sha_files)
  message(FATAL_ERROR "No files provided for SHA256SUMS")
endif()

list(REMOVE_DUPLICATES _sha_files)
list(SORT _sha_files)

if(NOT DEFINED ICCDEV_SHA256_OUTPUT OR ICCDEV_SHA256_OUTPUT STREQUAL "")
  message(FATAL_ERROR "ICCDEV_SHA256_OUTPUT is required")
endif()

file(WRITE "${ICCDEV_SHA256_OUTPUT}" "")
foreach(_sha_file IN LISTS _sha_files)
  if(NOT EXISTS "${_sha_file}")
    message(FATAL_ERROR "Cannot checksum missing file: ${_sha_file}")
  endif()
  get_filename_component(_sha_name "${_sha_file}" NAME)
  if(_sha_name STREQUAL "SHA256SUMS" OR _sha_name MATCHES "\\.sha256$")
    continue()
  endif()
  file(SHA256 "${_sha_file}" _sha_value)
  file(APPEND "${ICCDEV_SHA256_OUTPUT}" "${_sha_value}  ${_sha_name}\n")
endforeach()
