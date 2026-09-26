#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Shared target definitions for Valgrind-family analysis.
###############################################################################

# This file is sourced by the build, run, and validation entry points.
# shellcheck disable=SC2034

VG_TARGETS=(
  connect-thread
  pawg
  dump
  roundtrip
  fromxml
  fromjson
  toxml
  tojson
  tiffdump
  applynamedcmm
  applyprofiles-row
  applysearch-row
  applytolink
  benchapply
)

VG_BUILD_TARGETS=(
  iccConnectThreadTest
  iccPawgReport
  iccDumpProfile
  iccRoundTrip
  iccFromXml
  iccFromJson
  iccToXml
  iccToJson
  iccTiffDump
  iccApplyNamedCmm
  iccApplyProfiles
  iccApplySearch
  iccApplyToLink
  iccBenchApply
)

vg_print_targets()
{
  echo "Available targets:"
  echo "  connect-thread    - threaded CMM regression (best Helgrind/DRD target)"
  echo "  pawg              - PAWG security, conformance, and quality report"
  echo "  dump              - binary profile load, validate, and dump"
  echo "  roundtrip         - MPE profile load/write/reload path"
  echo "  fromxml           - XML profile parser and binary writer"
  echo "  fromjson          - JSON profile parser and binary writer"
  echo "  toxml             - binary profile to XML serializer"
  echo "  tojson            - binary profile to deterministic JSON serializer"
  echo "  tiffdump          - TIFF parser and embedded-profile extraction"
  echo "  applynamedcmm     - named CMM data application"
  echo "  applyprofiles-row - four-worker TIFF row application"
  echo "  applysearch-row   - four-worker inverse-search application"
  echo "  applytolink       - DeviceLink generation"
  echo "  benchapply        - threaded benchmark application path"
}

vg_tool_binary()
{
  local tool_dir="$1"
  local binary_name="$2"
  printf '%s/Tools/%s/%s' "$VG_BUILD_DIR" "$tool_dir" "$binary_name"
}

