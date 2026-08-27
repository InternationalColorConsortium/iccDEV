#!/usr/bin/env bash
#
# iccdev-qa-profile-manifest.sh - generate or enforce the QA profile manifest (#1809).
#
# The Testing corpus mixes three kinds of profile that a bare "did iccDumpProfile
# exit non-zero" sweep cannot tell apart: conformant examples, profiles that carry
# a known baselined warning, and fixtures that are malformed on purpose. Without a
# manifest, a deliberate rejection reads as an unexplained failure, and - worse - a
# real regression that turns a conformant profile critical is indistinguishable
# from the negatives that are supposed to be critical.
#
# Testing/qa-profile-manifest.tsv records, per profile, the validation verdict the
# corpus is expected to produce. This script has two modes:
#
#   check     re-validate every profile and diff the result against the manifest
#             (this is what the CTest runs)
#   generate  rewrite the manifest from the current corpus (maintainer action -
#             review the diff, it is a deliberate re-baseline)
#
# Both modes require the corpus to have been generated already (Testing/CreateAllProfiles.sh);
# under CTest that is the iccdev_profiles fixture.
set -uo pipefail

MODE="${1:-check}"
case "$MODE" in
  check|generate) ;;
  *) echo "usage: $0 {check|generate}" >&2; exit 2 ;;
esac

TESTING_DIR="${ICCDEV_TESTING_DIR:-$(cd "$(dirname "$0")/../../Testing" && pwd)}"
MANIFEST="${ICCDEV_QA_MANIFEST:-$TESTING_DIR/qa-profile-manifest.tsv}"

if [ ! -d "$TESTING_DIR" ]; then
  echo "[FAIL] Testing directory not found: $TESTING_DIR" >&2
  exit 1
fi

if ! command -v iccDumpProfile >/dev/null 2>&1; then
  echo "[FAIL] iccDumpProfile not on PATH" >&2
  exit 1
fi

# Validation is what the manifest describes, so -v is required. Verbosity 26 matches
# the sweep in #1809. The issue's own repro additionally passes "ALL" to dump every
# tag; that was measured to produce an identical status line and exit code for all
# 213 profiles, so the cheaper form is used here and tag-dump coverage is left to
# the tests that exist for it.
dump_verbosity=26

# Map the printed report line to the manifest's expected_status vocabulary. These
# are the exact strings iccDumpProfile.cpp prints for each icValidateStatus.
classify_status() {
  local log="$1"
  if   grep -Fq 'Profile has Critical Error(s)'      "$log"; then echo critical
  elif grep -Fq 'Profile violates ICC specification' "$log"; then echo noncompliant
  elif grep -Fq 'Profile has warning(s)'             "$log"; then echo warning
  elif grep -Fq 'Profile is valid'                   "$log"; then echo valid
  elif grep -Fq 'Profile has unknown status!'        "$log"; then echo unknown
  else echo none
  fi
}

# A sanitizer finding or a fatal signal is never baselined: those fail every suite,
# including the negative one, where the profile is meant to be *rejected* rather
# than to crash the validator.
classify_sanitizer() {
  local log="$1"
  if   grep -Fq 'ERROR: AddressSanitizer' "$log"; then echo asan
  elif grep -Fq 'UndefinedBehaviorSanitizer' "$log"; then echo ubsan
  elif grep -Fq 'runtime error:' "$log"; then echo ubsan
  elif grep -Eq 'DEADLYSIGNAL|Segmentation fault|Bus error|core dumped' "$log"; then echo signal
  else echo none
  fi
}

# Suite follows mechanically from the verdict, so the manifest stays regenerable
# and nobody has to hand-adjudicate 213 rows:
#   valid        -> positive       must stay clean; any diagnostic is a regression
#   warning      -> compatibility  a known diagnostic is baselined, escalation fails
#   critical     -> negative       rejection is the expected result; acceptance fails
classify_suite() {
  case "$1" in
    valid)                   echo positive ;;
    warning)                 echo compatibility ;;
    noncompliant|critical)   echo negative ;;
    *)                       echo unknown ;;
  esac
}

