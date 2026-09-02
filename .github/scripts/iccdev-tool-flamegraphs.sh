#!/bin/bash
###############################################################################
# iccDEV all-tool perf/FlameGraph profiling helper
###############################################################################
#
# Environment variables:
#   ICCDEV_TOOLS_DIR              -- path to build/Tools
#   ICCDEV_TESTING_DIR            -- path to Testing
#   ICCDEV_FLAMEGRAPH_OUTDIR      -- output directory
#   ICCDEV_FLAMEGRAPH_REPEAT      -- command repetitions per tool
#   ICCDEV_FLAMEGRAPH_FREQ        -- perf sample frequency
#   ICCDEV_FLAMEGRAPH_MIN_SAMPLES -- low-sample threshold per tool
#   ICCDEV_FLAMEGRAPH_CALL_GRAPH  -- fp, dwarf, or lbr
#   ICCDEV_FLAMEGRAPH_USER_ONLY   -- set to 1 to sample user space only
#   ICCDEV_FLAMEGRAPH_TIMEOUT     -- timeout per profiled tool
#   ICCDEV_FLAMEGRAPH_DIR         -- optional FlameGraph checkout
###############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [ -d "$REPO_ROOT/IccProfLib" ]; then
  TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/build/Tools}"
  ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
else
  TOOLS="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/iccDEV/Build/Tools}"
  ICCDEV_TESTING="${ICCDEV_TESTING_DIR:-$REPO_ROOT/iccDEV/Testing}"
fi

OUTDIR="${ICCDEV_FLAMEGRAPH_OUTDIR:-/tmp/iccdev-tool-flamegraphs}"
WORKDIR="$OUTDIR/work"
REPORTS="$OUTDIR/reports"
MANIFEST="$OUTDIR/manifest.tsv"
REPEAT="${ICCDEV_FLAMEGRAPH_REPEAT:-10}"
FREQ="${ICCDEV_FLAMEGRAPH_FREQ:-199}"
MIN_SAMPLES="${ICCDEV_FLAMEGRAPH_MIN_SAMPLES:-25}"
CALL_GRAPH="${ICCDEV_FLAMEGRAPH_CALL_GRAPH:-dwarf}"
USER_ONLY="${ICCDEV_FLAMEGRAPH_USER_ONLY:-1}"
TIMEOUT_SEC="${ICCDEV_FLAMEGRAPH_TIMEOUT:-45}"
FLAMEGRAPH_DIR="${ICCDEV_FLAMEGRAPH_DIR:-}"

usage()
{
  cat <<'EOF'
Usage:
  ICCDEV_TOOLS_DIR=$PWD/build/Tools \
  ICCDEV_TESTING_DIR=$PWD/Testing \
  ICCDEV_FLAMEGRAPH_OUTDIR=/tmp/iccdev-tool-flamegraphs \
    .github/scripts/iccdev-tool-flamegraphs.sh

Profiles representative invocations for each available iccDEV command-line
tool. Each tool gets its own reports/<tool>/ directory containing perf.data,
perf.script, perf.report.txt, perf.folded, and flamegraph.svg when the host
provides perf and FlameGraph scripts. Missing tools/inputs and perf permission
failures are recorded in manifest.tsv instead of aborting the whole run.
EOF
}

case "${1:-}" in
  -h|--help)
    usage
    exit 0
    ;;
esac

case "$REPEAT" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid ICCDEV_FLAMEGRAPH_REPEAT: $REPEAT" >&2
    exit 1
    ;;
esac

case "$FREQ" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid ICCDEV_FLAMEGRAPH_FREQ: $FREQ" >&2
    exit 1
    ;;
esac

case "$MIN_SAMPLES" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid ICCDEV_FLAMEGRAPH_MIN_SAMPLES: $MIN_SAMPLES" >&2
    exit 1
    ;;
esac

case "$TIMEOUT_SEC" in
  ""|*[!0-9]*)
    echo "[FAIL] invalid ICCDEV_FLAMEGRAPH_TIMEOUT: $TIMEOUT_SEC" >&2
    exit 1
    ;;
esac

case "$CALL_GRAPH" in
  fp|dwarf|lbr)
    ;;
  *)
    echo "[FAIL] invalid ICCDEV_FLAMEGRAPH_CALL_GRAPH: $CALL_GRAPH" >&2
    exit 1
    ;;
