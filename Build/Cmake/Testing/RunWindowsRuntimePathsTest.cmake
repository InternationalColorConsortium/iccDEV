#################################################################################
# Windows runtime dependency path collector regression
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################
# Regression coverage for Build/Cmake/Testing/WindowsRuntimePaths.cmake, the
# tree's shared collector of Windows runtime dependency directories.
#
# The two defects this pins are Windows-only in EFFECT -- an out-of-tree
# consumer dies at load with 0xc0000135 (STATUS_DLL_NOT_FOUND, which names no
# DLL) -- but their CAUSE is pure CMakeCache.txt parsing, so they reproduce on
# any platform against a synthetic cache. That is why this test is registered
# everywhere rather than behind if(WIN32): the collector had no coverage at all,
# and the Windows Debug leg is an expensive place to discover a parsing bug.
#
#   1. vcpkg splits DLLs by configuration -- release in <installed>/<triplet>/bin
#      and debug in <installed>/<triplet>/debug/bin. The collector added only the
#      release tree, so a Debug consumer found nothing.
#   2. Under a multi-config generator LIBXML2_LIBRARY / ZLIB_LIBRARY and friends
#      hold the list "optimized;<path>;debug;<path>". EXISTS on that whole string
#      is false, so those entries contributed nothing at all.

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/WindowsRuntimePaths.cmake")

set(_fixture_root "${ICCDEV_TEST_OUTDIR}/fixture")
file(REMOVE_RECURSE "${_fixture_root}")

# A stand-in vcpkg installed tree and a stand-in dependency prefix, both with
# populated release and debug halves. The directories must really exist: every
# entry the collector emits has passed an EXISTS test.
set(_vcpkg_root "${_fixture_root}/vcpkg")
set(_triplet x64-windows)
set(_dep_root "${_fixture_root}/dep")
foreach(_dir
    "${_vcpkg_root}/${_triplet}/bin"
    "${_vcpkg_root}/${_triplet}/debug/bin"
    "${_dep_root}/bin"
    "${_dep_root}/lib"
    "${_dep_root}/debug/bin"
    "${_dep_root}/debug/lib")
  file(MAKE_DIRECTORY "${_dir}")
endforeach()
file(TOUCH "${_dep_root}/lib/zlib.lib")
file(TOUCH "${_dep_root}/debug/lib/zlibd.lib")

# The collector emits every entry through file(TO_CMAKE_PATH), so normalise the
# expectations the same way. On Windows ICCDEV_TEST_OUTDIR can still reach this
# script with backslashes, and a raw string compare would then miss on spelling
# alone -- a false red that says nothing about the collector.
foreach(_expect_pair
    "_expect_vcpkg_release;${_vcpkg_root}/${_triplet}/bin"
    "_expect_vcpkg_debug;${_vcpkg_root}/${_triplet}/debug/bin"
    "_expect_dep_release;${_dep_root}/bin"
    "_expect_dep_debug;${_dep_root}/debug/bin")
  list(GET _expect_pair 0 _expect_name)
  list(GET _expect_pair 1 _expect_raw)
  file(TO_CMAKE_PATH "${_expect_raw}" ${_expect_name})
endforeach()

# Writes a synthetic CMakeCache.txt and returns the directory holding it. BODY
# is one quoted string whose lines are separated by "|", NOT a varargs list: the
# whole point of these fixtures is cache values that themselves contain ";", and
# ARGN would silently split those across lines.
function(iccdev_write_fixture_cache OUT_DIR NAME BODY)
  set(_dir "${_fixture_root}/${NAME}")
  file(MAKE_DIRECTORY "${_dir}")
  string(REPLACE "|" "\n" _body "${BODY}")
  file(WRITE "${_dir}/CMakeCache.txt" "${_body}\n")
  set(${OUT_DIR} "${_dir}" PARENT_SCOPE)
endfunction()

# Calls the collector with ICCDEV_CONFIG shadowed to CONFIG for this scope only,
# which is exactly how the CTest wrappers reach it.
function(iccdev_probe_entries OUT_VAR CACHE_DIR CONFIG)
  set(ICCDEV_CONFIG "${CONFIG}")
  iccdev_collect_cache_runtime_path_entries(_entries "${CACHE_DIR}")
  set(${OUT_VAR} "${_entries}" PARENT_SCOPE)
