#!/usr/bin/env bash
# Scan ICC files with iccPawgReport across text, JSON, and eager-read modes.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

export ICC_QA_TOOL_NAME="iccPawgReport"
export ICC_QA_TOOL="${ICC_PAWG_REPORT:-$REPO_ROOT/Build/Tools/IccPawgReport/iccPawgReport}"
export ICC_QA_TOOL_USAGE="iccPawgReport [--json] profile.icc"
export ICC_QA_OUT_PREFIX="pawg-qa"
export ICC_QA_TIMEOUT_DEFAULT="${ICC_PAWG_QA_TIMEOUT:-60}"
# The read/json-read variants drove the --read option, retired in #1977 because it
# could never recover a profile the strict parse rejected. They are not replaced:
# both ran the same code path as their non---read counterparts above.
export ICC_QA_VARIANTS='text||
json|--json|'

# shellcheck source=.github/scripts/icc-tool-qa-scan-common.sh
source "$SCRIPT_DIR/icc-tool-qa-scan-common.sh"