esac

case "$USER_ONLY" in
  0|1)
    ;;
  *)
    echo "[FAIL] invalid ICCDEV_FLAMEGRAPH_USER_ONLY: $USER_ONLY" >&2
    exit 1
    ;;
esac

BUILD_ROOT="$(cd "$TOOLS/.." 2>/dev/null && pwd -P)"

export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${PATH:+:$PATH}"

mkdir -p "$OUTDIR" "$WORKDIR" "$REPORTS"
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  tool status exit_code samples unknown_folded svg perf_report command note > "$MANIFEST"

TP="$WORKDIR/profiles"
TP_TIFF="$WORKDIR/tiff"
TP_IMG="$WORKDIR/images"
TP_SPECTRAL="$WORKDIR/spectral"
TD="$REPO_ROOT/.github/ci/test-data"
mkdir -p "$TP" "$TP_TIFF" "$TP_IMG" "$TP_SPECTRAL"

if [ -d "$ICCDEV_TESTING" ]; then
  for gendir in Display Named CMYK-3DLUTs Calc CalcTest Encoding HDR ICS \
                Overprint PCC SpecRef mcs mcs/Flexo-CMYKOGP mcs/Spot-MVIS \
                hybrid/ICC hybrid/Results; do
    if [ -d "$ICCDEV_TESTING/$gendir" ]; then
      for icc in "$ICCDEV_TESTING/$gendir"/*.icc; do
        [ -f "$icc" ] || continue
        cp -f "$icc" "$TP/" 2>/dev/null || true
      done
    fi
  done
fi

if [ -d "$REPO_ROOT/.github/ci/test-data/spectral" ]; then
  cp -f "$REPO_ROOT/.github/ci/test-data/spectral"/spec_* "$TP_SPECTRAL/" 2>/dev/null || true
fi

find_perf()
{
  local candidate
  if command -v perf >/dev/null 2>&1 && perf --version >/dev/null 2>&1; then
    command -v perf
    return 0
  fi

  for candidate in /usr/lib/linux-tools/*/perf; do
    if [ -x "$candidate" ] && "$candidate" --version >/dev/null 2>&1; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

find_flamegraph_tool()
{
  local name="$1"
  local path=""
  if [ -n "$FLAMEGRAPH_DIR" ]; then
    path="$FLAMEGRAPH_DIR/$name"
    if [ -x "$path" ]; then
      printf '%s\n' "$path"
      return 0
    fi
  fi
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  return 1
}

tool_path()
{
  local dir="$1"
  local exe="$2"
  local path="$TOOLS/$dir/$exe"
  if [ ! -x "$path" ] && [ -x "$path.exe" ]; then
    path="$path.exe"
  fi
  if [ ! -x "$path" ]; then
    return 1
  fi
  printf '%s\n' "$path"
}

command_string()
{
  local out=""
  local arg
  for arg in "$@"; do
    if [ -n "$out" ]; then
      out+=" "
    fi
    out+="$(printf '%q' "$arg")"
  done
  printf '%s\n' "$out"
}

html_escape()
{
  printf '%s' "$1" | sed \
    -e 's/&/\&amp;/g' \
    -e 's/</\&lt;/g' \
    -e 's/>/\&gt;/g' \
    -e 's/"/\&quot;/g' \
    -e "s/'/\&#39;/g"
}

sample_count()
{
  local report="$1"
  awk '
    /^# Samples:/ {
      value=$3
      gsub(/,/, "", value)
      if (value ~ /K$/) {
        sub(/K$/, "", value)
        printf "%.0f\n", value * 1000
      } else if (value ~ /M$/) {
        sub(/M$/, "", value)
        printf "%.0f\n", value * 1000000
      } else {
        print value
      }
      found=1
    }
    END { if (!found) print 0 }
  ' "$report"
}

unknown_folded_lines()
{
  local folded="$1"
  if [ ! -f "$folded" ]; then
    printf '0\n'
    return 0
  fi
  grep -Ec '(^|;)(\[unknown\]|unknown)(;| )' "$folded" || true
}

manifest_row()
{
  local tool="$1"
  local status="$2"
  local exit_code="$3"
  local samples="$4"
  local unknown="$5"
  local svg="$6"
  local report="$7"
  local command="$8"
  local note="$9"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$tool" "$status" "$exit_code" "$samples" "$unknown" "$svg" "$report" "$command" "$note" >> "$MANIFEST"
}