endfunction()

set(_failures)

# The collected list is reported on every failure: a runtime PATH defect leaves
# no other evidence behind, so guessing from a bare "expected X" is how #2236
# spent three blind CI cycles.
function(iccdev_expect_entry ARM ENTRIES WANTED)
  list(FIND ENTRIES "${WANTED}" _index)
  if(_index LESS 0)
    string(REPLACE ";" "\n    " _pretty "${ENTRIES}")
    set(_failures_local "${_failures}")
    list(APPEND _failures_local "${ARM}: missing ${WANTED}\n  collected:\n    ${_pretty}")
    set(_failures "${_failures_local}" PARENT_SCOPE)
  endif()
endfunction()

function(iccdev_expect_order ARM ENTRIES FIRST SECOND)
  list(FIND ENTRIES "${FIRST}" _first_index)
  list(FIND ENTRIES "${SECOND}" _second_index)
  if(_first_index LESS 0 OR _second_index LESS 0 OR NOT _first_index LESS _second_index)
    string(REPLACE ";" "\n    " _pretty "${ENTRIES}")
    set(_failures_local "${_failures}")
    list(APPEND _failures_local
      "${ARM}: expected ${FIRST} ahead of ${SECOND}\n  collected:\n    ${_pretty}")
    set(_failures "${_failures_local}" PARENT_SCOPE)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Arm 1: multi-config generator. No CMAKE_BUILD_TYPE in the cache and no
# ICCDEV_CONFIG, and the library cache entry is the "optimized;...;debug;..."
# list that used to defeat EXISTS outright.
# ---------------------------------------------------------------------------
iccdev_write_fixture_cache(_multi_config_dir multi-config
  "VCPKG_INSTALLED_DIR:PATH=${_vcpkg_root}|VCPKG_TARGET_TRIPLET:STRING=${_triplet}|ZLIB_LIBRARY:STRING=optimized;${_dep_root}/lib/zlib.lib;debug;${_dep_root}/debug/lib/zlibd.lib")

iccdev_probe_entries(_multi_config_entries "${_multi_config_dir}" "")
foreach(_wanted "${_expect_vcpkg_release}" "${_expect_vcpkg_debug}"
                "${_expect_dep_release}" "${_expect_dep_debug}")
  iccdev_expect_entry("multi-config" "${_multi_config_entries}" "${_wanted}")
endforeach()
# With no configuration to go on, the release tree keeps the position it had
# before the collector knew about debug trees.
iccdev_expect_order("multi-config" "${_multi_config_entries}"
  "${_expect_vcpkg_release}" "${_expect_vcpkg_debug}")

# ---------------------------------------------------------------------------
# Arm 2: the Windows Debug leg. ICCDEV_CONFIG must put the debug tree ahead of
# the release one, because the loader takes the first match on PATH.
# ---------------------------------------------------------------------------
iccdev_probe_entries(_debug_entries "${_multi_config_dir}" "Debug")
foreach(_wanted "${_expect_vcpkg_release}" "${_expect_vcpkg_debug}"
                "${_expect_dep_release}" "${_expect_dep_debug}")
  iccdev_expect_entry("config-debug" "${_debug_entries}" "${_wanted}")
endforeach()
iccdev_expect_order("config-debug" "${_debug_entries}"
  "${_expect_vcpkg_debug}" "${_expect_vcpkg_release}")

# ---------------------------------------------------------------------------
# Arm 3: single-config build tree with no ICCDEV_CONFIG passed. The cache's own
# CMAKE_BUILD_TYPE is the fallback and must produce the same debug ordering.
# ---------------------------------------------------------------------------
iccdev_write_fixture_cache(_cache_debug_dir cache-debug
  "CMAKE_BUILD_TYPE:STRING=Debug|VCPKG_INSTALLED_DIR:PATH=${_vcpkg_root}|VCPKG_TARGET_TRIPLET:STRING=${_triplet}")

iccdev_probe_entries(_cache_debug_entries "${_cache_debug_dir}" "")
iccdev_expect_order("cache-debug" "${_cache_debug_entries}"
  "${_expect_vcpkg_debug}" "${_expect_vcpkg_release}")

