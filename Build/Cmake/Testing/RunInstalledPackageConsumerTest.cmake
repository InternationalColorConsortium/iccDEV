#################################################################################
# Installed-package consumer regression for the iccDEV find_package() surface
# Copyright (c) 2026 The International Color Consortium.
#                                        All rights reserved.
#################################################################################
#
# #2154 item 3 -- "does the installed surface CONSUME?"
#
# Before this test nothing in CI ran `cmake --install` at all, and no workflow
# or CTest built examples/hello-iccdev. The only find_package(RefIccMAX) in CI
# was ci-vcpkg-ports.yml, CONFIG mode against a port that forces
# -DENABLE_SHARED_LIBS=OFF. So every consumer path that reaches a SHARED
# install -- CONFIG mode, MODULE mode via Build/Cmake/Modules/FindRefIccMAX.cmake,
# and the example itself -- was uncovered, which is how the two Find-module
# defects fixed alongside this test survived: neither is visible from inside the
# build tree, only to a consumer of the installed prefix.
#
# The test stages a real install into a temporary prefix and then builds and
# RUNS consumers against it -- four arms over three separate consumer projects. Building out of tree at test time is
# the only shape that works here for the same reason it was in
# RunWindowsExportedDataLinkageTest.cmake: a consumer that fails to configure or
# link inside the main tree takes the BUILD down, so it could never be expressed
# as a failing CTest.
#
# Arms:
#   1. CONFIG mode  -- find_package(RefIccMAX CONFIG) against the staged prefix.
#   2. MODULE mode  -- find_package(RefIccMAX MODULE) against a SECOND prefix
#                      staged without the CMake package files, so
#                      FindRefIccMAX.cmake's manual-discovery phase is the code
#                      under test. Pointing MODULE mode at a prefix that has a
#                      working RefIccMAXConfig.cmake is vacuous: the module
#                      returns at its CONFIG phase and never runs the manual
#                      search that builds the imported targets by hand.
#   2b. MODULE mode again, with a target-less RefIccMAXConfig.cmake planted
#                      earlier on CMAKE_PREFIX_PATH -- the shape a pre-2.3.2
#                      install has. Planted rather than assumed so the arm is
#                      red on a clean runner too.
#   3. examples/hello-iccdev -- the documented example, built as shipped.
#
# Each arm pins the library it resolved back to this test's own staged prefix.
# Without that pin a machine with iccDEV installed system-wide would quietly
# test the system copy and pass no matter what this build tree contains.

# The tree's own collector for Windows runtime dependency directories. It reads
# the parent CMakeCache.txt for CMAKE_PREFIX_PATH, both of vcpkg's
# per-configuration installed trees, the compiler/CRT directories and the
# prefixes behind LIBXML2_LIBRARY / ZLIB_LIBRARY and friends. Reusing it rather
# than hand-rolling a PATH is what three sibling Windows tests already do -- and
# hand-rolling is precisely how this test first failed on Windows with
# 0xc0000135 (STATUS_DLL_NOT_FOUND): the
# staged prefix carries the iccDEV DLLs but nothing tells the loader where
# libxml2/iconv/zlib live, which under a vcpkg debug build is
# <installed>/<triplet>/debug/bin.
include("${CMAKE_CURRENT_LIST_DIR}/WindowsRuntimePaths.cmake")

set(_required_vars
  ICCDEV_TEST_NAME
  ICCDEV_TEST_OUTDIR
  ICCDEV_REPO_ROOT
  ICCDEV_BUILD_DIR
  ICCDEV_CONFIG
  ICCDEV_GENERATOR
  ICCDEV_LIB_SUBDIRS
)

foreach(_required_var IN LISTS _required_vars)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "${_required_var} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${ICCDEV_TEST_OUTDIR}")
file(MAKE_DIRECTORY "${ICCDEV_TEST_OUTDIR}")
set(_log_file "${ICCDEV_TEST_OUTDIR}/output.log")
set(_log "")

# A function, NOT a macro. Macro arguments are substituted textually and the
# result is re-parsed, so feeding raw compiler/linker output through one both
# expands any ${...} it contains and trips "Invalid escape sequence \U" on the
# backslash paths every Windows tool prints -- which silently discards the
# accumulated log this test's only diagnostic depends on.
function(_note _text)
  set(_log "${_log}${_text}\n" PARENT_SCOPE)
endfunction()

