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

# Adds one dependency prefix's runtime directories in the order the loader must
# see them. Every prefix this file deals with has the same two-tree shape --
# <prefix>/bin holds release DLLs, <prefix>/debug/bin holds debug ones -- and
# both trees spell the DLL identically (libxml2.dll either way), so whichever
# lands on PATH first is the one that binds.
#
# This is done per prefix rather than once for vcpkg because the ordering is
# otherwise trivially defeated: CMAKE_PREFIX_PATH is read before the vcpkg block
# below, and list(REMOVE_DUPLICATES) keeps an entry's FIRST position, so a prefix
# path naming the same triplet directory would pin the release tree ahead of the
# debug one no matter what the vcpkg block did afterwards.
function(iccdev_add_prefix_runtime_entries OUT_VAR PREFIX CONFIG)
  set(_prefix_entries_local "${${OUT_VAR}}")
  if(CONFIG MATCHES "^[Dd][Ee][Bb][Uu][Gg]$")
    iccdev_add_existing_path_entry(_prefix_entries_local "${PREFIX}/debug/bin")
    iccdev_add_existing_path_entry(_prefix_entries_local "${PREFIX}/bin")
  else()
    iccdev_add_existing_path_entry(_prefix_entries_local "${PREFIX}/bin")
    iccdev_add_existing_path_entry(_prefix_entries_local "${PREFIX}/debug/bin")
  endif()
  set(${OUT_VAR} "${_prefix_entries_local}" PARENT_SCOPE)
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

  # Resolved first, because every prefix added below is ordered by it.
  # ICCDEV_CONFIG is set by StageWindowsRuntime.cmake, RunWindowsBatchTest.cmake
  # and RunInstalledPackageConsumerTest.cmake; RunWindowsDumpProfileSmokeTest and
  # RunWindowsPawgReportSmokeTest do not set it, and a multi-config generator
  # leaves the cache's CMAKE_BUILD_TYPE empty too. Those callers get the
  # release-first order, which is what every caller got before this helper knew
  # about debug trees at all -- in-tree tests are unaffected either way because
  # StageWindowsRuntime.cmake copies dependency DLLs next to the staged binaries.
  set(_runtime_config "")
  if(DEFINED ICCDEV_CONFIG)
    set(_runtime_config "${ICCDEV_CONFIG}")
  endif()
  if("${_runtime_config}" STREQUAL "")
    iccdev_read_cache_value(_runtime_config "${BUILD_DIR}" CMAKE_BUILD_TYPE)
  endif()

  iccdev_read_cache_value(_cmake_prefix_path "${BUILD_DIR}" CMAKE_PREFIX_PATH)
  foreach(_prefix IN LISTS _cmake_prefix_path)
    iccdev_add_prefix_runtime_entries(_runtime_path_entries "${_prefix}" "${_runtime_config}")
  endforeach()

  # vcpkg splits DLLs by configuration: release in <installed>/<triplet>/bin and
  # debug in <installed>/<triplet>/debug/bin. Adding only the release tree is
  # why an out-of-tree consumer on the Windows Debug leg dies at load with
  # 0xc0000135 (STATUS_DLL_NOT_FOUND, which names no DLL). In-tree tests never
  # notice, because StageWindowsRuntime.cmake copies the dependency DLLs next to
  # the binaries it stages. `_VCPKG_INSTALLED_DIR` is the vcpkg toolchain's own
  # internal spelling and is the only one present in some vcpkg versions.
  set(_vcpkg_installed_dir "")
  foreach(_vcpkg_cache_name VCPKG_INSTALLED_DIR _VCPKG_INSTALLED_DIR)
    iccdev_read_cache_value(_vcpkg_installed_dir "${BUILD_DIR}" "${_vcpkg_cache_name}")
    if(NOT "${_vcpkg_installed_dir}" STREQUAL "")
      break()
    endif()
  endforeach()
  iccdev_read_cache_value(_vcpkg_target_triplet "${BUILD_DIR}" VCPKG_TARGET_TRIPLET)
  if(NOT "${_vcpkg_installed_dir}" STREQUAL ""
      AND NOT "${_vcpkg_target_triplet}" STREQUAL "")
    iccdev_add_prefix_runtime_entries(
      _runtime_path_entries
      "${_vcpkg_installed_dir}/${_vcpkg_target_triplet}"
      "${_runtime_config}")
  endif()

  foreach(_compiler_cache_name CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
    iccdev_read_cache_value(_compiler_path "${BUILD_DIR}" "${_compiler_cache_name}")
    if(NOT "${_compiler_path}" STREQUAL "" AND EXISTS "${_compiler_path}")
      get_filename_component(_compiler_dir "${_compiler_path}" DIRECTORY)
      iccdev_add_existing_path_entry(_runtime_path_entries "${_compiler_dir}")
    endif()
  endforeach()

  foreach(_tool_cache_name CMAKE_LINKER CMAKE_AR)
    iccdev_read_cache_value(_tool_path "${BUILD_DIR}" "${_tool_cache_name}")
    if(NOT "${_tool_path}" STREQUAL "" AND EXISTS "${_tool_path}")
      get_filename_component(_tool_dir "${_tool_path}" DIRECTORY)
      iccdev_add_existing_path_entry(_runtime_path_entries "${_tool_dir}")
    endif()
  endforeach()

  foreach(_compiler_path IN ITEMS "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}")
    if(NOT "${_compiler_path}" STREQUAL "" AND EXISTS "${_compiler_path}")
      get_filename_component(_compiler_dir "${_compiler_path}" DIRECTORY)
      iccdev_add_existing_path_entry(_runtime_path_entries "${_compiler_dir}")

      get_filename_component(_vc_tools_bin_host_dir "${_compiler_dir}" DIRECTORY)
      get_filename_component(_vc_tools_bin_dir "${_vc_tools_bin_host_dir}" DIRECTORY)
      get_filename_component(_vc_tools_msvc_version_dir "${_vc_tools_bin_dir}" DIRECTORY)
      get_filename_component(_vc_tools_msvc_dir "${_vc_tools_msvc_version_dir}" DIRECTORY)
      get_filename_component(_vc_tools_dir "${_vc_tools_msvc_dir}" DIRECTORY)
      get_filename_component(_vc_dir "${_vc_tools_dir}" DIRECTORY)
      set(_vc_redist_dir "${_vc_dir}/Redist/MSVC")
      if(EXISTS "${_vc_redist_dir}")
        file(GLOB _vc_debug_crt_dirs
          "${_vc_redist_dir}/*/debug_nonredist/x64/Microsoft.VC*.DebugCRT")
        foreach(_vc_debug_crt_dir IN LISTS _vc_debug_crt_dirs)
          iccdev_add_existing_path_entry(_runtime_path_entries "${_vc_debug_crt_dir}")
        endforeach()
      endif()
    endif()
  endforeach()

  # select_library_configurations() sets the singular <LIB>_LIBRARY_RELEASE and
  # <LIB>_LIBRARY_DEBUG alongside the combined <LIB>_LIBRARY, so reading the
  # configuration-matching spelling FIRST is what gives these prefixes the same
  # debug-first ordering the vcpkg tree gets. Without it a Debug consumer whose
  # cache carries no vcpkg variables -- the case the _VCPKG_INSTALLED_DIR
  # fallback above exists for -- would still bind the release DLL.
  set(_library_cache_names)
  foreach(_library_stem LIBXML2 PNG JPEG TIFF ZLIB)
    if(_runtime_config MATCHES "^[Dd][Ee][Bb][Uu][Gg]$")
      list(APPEND _library_cache_names
        "${_library_stem}_LIBRARY_DEBUG" "${_library_stem}_LIBRARY_RELEASE")
    else()
      list(APPEND _library_cache_names
        "${_library_stem}_LIBRARY_RELEASE" "${_library_stem}_LIBRARY_DEBUG")
    endif()
    # The combined value last: it is the fallback for a cache that carries no
    # per-configuration spelling at all, and it cannot express an order.
    list(APPEND _library_cache_names "${_library_stem}_LIBRARY")
  endforeach()

  foreach(_library_cache_name IN LISTS _library_cache_names)
    iccdev_read_cache_value(_library_value "${BUILD_DIR}" "${_library_cache_name}")
    # Under a multi-config generator these cache entries are the per-config list
    # "optimized;<release path>;debug;<debug path>", so an EXISTS test on the
    # whole string is false and the entry used to contribute nothing at all.
    # Iterating the halves covers both spellings; the "optimized"/"debug"/
    # "general" keywords are not paths and drop out of the EXISTS test below.
    foreach(_library_path IN LISTS _library_value)
      if(EXISTS "${_library_path}" AND NOT IS_DIRECTORY "${_library_path}")
        get_filename_component(_library_dir "${_library_path}" DIRECTORY)
        get_filename_component(_library_prefix "${_library_dir}/.." ABSOLUTE)
        iccdev_add_existing_path_entry(_runtime_path_entries "${_library_prefix}/bin")
      endif()
    endforeach()
  endforeach()

  iccdev_add_existing_path_entry(_runtime_path_entries "$ENV{SystemRoot}/System32")
  iccdev_add_existing_path_entry(_runtime_path_entries "$ENV{SystemRoot}")

  set(${OUT_VAR} "${_runtime_path_entries}" PARENT_SCOPE)
endfunction()
