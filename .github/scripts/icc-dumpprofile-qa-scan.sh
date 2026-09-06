#!/usr/bin/env bash
# Scan ICC files with iccDumpProfile across dump, validation, tag, and read modes.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

export ICC_QA_TOOL_NAME="iccDumpProfile"
export ICC_QA_TOOL="${ICC_DUMP_PROFILE-}"
export ICC_QA_TOOL_EXPLICIT="${ICC_DUMP_PROFILE+x}"
export ICC_QA_TOOL_SUBDIR="IccDumpProfile"
export ICC_QA_TOOL_USAGE="iccDumpProfile {-v} {verbosity_int} {--diag} {--read} profile.icc {tagId|ALL}"
export ICC_QA_OUT_PREFIX="dumpprofile-qa"
export ICC_QA_TIMEOUT_DEFAULT="${ICC_DUMP_QA_TIMEOUT:-30}"
export ICC_QA_VARIANTS='basic||
validate|-v|
verbosity-25|25|
tag-desc||desc
all||ALL
validate-all|-v|ALL
validate-100-all|-v 100|ALL
read-all|--read|ALL
diag-read-all|--diag --read|ALL'

# shellcheck source=.github/scripts/icc-tool-qa-scan-common.sh
# shellcheck disable=SC1091
source "$SCRIPT_DIR/icc-tool-qa-scan-common.sh"