PERF_BIN=""
if PERF_BIN="$(find_perf)"; then
  echo "[INFO] using perf: $PERF_BIN"
else
  echo "[WARN] perf unavailable; manifest will contain SKIP rows"
fi

STACKCOLLAPSE="$(find_flamegraph_tool stackcollapse-perf.pl || true)"
FLAMEGRAPH="$(find_flamegraph_tool flamegraph.pl || true)"
if [ -z "$STACKCOLLAPSE" ] || [ -z "$FLAMEGRAPH" ]; then
  echo "[WARN] FlameGraph scripts unavailable; SVG generation will be skipped"
fi

profile_tool()
{
  local name="$1"
  shift
  local report_dir="$REPORTS/$name"
  local cmd_string
  cmd_string="$(command_string "$@")"
  mkdir -p "$report_dir"

  if [ -z "$PERF_BIN" ]; then
    manifest_row "$name" "SKIP" "" "0" "0" "" "" "$cmd_string" "perf unavailable"
    return 0
  fi

  local event="task-clock"
  if [ "$USER_ONLY" = "1" ]; then
    event="task-clock:u"
  fi

  set +e
  # shellcheck disable=SC2016
  timeout "$TIMEOUT_SEC" "$PERF_BIN" record \
    -F "$FREQ" \
    -e "$event" \
    --call-graph "$CALL_GRAPH" \
    --user-callchains \
    -o "$report_dir/perf.data" \
    -- bash -c '
      repeat="$1"
      shift
      i=0
      while [ "$i" -lt "$repeat" ]; do
        "$@"
        i=$((i + 1))
      done
    ' _ "$REPEAT" "$@" > "$report_dir/run.log" 2> "$report_dir/perf.record.err"
  local rc=$?
  set -e

  if [ "$rc" -ne 0 ]; then
    manifest_row "$name" "PERF_FAIL" "$rc" "0" "0" "" "" "$cmd_string" "perf record failed"
    return 0
  fi

  "$PERF_BIN" script --demangle -i "$report_dir/perf.data" > "$report_dir/perf.script" 2> "$report_dir/perf.script.err" || true
  "$PERF_BIN" report --stdio --demangle -i "$report_dir/perf.data" > "$report_dir/perf.report.txt" 2> "$report_dir/perf.report.err" || true

  local samples
  samples="$(sample_count "$report_dir/perf.report.txt")"
  local status="PROFILED"
  local note=""
  if [ "$samples" -lt "$MIN_SAMPLES" ]; then
    status="LOW_SAMPLES"
    note="sample count below threshold"
  fi

  local svg=""
  local unknown="0"
  if [ -x "$STACKCOLLAPSE" ] && [ -x "$FLAMEGRAPH" ]; then
    "$STACKCOLLAPSE" "$report_dir/perf.script" > "$report_dir/perf.folded" 2> "$report_dir/stackcollapse.err" || true
    unknown="$(unknown_folded_lines "$report_dir/perf.folded")"
    if [ -s "$report_dir/perf.folded" ]; then
      "$FLAMEGRAPH" "$report_dir/perf.folded" > "$report_dir/flamegraph.svg" 2> "$report_dir/flamegraph.err" || true
      if [ -s "$report_dir/flamegraph.svg" ]; then
        svg="$report_dir/flamegraph.svg"
      fi
    fi
  else
    note="${note:+$note; }FlameGraph scripts unavailable"
  fi

  manifest_row "$name" "$status" "0" "$samples" "$unknown" "$svg" "$report_dir/perf.report.txt" "$cmd_string" "$note"
}

skip_tool()
{
  local name="$1"
  local note="$2"
  manifest_row "$name" "SKIP" "" "0" "0" "" "" "" "$note"
}

read_manifest_row()
{
  local manifest_line="$1"
  local separator=$'\034'
  manifest_line="${manifest_line//$'\t'/$separator}"
  IFS="$separator" read -r tool status exit_code samples unknown svg report command note <<< "$manifest_line"
}