# Same %TEMP% relocation and path-digest rationale as
# RunWindowsExportedDataLinkageTest.cmake: the staged prefix plus a nested
# configure pushes several levels deeper than the build tree and Windows still
# enforces 260 characters, and the digest keeps two build trees at one
# configuration from clearing each other's work.
set(_work_root "${ICCDEV_TEST_OUTDIR}/work")
if(DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
  string(SHA256 _work_hash "${ICCDEV_BUILD_DIR}|${ICCDEV_TEST_NAME}|${ICCDEV_CONFIG}")
  string(SUBSTRING "${_work_hash}" 0 12 _work_hash)
  set(_work_root "$ENV{TEMP}/iccdev-inst-${_work_hash}")
endif()
file(REMOVE_RECURSE "${_work_root}")
file(MAKE_DIRECTORY "${_work_root}")
# Canonicalise once, after the directory exists. On a Windows runner $ENV{TEMP}
# is the 8.3 short form (C:\Users\RUNNER~1\...) while CMake reports discovered
# paths in the long form (C:/Users/runneradmin/...). Both name the same
# directory, so leaving the two spellings in play makes every later path
# comparison a coin flip.
get_filename_component(_work_root "${_work_root}" REALPATH)

set(_prefix_config "${_work_root}/prefix-config")
set(_prefix_module "${_work_root}/prefix-module")

# ---------------------------------------------------------------------------
# Stage the install
# ---------------------------------------------------------------------------
#
# Deliberately NOT a whole-tree `cmake --install`. That runs every install rule
# in the tree, including the command-line tools, so it fails outright unless
# every tool target happens to have been built -- which would make this test
# report a missing tool as a broken package. The two steps below install exactly
# what a library consumer resolves:
#
#   * component "dev"      -- public headers (including the generated
#                             IccProfLibVer.h), the static archives, the Windows
#                             import libraries, and the CMake package files that
#                             install(EXPORT) generates.
#   * component "runtime", -- the shared libraries. Run per library directory
#     per directory          rather than tree-wide because the tools default to
#                            this component too.
function(_stage_prefix _prefix _with_package_files)
  set(_stage_log "")

  # --config is required, not decorative: without it a multi-config generator
  # installs whatever configuration was baked in at generate time, and
  # Build/Cmake/CMakeLists.txt force-sets CMAKE_BUILD_TYPE to Release whenever
  # it is unset -- which every Visual Studio and Xcode preset leaves unset. A
  # Debug ctest run would then stage Release artefacts and fail on a missing
  # .lib for reasons having nothing to do with package consumability.
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${ICCDEV_BUILD_DIR}"
            --prefix "${_prefix}" --component dev --config "${ICCDEV_CONFIG}"
    RESULT_VARIABLE _dev_result
    OUTPUT_VARIABLE _dev_stdout
    ERROR_VARIABLE _dev_stderr
  )
  set(_stage_log "${_stage_log}----- install dev stdout -----\n${_dev_stdout}\n")
  set(_stage_log "${_stage_log}----- install dev stderr -----\n${_dev_stderr}\n")

  if(NOT _dev_result EQUAL 0)
    set(_stage_error
      "Staging the dev component into ${_prefix} failed with ${_dev_result}. This is \
the install(FILES)/install(TARGETS ... ARCHIVE)/install(EXPORT) surface; a \
missing static archive here means the library targets were not all built."
      PARENT_SCOPE)
    set(_stage_log "${_stage_log}" PARENT_SCOPE)
    return()
  endif()

  foreach(_subdir IN LISTS ICCDEV_LIB_SUBDIRS)
    set(_subdir_script "${ICCDEV_BUILD_DIR}/${_subdir}/cmake_install.cmake")
    if(NOT EXISTS "${_subdir_script}")
      set(_stage_error "Install script not found: ${_subdir_script}" PARENT_SCOPE)
      set(_stage_log "${_stage_log}" PARENT_SCOPE)
      return()
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
              "-DCMAKE_INSTALL_PREFIX=${_prefix}"
              "-DCMAKE_INSTALL_COMPONENT=runtime"
              "-DCMAKE_INSTALL_CONFIG_NAME=${ICCDEV_CONFIG}"
              -P "${_subdir_script}"
      RESULT_VARIABLE _rt_result
      OUTPUT_VARIABLE _rt_stdout
      ERROR_VARIABLE _rt_stderr
    )
    set(_stage_log "${_stage_log}----- install runtime ${_subdir} -----\n${_rt_stdout}${_rt_stderr}\n")
    if(NOT _rt_result EQUAL 0)
      set(_stage_error
        "Staging the runtime component of ${_subdir} into ${_prefix} failed with ${_rt_result}."
        PARENT_SCOPE)
      set(_stage_log "${_stage_log}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  # The MODULE-mode prefix must not carry package files; see the header comment.
  #
  # Found by searching for the files themselves rather than by globbing a
  # hardcoded lib/ lib64/ share/ list. The package lands in
  # ${CMAKE_INSTALL_LIBDIR}/cmake/reficcmax, and CMAKE_INSTALL_LIBDIR comes from
  # GNUInstallDirs and is a settable cache variable besides -- anything other
  # than plain lib or lib64 would leave the config in place, FindRefIccMAX.cmake
  # would return at its CONFIG phase, and arm 2 would silently stop testing the
  # manual-discovery path it exists for while still passing.
  if(NOT _with_package_files)
    file(GLOB_RECURSE _pkg_files "${_prefix}/RefIccMAXConfig.cmake")
    foreach(_pkg_file IN LISTS _pkg_files)
      get_filename_component(_pkg_dir "${_pkg_file}" DIRECTORY)
      file(REMOVE_RECURSE "${_pkg_dir}")
    endforeach()
    # Assert the scrub worked rather than trusting it; a surviving config makes
    # the whole arm vacuous.
    file(GLOB_RECURSE _pkg_left "${_prefix}/RefIccMAXConfig.cmake")
    if(_pkg_left)
      set(_stage_error
        "Could not strip the CMake package files from ${_prefix}; ${_pkg_left} survived, which would make the MODULE-mode arm vacuous."
        PARENT_SCOPE)
      set(_stage_log "${_stage_log}" PARENT_SCOPE)
      return()
    endif()
  endif()

  set(_stage_error "" PARENT_SCOPE)
  set(_stage_log "${_stage_log}" PARENT_SCOPE)
endfunction()

_stage_prefix("${_prefix_config}" TRUE)
_note("${_stage_log}")
set(_stage_failure "${_stage_error}")

if(NOT _stage_failure)
  _stage_prefix("${_prefix_module}" FALSE)
  _note("${_stage_log}")
  set(_stage_failure "${_stage_error}")
endif()

function(_fail _message)
  file(WRITE "${_log_file}" "CTest test: ${ICCDEV_TEST_NAME}\n"
       "Configuration: ${ICCDEV_CONFIG}\n"
       "Staged prefix (CONFIG): ${_prefix_config}\n"
       "Staged prefix (MODULE): ${_prefix_module}\n\n${_log}")
  # Inline the tail of the log into the CTest output as well as writing the
  # file. On a CI runner the file is unreachable -- the log path in a failure
  # message is only useful to someone sitting on the machine -- so without this
  # a red leg says which arm failed and nothing about why. The tail is where the
  # failing arm's own stdout/stderr has just been appended; bounded so a long
  # compiler transcript cannot bury the message it is attached to.
  set(_tail "${_log}")
  string(LENGTH "${_tail}" _tail_len)
  if(_tail_len GREATER 4000)
    math(EXPR _tail_start "${_tail_len} - 4000")
    string(SUBSTRING "${_tail}" ${_tail_start} -1 _tail)
    set(_tail "...(truncated; full log in the file below)...\n${_tail}")
  endif()
  message(FATAL_ERROR
    "${_message}\n\n----- tail of ${ICCDEV_TEST_NAME} log -----\n${_tail}\n"
    "----- end of tail -----\nFull log: ${_log_file}")
endfunction()

if(_stage_failure)
  _fail("${_stage_failure}")
endif()

# ---------------------------------------------------------------------------
# The installed surface itself
# ---------------------------------------------------------------------------
#
# Checked before any consumer runs so that a missing header is reported as a
# missing header rather than as a compile error three layers down.
set(_incdir "${_prefix_config}/include/RefIccMAX/IccProfLib2")
set(_json_incdir "${_prefix_config}/include/RefIccMAX/IccJSON2")
set(_required_files
  "${_incdir}/IccProfile.h"
  "${_incdir}/IccUtil.h"
  "${_incdir}/IccJsonTypes.h"
  "${_incdir}/IccProfLibConf.h"
  "${_incdir}/IccFileUtil.h"
  "${_incdir}/IccCmdLineUtil.h"
  # Generated at build time from the version + git hash (#823), so its absence
  # means the generated-header install rule regressed, not the static list.
  "${_incdir}/IccProfLibVer.h"
  "${_json_incdir}/IccJsonTypes.h"
)
foreach(_required_file IN LISTS _required_files)
  if(NOT EXISTS "${_required_file}")
    _fail("Installed package is missing ${_required_file}")
  endif()
endforeach()

file(GLOB_RECURSE _config_files "${_prefix_config}/*RefIccMAXConfig.cmake")
file(GLOB_RECURSE _targets_files "${_prefix_config}/*RefIccMAXTargets.cmake")
if(NOT _config_files)
  _fail("Installed package has no RefIccMAXConfig.cmake under ${_prefix_config}")
endif()
if(NOT _targets_files)
  _fail("Installed package has no RefIccMAXTargets.cmake under ${_prefix_config} -- install(EXPORT RefIccMAXTargets) did not run")
endif()
list(GET _config_files 0 _config_file)
get_filename_component(_package_dir "${_config_file}" DIRECTORY)
_note("Resolved package dir: ${_package_dir}")

# ---------------------------------------------------------------------------
# Consumer arms
# ---------------------------------------------------------------------------
set(_common_args
  -G "${ICCDEV_GENERATOR}"
  -Wno-dev
)
if(DEFINED ICCDEV_GENERATOR_PLATFORM AND NOT "${ICCDEV_GENERATOR_PLATFORM}" STREQUAL "")
  list(APPEND _common_args -A "${ICCDEV_GENERATOR_PLATFORM}")
endif()
# Matters on the ClangCL leg: the parent's CMAKE_CXX_COMPILER is clang-cl, so
# leaving the nested configure on the default v143 toolset would build the
# consumer with a different compiler than the libraries it links.
if(DEFINED ICCDEV_GENERATOR_TOOLSET AND NOT "${ICCDEV_GENERATOR_TOOLSET}" STREQUAL "")
  list(APPEND _common_args -T "${ICCDEV_GENERATOR_TOOLSET}")
endif()
if(DEFINED ICCDEV_CXX_COMPILER AND NOT "${ICCDEV_CXX_COMPILER}" STREQUAL "")
  list(APPEND _common_args "-DCMAKE_CXX_COMPILER=${ICCDEV_CXX_COMPILER}")
endif()
# Forwarded so the nested configure resolves LibXml2/nlohmann_json the same way
# the parent did. On the Windows legs those come from vcpkg, and without the
# toolchain the consumer's find_dependency(LibXml2) fails for a reason that has
# nothing to do with the iccDEV package under test.
if(DEFINED ICCDEV_TOOLCHAIN_FILE AND NOT "${ICCDEV_TOOLCHAIN_FILE}" STREQUAL "")
  list(APPEND _common_args "-DCMAKE_TOOLCHAIN_FILE=${ICCDEV_TOOLCHAIN_FILE}")
  # examples/hello-iccdev ships a vcpkg.json. Under a vcpkg toolchain that puts
  # the nested configure into manifest mode, which would try to resolve and
  # build that manifest -- a network fetch inside a CTest. The consumer needs
  # the dependencies the PARENT build already resolved, so classic mode against
  # the installed tree is both faster and what is actually under test.
  list(APPEND _common_args "-DVCPKG_MANIFEST_MODE=OFF")
endif()
if(DEFINED ICCDEV_VCPKG_TRIPLET AND NOT "${ICCDEV_VCPKG_TRIPLET}" STREQUAL "")
  list(APPEND _common_args "-DVCPKG_TARGET_TRIPLET=${ICCDEV_VCPKG_TRIPLET}")
endif()
if(DEFINED ICCDEV_MSVC_RUNTIME_LIBRARY AND NOT "${ICCDEV_MSVC_RUNTIME_LIBRARY}" STREQUAL "")
  list(APPEND _common_args "-DCMAKE_MSVC_RUNTIME_LIBRARY=${ICCDEV_MSVC_RUNTIME_LIBRARY}")
endif()
if(NOT ICCDEV_GENERATOR MATCHES "Visual Studio|Xcode|Multi-Config")
  list(APPEND _common_args "-DCMAKE_BUILD_TYPE=${ICCDEV_CONFIG}")
endif()

# The consumer source is shared by all three CMake-side arms. It touches one
# ICCPROFLIB_DATA_API global and one plain exported function, includes the
# relocated IccCmdLineUtil.h, and drives IccXML so the libxml2 include path has
# to have propagated -- that last one is what the MODULE-mode arm regression
# tests, since a raw library path carries no include directories.
set(_consumer_source "\
#include \"IccProfile.h\"\n\
#include \"IccUtil.h\"\n\
#include \"IccCmdLineUtil.h\"\n\
#include \"IccProfileXml.h\"\n\
#ifdef ICCDEV_INSTALLED_JSON_CONNECT\n\
#include \"IccJsonTypes.h\"\n\
#include \"IccProfileJson.h\"\n\
#include \"IccConnect.h\"\n\
#endif\n\
\n\
#include <cmath>\n\
#include <cstdio>\n\
#include <cstring>\n\
#include <string>\n\
\n\
int main()\n\
{\n\
  int failures = 0;\n\
\n\
  // An ICCPROFLIB_DATA_API global: exported data, which is what needs the\n\
  // dllimport annotation the package has to carry INTERFACE on Windows.\n\
  if (std::fabs(icD50XYZ[1] - 1.0) > 1e-6) {\n\
    std::fprintf(stderr, \"FAIL icD50XYZ[1]=%f\\n\", icD50XYZ[1]);\n\
    ++failures;\n\
  }\n\
  if (!icMsgValidateWarning || std::strcmp(icMsgValidateWarning, \"Warning! - \") != 0) {\n\
    std::fprintf(stderr, \"FAIL icMsgValidateWarning\\n\");\n\
    ++failures;\n\
  }\n\
\n\
  // A plain exported function, to tell a data-export failure apart from the\n\
  // package being unusable altogether.\n\
  CIccInfo info;\n\
  if (!info.GetColorSpaceSigName(icSigRgbData)) {\n\
    std::fprintf(stderr, \"FAIL GetColorSpaceSigName\\n\");\n\
    ++failures;\n\
  }\n\
\n\
  // Reaches IccXML, whose installed header includes <libxml/parser.h>. The\n\
  // assertion is that the CALL SUCCEEDS, deliberately not that it produces\n\
  // bytes: examples/hello-iccdev treats empty output from a bare header as an\n\
  // expected outcome, so requiring content here would assert something the\n\
  // tree itself documents as not guaranteed. What this arm needs to establish\n\
  // is that IccXML2 resolved, loaded and ran from the installed package.\n\
  CIccProfileXml profile;\n\
  profile.InitHeader();\n\
  profile.m_Header.colorSpace = icSigRgbData;\n\
  profile.m_Header.pcs = icSigLabData;\n\
  profile.m_Header.deviceClass = icSigDisplayClass;\n\
  std::string xml;\n\
  if (!profile.ToXml(xml)) {\n\
    std::fprintf(stderr, \"FAIL ToXml returned false\\n\");\n\
    ++failures;\n\
  }\n\
\n\
\n\
#ifdef ICCDEV_INSTALLED_JSON_CONNECT\n\
  IccJson json = IccJson::object();\n\
  if (!json.is_object()) {\n\
    std::fprintf(stderr, \"FAIL IccJsonTypes\\n\");\n\
    ++failures;\n\
  }\n\
  auto create_standard = &CIccConnectCmm::CreateStandard;\n\
  if (!create_standard) {\n\
    std::fprintf(stderr, \"FAIL CIccConnectCmm::CreateStandard\\n\");\n\
    ++failures;\n\
  }\n\
\n\
#endif\n\
\n\
  if (failures) {\n\
    std::fprintf(stderr, \"installed-consumer: %d failure(s)\\n\", failures);\n\
    return 1;\n\
  }\n\
  std::printf(\"installed-consumer: OK xml=%zu\\n\", xml.size());\n\
  return 0;\n\
}\n")

# Runs one consumer: configure, build, then execute with the staged shared
# libraries reachable.
function(_run_consumer _label _src_dir _build_dir _prefix _extra_args _out_ok)
  set(_arm_log "===== arm: ${_label} =====\n")
  set(_arm_stdout "" PARENT_SCOPE)
  set(_arm_run_stdout "" PARENT_SCOPE)

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src_dir}" -B "${_build_dir}"
            ${_common_args} ${_extra_args}
    RESULT_VARIABLE _cfg_result
    OUTPUT_VARIABLE _cfg_stdout
    ERROR_VARIABLE _cfg_stderr
  )
  set(_arm_log "${_arm_log}----- configure stdout -----\n${_cfg_stdout}\n")
  set(_arm_log "${_arm_log}----- configure stderr -----\n${_cfg_stderr}\n")
  if(NOT _cfg_result EQUAL 0)
    set(${_out_ok} "configure failed with ${_cfg_result}" PARENT_SCOPE)
    set(_arm_log "${_arm_log}" PARENT_SCOPE)
    set(_arm_stdout "${_cfg_stdout}" PARENT_SCOPE)
    return()
  endif()

  execute_process(
    # No --parallel: each consumer is one or two translation units, so an
    # unbounded job count buys nothing and only competes with the other tests
    # CTest may be running alongside this one.
    COMMAND "${CMAKE_COMMAND}" --build "${_build_dir}" --config "${ICCDEV_CONFIG}"
    RESULT_VARIABLE _bld_result
    OUTPUT_VARIABLE _bld_stdout
    ERROR_VARIABLE _bld_stderr
  )
  set(_arm_log "${_arm_log}----- build stdout -----\n${_bld_stdout}\n")
  set(_arm_log "${_arm_log}----- build stderr -----\n${_bld_stderr}\n")
  if(NOT _bld_result EQUAL 0)
    set(${_out_ok} "build failed with ${_bld_result}" PARENT_SCOPE)
    set(_arm_log "${_arm_log}" PARENT_SCOPE)
    set(_arm_stdout "${_cfg_stdout}" PARENT_SCOPE)
    return()
  endif()

  # Locate the executable across single- and multi-config generators.
  set(_exe "")
  file(GLOB_RECURSE _candidates
    "${_build_dir}/iccdev-installed-consumer"
    "${_build_dir}/iccdev-installed-consumer.exe"
    "${_build_dir}/hello-iccdev"
    "${_build_dir}/hello-iccdev.exe")
  foreach(_candidate IN LISTS _candidates)
    if(NOT IS_DIRECTORY "${_candidate}")
      set(_exe "${_candidate}")
      break()
    endif()
  endforeach()
  if(NOT _exe)
    set(${_out_ok} "consumer executable not found under ${_build_dir}" PARENT_SCOPE)
    set(_arm_log "${_arm_log}" PARENT_SCOPE)
    set(_arm_stdout "${_cfg_stdout}" PARENT_SCOPE)
    return()
  endif()
  set(_arm_log "${_arm_log}executable: ${_exe}\n")

  # The staged prefix is not on any default loader path, so point the loader at
  # it explicitly: bin/ for Windows DLLs, lib/ elsewhere.
  if(WIN32)
    # Staged prefix first (the package under test), then everything the parent
    # build resolved its own dependencies from.
    set(_win_path "${_prefix}/bin;${_prefix}/lib")
    # The collector reads ICCDEV_CONFIG from this script's scope, so the vcpkg
    # debug tree that an out-of-tree consumer needs on the Debug leg is ordered
    # ahead of the release one. This test used to carry its own copy of that
    # logic; the shared helper owns it now.
    iccdev_collect_cache_runtime_path_entries(_runtime_entries "${ICCDEV_BUILD_DIR}")

    foreach(_entry IN LISTS _runtime_entries)
      set(_win_path "${_win_path};${_entry}")
    endforeach()
    set(_env_args "PATH=${_win_path};$ENV{PATH}")
    # Logged because a load failure names no DLL: 0xc0000135 tells you something
    # was missing and nothing about what, so the search path and what the
    # package actually staged are the only evidence a CI run can leave behind.
    set(_arm_log "${_arm_log}----- runtime PATH -----\n${_win_path}\n")
    file(GLOB _staged_bin "${_prefix}/bin/*" "${_prefix}/lib/*.dll")
    set(_arm_log "${_arm_log}----- staged binaries -----\n")
    foreach(_staged IN LISTS _staged_bin)
      set(_arm_log "${_arm_log}  ${_staged}\n")
    endforeach()
  elseif(APPLE)
    set(_env_args "DYLD_LIBRARY_PATH=${_prefix}/lib:$ENV{DYLD_LIBRARY_PATH}")
  else()
    set(_env_args "LD_LIBRARY_PATH=${_prefix}/lib:${_prefix}/lib64:$ENV{LD_LIBRARY_PATH}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "${_env_args}" "${_exe}"
    RESULT_VARIABLE _run_result
    OUTPUT_VARIABLE _run_stdout
    ERROR_VARIABLE _run_stderr
  )
  set(_arm_log "${_arm_log}----- run stdout -----\n${_run_stdout}\n")
  set(_arm_log "${_arm_log}----- run stderr -----\n${_run_stderr}\n")
  if(NOT _run_result EQUAL 0)
    set(${_out_ok} "consumer exited with ${_run_result}" PARENT_SCOPE)
    set(_arm_log "${_arm_log}" PARENT_SCOPE)
    set(_arm_stdout "${_cfg_stdout}" PARENT_SCOPE)
    return()
  endif()

  set(${_out_ok} "" PARENT_SCOPE)
  set(_arm_log "${_arm_log}" PARENT_SCOPE)
  set(_arm_stdout "${_cfg_stdout}" PARENT_SCOPE)
  set(_arm_run_stdout "${_run_stdout}" PARENT_SCOPE)