# One short, stable reason per diagnostic family, so a reviewer reading the manifest
# can see why a row is baselined without re-running the tool.
classify_rationale() {
  local status="$1" log="$2"
  case "$status" in
    valid)
      echo "conformant example profile; validates clean"
      ;;
    warning)
      # Matches CIccInfo::CheckLuminance's message text. #1811 reworded it from
      # "appears to be normalized!" to "possibly normalized" because a normalized Y
      # and a physical 1 cd/m^2 Y are indistinguishable, so the tool no longer
      # asserts what it cannot prove. The rationale string below is deliberately
      # unchanged: the 27 rows it labels are the same 27 profiles, so the manifest
      # itself does not need regenerating for a wording change.
      if grep -Fq 'possibly normalized' "$log"; then
        echo "baselined: spectralViewingConditions XYZ carries normalized luminance (#1811)"
      elif grep -Fq 'sampled curve has a range of zero' "$log"; then
        echo "baselined: sampled curve has a range of zero"
      else
        echo "baselined: profile validates with warnings"
      fi
      ;;
    critical)
      # Header damage is checked first because it is the broader defect: the one
      # fuzz-derived fixture in this corpus reports a truncated header *and* a
      # calculator underflow, and describing it by the underflow alone would put it
      # in the same family as the 77 hand-authored calculator negatives, which are
      # structurally well-formed profiles that fail only on calculator semantics.
      if grep -Eq 'Bad Header File Size|Bad Profile ID' "$log"; then
        echo "negative fixture: fuzz-derived malformed profile must be rejected"
      elif grep -Fq 'causes an evaluation stack underflow' "$log"; then
        echo "negative fixture: calculator evaluation stack underflow must be rejected"
      elif grep -Fq 'accesses illegal temporary channels' "$log"; then
        echo "negative fixture: illegal temporary channel access must be rejected"
      elif grep -Fq 'function has invalid operations' "$log"; then
        # calcUnderStack_fLab is the one member of the calcUnderStack_* family that
        # reports this instead of an underflow: the operation is rejected as invalid
        # before the stack depth is ever assessed. Recorded as its own reason so the
        # manifest does not imply it exercises the same validator path as its 73 siblings.
        echo "negative fixture: invalid calculator operation must be rejected"
      else
        echo "negative fixture: malformed profile must be rejected"
      fi
      ;;
    *)
      echo "unclassified"
      ;;
  esac
}

# sha256 is populated only for profiles git actually tracks (#1809 resolution path 1).
# The generated corpus cannot carry a stable digest: almost every source XML sets
# <CreationDateTime>now</CreationDateTime>, which icGetDateTimeValue resolves through
# localtime_r at conversion time, and the header date feeds the recomputed profile ID.
# Two conversions a second apart differ, and two machines in different time zones
# differ as well. For those rows expected_status/expected_exit carry the contract.
# The digest is read from the git object rather than from disk so that regenerating
# the corpus in place can never silently re-baseline it.
git_blob_sha256() {
  local rel="$1"
  if git -C "$REPO_ROOT" ls-files --error-unmatch "$rel" >/dev/null 2>&1; then
    git -C "$REPO_ROOT" show "HEAD:$rel" 2>/dev/null | sha256sum | cut -d' ' -f1
  else
    echo "-"
  fi
}

REPO_ROOT="$(git -C "$TESTING_DIR" rev-parse --show-toplevel 2>/dev/null || echo "")"

# The corpus this manifest describes is what CreateAllProfiles.sh produces plus the
# profiles git tracks. Testing/hybrid/ICC and Testing/hybrid/Results are excluded
# because they are not corpus at all: iccdev.hybrid-pipeline writes its own output
# there while the suite runs, and Testing/hybrid/.gitignore - a committed file - says
# so, listing "ICC/*.icc" and "Results/*.icc" as generated artifacts. Testing/HDR is
# likewise produced by HDR/mkprofiles.sh for the hybrid pipeline, not by
# CreateAllProfiles.sh. Without these exclusions the manifest test is
# ordering-dependent, and the difference is not something a verdict baseline should
# adjudicate.
list_corpus() {
  find "$TESTING_DIR" -name '*.icc' \
    ! -path "$TESTING_DIR/HDR/*" \
    ! -path "$TESTING_DIR/hybrid/ICC/*" \
    ! -path "$TESTING_DIR/hybrid/Results/*" \
    | sort
}

