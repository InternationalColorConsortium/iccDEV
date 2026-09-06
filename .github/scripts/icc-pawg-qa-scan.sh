#!/usr/bin/env bash
# Scan ICC files with iccPawgReport across text, JSON, and eager-read modes.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

export ICC_QA_TOOL_NAME="iccPawgReport"
export ICC_QA_TOOL="${ICC_PAWG_REPORT-}"
export ICC_QA_TOOL_EXPLICIT="${ICC_PAWG_REPORT+x}"
export ICC_QA_TOOL_SUBDIR="IccPawgReport"
export ICC_QA_TOOL_USAGE="iccPawgReport [--json] profile.icc"
export ICC_QA_OUT_PREFIX="pawg-qa"
export ICC_QA_TIMEOUT_DEFAULT="${ICC_PAWG_QA_TIMEOUT:-60}"
# The read/json-read variants drove the --read option, retired in #1977 because it
# could never recover a profile the strict parse rejected. They are not replaced:
# both ran the same code path as their non---read counterparts above.
export ICC_QA_VARIANTS='text||
json|--json|'

# The source= directive records the path for anyone running shellcheck -x; without
# -x, which is how the pre-flight gate invokes it, shellcheck still reports SC1091
# for the unfollowed file, so suppress it here as well. Pre-existing, and surfaced
# only because the gate lints changed files and this is the first change to touch
# this one -- the two sibling icc-*-qa-scan.sh scripts carry the same latent finding.
# shellcheck source=.github/scripts/icc-tool-qa-scan-common.sh
# shellcheck disable=SC1091
source "$SCRIPT_DIR/icc-tool-qa-scan-common.sh"