endfunction()

# --- Anti-vacuity: both arms must have resolved THIS test's staged prefix -----
#
# Not packed into a list and looped: configure output routinely contains
# semicolons, and list(GET) over that would silently read the wrong field.
function(_assert_resolved_under _arm_name _arm_out _arm_prefix)
  if(NOT _arm_out MATCHES "ICCDEV_RESOLVED_LIB=([^\n\r]+)")
    _fail("${_arm_name}-mode arm did not report the library it resolved")
  endif()
  set(_resolved "${CMAKE_MATCH_1}")
  string(STRIP "${_resolved}" _resolved)
  file(TO_CMAKE_PATH "${_resolved}" _resolved)
  file(TO_CMAKE_PATH "${_arm_prefix}" _expected_prefix)
  # Compare canonical forms, not the spellings each side happened to print.
  get_filename_component(_resolved_real "${_resolved}" REALPATH)
  get_filename_component(_expected_real "${_expected_prefix}" REALPATH)
  if(WIN32)
    # NTFS is case-insensitive, and the two sides reach this point from
    # different APIs, so a case difference here is not a real mismatch.
    string(TOLOWER "${_resolved_real}" _resolved_real)
    string(TOLOWER "${_expected_real}" _expected_real)
  endif()
  # Compare against the prefix WITH a trailing separator, so the match is on a
  # path boundary rather than on a string prefix: without it a sibling directory
  # whose name merely starts with the prefix -- "<root>/prefix-config-elsewhere"
  # against "<root>/prefix-config" -- satisfies the check, which is exactly the
  # false accept this assertion exists to prevent.
  string(FIND "${_resolved_real}/" "${_expected_real}/" _prefix_pos)
  if(NOT _prefix_pos EQUAL 0)
    _fail("${_arm_name}-mode arm resolved ${_resolved}, which is outside this test's staged prefix ${_expected_prefix}. The arm tested some other iccDEV installation and proves nothing about this build tree.\nCanonicalised comparison was:\n  resolved: ${_resolved_real}\n  expected: ${_expected_real}")
  endif()
  message(STATUS "${_arm_name} arm resolved ${_resolved}")
