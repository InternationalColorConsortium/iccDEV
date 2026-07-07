# Stage Windows runtime DLL dependencies next to build-tree tools.

set(_required_vars
  ICCDEV_BUILD_DIR
  ICCDEV_RUNTIME_DIR
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/Testing/WindowsRuntimePaths.cmake")

file(MAKE_DIRECTORY "${ICCDEV_RUNTIME_DIR}")

set(_runtime_dependency_dirs)

iccdev_read_cache_value(_cmake_prefix_path "${ICCDEV_BUILD_DIR}" CMAKE_PREFIX_PATH)
foreach(_prefix IN LISTS _cmake_prefix_path)
  iccdev_add_existing_path_entry(_runtime_dependency_dirs "${_prefix}/bin")
endforeach()

iccdev_read_cache_value(_vcpkg_installed_dir "${ICCDEV_BUILD_DIR}" VCPKG_INSTALLED_DIR)
iccdev_read_cache_value(_vcpkg_target_triplet "${ICCDEV_BUILD_DIR}" VCPKG_TARGET_TRIPLET)
if(NOT "${_vcpkg_installed_dir}" STREQUAL ""
    AND NOT "${_vcpkg_target_triplet}" STREQUAL "")
  if(DEFINED ICCDEV_CONFIG AND ICCDEV_CONFIG MATCHES "^[Dd]ebug$")
    iccdev_add_existing_path_entry(
      _runtime_dependency_dirs
      "${_vcpkg_installed_dir}/${_vcpkg_target_triplet}/debug/bin")
  endif()
  iccdev_add_existing_path_entry(
    _runtime_dependency_dirs
    "${_vcpkg_installed_dir}/${_vcpkg_target_triplet}/bin")
endif()

foreach(_library_cache_name
    LIBXML2_LIBRARY
    PNG_LIBRARY
    PNG_LIBRARY_RELEASE
    JPEG_LIBRARY
    JPEG_LIBRARY_RELEASE
    TIFF_LIBRARY
    TIFF_LIBRARY_RELEASE
    TIFF_LIBRARY_DEBUG
    ZLIB_LIBRARY
    ZLIB_LIBRARY_RELEASE
    ZLIB_LIBRARY_DEBUG
    LIBXML2_LIBRARY_RELEASE
    LIBXML2_LIBRARY_DEBUG
    PNG_LIBRARY_DEBUG
    JPEG_LIBRARY_DEBUG
)
  iccdev_read_cache_value(_library_path "${ICCDEV_BUILD_DIR}" "${_library_cache_name}")
  if(NOT "${_library_path}" STREQUAL "" AND EXISTS "${_library_path}")
    get_filename_component(_library_dir "${_library_path}" DIRECTORY)
    get_filename_component(_library_prefix "${_library_dir}/.." ABSOLUTE)
    iccdev_add_existing_path_entry(_runtime_dependency_dirs "${_library_prefix}/bin")
  endif()
endforeach()

iccdev_read_cache_value(_clangcl_asan_runtime_dll "${ICCDEV_BUILD_DIR}" ICCDEV_CLANGCL_ASAN_RUNTIME_DLL)
if(NOT "${_clangcl_asan_runtime_dll}" STREQUAL "" AND EXISTS "${_clangcl_asan_runtime_dll}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_clangcl_asan_runtime_dll}"
      "${ICCDEV_RUNTIME_DIR}"
    RESULT_VARIABLE _copy_result)
  if(NOT _copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy ${_clangcl_asan_runtime_dll} to ${ICCDEV_RUNTIME_DIR}")
  endif()
endif()

list(REMOVE_DUPLICATES _runtime_dependency_dirs)

foreach(_runtime_dependency_dir IN LISTS _runtime_dependency_dirs)
  file(GLOB _runtime_dlls "${_runtime_dependency_dir}/*.dll")
  foreach(_runtime_dll IN LISTS _runtime_dlls)
    get_filename_component(_runtime_dll_name "${_runtime_dll}" NAME)
    if(NOT "${_runtime_dependency_dir}" STREQUAL "${ICCDEV_RUNTIME_DIR}")
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${_runtime_dll}"
          "${ICCDEV_RUNTIME_DIR}/${_runtime_dll_name}"
        RESULT_VARIABLE _copy_result)
      if(NOT _copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to copy ${_runtime_dll} to ${ICCDEV_RUNTIME_DIR}")
      endif()
    endif()
  endforeach()
endforeach()

message(STATUS "Staged Windows runtime DLL dependencies into ${ICCDEV_RUNTIME_DIR}")
