#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 LOGDIR EXPECTED_MANIFEST ROUNDTRIP_DIR" >&2
  exit 2
fi

LOGDIR="$1"
MANIFEST="$2"
RTDIR="$3"

if [ ! -d "$LOGDIR" ]; then
  echo "[FAIL] fromxml log directory not found: $LOGDIR" >&2
  exit 1
fi

if [ ! -f "$MANIFEST" ]; then
  echo "[FAIL] expected-invalid manifest not found: $MANIFEST" >&2
  exit 1
fi

if [ ! -d "$RTDIR" ]; then
  echo "[FAIL] fromxml round-trip directory not found: $RTDIR" >&2
  exit 1
fi

classify_status() {
  local logf="$1"

  if grep -Fq "ERROR: AddressSanitizer" "$logf"; then
    printf '%s\n' "asan"
  elif grep -Fq "runtime error:" "$logf"; then
    printf '%s\n' "ubsan"
  elif grep -Fq "Profile parsed and saved correctly" "$logf"; then
    printf '%s\n' "valid-saved"
  elif grep -Fq "Profile parsed.  Profile is invalid" "$logf"; then
    printf '%s\n' "invalid-saved"
  elif grep -Eq "Unable to Parse|Unable to parse" "$logf"; then
    printf '%s\n' "parse-fail"
  else
    printf '%s\n' "unknown"
  fi
}

expected_reason=""

matches_manifest() {
  local base="$1"
  local status="$2"
  local logf="$3"
  local pattern expected needle reason

  expected_reason=""

  while IFS=$'\t' read -r pattern expected needle reason || [ -n "${pattern:-}" ]; do
    if [ -z "${pattern:-}" ] || [[ "$pattern" == \#* ]]; then
      continue
    fi

    # Manifest patterns are trusted shell globs maintained in this repository.
    # shellcheck disable=SC2053
    if [[ "$base" == $pattern && "$status" == "$expected" ]]; then
      if [ "$needle" = "-" ] || grep -Fq -- "$needle" "$logf"; then
        expected_reason="$reason"
        return 0
      fi
    fi
  done < "$MANIFEST"

  return 1
}

total=0
valid=0
expected_invalid=0
expected_parse_fail=0
unclassified=0
hard_fail=0

shopt -s nullglob
for logf in "$LOGDIR"/*.log; do
  total=$((total + 1))
  base="$(basename "$logf" .log)"
  status="$(classify_status "$logf")"
  iccout="$RTDIR/${base}_rt.icc"

  case "$status" in
    valid-saved)
      if [ -s "$iccout" ]; then
        valid=$((valid + 1))
      else
        unclassified=$((unclassified + 1))
        echo "  [UNCLASSIFIED_MISSING_OUTPUT] $base"
      fi
      ;;
    invalid-saved)
      if [ ! -s "$iccout" ]; then
        unclassified=$((unclassified + 1))
        echo "  [UNCLASSIFIED_MISSING_INVALID_OUTPUT] $base"
      elif matches_manifest "$base" "$status" "$logf"; then
        expected_invalid=$((expected_invalid + 1))
        echo "  [EXPECTED_INVALID] $base -- $expected_reason"
      else
        unclassified=$((unclassified + 1))
        echo "  [UNCLASSIFIED_INVALID] $base"
      fi
      ;;
    parse-fail)
      if [ -s "$iccout" ]; then
        unclassified=$((unclassified + 1))
        echo "  [UNCLASSIFIED_PARSE_WITH_OUTPUT] $base"
      elif matches_manifest "$base" "$status" "$logf"; then
        expected_parse_fail=$((expected_parse_fail + 1))
        echo "  [EXPECTED_PARSE_FAIL] $base -- $expected_reason"
      else
        unclassified=$((unclassified + 1))
        echo "  [UNCLASSIFIED_PARSE_FAIL] $base"
      fi
      ;;
    asan | ubsan)
      hard_fail=$((hard_fail + 1))
      echo "  [HARD_FAIL:$status] $base"
      ;;
    *)
      unclassified=$((unclassified + 1))
      echo "  [UNCLASSIFIED_STATUS] $base -- $status"
      ;;
  esac
done
shopt -u nullglob

echo ""
echo "iccFromXml classification: $total logs, $valid valid-saved, $expected_invalid expected-invalid-saved, $expected_parse_fail expected-parse-fail, $unclassified unclassified, $hard_fail hard-fail"

if [ "$total" -eq 0 ]; then
  echo "[FAIL] no iccFromXml logs found" >&2
  exit 1
fi

if [ "$unclassified" -gt 0 ] || [ "$hard_fail" -gt 0 ]; then
  echo "[FAIL] iccFromXml produced unclassified or hard failures" >&2
  exit 1
fi

echo "[OK] iccFromXml non-clean outputs are classified"