endfunction()

# --- Arm 1: CONFIG mode ------------------------------------------------------
set(_config_src "${_work_root}/consumer-config")
file(MAKE_DIRECTORY "${_config_src}")
file(WRITE "${_config_src}/consumer.cpp" "${_consumer_source}")
# RefIccMAX_DIR is set explicitly rather than left to CMAKE_PREFIX_PATH so the
# arm cannot drift onto an unrelated iccDEV installed elsewhere on the machine.
file(WRITE "${_config_src}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.16...3.29)
project(IccDevInstalledConfigConsumer LANGUAGES CXX)
find_package(RefIccMAX CONFIG REQUIRED)
# A static-only install exports only RefIccMAX::IccProfLib2-static, and
# RefIccMAXConfig.cmake then synthesises the generic name as an INTERFACE
# IMPORTED wrapper that has neither IMPORTED_LOCATION nor
# IMPORTED_CONFIGURATIONS. Resolve through to whichever target actually names a
# file, or the anti-vacuity pin fails every static-only build.
function(_iccdev_imported_location _target _out)
  set(_result "")
  if(TARGET ${_target})
    get_target_property(_result ${_target} IMPORTED_LOCATION)
    if(NOT _result)
      get_target_property(_cfgs ${_target} IMPORTED_CONFIGURATIONS)
      foreach(_cfg IN LISTS _cfgs)
        get_target_property(_candidate ${_target} IMPORTED_LOCATION_${_cfg})
        if(_candidate)
          set(_result "${_candidate}")
          break()
        endif()
      endforeach()
    endif()
  endif()
  if(NOT _result)
    set(_result "")
  endif()
  set(${_out} "${_result}" PARENT_SCOPE)
endfunction()

_iccdev_imported_location(RefIccMAX::IccProfLib2 _loc)
if(NOT _loc)
  _iccdev_imported_location(RefIccMAX::IccProfLib2-static _loc)
endif()
message(STATUS "ICCDEV_RESOLVED_LIB=${_loc}")
add_executable(iccdev-installed-consumer consumer.cpp)
target_compile_features(iccdev-installed-consumer PRIVATE cxx_std_17)
target_compile_definitions(iccdev-installed-consumer PRIVATE ICCDEV_INSTALLED_JSON_CONNECT=1)
target_link_libraries(iccdev-installed-consumer PRIVATE
  RefIccMAX::IccProfLib2 RefIccMAX::IccXML2 RefIccMAX::IccJSON2
  RefIccMAX::IccConnect2)
]=])

