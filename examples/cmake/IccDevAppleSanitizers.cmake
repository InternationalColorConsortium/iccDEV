# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

function(iccdev_enable_apple_app_sanitizers target_name)
  if(NOT APPLE)
    message(FATAL_ERROR "Apple sanitizer helper is only supported on Apple platforms")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Apple sanitizer helper requires a Clang compiler")
  endif()

  set(_iccdev_sanitizers
    address
    undefined
    integer
    float-divide-by-zero
    float-cast-overflow)
  string(REPLACE ";" "," _iccdev_sanitizer_list "${_iccdev_sanitizers}")
  set(_iccdev_sanitizer_flag "-fsanitize=${_iccdev_sanitizer_list}")

  target_compile_options(${target_name} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:${_iccdev_sanitizer_flag}>"
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-fno-omit-frame-pointer>")
  target_link_options(${target_name} PRIVATE
    "${_iccdev_sanitizer_flag}"
    "-fno-omit-frame-pointer")

  set(_iccdev_runtime_name "")
  if(CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
    set(_iccdev_runtime_name "libclang_rt.asan_iossim_dynamic.dylib")
  elseif(CMAKE_OSX_SYSROOT MATCHES "iphoneos")
    set(_iccdev_runtime_name "libclang_rt.asan_ios_dynamic.dylib")
  elseif(CMAKE_OSX_SYSROOT MATCHES "macosx")
    set(_iccdev_runtime_name "libclang_rt.asan_osx_dynamic.dylib")
  else()
    message(FATAL_ERROR
      "Unsupported Apple SDK for sanitizer runtime: ${CMAKE_OSX_SYSROOT}")
  endif()

  execute_process(
    COMMAND xcrun --sdk "${CMAKE_OSX_SYSROOT}" --find clang
    OUTPUT_VARIABLE _iccdev_clang
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT _iccdev_clang)
    message(FATAL_ERROR "Unable to locate clang with xcrun for ${CMAKE_OSX_SYSROOT}")
  endif()
  execute_process(
    COMMAND "${_iccdev_clang}" --print-resource-dir
    OUTPUT_VARIABLE _iccdev_resource_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  set(_iccdev_asan_runtime
    "${_iccdev_resource_dir}/lib/darwin/${_iccdev_runtime_name}")
  if(NOT EXISTS "${_iccdev_asan_runtime}")
    message(FATAL_ERROR
      "Unable to locate AddressSanitizer runtime: ${_iccdev_asan_runtime}")
  endif()

  set_source_files_properties("${_iccdev_asan_runtime}" PROPERTIES
    MACOSX_PACKAGE_LOCATION Frameworks
    XCODE_FILE_ATTRIBUTES CodeSignOnCopy)
  target_sources(${target_name} PRIVATE "${_iccdev_asan_runtime}")
  set_target_properties(${target_name} PROPERTIES
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS
      "$(inherited) @executable_path/Frameworks")
endfunction()
