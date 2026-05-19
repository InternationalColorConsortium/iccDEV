#!/bin/bash
###############################################################
#
# Copyright (c) 2025-2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
## Intent: Run CodeQL analysis locally and generate a report
#
## Prerequisites:
##   - gh codeql (GitHub CLI CodeQL extension)
##   - clang/clang++ available
##
## Usage:
##   .github/scripts/run-codeql-local.sh              # full analysis
##   .github/scripts/run-codeql-local.sh --custom-only # iccDEV core queries only
##   .github/scripts/run-codeql-local.sh --jsonlib-only # IccJSON/IccConnect queries only
##   .github/scripts/run-codeql-local.sh --skip-build  # reuse existing DB
##   .github/scripts/run-codeql-local.sh --help
##
## Output: codeql-results/ directory with SARIF files and report.txt
##
## Last Updated: 2026-04-09 by David Hoyt
#
###############################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DB_DIR="/tmp/codeql-db-iccdev"
BUILD_DIR="/tmp/codeql-build-iccdev"
RESULTS_DIR="$REPO_ROOT/codeql-results"

CUSTOM_ONLY=0
JSONLIB_ONLY=0
SKIP_BUILD=0
JOBS="$(nproc 2>/dev/null || echo 4)"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Run CodeQL security analysis on iccDEV and generate a report.

Options:
  --custom-only   Run only the custom iccDEV core security queries
  --jsonlib-only  Run only the IccJSON/IccConnect targeted security queries
  --standard-only Run only the standard cpp-security-and-quality suite
  --skip-build    Skip database creation (reuse existing at $DB_DIR)
  --db-dir DIR    Use custom database directory (default: $DB_DIR)
  --build-dir DIR Use custom CMake build directory (default: $BUILD_DIR)
  --jobs N        Parallel build jobs (default: $JOBS)
  --help          Show this help

Output:
  codeql-results/cpp-security-and-quality.sarif   Standard suite results
  codeql-results/iccdev-security.sarif            Custom query results
  codeql-results/iccdev-jsonlib-security.sarif     IccJSON/IccConnect query results
  codeql-results/report.txt                       Human-readable report

Build configuration:
  The CodeQL database is built out-of-tree with ICC_JSON_ORDERED=ON so CI and
  local analysis use deterministic JSON key ordering.

Examples:
  # Full analysis from scratch
  $(basename "$0")

  # Quick re-analysis after code change (rebuild DB, run all)
  $(basename "$0")

  # Re-run queries only (no rebuild)
  $(basename "$0") --skip-build

  # Custom core queries only, fast iteration
  $(basename "$0") --skip-build --custom-only

  # IccJSON/IccConnect queries only, fast iteration
  $(basename "$0") --skip-build --jsonlib-only
EOF
    exit 0
}

STANDARD_ONLY=0

while [ $# -gt 0 ]; do
    case "$1" in
        --custom-only)  CUSTOM_ONLY=1; shift ;;
        --jsonlib-only) JSONLIB_ONLY=1; shift ;;
        --standard-only) STANDARD_ONLY=1; shift ;;
        --skip-build)   SKIP_BUILD=1; shift ;;
        --db-dir)       DB_DIR="$2"; shift 2 ;;
        --build-dir)    BUILD_DIR="$2"; shift 2 ;;
        --jobs)         JOBS="$2"; shift 2 ;;
        --help|-h)      usage ;;
        *) echo "Unknown option: $1" >&2; usage ;;
    esac
done

if [ $((CUSTOM_ONLY + JSONLIB_ONLY + STANDARD_ONLY)) -gt 1 ]; then
    echo "[FAIL] Choose only one of --custom-only, --jsonlib-only, or --standard-only" >&2
    exit 1
fi

cd "$REPO_ROOT"

# Verify prerequisites
echo "=== Checking prerequisites ==="
if ! command -v gh >/dev/null 2>&1; then
    echo "[FAIL] gh CLI not found. Install: https://cli.github.com/" >&2
    exit 1
fi
if ! gh codeql version >/dev/null 2>&1; then
    echo "[FAIL] gh codeql extension not found. Install: gh extension install github/gh-codeql" >&2
    exit 1
fi
CODEQL_VERSION="$(gh codeql version 2>&1)"
CODEQL_VERSION="${CODEQL_VERSION%%$'\n'*}"
echo "[OK] gh codeql $CODEQL_VERSION"