_run_consumer("CONFIG mode" "${_config_src}" "${_work_root}/build-config"
  "${_prefix_config}" "-DRefIccMAX_DIR=${_package_dir}" _config_error)
_note("${_arm_log}")
set(_config_cfg_stdout "${_arm_stdout}")
set(_config_run_stdout "${_arm_run_stdout}")

if(_config_error)
  _fail("CONFIG-mode consumer: ${_config_error}.\nfind_package(RefIccMAX CONFIG) against the staged prefix must configure, link and run -- this is the install(EXPORT) surface.")
endif()
if(NOT _config_run_stdout MATCHES "installed-consumer: OK")
  _fail("CONFIG-mode consumer did not report success")
endif()
_assert_resolved_under("CONFIG" "${_config_cfg_stdout}" "${_prefix_config}")

# --- Arms 2 and 2b: MODULE mode ---------------------------------------------
#
# Skipped, loudly, on a static-only build. FindRefIccMAX.cmake reports
# not-found for a static-only install by design -- see the REQUIRED_VARS comment
# there -- so these arms would be testing that documented refusal, not the
# manual-discovery path they exist for. The CONFIG and hello-iccdev arms still
# run and still cover a static-only package.
if(NOT ICCDEV_HAVE_SHARED)
  message(STATUS
    "${ICCDEV_TEST_NAME}: static-only build -- MODULE-mode arms skipped "
    "(FindRefIccMAX.cmake requires a shared IccProfLib2); CONFIG and "
    "hello-iccdev arms still run")
  _note("SKIPPED: MODULE-mode arms (static-only build)")
