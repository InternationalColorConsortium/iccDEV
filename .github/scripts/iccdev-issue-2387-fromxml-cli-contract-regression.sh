#!/bin/bash
###############################################################################
# iccFromXml CLI-contract regression (#2387)
###############################################################################
#
# Four defects in IccFromXml.cpp's argument handling, all reported against the
# same tool and all of them silent -- each one exits 0 while doing something
# other than what it was asked.
#
#   1. -noid preserved an existing profile ID.  CIccProfile::Write() emits
#      m_Header.profileID unconditionally; icNeverWriteID only suppresses the
#      RECALCULATION and rewrite at offset 84.  So a document already carrying a
#      <ProfileID> -- which is what iccToXml emits for any v4 profile -- kept its
#      original ID straight through `iccFromXml ... -noid`.
#   2. Bare -v failed OPEN.  The lookup left the schema string empty when it
#      found nothing, and LoadXml validates only against a non-empty string, so
#      -v was behaviourally identical to passing nothing.  The lookup also only
#      ever derived the path from argv[0] while the README documents the CURRENT
#      directory, so the documented invocation never found the schema at all.
#   3. A missing output path returned success.  `iccFromXml foo.xml` printed
#      usage and exited 0, so automation saw success for a run that converted
#      nothing.
#   4. Unknown options were ignored in silence.  An ordinary "-no-id" typo left
#      bNoId false, wrote a profile with an ID, and still exited 0.
#
# The help/error split follows the ruling already applied to iccSpecSepToTiff for
# this same defect class (#1514): a help request prints to stdout and succeeds, a
# malformed invocation prints to stderr and fails.  The STREAM is what separates
# them -- a shared stdout cannot express the difference whatever the status says.
# The status here is 1 rather than #1514's 255 because that is what every other
# error exit in this tool's main() already returns.
#
# Every rejection case is paired with a control that must still SUCCEED, because
# a change that refused everything would satisfy the rejections on its own.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-fromxml-cli-contract}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi
TOOLS_DIR="$(cd "$TOOLS_DIR" 2>/dev/null && pwd -P)"

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
TOXML="$TOOLS_DIR/IccToXml/iccToXml"

WORKDIR="$OUTDIR/fromxml-cli-contract"
STDOUT_LOG="$OUTDIR/fromxml-cli.stdout.log"
STDERR_LOG="$OUTDIR/fromxml-cli.stderr.log"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

pass_case() { PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1)); echo "  [PASS] $1 -- $2"; }
fail_case() { FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1)); echo "  [FAIL] $1 -- $2"; }
skip_case() { SKIP=$((SKIP + 1)); TOTAL=$((TOTAL + 1)); echo "  [SKIP] $1 -- $2"; }

check_sanitizers() {
  local name="$1" log="$2"
  if grep -Eq "ERROR: AddressSanitizer|LeakSanitizer: detected memory leaks|runtime error:" "$log" 2>/dev/null; then
    fail_case "$name" "sanitizer finding in tool output"
    return 1
  fi
  return 0
}

if [ ! -x "$FROMXML" ]; then
  echo "=== iccFromXml CLI-contract regression (#2387) ==="
  skip_case "fromxml-cli-contract" "iccFromXml not built at $FROMXML"
  echo ""
  echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
  exit 77
fi

# A minimal but complete v4 profile, so iccFromXml writes a real profile ID for it
# and the -noid case below has something to suppress.
cat > "$WORKDIR/base.xml" <<'XMLEOF'
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <ProfileVersion>4.30</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
    <RenderingIntent>Perceptual</RenderingIntent>
  </Header>
  <Tags>
    <grayTRCTag> <curveType><Curve>563</Curve></curveType> </grayTRCTag>
    <profileDescriptionTag> <textDescriptionType><TextData>2387 CLI contract fixture</TextData></textDescriptionType> </profileDescriptionTag>
    <copyrightTag> <textType><TextData>ICC regression fixture</TextData></textType> </copyrightTag>
    <mediaWhitePointTag> <XYZArrayType><XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/></XYZArrayType> </mediaWhitePointTag>
  </Tags>
</IccProfile>
XMLEOF

