#!/usr/bin/env bash
###############################################################################
# Build/Cmake/wasm-package/stage.sh
#
# Stage Emscripten WASM build outputs into an npm-shaped bundle.
#
# Inputs:
#   $1 = build directory containing built WASM tools (e.g. build-wasm)
#   $2 = output staging directory (e.g. wasm-stage)
#   $3 = (optional) version string for package.json (default: 0.0.0-dev)
#
# Output layout (mirrors npm package "iccdev"):
#   <stage>/
#     index.js
#     test_all.js
#     test.icc                (if present alongside this script)
#     README.md
#     LICENSE
#     package.json
#     IccDumpProfile/iccDumpProfile.{js,wasm}
#     IccToXml/iccToXml.{js,wasm}
#     ... (16 tool dirs)
#     IccProfLib/*.a
#     IccXML/*.a
#     IccJSON/*.a
#     Testing/                (full upstream Testing tree)
###############################################################################
set -euo pipefail

BUILD_DIR="${1:?usage: stage.sh <build-dir> <stage-dir> [version]}"
STAGE_DIR="${2:?usage: stage.sh <build-dir> <stage-dir> [version]}"
VERSION="${3:-0.0.0-dev}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "::error::stage.sh: build dir '$BUILD_DIR' not found" >&2
    exit 1
fi

mkdir -p "$STAGE_DIR"

# 16 tools: dir name -> built js basename (camelCase)
TOOLS=(
    "IccDumpProfile:iccDumpProfile"
    "IccToXml:iccToXml"
    "IccFromXml:iccFromXml"
    "IccToJson:iccToJson"
    "IccFromJson:iccFromJson"
    "IccRoundTrip:iccRoundTrip"
    "IccFromCube:iccFromCube"
    "IccApplyNamedCmm:iccApplyNamedCmm"
    "IccApplyProfiles:iccApplyProfiles"
    "IccApplySearch:iccApplySearch"
    "IccApplyToLink:iccApplyToLink"
    "IccTiffDump:iccTiffDump"
    "IccJpegDump:iccJpegDump"
    "IccPngDump:iccPngDump"
    "IccSpecSepToTiff:iccSpecSepToTiff"
    "IccV5DspObsToV4Dsp:iccV5DspObsToV4Dsp"
)

STAGED=0
MISSING=0
for entry in "${TOOLS[@]}"; do
    dir="${entry%%:*}"
    base="${entry##*:}"
    js_src="$(find "$BUILD_DIR" -type f -name "${base}.js" -not -path '*/CMakeFiles/*' | head -1 || true)"
    wasm_src="$(find "$BUILD_DIR" -type f -name "${base}.wasm" -not -path '*/CMakeFiles/*' | head -1 || true)"
    if [[ -n "$js_src" && -n "$wasm_src" ]]; then
        mkdir -p "$STAGE_DIR/$dir"
        cp -f "$js_src"   "$STAGE_DIR/$dir/${base}.js"
        cp -f "$wasm_src" "$STAGE_DIR/$dir/${base}.wasm"
        STAGED=$((STAGED + 1))
    else
        echo "::warning::stage.sh: missing artifacts for $dir ($base)" >&2
        MISSING=$((MISSING + 1))
    fi
done

# Static libs (best effort - WASM .a files)
for libdir in IccProfLib IccXML IccJSON; do
    src="$BUILD_DIR/$libdir"
    if [[ -d "$src" ]]; then
        mkdir -p "$STAGE_DIR/$libdir"
        find "$src" -maxdepth 1 -type f -name '*.a' -exec cp -f {} "$STAGE_DIR/$libdir/" \;
    fi
done

# Wrapper files
cp -f "$SCRIPT_DIR/index.js"     "$STAGE_DIR/index.js"
cp -f "$SCRIPT_DIR/test_all.js"  "$STAGE_DIR/test_all.js"
cp -f "$SCRIPT_DIR/README.md"    "$STAGE_DIR/README.md"

# Templated package.json
sed "s/@WASM_PKG_VERSION@/${VERSION}/g" "$SCRIPT_DIR/package.json.in" > "$STAGE_DIR/package.json"

# LICENSE
if [[ -f "$REPO_ROOT/LICENSE.md" ]]; then
    cp -f "$REPO_ROOT/LICENSE.md" "$STAGE_DIR/LICENSE"
elif [[ -f "$REPO_ROOT/LICENSE" ]]; then
    cp -f "$REPO_ROOT/LICENSE"    "$STAGE_DIR/LICENSE"
fi

# Optional fallback test profile (if maintainer ships one alongside this script)
if [[ -f "$SCRIPT_DIR/test.icc" ]]; then
    cp -f "$SCRIPT_DIR/test.icc" "$STAGE_DIR/test.icc"
fi

# Testing/ tree - full upstream tree (XML sources + driver scripts).
# Driver scripts (CreateAllProfiles.sh, RunTests.sh, etc) drive the
# regression test (regression.js) via the WASM shims under PATH.
if [[ -d "$REPO_ROOT/Testing" ]]; then
    mkdir -p "$STAGE_DIR/Testing"
    cp -a "$REPO_ROOT/Testing/." "$STAGE_DIR/Testing/"
fi

# Wrapper files (post-Testing so regression.js is alongside)
cp -f "$SCRIPT_DIR/regression.js" "$STAGE_DIR/regression.js"

echo ""
echo "=== WASM stage summary ==="
echo "  staged tools:  $STAGED / 16"
echo "  missing tools: $MISSING"
echo "  stage dir:     $STAGE_DIR"
echo "  version:       $VERSION"
echo ""
echo "Stage contents (top level):"
ls -lh "$STAGE_DIR" | sed -n '2,40p'

if [[ $STAGED -lt 16 ]]; then
    echo "::warning::stage.sh: only $STAGED of 16 tools staged" >&2
fi