# ---------------------------------------------------------------------------
# Arm 4: some vcpkg toolchain versions publish only the internal spelling
# _VCPKG_INSTALLED_DIR. Reading just VCPKG_INSTALLED_DIR finds no tree at all.
# ---------------------------------------------------------------------------
iccdev_write_fixture_cache(_vcpkg_internal_dir vcpkg-internal
  "_VCPKG_INSTALLED_DIR:PATH=${_vcpkg_root}|VCPKG_TARGET_TRIPLET:STRING=${_triplet}")

iccdev_probe_entries(_vcpkg_internal_entries "${_vcpkg_internal_dir}" "Debug")
iccdev_expect_entry("vcpkg-internal-spelling" "${_vcpkg_internal_entries}"
  "${_expect_vcpkg_debug}")

# ---------------------------------------------------------------------------
# Arm 5: CMAKE_PREFIX_PATH names the same triplet directory the vcpkg variables
# do. The prefix loop runs first and iccdev_add_existing_path_entry keeps an
# entry's FIRST position, so ordering the vcpkg block alone was not enough --
# the release tree got pinned ahead of the debug one regardless.
# ---------------------------------------------------------------------------
iccdev_write_fixture_cache(_prefix_path_dir prefix-path
  "CMAKE_PREFIX_PATH:STRING=${_vcpkg_root}/${_triplet}|VCPKG_INSTALLED_DIR:PATH=${_vcpkg_root}|VCPKG_TARGET_TRIPLET:STRING=${_triplet}")

iccdev_probe_entries(_prefix_path_entries "${_prefix_path_dir}" "Debug")
iccdev_expect_order("prefix-path-debug" "${_prefix_path_entries}"
  "${_expect_vcpkg_debug}" "${_expect_vcpkg_release}")

# ---------------------------------------------------------------------------
# Arm 6: no vcpkg variables at all, only the per-configuration library cache
# spellings select_library_configurations() writes. This is the shape the
# _VCPKG_INSTALLED_DIR fallback exists to rescue, and the debug prefix must
# still come first -- both trees spell the DLL identically, so second place
# means the release DLL binds into a Debug consumer.
# ---------------------------------------------------------------------------
iccdev_write_fixture_cache(_library_only_dir library-only
  "ZLIB_LIBRARY_RELEASE:FILEPATH=${_dep_root}/lib/zlib.lib|ZLIB_LIBRARY_DEBUG:FILEPATH=${_dep_root}/debug/lib/zlibd.lib")

iccdev_probe_entries(_library_only_debug "${_library_only_dir}" "Debug")
iccdev_expect_order("library-only-debug" "${_library_only_debug}"
  "${_expect_dep_debug}" "${_expect_dep_release}")

# The release build must keep the mirror-image order, so the arm above cannot
# pass by the collector simply always emitting debug first.
iccdev_probe_entries(_library_only_release "${_library_only_dir}" "Release")
iccdev_expect_order("library-only-release" "${_library_only_release}"
  "${_expect_dep_release}" "${_expect_dep_debug}")

# ---------------------------------------------------------------------------
# Anti-vacuity: a cache naming trees that do not exist must yield none of them.
# Without this, a collector that appended candidates unconditionally would pass
# every assertion above while emitting directories the loader cannot use.
# ---------------------------------------------------------------------------
iccdev_write_fixture_cache(_absent_dir absent
  "VCPKG_INSTALLED_DIR:PATH=${_fixture_root}/no-such-vcpkg|VCPKG_TARGET_TRIPLET:STRING=${_triplet}")

iccdev_probe_entries(_absent_entries "${_absent_dir}" "Debug")
foreach(_entry IN LISTS _absent_entries)
  if(_entry MATCHES "no-such-vcpkg")
    list(APPEND _failures "absent-tree: collector emitted nonexistent ${_entry}")
  endif()
endforeach()

if(_failures)
  set(_report "")
  foreach(_failure IN LISTS _failures)
    set(_report "${_report}\n- ${_failure}")
  endforeach()
  message(FATAL_ERROR "${ICCDEV_TEST_NAME} failed:${_report}")
endif()

message(STATUS "${ICCDEV_TEST_NAME}: collector covers both vcpkg configuration trees "
               "and per-configuration library cache lists")