write_dashboard()
{
  local dashboard="$OUTDIR/index.html"
  local dashboard_rows="$WORKDIR/dashboard-rows.tsv"
  local profiled_count
  local low_sample_count
  local skipped_count
  local manifest_line
  local tool status exit_code samples unknown svg report command note

  : > "$dashboard_rows"
  while IFS= read -r manifest_line; do
    read_manifest_row "$manifest_line"
    [ "$tool" = "tool" ] && continue
    if [ -n "$svg" ] && [ -f "$svg" ]; then
      printf '%012d\t%s\t%s\t%s\t%s\t%s\n' \
        "$samples" "$tool" "$status" "$unknown" \
        "${svg#"$OUTDIR"/}" "${report#"$OUTDIR"/}" >> "$dashboard_rows"
    fi
  done < "$MANIFEST"
  sort -t $'\t' -k1,1nr -k2,2 "$dashboard_rows" > "${dashboard_rows}.sorted"
  mv "${dashboard_rows}.sorted" "$dashboard_rows"

  profiled_count="$(awk -F'\t' 'NR > 1 && $2 == "PROFILED" { count++ } END { print count + 0 }' "$MANIFEST")"
  low_sample_count="$(awk -F'\t' 'NR > 1 && $2 == "LOW_SAMPLES" { count++ } END { print count + 0 }' "$MANIFEST")"
  skipped_count="$(awk -F'\t' 'NR > 1 && $2 ~ /^(SKIP|PERF_FAIL)$/ { count++ } END { print count + 0 }' "$MANIFEST")"

  {
    printf '%s\n' '<!doctype html>'
    printf '%s\n' '<html lang="en"><head><meta charset="utf-8">'
    printf '%s\n' '<meta name="viewport" content="width=device-width, initial-scale=1">'
    printf '%s\n' '<title>iccDEV FlameGraph Dashboard</title>'
    printf '%s\n' '<style>'
    printf '%s\n' 'body{font-family:system-ui,sans-serif;line-height:1.45;margin:2rem;color:#17202a}'
    printf '%s\n' 'h1,h2{margin-bottom:.35rem}.lede{color:#4b5563;max-width:70rem}'
    printf '%s\n' '.counts{display:flex;gap:.75rem;flex-wrap:wrap;margin:1rem 0}'
    printf '%s\n' '.count{border:1px solid #d0d7de;border-radius:.5rem;padding:.5rem .8rem;background:#f6f8fa}'
    printf '%s\n' '.primary{border:1px solid #d0d7de;border-radius:.5rem;padding:.75rem;margin:1rem 0}'
    printf '%s\n' '.primary object{width:100%;height:34rem;border:0}.gallery{display:grid;grid-template-columns:repeat(auto-fit,minmax(19rem,1fr));gap:1rem}'
    printf '%s\n' '.card{border:1px solid #d0d7de;border-radius:.5rem;padding:.5rem;color:inherit;text-decoration:none}'
    printf '%s\n' '.card img{width:100%;height:11rem;object-fit:contain}.card strong{display:block;margin-top:.4rem}'
    printf '%s\n' 'table{border-collapse:collapse;width:100%;margin:1rem 0}th,td{border:1px solid #d0d7de;padding:.45rem;text-align:left}'
    printf '%s\n' 'th{background:#f6f8fa}.pass{color:#1a7f37}.warn{color:#9a6700}.skip{color:#57606a}code{background:#f6f8fa;padding:.1rem .25rem}'
    printf '%s\n' '@media(max-width:700px){body{margin:1rem}.primary object{height:23rem}}'
    printf '%s\n' '</style></head><body>'
    printf '%s\n' '<h1>iccDEV FlameGraph Dashboard</h1>'
    printf '%s\n' '<p class="lede">Interactive call-stack visualization is the primary view. Timing and sample counts help judge capture quality; they are not performance thresholds.</p>'
    printf '<div class="counts"><span class="count"><strong>%s</strong> profiled</span>' "$(html_escape "$profiled_count")"
    printf '<span class="count"><strong>%s</strong> low samples</span>' "$(html_escape "$low_sample_count")"
    printf '<span class="count"><strong>%s</strong> skipped or perf failures</span></div>\n' "$(html_escape "$skipped_count")"

    if [ -s "$dashboard_rows" ]; then
      IFS=$'\t' read -r samples tool status unknown svg report < "$dashboard_rows"
      printf '<h2>Primary Graph: %s</h2>\n' "$(html_escape "$tool")"
      printf '<div class="primary"><object data="%s" type="image/svg+xml">' "$(html_escape "$svg")"
      printf '<a href="%s">Open %s FlameGraph</a></object></div>\n' \
        "$(html_escape "$svg")" "$(html_escape "$tool")"
      printf '%s\n' '<h2>All Interactive FlameGraphs</h2><div class="gallery">'
      while IFS=$'\t' read -r samples tool status unknown svg report; do
        printf '<a class="card" href="%s"><img src="%s" alt="%s FlameGraph">' \
          "$(html_escape "$svg")" "$(html_escape "$svg")" "$(html_escape "$tool")"
        printf '<strong>%s</strong><span>%s samples; %s unknown frames</span></a>\n' \
          "$(html_escape "$tool")" "$(html_escape "$samples")" "$(html_escape "$unknown")"
      done < "$dashboard_rows"
      printf '%s\n' '</div>'
    else
      printf '%s\n' '<h2>No Interactive FlameGraphs Captured</h2>'
      printf '%s\n' '<p>Review the manifest for perf availability, capture failures, and skipped inputs.</p>'
    fi

    printf '%s\n' '<h2>Capture Manifest</h2>'
    printf '%s\n' '<table><tr><th>Tool</th><th>Status</th><th>Samples</th><th>Unknown Frames</th><th>Graph</th><th>Perf Report</th><th>Note</th></tr>'
    while IFS= read -r manifest_line; do
      read_manifest_row "$manifest_line"
      [ "$tool" = "tool" ] && continue
      status_class="skip"
      case "$status" in
        PROFILED)
          status_class="pass"
          ;;
        LOW_SAMPLES)
          status_class="warn"
          ;;
      esac
      svg_link="-"
      report_link="-"
      if [ -n "$svg" ] && [ -f "$svg" ]; then
        svg_rel="${svg#"$OUTDIR"/}"
        svg_link="<a href=\"$(html_escape "$svg_rel")\">Open graph</a>"
      fi
      if [ -n "$report" ] && [ -f "$report" ]; then
        report_rel="${report#"$OUTDIR"/}"
        report_link="<a href=\"$(html_escape "$report_rel")\">Open report</a>"
      fi
      printf '<tr><td>%s</td><td class="%s">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n' \
        "$(html_escape "$tool")" "$status_class" "$(html_escape "$status")" \
        "$(html_escape "$samples")" "$(html_escape "$unknown")" \
        "$svg_link" "$report_link" "$(html_escape "$note")"
    done < "$MANIFEST"
    printf '%s\n' '</table>'
    printf '<p>Raw capture files are listed in <a href="%s">manifest.tsv</a>.</p>\n' "$(html_escape "${MANIFEST#"$OUTDIR"/}")"
    printf '%s\n' '</body></html>'
  } > "$dashboard"
}

