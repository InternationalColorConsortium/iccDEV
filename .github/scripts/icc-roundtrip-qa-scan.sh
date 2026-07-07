#!/usr/bin/env bash
# Scan ICC files with iccRoundTrip across intents and LUT/MPE modes.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

export ICC_QA_TOOL_NAME="iccRoundTrip"
export ICC_QA_TOOL="${ICC_ROUND_TRIP:-$REPO_ROOT/Build/Tools/IccRoundTrip/iccRoundTrip}"
export ICC_QA_TOOL_USAGE="iccRoundTrip profile.icc {rendering_intent=1 {use_mpe=0}}"
export ICC_QA_OUT_PREFIX="roundtrip-qa"
export ICC_QA_TIMEOUT_DEFAULT="${ICC_ROUNDTRIP_QA_TIMEOUT:-30}"
export ICC_QA_VARIANTS='intent-0||0
intent-1||1
intent-2||2
intent-3||3
intent-0-mpe||0 1
intent-1-mpe||1 1
intent-2-mpe||2 1
intent-3-mpe||3 1'

# shellcheck source=.github/scripts/icc-tool-qa-scan-common.sh
source "$SCRIPT_DIR/icc-tool-qa-scan-common.sh"