else()

# --- Arm 2: MODULE mode ------------------------------------------------------
set(_module_src "${_work_root}/consumer-module")
file(MAKE_DIRECTORY "${_module_src}")
file(WRITE "${_module_src}/consumer.cpp" "${_consumer_source}")
file(WRITE "${_module_src}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.16...3.29)
project(IccDevInstalledModuleConsumer LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH "${ICCDEV_MODULE_PATH}")
find_package(RefIccMAX MODULE REQUIRED)
get_target_property(_loc RefIccMAX::IccProfLib2 IMPORTED_LOCATION)
message(STATUS "ICCDEV_RESOLVED_LIB=${_loc}")
get_target_property(_defs RefIccMAX::IccProfLib2 INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "ICCDEV_MODULE_DEFS=${_defs}")
add_executable(iccdev-installed-consumer consumer.cpp)
target_compile_features(iccdev-installed-consumer PRIVATE cxx_std_17)
target_link_libraries(iccdev-installed-consumer PRIVATE
  RefIccMAX::IccProfLib2 RefIccMAX::IccXML2)
]=])

_run_consumer("MODULE mode" "${_module_src}" "${_work_root}/build-module"
  "${_prefix_module}"
  "-DICCDEV_MODULE_PATH=${ICCDEV_REPO_ROOT}/Build/Cmake/Modules;-DCMAKE_PREFIX_PATH=${_prefix_module}"
  _module_error)