require_file()
{
  local path="$1"
  [ -f "$path" ]
}

require_dir()
{
  local path="$1"
  [ -d "$path" ]
}

SRGB="$TP/sRGB_D65_MAT.icc"
CMYK="$TP/CMYK-3DLUTs2.icc"
V5_DISPLAY="$TP/LCDDisplay.icc"
DISPLAY_P3="$TP/ios-gen-DisplayP3.icc"
[ -f "$DISPLAY_P3" ] || DISPLAY_P3="$SRGB"
SRGB_CALC_DATA="$ICCDEV_TESTING/Calc/srgbCalcTest.txt"
TIFF_8BIT="$ICCDEV_TESTING/ApplyDataFiles/seed-tiff-none-rgb-8x8.tif"
TIFF_ALT="$ICCDEV_TESTING/hybrid/Data/TShirtDesignKW.tif"
[ -f "$TIFF_8BIT" ] || TIFF_8BIT="$TIFF_ALT"
V5_OBS="$ICCDEV_TESTING/PCC/Spec400_10_700-R1_2deg-CAT02.icc"
[ -f "$V5_OBS" ] || V5_OBS="$ICCDEV_TESTING/ICS/Spec400_10_700-D50_2deg-Part1.icc"

DUMP="$(tool_path IccDumpProfile iccDumpProfile || true)"
TOXML="$(tool_path IccToXml iccToXml || true)"
FROMXML="$(tool_path IccFromXml iccFromXml || true)"
ROUNDTRIP="$(tool_path IccRoundTrip iccRoundTrip || true)"
TOJSON="$(tool_path IccToJson iccToJson || true)"
FROMJSON="$(tool_path IccFromJson iccFromJson || true)"
FROMCUBE="$(tool_path IccFromCube iccFromCube || true)"
APPLYNCM="$(tool_path IccApplyNamedCmm iccApplyNamedCmm || true)"
APPLYSEARCH="$(tool_path IccApplySearch iccApplySearch || true)"
APPLYLINK="$(tool_path IccApplyToLink iccApplyToLink || true)"
APPLYPROF="$(tool_path IccApplyProfiles iccApplyProfiles || true)"
TIFFDUMP="$(tool_path IccTiffDump iccTiffDump || true)"
JPEGDUMP="$(tool_path IccJpegDump iccJpegDump || true)"
PNGDUMP="$(tool_path IccPngDump iccPngDump || true)"
V5CONV="$(tool_path IccV5DspObsToV4Dsp iccV5DspObsToV4Dsp || true)"
SPECSEP="$(tool_path IccSpecSepToTiff iccSpecSepToTiff || true)"
PAWG="$(tool_path IccPawgReport iccPawgReport || true)"
VISUALIZE="$(tool_path IccProfileVisualize iccProfileVisualize || true)"
DESCRIBE="$(tool_path IccDescribeSinkTest iccDescribeSinkTest || true)"