vg_configure_target()
{
  local target="$1"
  local profile="$VG_SOURCE_DIR/Testing/sRGB_v4_ICC_preference.icc"
  local xml_profile="$VG_SOURCE_DIR/Testing/Display/sRGB_D65_MAT.xml"
  local rgb_data="$VG_SOURCE_DIR/.github/ci/test-data/test-data-rgb-16bit.txt"
  local tiff_input="$VG_SOURCE_DIR/Testing/hybrid/Data/MS_smCowsIcon.tif"

  VG_BINARY=""
  VG_CMAKE_TARGET=""
  VG_NOTE=""
  VG_RECOMMENDED_TOOL="memcheck"
  VG_ARGS=()
  VG_EXTRA_CMAKE_TARGETS=()
  VG_PREPARE=()
  VG_REQUIRED_FILES=()

  case "$target" in
    connect-thread)
      VG_CMAKE_TARGET="iccConnectThreadTest"
      VG_BINARY="$VG_BUILD_DIR/Testing/iccConnectThreadTest"
      VG_ARGS=("$profile")
      VG_REQUIRED_FILES=("$profile")
      VG_RECOMMENDED_TOOL="helgrind"
      VG_NOTE="Exercises concurrent CIccCmm apply-object creation."
      ;;
    pawg)
      VG_CMAKE_TARGET="iccPawgReport"
      VG_BINARY="$(vg_tool_binary IccPawgReport iccPawgReport)"
      VG_ARGS=("$profile")
      VG_REQUIRED_FILES=("$profile")
      VG_NOTE="Exercises PAWG security, conformance, and quality reporting."
      ;;
    dump)
      VG_CMAKE_TARGET="iccDumpProfile"
      VG_BINARY="$(vg_tool_binary IccDumpProfile iccDumpProfile)"
      VG_ARGS=("-v" "100" "$profile" "ALL")
      VG_REQUIRED_FILES=("$profile")
      VG_NOTE="Exercises eager profile validation and tag dumping."
      ;;
    roundtrip)
      VG_CMAKE_TARGET="iccRoundTrip"
      VG_BINARY="$(vg_tool_binary IccRoundTrip iccRoundTrip)"
      VG_ARGS=("$profile" "1" "1")
      VG_REQUIRED_FILES=("$profile")
      VG_NOTE="Exercises the MPE round-trip path."
      ;;
    fromxml)
      VG_CMAKE_TARGET="iccFromXml"
      VG_BINARY="$(vg_tool_binary IccFromXml iccFromXml)"
      VG_ARGS=("$xml_profile" "$VG_RUN_WORK/fromxml.icc" "-noid")
      VG_REQUIRED_FILES=("$xml_profile")
      VG_NOTE="Exercises XML parsing and binary serialization."
      ;;
    fromjson)
      VG_CMAKE_TARGET="iccFromJson"
      VG_BINARY="$(vg_tool_binary IccFromJson iccFromJson)"
      VG_PREPARE=("$(vg_tool_binary IccToJson iccToJson)" "$profile" "$VG_RUN_WORK/input.json" "-sort")
      VG_EXTRA_CMAKE_TARGETS=("iccToJson")
      VG_ARGS=("$VG_RUN_WORK/input.json" "$VG_RUN_WORK/fromjson.icc" "-noid")
      VG_REQUIRED_FILES=("$profile" "$(vg_tool_binary IccToJson iccToJson)")
      VG_NOTE="Generates deterministic JSON, then analyzes JSON parsing."
      ;;
    toxml)
      VG_CMAKE_TARGET="iccToXml"
      VG_BINARY="$(vg_tool_binary IccToXml iccToXml)"
      VG_ARGS=("$profile" "$VG_RUN_WORK/output.xml")
      VG_REQUIRED_FILES=("$profile")
      VG_NOTE="Exercises XML serialization."
      ;;
    tojson)
      VG_CMAKE_TARGET="iccToJson"
      VG_BINARY="$(vg_tool_binary IccToJson iccToJson)"
      VG_ARGS=("$profile" "$VG_RUN_WORK/output.json" "-sort")
      VG_REQUIRED_FILES=("$profile")
      VG_NOTE="Exercises deterministic JSON serialization."
      ;;
    tiffdump)
      VG_CMAKE_TARGET="iccTiffDump"
      VG_BINARY="$(vg_tool_binary IccTiffDump iccTiffDump)"
      VG_ARGS=("$tiff_input" "$VG_RUN_WORK/embedded.icc")
      VG_REQUIRED_FILES=("$tiff_input")
      VG_NOTE="Exercises TIFF parsing and embedded-profile extraction."
      ;;
    applynamedcmm)
      VG_CMAKE_TARGET="iccApplyNamedCmm"
      VG_BINARY="$(vg_tool_binary IccApplyNamedCmm iccApplyNamedCmm)"
      VG_ARGS=("$rgb_data" "5" "1" "$profile" "3")
      VG_REQUIRED_FILES=("$rgb_data" "$profile")
      VG_NOTE="Exercises 16-bit tetrahedral named CMM application."
      ;;
    applyprofiles-row)
      VG_CMAKE_TARGET="iccApplyProfiles"
      VG_BINARY="$(vg_tool_binary IccApplyProfiles iccApplyProfiles)"
      VG_ARGS=("-threads" "4" "$tiff_input" "$VG_RUN_WORK/applied.tif" "1" "0" "0" "1" "1" "$profile" "1")
      VG_REQUIRED_FILES=("$tiff_input" "$profile")
      VG_RECOMMENDED_TOOL="helgrind"
      VG_NOTE="Exercises threaded row application and TIFF output."
      ;;
    applysearch-row)
      VG_CMAKE_TARGET="iccApplySearch"
      VG_BINARY="$(vg_tool_binary IccApplySearch iccApplySearch)"
      VG_ARGS=("-threads" "4" "$rgb_data" "5" "1" "$profile" "1" "$profile" "1" "-INIT" "1" "$profile" "1")
      VG_REQUIRED_FILES=("$rgb_data" "$profile")
      VG_RECOMMENDED_TOOL="helgrind"
      VG_NOTE="Exercises threaded inverse-search apply objects."
      ;;
    applytolink)
      VG_CMAKE_TARGET="iccApplyToLink"
      VG_BINARY="$(vg_tool_binary IccApplyToLink iccApplyToLink)"
      VG_ARGS=("$VG_RUN_WORK/link.icc" "0" "9" "0" "Valgrind" "0.0" "1.0" "1" "1" "$profile" "1" "$profile" "1")
      VG_REQUIRED_FILES=("$profile")
      VG_NOTE="Exercises DeviceLink generation with two profile transforms."
      ;;
    benchapply)
      VG_CMAKE_TARGET="iccBenchApply"
      VG_BINARY="$(vg_tool_binary IccBenchApply iccBenchApply)"
      VG_ARGS=("-pixels" "4" "-repeats" "1" "-threads" "1,2,4" "1" "$profile" "140")
      VG_REQUIRED_FILES=("$profile")
      VG_RECOMMENDED_TOOL="helgrind"
      VG_NOTE="Exercises the benchmark path across three thread counts."
      ;;
    *)
      return 1
      ;;
  esac
}
