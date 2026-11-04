#!/bin/bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Validate exact-name ClusterFuzzLite corpus artifact refresh behavior.
###############################################################################

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
refresh_script="$script_dir/iccdev-cfl-artifact-refresh.sh"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/iccdev-cfl-refresh-test.XXXXXX")"
trap 'rm -rf -- "$test_dir"' EXIT

make_artifact_zip()
{
  local artifact_name="$1"
  local seed_name="$2"
  local fixture_dir="$3"
  local zip_path="$4"

  mkdir -p "$fixture_dir/corpus" "$fixture_dir/zip"
  printf 'seed\n' > "$fixture_dir/corpus/$seed_name"
  tar -cf "$fixture_dir/zip/$artifact_name.tar" \
    -C "$fixture_dir/corpus" .
  (
    cd "$fixture_dir/zip"
    cmake -E tar cf "$zip_path" --format=zip "$artifact_name.tar"
  )
}

profile_name=cifuzz-corpus-icc_profileparse_fuzzer
visual_name=cifuzz-corpus-icc_profilevisualize_fuzzer
make_artifact_zip "$profile_name" profile-seed \
  "$test_dir/profile-fixture" "$test_dir/profile.zip"
make_artifact_zip "$visual_name" visual-seed \
  "$test_dir/visual-fixture" "$test_dir/visual.zip"
mkdir -p "$test_dir/invalid-zip"
printf 'invalid\n' > "$test_dir/invalid-zip/wrong.tar"
(
  cd "$test_dir/invalid-zip"
  cmake -E tar cf "$test_dir/invalid.zip" --format=zip wrong.tar
)

mock_bin="$test_dir/bin"
mkdir -p "$mock_bin"
cat > "$mock_bin/gh" <<'MOCK'
#!/bin/bash
set -euo pipefail

if [ "${1:-}" != "api" ]; then
  exit 2
fi
case "${2:-}" in
  *'name=cifuzz-corpus-icc_profileparse_fuzzer&per_page=100')
    printf '2026-11-03T00:00:00Z\t101\n'
    ;;
  *'name=cifuzz-corpus-icc_cmmapply_fuzzer&per_page=100')
    ;;
  *'name=cifuzz-corpus-icc_profilevisualize_fuzzer&per_page=100')
    printf '2026-11-03T02:00:00Z\t201\n'
    printf '2026-11-03T01:00:00Z\t202\n'
    ;;
  */actions/artifacts/101/zip)
    cat "$MOCK_PROFILE_ZIP"
    ;;
  */actions/artifacts/201/zip)
    cat "$MOCK_INVALID_ZIP"
    ;;
  */actions/artifacts/202/zip)
    cat "$MOCK_VISUAL_ZIP"
    ;;
  *)
    echo "Unexpected mock gh request: $*" >&2
    exit 2
    ;;
esac
MOCK
chmod +x "$mock_bin/gh"

export GH_TOKEN=test-token
export MOCK_PROFILE_ZIP="$test_dir/profile.zip"
export MOCK_INVALID_ZIP="$test_dir/invalid.zip"
export MOCK_VISUAL_ZIP="$test_dir/visual.zip"
export PATH="$mock_bin:$PATH"

mkdir -p "$test_dir/no-tools"
if PATH="$test_dir/no-tools" "$refresh_script" --prepare owner/repo \
  "$test_dir/missing-unzip" icc_profileparse_fuzzer \
  >"$test_dir/missing-unzip.log" 2>&1; then
  echo "[FAIL] Artifact refresh accepted a missing unzip dependency" >&2
  exit 1
fi
grep -Fq '[FAIL] unzip is required for corpus artifact refresh' \
  "$test_dir/missing-unzip.log"

output_dir="$test_dir/output"
refresh_output="$("$refresh_script" --prepare owner/repo "$output_dir" \
  icc_profileparse_fuzzer \
  icc_cmmapply_fuzzer \
  icc_profilevisualize_fuzzer 2>&1)"

tar -tf "$output_dir/$profile_name.tar" | grep -Fq './profile-seed'
test -z "$(tar -tf "$output_dir/cifuzz-corpus-icc_cmmapply_fuzzer.tar")"
tar -tf "$output_dir/$visual_name.tar" | grep -Fq './visual-seed'
grep -Fq "Refreshed corpus artifact name=$profile_name id=101" \
  <<< "$refresh_output"
grep -Fq 'Created empty cold-start corpus artifact name=cifuzz-corpus-icc_cmmapply_fuzzer' \
  <<< "$refresh_output"
grep -Fq "Unexpected artifact layout for $visual_name id=201" \
  <<< "$refresh_output"
grep -Fq "Refreshed corpus artifact name=$visual_name id=202" \
  <<< "$refresh_output"

if "$refresh_script" --prepare owner/repo "$test_dir/rejected" \
  icc_not_a_real_fuzzer >/dev/null 2>&1; then
  echo "[FAIL] Artifact refresh accepted an unknown target" >&2
  exit 1
fi

echo "[PASS] ClusterFuzzLite artifact refresh contract"