if [ -n "$TOXML" ] && [ -f "$SRGB" ]; then
  "$TOXML" "$SRGB" "$WORKDIR/srgb.xml" >/dev/null 2>&1 || true
fi
if [ -n "$TOJSON" ] && [ -f "$SRGB" ]; then
  "$TOJSON" "$SRGB" "$WORKDIR/srgb.json" >/dev/null 2>&1 || true
fi

if [ -n "$DUMP" ] && require_file "$SRGB"; then
  profile_tool iccDumpProfile "$DUMP" -v 100 "$SRGB" ALL
else
  skip_tool iccDumpProfile "missing tool or sRGB profile"
fi

if [ -n "$TOXML" ] && require_file "$CMYK"; then
  profile_tool iccToXml "$TOXML" "$CMYK" "$WORKDIR/cmyk.xml"
else
  skip_tool iccToXml "missing tool or CMYK profile"
fi

if [ -n "$FROMXML" ] && require_file "$WORKDIR/srgb.xml"; then
  profile_tool iccFromXml "$FROMXML" "$WORKDIR/srgb.xml" "$WORKDIR/srgb_rt.icc"
else
  skip_tool iccFromXml "missing tool or generated XML"
fi

if [ -n "$ROUNDTRIP" ] && require_file "$SRGB"; then
  profile_tool iccRoundTrip "$ROUNDTRIP" "$SRGB" 1
else
  skip_tool iccRoundTrip "missing tool or sRGB profile"
fi

if [ -n "$TOJSON" ] && require_file "$SRGB"; then
  profile_tool iccToJson "$TOJSON" "$SRGB" "$WORKDIR/tojson.json"
else
  skip_tool iccToJson "missing tool or sRGB profile"
fi

if [ -n "$FROMJSON" ] && require_file "$WORKDIR/srgb.json"; then
  profile_tool iccFromJson "$FROMJSON" "$WORKDIR/srgb.json" "$WORKDIR/fromjson.icc"
else
  skip_tool iccFromJson "missing tool or generated JSON"
fi

if [ -n "$FROMCUBE" ] && require_file "$TD/test-warmfilm-5x5x5.cube"; then
  profile_tool iccFromCube "$FROMCUBE" "$TD/test-warmfilm-5x5x5.cube" "$WORKDIR/warmfilm.icc"
else
  skip_tool iccFromCube "missing tool or cube fixture"
fi

if [ -n "$APPLYNCM" ] && require_file "$SRGB_CALC_DATA" && require_file "$SRGB"; then
  profile_tool iccApplyNamedCmm "$APPLYNCM" "$SRGB_CALC_DATA" 3 0 "$SRGB" 1
else
  skip_tool iccApplyNamedCmm "missing tool, calc data, or sRGB profile"
fi

