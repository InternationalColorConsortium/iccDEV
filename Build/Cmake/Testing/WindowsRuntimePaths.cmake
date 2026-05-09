function(iccdev_add_existing_path_entry OUT_VAR CANDIDATE)
  if("${CANDIDATE}" STREQUAL "")
    return()
  endif()

  file(TO_CMAKE_PATH "${CANDIDATE}" _candidate_path)
  if(EXISTS "${_candidate_path}")
    set(_path_entries_local "${${OUT_VAR}}")
    list(APPEND _path_entries_local "${_candidate_path}")
    list(REMOVE_DUPLICATES _path_entries_local)
    set(${OUT_VAR} "${_path_entries_local}" PARENT_SCOPE)
  endif()
endfunction()

function(iccdev_read_cache_value OUT_VAR BUILD_DIR CACHE_NAME)
  set(${OUT_VAR} "" PARENT_SCOPE)
  set(_cache_file "${BUILD_DIR}/CMakeCache.txt")
  if(NOT EXISTS "${_cache_file}")
    return()
  endif()

  file(STRINGS "${_cache_file}" _cache_lines REGEX "^${CACHE_NAME}:[^=]*=")
  if(NOT _cache_lines)
    return()
  endif()

  list(GET _cache_lines 0 _cache_line)
  string(REGEX REPLACE "^[^=]*=" "" _cache_value "${_cache_line}")
  set(${OUT_VAR} "${_cache_value}" PARENT_SCOPE)
endfunction()

function(iccdev_collect_cache_runtime_path_entries OUT_VAR BUILD_DIR)
  set(_runtime_path_entries)

  iccdev_read_cache_value(_cmake_prefix_path "${BUILD_DIR}" CMAKE_PREFIX_PATH)
  foreach(_prefix IN LISTS _cmake_prefix_path)
    iccdev_add_existing_path_entry(_runtime_path_entries "${_prefix}/bin")
  endforeach()

  iccdev_read_cache_value(_vcpkg_installed_dir "${BUILD_DIR}" VCPKG_INSTALLED_DIR)
  iccdev_read_cache_value(_vcpkg_target_triplet "${BUILD_DIR}" VCPKG_TARGET_TRIPLET)
  if(NOT "${_vcpkg_installed_dir}" STREQUAL ""
      AND NOT "${_vcpkg_target_triplet}" STREQUAL "")
    iccdev_add_existing_path_entry(
      _runtime_path_entries
      "${_vcpkg_installed_dir}/${_vcpkg_target_triplet}/bin")
  endif()

  foreach(_compiler_cache_name CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
    iccdev_read_cache_value(_compiler_path "${BUILD_DIR}" "${_compiler_cache_name}")
    if(NOT "${_compiler_path}" STREQUAL "" AND EXISTS "${_compiler_path}")
      get_filename_component(_compiler_dir "${_compiler_path}" DIRECTORY)
      iccdev_add_existing_path_entry(_runtime_path_entries "${_compiler_dir}")
    endif()
  endforeach()

  foreach(_library_cache_name
      LIBXML2_LIBRARY
      PNG_LIBRARY
      PNG_LIBRARY_RELEASE
      JPEG_LIBRARY
      JPEG_LIBRARY_RELEASE
      TIFF_LIBRARY
      TIFF_LIBRARY_RELEASE
      ZLIB_LIBRARY
      ZLIB_LIBRARY_RELEASE
  )
    iccdev_read_cache_value(_library_path "${BUILD_DIR}" "${_library_cache_name}")
    if(NOT "${_library_path}" STREQUAL "" AND EXISTS "${_library_path}")
      get_filename_component(_library_dir "${_library_path}" DIRECTORY)
      get_filename_component(_library_prefix "${_library_dir}/.." ABSOLUTE)
      iccdev_add_existing_path_entry(_runtime_path_entries "${_library_prefix}/bin")
    endif()
  endforeach()

  iccdev_add_existing_path_entry(_runtime_path_entries "$ENV{SystemRoot}/System32")
  iccdev_add_existing_path_entry(_runtime_path_entries "$ENV{SystemRoot}")

  set(${OUT_VAR} "${_runtime_path_entries}" PARENT_SCOPE)
endfunction()