# Permissive RelaxNG: accepts any well-formed document.  Used as the "validation
# still works" control, so a fix that simply refused every -v cannot pass.
cat > "$WORKDIR/permissive.rng" <<'RNGEOF'
<?xml version="1.0" encoding="UTF-8"?>
<grammar xmlns="http://relaxng.org/ns/structure/1.0">
  <start><ref name="anyElement"/></start>
  <define name="anyElement">
    <element><anyName/>
      <zeroOrMore>
        <choice>
          <attribute><anyName/></attribute>
          <text/>
          <ref name="anyElement"/>
        </choice>
      </zeroOrMore>
    </element>
  </define>
</grammar>
RNGEOF

# Reject-all RelaxNG.  This is what proves the schema is APPLIED rather than merely
# located: a fix that found the file and then ignored it would pass every other
# -v case here.
cat > "$WORKDIR/reject.rng" <<'RNGEOF'
<?xml version="1.0" encoding="UTF-8"?>
<element name="ThisWillNeverMatch" xmlns="http://relaxng.org/ns/structure/1.0"><empty/></element>
RNGEOF

# run <name> <expected-exit> <stream: out|err> <description> -- <args...>
# Asserts the status AND which stream carried the output AND that the other one is
# empty.  Exit status alone cannot separate a help request from a malformed
# invocation once both are non-silent, which is the distinction #1514 established.
run_case() {
  local name="$1" want_exit="$2" want_stream="$3" desc="$4"
  shift 5   # drop the "--" too
  local exit_code=0

  ( cd "$WORKDIR" && timeout 60 "$FROMXML" "$@" ) > "$STDOUT_LOG" 2> "$STDERR_LOG" || exit_code=$?

  check_sanitizers "$name" "$STDOUT_LOG" || return
  check_sanitizers "$name" "$STDERR_LOG" || return

  if [ "$exit_code" -ge 128 ]; then
    fail_case "$name" "iccFromXml died on a signal (exit $exit_code)"
    return
  fi
  if [ "$exit_code" -ne "$want_exit" ]; then
    fail_case "$name" "expected exit=$want_exit, got exit=$exit_code ($desc)"
    sed -n '1,6p' "$STDERR_LOG"
    return
  fi

  if [ "$want_stream" = "err" ]; then
    if [ ! -s "$STDERR_LOG" ]; then
      fail_case "$name" "expected a diagnostic on stderr, got none"
      return
    fi
    if [ -s "$STDOUT_LOG" ]; then
      fail_case "$name" "a failed invocation wrote to stdout"
      sed -n '1,6p' "$STDOUT_LOG"
      return
    fi
  elif [ "$want_stream" = "out" ]; then
    if [ ! -s "$STDOUT_LOG" ]; then
      fail_case "$name" "expected output on stdout, got none"
      return
    fi
    if [ -s "$STDERR_LOG" ]; then
      fail_case "$name" "a successful invocation wrote to stderr"
      sed -n '1,6p' "$STDERR_LOG"
      return
    fi
  fi

  pass_case "$name" "$desc"
}

echo "=== iccFromXml CLI-contract regression (#2387) ==="
echo "tools: $TOOLS_DIR"

###############################################################################
# Finding 3 -- incomplete invocations must not report success, and a real help
# request must not report failure.
###############################################################################
run_case "no-operands"   1 err "a bare invocation is a malformed conversion, not a help request" --
run_case "one-operand"   1 err "an input with no output path converts nothing" -- base.xml
run_case "help-short"    0 out "-h is an explicit help request and succeeds" -- -h
run_case "help-long"     0 out "--help is an explicit help request and succeeds" -- --help

###############################################################################
# Finding 4 -- an unrecognised option is an error, not something to skip.
###############################################################################
run_case "typo-no-id"    1 err "the '-no-id' typo is refused instead of silently ignored" -- base.xml out.icc -no-id
# -verbose additionally pins the tightening of the option match itself: the old
# test was strncmp(argv[i], "-v", 2), so this string was accepted AS a validation
# request. It must now be refused as unknown rather than turning validation on.
run_case "unknown-dash-v-prefix" 1 err "'-verbose' is not accepted as a '-v' prefix match" -- base.xml out.icc -verbose

# A help flag mixed into a real conversion is a malformed invocation, not a help
# request. Caught in review: scanning every argv position for -h made
# "iccFromXml in.xml out.icc -h" return 0 having written no profile -- the exact
# "exit 0 while doing something else" defect this suite exists to pin, reintroduced
# one argument to the right. Before that regression the tool ignored the flag and
# converted, so silence here is not an option either way.
run_case "help-flag-after-operands" 1 err "a help flag mixed into a conversion is refused, not silently obeyed" -- base.xml out-h.icc -h
if [ -f "$WORKDIR/out-h.icc" ]; then
  fail_case "help-flag-after-operands-no-output" "a refused invocation still wrote a profile"