write_header() {
  cat <<'EOF'
# Testing/qa-profile-manifest.tsv - expected validation verdict per corpus profile (#1809)
#
# Regenerate with:  .github/scripts/iccdev-qa-profile-manifest.sh generate
# Enforced by CTest: iccdev.qa-profile-manifest
#
# Columns (tab separated):
#   path                repository path, relative to Testing/
#   suite               positive | compatibility | negative
#   expected_status     valid | warning | noncompliant | critical
#   expected_exit       decimal exit code from iccDumpProfile -v
#   expected_sanitizer  none  (a sanitizer finding fails every suite, negatives included)
#   source              tracked | generated
#   sha256              digest of the tracked git blob, or - when generated
#   rationale           why this row reads the way it does
#
# Suite policy:
#   positive       must validate clean; any new diagnostic is a regression
#   compatibility  a known diagnostic is baselined; escalation beyond it fails
#   negative       rejection is the expected result; acceptance is a failure
#
# iccDumpProfile status-to-exit mapping (Tools/CmdLine/IccDumpProfile/iccDumpProfile.cpp).
# Note that the text verdict and the exit code deliberately disagree for one status,
# so a harness reading $? and a harness reading the report do not see the same thing:
#   valid         "Profile is valid"                            exit 0
#   warning       "Profile has warning(s)"                      exit 0
#   noncompliant  "Profile violates ICC specification"          exit 0    <-- asymmetry
#   critical      "Profile has Critical Error(s) ..."           exit 255
#   unknown       "Profile has unknown status!"                 exit 254
# The manifest therefore records expected_status and expected_exit as independent
# fields rather than deriving one from the other. No profile in the corpus currently
# validates as overall noncompliant, so no row exercises that pairing today; the
# mapping is recorded here so the contract is explicit if one appears.
#
# sha256 is populated only for git-tracked profiles. The generated corpus resolves
# <CreationDateTime>now</CreationDateTime> through localtime_r at conversion time and
# recomputes the profile ID from it, so its bytes are neither clock- nor timezone-stable.
#
# Scope: this manifest covers Testing/ only. The repository also tracks 24 profiles
# under .github/ci/regression/ and .github/ci/test-data/ - deliberately malformed PoC
# and fuzz fixtures - which are deliberately NOT listed here. Each is already the
# input to a dedicated regression test that asserts a specific behaviour, and those
# assertions are narrower than a whole-profile validation verdict. Extending the
# manifest to cover them would be a separate change.
EOF
  printf '#\n'
  printf '# path\tsuite\texpected_status\texpected_exit\texpected_sanitizer\tsource\tsha256\trationale\n'
}

tmp_log="$(mktemp)"
trap 'rm -f "$tmp_log"' EXIT

emit_rows() {
  while IFS= read -r f; do
    local_rel="${f#"$TESTING_DIR"/}"

    timeout -k 5s 120s iccDumpProfile -v "$dump_verbosity" "$f" > "$tmp_log" 2>&1
    rc=$?

    status="$(classify_status "$tmp_log")"
    san="$(classify_sanitizer "$tmp_log")"
    suite="$(classify_suite "$status")"
    rationale="$(classify_rationale "$status" "$tmp_log")"

    repo_rel="Testing/$local_rel"
    if [ -n "$REPO_ROOT" ] && git -C "$REPO_ROOT" ls-files --error-unmatch "$repo_rel" >/dev/null 2>&1; then
      source_kind="tracked"
      sha="$(git_blob_sha256 "$repo_rel")"
    else
      source_kind="generated"
      sha="-"
    fi

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$local_rel" "$suite" "$status" "$rc" "$san" "$source_kind" "$sha" "$rationale"
  done < <(list_corpus)
}

if [ "$MODE" = "generate" ]; then
  {
    write_header
    emit_rows
  } > "$MANIFEST.new"
  mv "$MANIFEST.new" "$MANIFEST"
  rows=$(grep -cv '^#' "$MANIFEST")
  echo "[OK] wrote $MANIFEST ($rows profiles)"
  exit 0
fi

# ---- check mode ----

if [ ! -f "$MANIFEST" ]; then
  echo "[FAIL] manifest not found: $MANIFEST" >&2
  exit 1