_note("${_arm_log}")
set(_module_cfg_stdout "${_arm_stdout}")
set(_module_run_stdout "${_arm_run_stdout}")

if(_module_error)
  _fail("MODULE-mode consumer: ${_module_error}.\nThis arm is FindRefIccMAX.cmake's manual-discovery phase. A configure failure on a non-existent RefIccMAX::IccProfLib2 means the module returned a package with no imported targets; a compile failure on <libxml/parser.h> means the IccXML2 target lost libxml2's include directories (#2154).")
endif()
if(NOT _module_run_stdout MATCHES "installed-consumer: OK")
  _fail("MODULE-mode consumer did not report success")
endif()

# The module must hand a Windows consumer the same dllimport annotation the
# CONFIG package carries INTERFACE. This is a configure-time property check
# because off Windows the macro is inert -- IccProfLibConf.h's non-PC branch
# does not read it -- so nothing at compile or link time would notice its loss
# on the platforms where this test usually runs (#2154, from #1888).
if(NOT _module_cfg_stdout MATCHES "ICCDEV_MODULE_DEFS=[^\n]*ICCPROFLIBDLL_DATA_IMPORTS")
  _fail("FindRefIccMAX.cmake's RefIccMAX::IccProfLib2 lost ICCPROFLIBDLL_DATA_IMPORTS from INTERFACE_COMPILE_DEFINITIONS. A Windows consumer reaching iccDEV through the module would get LNK2019 on the eight ICCPROFLIB_DATA_API globals.")