if [ "$SKIP_BUILD" -eq 0 ]; then
    if [ ! -f "$REPO_ROOT/Build/Cmake/CMakeLists.txt" ]; then
        echo "[FAIL] Build/Cmake/CMakeLists.txt not found. Run from iccDEV root." >&2
        exit 1
    fi
fi

# Install pack dependencies
echo ""
echo "=== Installing CodeQL pack dependencies ==="
gh codeql pack install .github/codeql-queries/

# Build database
if [ "$SKIP_BUILD" -eq 0 ]; then
    echo ""
    echo "=== Building iccDEV and creating CodeQL database ==="

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cmake -S "$REPO_ROOT/Build/Cmake" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DENABLE_TOOLS=ON \
        -DICC_JSON_ORDERED=ON \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++

    gh codeql database create "$DB_DIR" \
        --language=cpp \
        --overwrite \
        --command="cmake --build $BUILD_DIR --clean-first -j $JOBS" \
        --source-root="."
    echo "[OK] Database created at $DB_DIR"
else
    if [ ! -d "$DB_DIR" ]; then
        echo "[FAIL] Database not found at $DB_DIR. Run without --skip-build first." >&2
        exit 1
    fi
    echo "[OK] Reusing existing database at $DB_DIR"
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

echo ""
echo "=== Running CodeQL analysis ==="

# Standard suite
if [ "$CUSTOM_ONLY" -eq 0 ] && [ "$JSONLIB_ONLY" -eq 0 ]; then
    echo "  Running standard cpp-security-and-quality suite..."
    gh codeql database analyze "$DB_DIR" \
        --format=sarif-latest \
        --output="$RESULTS_DIR/cpp-security-and-quality.sarif" \
        --threads=0 \
        codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls
    echo "[OK] Standard suite complete"
fi

# Custom core suite
if [ "$STANDARD_ONLY" -eq 0 ] && [ "$JSONLIB_ONLY" -eq 0 ]; then
    echo "  Running custom iccDEV security queries..."
    gh codeql database analyze "$DB_DIR" \
        --format=sarif-latest \
        --output="$RESULTS_DIR/iccdev-security.sarif" \
        --threads=0 \
        .github/codeql-queries/iccdev-security-suite.qls
    echo "[OK] Custom suite complete"
fi

# IccJSON/IccConnect suite
if [ "$STANDARD_ONLY" -eq 0 ] && [ "$CUSTOM_ONLY" -eq 0 ]; then
    echo "  Running IccJSON/IccConnect targeted security queries..."
    gh codeql database analyze "$DB_DIR" \
        --format=sarif-latest \
        --output="$RESULTS_DIR/iccdev-jsonlib-security.sarif" \
        --threads=0 \
        .github/codeql-queries/iccdev-jsonlib-suite.qls
    echo "[OK] IccJSON/IccConnect suite complete"
fi

# Generate report
echo ""
echo "=== Generating report ==="
SARIF_FILES=()
if [ -f "$RESULTS_DIR/cpp-security-and-quality.sarif" ]; then
    SARIF_FILES+=("$RESULTS_DIR/cpp-security-and-quality.sarif")
fi
if [ -f "$RESULTS_DIR/iccdev-security.sarif" ]; then
    SARIF_FILES+=("$RESULTS_DIR/iccdev-security.sarif")
fi
if [ -f "$RESULTS_DIR/iccdev-jsonlib-security.sarif" ]; then
    SARIF_FILES+=("$RESULTS_DIR/iccdev-jsonlib-security.sarif")
fi

if [ ${#SARIF_FILES[@]} -gt 0 ]; then
    "$SCRIPT_DIR/codeql-report.sh" "${SARIF_FILES[@]}" > "$RESULTS_DIR/report.txt"
    echo "[OK] Report saved to $RESULTS_DIR/report.txt"
else
    echo "[WARN] No SARIF files found to generate report"
fi

# Print summary
echo ""
echo "=== Results ==="
find "$RESULTS_DIR" -maxdepth 1 \( -name "*.sarif" -o -name "report.txt" \) -print | sort | xargs -r ls -lh
echo ""
echo "View report:  cat $RESULTS_DIR/report.txt"
echo "View SARIF:   python3 -m json.tool $RESULTS_DIR/iccdev-security.sarif | sed -n '1,100p'"
echo ""
echo "[OK] CodeQL analysis complete"