fi

# Measure the corpus into a temp table first, then let awk join it against the
# manifest. The lookup deliberately lives in awk rather than in a bash associative
# array: macOS still ships bash 3.2, where "declare -A" is a syntax error, and
# nothing else under .github/scripts requires bash 4.
measured="$(mktemp)"
trap 'rm -f "$tmp_log" "$measured"' EXIT

list_corpus | while IFS= read -r f; do
  rel="${f#"$TESTING_DIR"/}"

  timeout -k 5s 120s iccDumpProfile -v "$dump_verbosity" "$f" > "$tmp_log" 2>&1
  rc=$?

  # Recomputed here rather than trusted from the manifest, so a tracked fixture
  # edited under a stale baseline is caught rather than confirmed.
  if [ -n "$REPO_ROOT" ] && git -C "$REPO_ROOT" ls-files --error-unmatch "Testing/$rel" >/dev/null 2>&1; then
    sha="$(git_blob_sha256 "Testing/$rel")"
  else
    sha="-"
  fi

  printf '%s\t%s\t%s\t%s\t%s\n' \
    "$rel" "$(classify_status "$tmp_log")" "$rc" "$(classify_sanitizer "$tmp_log")" "$sha"
done > "$measured"

awk -F'\t' '
  # Pass 1: the manifest.
  FNR == NR {
    if ($0 ~ /^#/ || $1 == "") next
    m_suite[$1]  = $2
    m_status[$1] = $3
    m_exit[$1]   = $4
    m_san[$1]    = $5
    m_source[$1] = $6
    m_sha[$1]    = $7
    rows++
    next
  }

  # Pass 2: what the corpus actually did.
  {
    path = $1; status = $2; rc = $3; san = $4; sha = $5
    total++

    if (!(path in m_status)) {
      unlisted++
      printf "  [UNLISTED] %s -- present in the corpus but absent from the manifest\n", path
      next
    }
    seen[path] = 1

    row_fail = 0
    if (status != m_status[path]) {
      printf "  [STATUS] %s -- expected %s, got %s (suite %s)\n", path, m_status[path], status, m_suite[path]
      row_fail = 1
    }
    if (rc != m_exit[path]) {
      printf "  [EXIT] %s -- expected exit %s, got %s\n", path, m_exit[path], rc
      row_fail = 1
    }
    # A sanitizer finding fails regardless of suite. A negative fixture is expected
    # to be rejected cleanly, not to trip the sanitizer on the way.
    if (san != m_san[path]) {
      printf "  [SANITIZER] %s -- expected %s, got %s\n", path, m_san[path], san
      row_fail = 1
    }
    # Only tracked rows carry a digest; a mismatch means the fixture changed under a
    # baseline that still claims to describe it.
    if (m_source[path] == "tracked" && m_sha[path] != "-" && sha != "-" && sha != m_sha[path]) {
      printf "  [SHA256] %s -- tracked fixture changed; manifest baseline is stale\n", path
      row_fail = 1
    }

    if (row_fail) fail++; else ok++
  }

  END {
    # A manifest row whose profile is gone is as much a drift signal as a new
    # profile that nothing describes.
    for (path in m_status) {
      if (!(path in seen)) {
        missing++
        printf "  [MISSING] %s -- listed in the manifest but not present in the corpus\n", path
      }
    }

    printf "\n"
    printf "qa-profile-manifest: %d profiles, %d matched, %d mismatched, %d unlisted, %d missing (%d manifest rows)\n",
           total, ok, fail, unlisted, missing, rows

    if (total == 0) {
      print "[FAIL] no profiles found - was CreateAllProfiles.sh run?" > "/dev/stderr"
      exit 1
    }
    if (fail > 0 || unlisted > 0 || missing > 0) {
      print "[FAIL] corpus validation verdicts do not match the manifest" > "/dev/stderr"
      print "       If this is an intended change, re-baseline with:" > "/dev/stderr"
      print "       .github/scripts/iccdev-qa-profile-manifest.sh generate" > "/dev/stderr"
      exit 1
    }
    print "[OK] every corpus profile matches its manifest verdict"
  }
' "$MANIFEST" "$measured"