else
  pass_case "help-flag-after-operands-no-output" "the refused invocation wrote no profile"
fi

###############################################################################
# Finding 2 -- -v must fail CLOSED, and must honour the documented location.
###############################################################################
# Precondition: neither lookup location may hold the schema, or this case proves
# nothing. Assert it rather than assume it.
if [ -f "$WORKDIR/SampleIccRELAX.rng" ] || [ -f "$TOOLS_DIR/IccFromXml/SampleIccRELAX.rng" ]; then
  skip_case "bare-v-fails-closed" "SampleIccRELAX.rng is present in a lookup location"
else
  run_case "bare-v-fails-closed" 1 err "bare -v with no schema found fails closed instead of validating nothing" -- base.xml out.icc -v
fi
run_case "v-equals-missing"  1 err "an explicitly named schema that cannot be opened is an error" -- base.xml out.icc -v=definitely-not-here.rng
run_case "v-equals-empty"    1 err "-v= with no path is an error" -- base.xml out.icc -v=

# The documented behaviour: README says SampleIccRELAX.rng is taken from the CURRENT
# directory. run_case cds into WORKDIR, so placing it there is exactly the invocation
# the README describes -- which never worked, because the lookup only ever derived the
# path from argv[0].
#
# The schema planted here REJECTS everything, and that choice is load-bearing. With a
# permissive one this case asserts only "exit 0", which the unfixed tool satisfies by
# failing open and validating nothing -- measured: it passed against master. Requiring
# a REFUSAL means the case can only pass if the schema was both found in the documented
# location and actually applied.
cp "$WORKDIR/reject.rng" "$WORKDIR/SampleIccRELAX.rng"
bare_v_exit=0
( cd "$WORKDIR" && timeout 60 "$FROMXML" base.xml out-barev.icc -v ) \
  > "$STDOUT_LOG" 2> "$STDERR_LOG" || bare_v_exit=$?
if [ "$bare_v_exit" -eq 0 ]; then
  fail_case "bare-v-current-dir" "bare -v accepted the document against a reject-all schema in the current directory -- the documented lookup still fails open"
elif ! grep -qi "relax-ng\|validity error" "$STDOUT_LOG" "$STDERR_LOG" 2>/dev/null; then
  fail_case "bare-v-current-dir" "bare -v refused, but with no RelaxNG diagnostic -- refused for some other reason"
  sed -n '1,6p' "$STDOUT_LOG"
else
  pass_case "bare-v-current-dir" "bare -v finds and APPLIES SampleIccRELAX.rng from the current directory, as documented"
fi
rm -f "$WORKDIR/SampleIccRELAX.rng"

###############################################################################
# Controls -- validation must still RUN, not merely be located.
###############################################################################
run_case "v-equals-permissive" 0 out "an explicit schema that accepts the document still converts" -- base.xml out-v.icc -v=permissive.rng

# The sharp one: a schema that rejects everything must actually reject. Without
# this, a fix that located the schema and then never applied it passes every case
# above.
reject_exit=0
( cd "$WORKDIR" && timeout 60 "$FROMXML" base.xml out-reject.icc -v=reject.rng ) \
  > "$STDOUT_LOG" 2> "$STDERR_LOG" || reject_exit=$?
if [ "$reject_exit" -eq 0 ]; then
  fail_case "reject-all-schema" "a reject-all schema was accepted -- the schema is located but not applied"
elif ! grep -qi "relax-ng\|validity error" "$STDOUT_LOG" "$STDERR_LOG" 2>/dev/null; then
  fail_case "reject-all-schema" "refused, but with no RelaxNG validity diagnostic -- refused for some other reason"
  sed -n '1,6p' "$STDOUT_LOG"
else
  pass_case "reject-all-schema" "a reject-all schema produces a RelaxNG validity error, so validation runs"
fi

###############################################################################
# Finding 1 -- -noid must leave no profile ID behind.
###############################################################################
# Built through a round trip on purpose: iccToXml emits <ProfileID> for a v4
# profile, and that emitted ID is what -noid used to carry straight through. A
# fixture written by hand without a <ProfileID> would have nothing to suppress and
# the case would pass against the unfixed tool.
profile_id_bytes() {
  python3 - "$1" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1]).read_bytes()
print(d[84:100].hex() if len(d) >= 100 else "short")
PY
}