endif()

# --- Arm 2b: MODULE mode with a target-less config shadowing the prefix ------
#
# The regression test for FindRefIccMAX.cmake's CONFIG phase, made independent
# of what happens to be installed on the machine running it.
#
# iccDEV <= 2.3.1 shipped a RefIccMAXConfig.cmake that only sets REFICCMAX_*
# variables and defines no RefIccMAX::* targets. CMake still reports the package
# as found when it loads one, so a module that returns on RefIccMAX_FOUND alone
# hands the caller a package with no imported targets and every documented
# target_link_libraries(RefIccMAX::IccProfLib2) then fails at configure time --
# and it does so in preference to a perfectly good newer install further along
# the search path. The stub below reproduces exactly that layout, so the arm
# fails on a clean runner rather than only where a stale install happens to sit.
set(_legacy_prefix "${_work_root}/legacy-prefix")
file(WRITE "${_legacy_prefix}/lib/cmake/reficcmax/RefIccMAXConfig.cmake" [=[
# Stands in for the pre-2.3.2 package: variables only, no imported targets.
set(REFICCMAX_VERSION "2.3.1")
set(REFICCMAX_INCLUDE_DIRS "")
set(REFICCMAX_LIBRARIES "")
]=])

_run_consumer("MODULE mode (legacy config shadowed)" "${_module_src}"
  "${_work_root}/build-module-legacy" "${_prefix_module}"
  # The two prefixes are ONE CMAKE_PREFIX_PATH value, so the separator between
  # them is an escaped semicolon; an unescaped one would split _extra_args into
  # separate arguments and drop the real prefix off the search path.
  "-DICCDEV_MODULE_PATH=${ICCDEV_REPO_ROOT}/Build/Cmake/Modules;-DCMAKE_PREFIX_PATH=${_legacy_prefix}\;${_prefix_module}"
  _legacy_error)
_note("${_arm_log}")
set(_legacy_cfg_stdout "${_arm_stdout}")
set(_legacy_run_stdout "${_arm_run_stdout}")

if(_legacy_error)
  _fail("MODULE mode with a target-less RefIccMAXConfig.cmake earlier on CMAKE_PREFIX_PATH: ${_legacy_error}.\nFindRefIccMAX.cmake must not return at its CONFIG phase on RefIccMAX_FOUND alone -- it has to confirm CONFIG mode actually created RefIccMAX::IccProfLib2, and fall through to manual discovery when it did not (#2154).")
endif()
if(NOT _legacy_run_stdout MATCHES "installed-consumer: OK")
  _fail("MODULE-mode (legacy config shadowed) consumer did not report success")
endif()

_assert_resolved_under("MODULE" "${_module_cfg_stdout}" "${_prefix_module}")
_assert_resolved_under("MODULE-legacy" "${_legacy_cfg_stdout}" "${_prefix_module}")

endif()

# --- Arm 3: examples/hello-iccdev -------------------------------------------
#
# Built exactly as shipped, so the example in the tree is covered rather than a
# copy of it. RefIccMAX_DIR pins its first discovery path to the staged prefix;
# ICCDEV_BUILD_DIR is pointed at a directory that does not exist so a stale
# build tree cannot satisfy it instead.
set(_hello_src "${ICCDEV_REPO_ROOT}/examples/hello-iccdev")
if(NOT EXISTS "${_hello_src}/CMakeLists.txt")
  _fail("examples/hello-iccdev not found at ${_hello_src}")
endif()

_run_consumer("hello-iccdev" "${_hello_src}" "${_work_root}/build-hello"
  "${_prefix_config}"
  "-DRefIccMAX_DIR=${_package_dir};-DICCDEV_BUILD_DIR=${_work_root}/no-such-build-dir"
  _hello_error)
_note("${_arm_log}")
set(_hello_cfg_stdout "${_arm_stdout}")
set(_hello_run_stdout "${_arm_run_stdout}")

if(_hello_error)
  _fail("examples/hello-iccdev: ${_hello_error}.\nThe documented example must build and run against an installed package.")
endif()
if(NOT _hello_cfg_stdout MATCHES "hello-iccdev: using installed RefIccMAX package")
  _fail("examples/hello-iccdev did not take its installed-package discovery path; it fell back to a build tree and did not test the install.")
endif()
if(NOT _hello_run_stdout MATCHES "Hello, iccDEV!")
  _fail("examples/hello-iccdev ran but did not produce its expected output")
endif()

file(WRITE "${_log_file}" "CTest test: ${ICCDEV_TEST_NAME}\n"
     "Configuration: ${ICCDEV_CONFIG}\n"
     "Staged prefix (CONFIG): ${_prefix_config}\n"
     "Staged prefix (MODULE): ${_prefix_module}\n\n${_log}")
message(STATUS "Wrote ${ICCDEV_TEST_NAME} log to ${_log_file}")
message(STATUS "${ICCDEV_TEST_NAME}: CONFIG, MODULE and hello-iccdev consumers "
               "all built and ran against the staged install")