if [ -n "$APPLYSEARCH" ] && require_file "$SRGB_CALC_DATA" && require_file "$SRGB"; then
  profile_tool iccApplySearch "$APPLYSEARCH" "$SRGB_CALC_DATA" 0 0 "$SRGB" 1 "$SRGB" 1 -INIT 1
else
  skip_tool iccApplySearch "missing tool, calc data, or sRGB profile"
fi

if [ -n "$APPLYLINK" ] && require_file "$SRGB"; then
  profile_tool iccApplyToLink "$APPLYLINK" "$WORKDIR/link.cube" 1 5 4 sRGB-cube 0.0 1.0 0 0 "$SRGB" 1
else
  skip_tool iccApplyToLink "missing tool or sRGB profile"
fi

if [ -n "$APPLYPROF" ] && require_file "$TIFF_8BIT" && require_file "$SRGB"; then
  profile_tool iccApplyProfiles "$APPLYPROF" "$TIFF_8BIT" "$WORKDIR/apply.tif" 0 0 0 0 0 "$SRGB" 1
else
  skip_tool iccApplyProfiles "missing tool, TIFF fixture, or sRGB profile"
fi

if [ -n "$TIFFDUMP" ] && require_file "$TIFF_8BIT"; then
  profile_tool iccTiffDump "$TIFFDUMP" "$TIFF_8BIT" "$WORKDIR/tiff.icc"
else
  skip_tool iccTiffDump "missing tool or TIFF fixture"
fi

JPG_FIXTURE="$(find "$REPO_ROOT/.github/ci/test-data" "$ICCDEV_TESTING" -type f \( -name '*.jpg' -o -name '*.jpeg' \) 2>/dev/null | sed -n '1p')"
if [ -n "$JPEGDUMP" ] && [ -n "$JPG_FIXTURE" ]; then
  profile_tool iccJpegDump "$JPEGDUMP" "$JPG_FIXTURE" "$WORKDIR/jpeg.icc"
else
  skip_tool iccJpegDump "missing tool or JPEG fixture"
fi

PNG_FIXTURE="$(find "$REPO_ROOT/.github/ci/test-data" "$ICCDEV_TESTING" -type f -name '*.png' 2>/dev/null | sed -n '1p')"
if [ -n "$PNGDUMP" ] && [ -n "$PNG_FIXTURE" ]; then
  profile_tool iccPngDump "$PNGDUMP" "$PNG_FIXTURE" "$WORKDIR/png.icc"
else
  skip_tool iccPngDump "missing tool or PNG fixture"
fi

if [ -n "$V5CONV" ] && require_file "$V5_DISPLAY" && require_file "$V5_OBS"; then
  profile_tool iccV5DspObsToV4Dsp "$V5CONV" "$V5_DISPLAY" "$V5_OBS" "$WORKDIR/v5out.icc"
else
  skip_tool iccV5DspObsToV4Dsp "missing tool or v5 observer fixtures"
fi

if [ -n "$SPECSEP" ] && require_dir "$TP_SPECTRAL" && require_file "$TP_SPECTRAL/spec_1"; then
  profile_tool iccSpecSepToTiff "$SPECSEP" "$WORKDIR/specsep.tiff" 0 0 "$TP_SPECTRAL/spec_" 1 10 1
else
  skip_tool iccSpecSepToTiff "missing tool or spectral TIFF sequence"
fi

if [ -n "$PAWG" ] && require_file "$SRGB"; then
  profile_tool iccPawgReport "$PAWG" "$SRGB"
else
  skip_tool iccPawgReport "missing tool or sRGB profile"
fi

if [ -n "$VISUALIZE" ] && require_file "$SRGB"; then
  profile_tool iccProfileVisualize "$VISUALIZE" "$SRGB" "$WORKDIR/visualize"
else
  skip_tool iccProfileVisualize "missing tool or sRGB profile"
fi

if [ -n "$DESCRIBE" ]; then
  if require_file "$SRGB"; then
    profile_tool iccDescribeSinkTest "$DESCRIBE" "$SRGB"
  else
    profile_tool iccDescribeSinkTest "$DESCRIBE"
  fi
else
  skip_tool iccDescribeSinkTest "missing tool"
fi

write_dashboard

echo "[INFO] FlameGraph output: $OUTDIR"
find "$OUTDIR" -maxdepth 3 -type f | sort
echo "[PASS] all-tool FlameGraph profiling completed"