if [ ! -x "$TOXML" ]; then
  skip_case "noid-clears-profile-id" "iccToXml not built, cannot build a fixture carrying an ID"
else
  ( cd "$WORKDIR" && "$FROMXML" base.xml with-id.icc >/dev/null 2>&1 )
  ( cd "$WORKDIR" && "$TOXML" with-id.icc with-id.xml >/dev/null 2>&1 )

  if ! grep -q "<ProfileID>" "$WORKDIR/with-id.xml" 2>/dev/null; then
    skip_case "noid-clears-profile-id" "round-trip XML carries no <ProfileID>, nothing to suppress"
  else
    ( cd "$WORKDIR" && "$FROMXML" with-id.xml noid.icc -noid >/dev/null 2>&1 )
    ( cd "$WORKDIR" && "$FROMXML" with-id.xml withid.icc >/dev/null 2>&1 )

    noid_id="$(profile_id_bytes "$WORKDIR/noid.icc")"
    withid_id="$(profile_id_bytes "$WORKDIR/withid.icc")"

    if [ "$withid_id" = "00000000000000000000000000000000" ]; then
      # The control must carry an ID, or "-noid produced zeroes" proves nothing.
      fail_case "noid-clears-profile-id" "the control without -noid also has a zero ID -- fixture proves nothing"
    elif [ "$noid_id" != "00000000000000000000000000000000" ]; then
      fail_case "noid-clears-profile-id" "-noid left profile ID bytes 84-99 as $noid_id"
    else
      pass_case "noid-clears-profile-id" "-noid zeroes the ID while the same document without it keeps $withid_id"
    fi
  fi
fi

###############################################################################
# The executable-directory fallback has to actually work.
###############################################################################
#
# Caught in review. The retained argv[0] derivation could not resolve a PATH-found
# invocation: it compared a 1-character substring with the 2-character literal "./"
# (always true) and then took substr(0, find_last_of("//")), which for a bare
# basename returns the basename itself -- so the candidate was
# "iccFromXml//SampleIccRELAX.rng". Harmless while a failed lookup silently disabled
# validation; once -v fails CLOSED it turned an installation that ships the schema
# beside the binary into a hard refusal. Measured: exit 1 via PATH, exit 0 via an
# absolute path, same files.
if command -v cp >/dev/null 2>&1; then
  BINDIR="$WORKDIR/bin"
  mkdir -p "$BINDIR"
  cp "$FROMXML" "$BINDIR/iccFromXml" 2>/dev/null
  cp "$WORKDIR/permissive.rng" "$BINDIR/SampleIccRELAX.rng" 2>/dev/null
  if [ -x "$BINDIR/iccFromXml" ] && [ -f "$BINDIR/SampleIccRELAX.rng" ]; then
    path_exit=0
    # No SampleIccRELAX.rng in the working directory, so only the executable-directory
    # lookup can satisfy this -- which is the point.
    rm -f "$WORKDIR/SampleIccRELAX.rng"
    ( cd "$WORKDIR" && PATH="$BINDIR:$PATH" timeout 60 iccFromXml base.xml out-path.icc -v ) \
      > "$STDOUT_LOG" 2> "$STDERR_LOG" || path_exit=$?
    if [ "$path_exit" -ne 0 ]; then
      fail_case "path-resolved-schema-lookup" "a PATH-found tool could not locate the schema installed beside it (exit $path_exit)"
      sed -n '1,4p' "$STDERR_LOG"
    elif ! grep -q "$BINDIR/SampleIccRELAX.rng" "$STDOUT_LOG" 2>/dev/null; then
      fail_case "path-resolved-schema-lookup" "converted, but did not report using the schema beside the executable"
      sed -n '1,4p' "$STDOUT_LOG"
    else
      pass_case "path-resolved-schema-lookup" "a PATH-found tool resolves the schema installed beside it"
    fi
  else
    skip_case "path-resolved-schema-lookup" "could not stage a PATH-resolved copy of the tool"
  fi
fi

###############################################################################
# Control -- an ordinary conversion is untouched by all of the above.
###############################################################################
run_case "plain-conversion" 0 out "an ordinary two-operand conversion still succeeds" -- base.xml plain.icc

echo ""
echo "=== summary: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

# Every case skipped means nothing was measured. Report that as a skip rather than
# success: a script CTest that exits 0 having asserted nothing reports GREEN.
if [ "$PASS" -eq 0 ]; then
  exit 77
fi

exit 0
