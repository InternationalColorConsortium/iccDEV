#!/bin/bash
#
# Copyright (c) 2026 International Color Consortium
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################
# Guards CIccXform::Create's DToBx/BToDx tag selection for dual-PCS profiles
# (#1982).
#
# ICC.2:2023 lets a profile encode an independent colorimetric PCS and spectral
# PCS.  iccApplyToLink's intent codes 11/13 decode to icXformLutColor with
# bUseD2BxB2DxTags cleared, i.e. an explicit opt-out of D2B/B2D selection, and
# CIccCmm::AddXform honours it (its space selection reads
# "bUseD2BxB2DxTags || !m_Header.pcs").  CIccXform::Create used to test the bare
# spectralPCS field instead, so it handed back a spectral transform while the CMM
# had recorded the colorimetric connection space.
#
# Two directions, because they fail differently and one hides the other:
#
#   Part 1 (source profile).  SixChanInputRef has AToB3 6->3 and DToB3 6->36 and
#   no reverse tags.  Before the fix the 6->36 DToB3 was selected against a
#   3-sample XYZ connection space and CIccCmm::Begin() rejected the chain
#   outright, so a plain exit-status check is enough: measured on master
#   f78e915a the tool exits 255 with "status 2: Invalid space link".
#
#   Part 2 (destination profile).  SixChanCameraRef carries all four of AToB3,
#   BToA3, DToB3 and BToD3.  Here an exit status proves nothing -- the source and
#   destination mis-selections cancel out, so before the fix the chain builds a
#   link happily, just the wrong one.  The discriminator has to be relative:
#   build the same two-profile chain twice, once with both ends opting out
#   (11,11) and once with both opting in (1,1), and require the two links to
#   DIFFER.  Both arms succeed in both builds -- what changes is the route -- so
#   this is an assertion about tag selection and not an exit-status accident.
#   Measured on master f78e915a the two links are byte-identical once the
#   volatile header fields are stripped, which is precisely the bug: the opt-out
#   changed nothing.  Note the two ends have to move together; opting out at the
#   source while opting in at the destination is a genuine PCS mismatch once the
#   fix lands, and the tool correctly refuses it.
#
# Exit codes:
#   0  - pass
#   2  - regression
#   77 - skipped because a required tool is not built (SKIP_RETURN_CODE)
###############################################################################

set -uo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
tools_dir="${ICCDEV_TOOLS_DIR:-$repo_root/Build/Tools}"
outdir="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1982}"

find_tool()
{
  find "$tools_dir" -maxdepth 2 -type f -name "$1" 2>/dev/null | head -n 1
}

fail()
{
  echo "[FAIL] $1"
  exit 2
}

# Missing tools are a SKIP, not a FAIL: this test is registered unconditionally,
# so a build configured without ENABLE_ICCXML must not turn it red.  Exit 77
# rather than the siblings' 0 so SKIP_RETURN_CODE turns it into a ctest "Skipped"
# -- an exit 0 here would report a green pass for a run that asserted nothing.
skip()
{
  echo "[SKIP] $1"
  exit 77
}

mkdir -p "$outdir" || fail "cannot create output directory $outdir"

from_xml="$(find_tool iccFromXml)"
apply_to_link="$(find_tool iccApplyToLink)"
dump_profile="$(find_tool iccDumpProfile)"

for tool in "$from_xml" "$apply_to_link" "$dump_profile"; do
  if [ -z "$tool" ] || [ ! -x "$tool" ]; then
    skip "required tool not found under $tools_dir"
  fi
done

sanitizer_check()
{
  # $1 = log file, $2 = context for the failure message
  #
  # LeakSanitizer/MemorySanitizer and DEADLYSIGNAL are in the alternation because
  # dump_tool() below has to tolerate 254 and 255 (they are validation verdicts),
  # so for the iccDumpProfile logs this regex is the only thing that can catch a
  # finding which still exits with one of those codes.  Every other status is
  # rejected there, so LSan's 23 and ASan's default 1 are caught either way.
  # ICCDEV_TEST_ENV sets detect_leaks=0, but the script is runnable standalone
  # against an ASan build, where LSan is on by default.  This is a
  # superset of the alternation the sibling harnesses use: theirs is anchored on
  # "ERROR: AddressSanitizer", which misses "SUMMARY: AddressSanitizer:" lines,
  # and only two of them carry MemorySanitizer.  DEADLYSIGNAL is kept for parity
  # with the family even though ASan prints it as "AddressSanitizer: DEADLYSIGNAL"
  # and the first alternative already matches that.
  if grep -Eq "AddressSanitizer:|LeakSanitizer:|MemorySanitizer:|runtime error:|UndefinedBehaviorSanitizer|DEADLYSIGNAL" "$1"; then
    fail "sanitizer finding $2; see $1"
  fi
}

# No ASAN_OPTIONS/UBSAN_OPTIONS override here on purpose: ICCDEV_TEST_ENV already
# sets halt_on_error=0 for both.  That does NOT by itself keep the screen
# reachable -- see run_tool() below for why the log has to be screened before the
# status is judged regardless of halt_on_error.

# Single choke point for every tool invocation (#2293).  Previously only the two
# iccApplyToLink calls were screened; the iccFromXml and iccDumpProfile logs were
# not, and the dump calls had no status check at all.  Routing every invocation
# through one function is what stops a later call site from reintroducing that.
#
# The log is screened BEFORE the status is judged, and the order is load-bearing.
# halt_on_error=0 only keeps a tool alive when the build can recover, and nothing
# in this tree passes -fsanitize-recover=address: Build/Cmake/CMakeLists.txt:977
# adds -fno-sanitize-recover= for UBSan/integer/float only, and SANITIZER_RECOVER
# defaults OFF (:584).  So an ASan finding always aborts and arrives WITH a
# non-zero status, and only the UBSan patterns are reachable while still exiting
# zero -- in the one lane that sets SANITIZER_RECOVER=ON
# (ci-iccdev-tool-tests.yml:434).  Checking the status first would report every
# ASan abort as a plain tool failure and never reach the screen, so both orders
# stay red but only this one names the sanitizer.
run_tool()
{
  # $1 = log file, $2 = context for the failure messages, $3.. = command
  local log="$1" ctx="$2" rc=0
  shift 2
  "$@" >"$log" 2>&1 || rc=$?
  sanitizer_check "$log" "in $ctx"
  [ "$rc" -eq 0 ] || fail "$ctx failed (exit $rc); see $log"
}

# iccDumpProfile needs its own variant because its exit code is a VALIDATION
# VERDICT, not a success flag: with -v the tool ends "return nValid", which is 0
# for OK/warning/noncompliant, -1 (255) for icValidateCriticalError and -2 (254)
# for an unknown status.  The four device links this test generates already
# validate as noncompliant ("Profile violates ICC
# specification for version 5.00") -- the last severity that still maps to 0 --
# so a plain "|| fail" would turn any future validation tightening into a phantom
# #1982 regression while the D2B/B2D selection this test guards was intact.  The
# same asymmetry is documented in iccdev-qa-profile-manifest.sh:197-207 and
# Build/Cmake/Testing/CMakeLists.txt:6538.
#
# The "-v 100" on these dumps is deliberate and is what makes the tolerance below
# necessary at all: none of the grep pins read the validation report, so dropping
# -v would let every dump use run_tool() and delete this whole variant.  It is
# kept because this test carries the "asan" label and CIccProfile::Validate()
# walks every tag in the profile -- measured, it is the only thing separating the
# 165-line -v dump from the 160-line plain one -- so dropping it would shrink the
# code actually executed under the sanitizer, which is the coverage #2293 exists
# to protect.  The verdict tolerance is the price of that surface.
#
# Tolerating a verdict has to be an ALLOW-list, not a blacklist, because the tool
# has more non-verdict exits than verdicts: 1 from the QA-flag argument paths
# (iccDumpProfile.cpp:582, :604), -1 (255) from three usage paths (:617, :632,
# :664), 253 from a failed dump write (:1010 when DumpTagEntry fails mid-"ALL",
# :1053 when WriteStringToStdout fails), and any death by signal.  253 in
# particular truncates the log, which matters most in Part 2 where the assertion
# is that the two normalized chain dumps DIFFER: a truncation hitting one arm and
# not the other would make them differ for a reason unrelated to tag selection.
#
# 255 is genuinely ambiguous and needs TWO log pins, because three different
# things produce it.  It is icValidateCriticalError; :703-706 forces that same
# verdict when the profile cannot be parsed at all; and the three usage paths
# (:617, :632, :664) return -1 for a bad invocation.  The two failure modes are
# separated from the verdict only by the log:
#
#   - a bad invocation prints "Usage:" and returns BEFORE the version banner at
#     :703, so requiring the banner catches it;
#   - an unparseable profile prints the banner and then "Unable to parse".
#
# Without both pins, a dump that never ran surfaces downstream as "fixture no
# longer exposes an XYZ PCS" -- blaming the fixture for a tool-output failure,
# which is the mislabeling this whole variant exists to avoid.
# What otherwise asserts the dump is real is the grep pins at each call site.
dump_tool()
{
  # $1 = log file, $2 = context for the failure messages, $3.. = command
  local log="$1" ctx="$2" rc=0
  shift 2
  "$@" >"$log" 2>&1 || rc=$?
  sanitizer_check "$log" "in $ctx"
  case "$rc" in
    0|254|255) ;;
    *) fail "$ctx failed (exit $rc); see $log" ;;
  esac
  grep -q "^Built with IccProfLib version" "$log" ||
    fail "$ctx produced no dump (bad invocation?); see $log"
  if grep -q "Unable to parse" "$log"; then
    fail "$ctx could not parse its input; see $log"
  fi
}

###############################################################################
# Part 1 -- dual-PCS source profile keeps its colorimetric AToBx transform
###############################################################################

src_xml="$repo_root/Testing/SpecRef/SixChanInputRef.xml"
src_icc="$outdir/SixChanInputRef.icc"
[ -f "$src_xml" ] || fail "missing fixture: $src_xml"

run_tool "$outdir/fromxml-input.log" "iccFromXml on $src_xml" \
  "$from_xml" "$src_xml" "$src_icc"
dump_tool "$outdir/input.log" "iccDumpProfile on $src_icc" \
  "$dump_profile" -v 100 "$src_icc" ALL

# Anti-vacuity pins: if the fixture ever loses the dual PCS or either transform
# the assertions below would still "pass" while testing nothing.
grep -Eq "PCS Color Space:[[:space:]]+XYZData" "$outdir/input.log" ||
  fail "fixture no longer exposes an XYZ PCS"
grep -Eq "Spectral PCS:[[:space:]]+0x0024ChannelReflectanceData" "$outdir/input.log" ||
  fail "fixture no longer exposes a 36-channel spectral PCS"
# Anchored: "6->3" is a prefix of "6->36", so an unanchored AToB pin is satisfied
# by the DToB line and can never detect the fixture change it names.
grep -Eq "Matrix Element \\('matf' = 6D617466\\) 6->3$" "$outdir/input.log" ||
  fail "fixture no longer has a 6-to-3 AToB transform"
grep -Eq "Matrix Element \\('matf' = 6D617466\\) 6->36$" "$outdir/input.log" ||
  fail "fixture no longer has a 6-to-36 DToB transform"

for intent in 11 13; do
  link_icc="$outdir/link-$intent.icc"
  log="$outdir/applytolink-$intent.log"
  rm -f "$link_icc"

  run_tool "$log" "iccApplyToLink for source intent $intent" \
    "$apply_to_link" "$link_icc" 0 2 1 Issue1982 0 1 1 0 "$src_icc" "$intent"

  # Same guard Part 2 carries: without it a tool that exits 0 without writing the
  # link is reported by the pins below as fixture drift, not as a tool failure.
  [ -f "$link_icc" ] || fail "source intent $intent wrote no device link"

  dump_tool "$outdir/link-$intent.log" "iccDumpProfile on $link_icc" \
    "$dump_profile" -v 100 "$link_icc" ALL
  grep -Eq "PCS Color Space:[[:space:]]+XYZData" "$outdir/link-$intent.log" ||
    fail "source intent $intent did not produce XYZ PCS output"
  grep -Eq "MPE Element Chain: 1 elements, 6->3 channels" "$outdir/link-$intent.log" ||
    fail "source intent $intent did not produce a 6-to-3 colorimetric link"
done

echo "[ok] part 1: dual-PCS D2B opt-out retained 6-to-3 XYZ links"

###############################################################################
# Part 2 -- dual-PCS destination profile honours the same opt-out
###############################################################################

dst_xml="$repo_root/Testing/SpecRef/SixChanCameraRef.xml"
dst_icc="$outdir/SixChanCameraRef.icc"
[ -f "$dst_xml" ] || fail "missing fixture: $dst_xml"

run_tool "$outdir/fromxml-dest.log" "iccFromXml on $dst_xml" \
  "$from_xml" "$dst_xml" "$dst_icc"
dump_tool "$outdir/dest.log" "iccDumpProfile on $dst_icc" \
  "$dump_profile" -v 100 "$dst_icc" ALL

# The whole point of this fixture is that BOTH reverse tags exist, so the
# colorimetric route is available and the choice between them is real.
grep -Eq "PCS Color Space:[[:space:]]+XYZData" "$outdir/dest.log" ||
  fail "destination fixture no longer exposes an XYZ PCS"
grep -Eq "Spectral PCS:[[:space:]]+0x0024ChannelReflectanceData" "$outdir/dest.log" ||
  fail "destination fixture no longer exposes a 36-channel spectral PCS"
for tag in BToA3Tag BToD3Tag; do
  grep -Eq "$tag" "$outdir/dest.log" ||
    fail "destination fixture no longer carries $tag"
done

# Strip the fields that differ between two runs of the same tool (creation time,
# profile ID/MD5, the dump banner and the file name/size) so the comparison is
# about the transform that was selected and nothing else.
normalize()
{
  grep -Ev "Creation Date|Profile ID|^Size:|^Profile:|^Built with" "$1" > "$2"
}

for intent in 11 1; do
  link_icc="$outdir/chain-link-$intent.icc"
  log="$outdir/applytolink-chain-$intent.log"
  rm -f "$link_icc" "$outdir/chain-link-$intent.norm"

  run_tool "$log" "iccApplyToLink for chain intent $intent" \
    "$apply_to_link" "$link_icc" 0 3 1 Issue1982 0 1 1 0 \
    "$src_icc" "$intent" "$dst_icc" "$intent"

  # Both arms must actually have produced a link.  Without this the comparison
  # below would "differ" simply because one dump is an open failure message,
  # and the test would pass against an unfixed library.
  [ -f "$link_icc" ] || fail "chain intent $intent wrote no device link"

  dump_tool "$outdir/chain-link-$intent.log" "iccDumpProfile on $link_icc" \
    "$dump_profile" -v 100 "$link_icc" ALL
  normalize "$outdir/chain-link-$intent.log" "$outdir/chain-link-$intent.norm"
  grep -Eq "MPE Element Chain" "$outdir/chain-link-$intent.norm" ||
    fail "chain intent $intent produced a dump with no transform chain in it"
done

if cmp -s "$outdir/chain-link-11.norm" "$outdir/chain-link-1.norm"; then
  fail "the D2B/B2D opt-out (11) and opt-in (1) produced identical links: the spectral tags were selected either way"
fi

echo "[ok] part 2: dual-PCS B2D opt-out selected a different link from the opt-in"
echo "[PASS] dual-PCS transform selection honours the D2B/B2D opt-out in both directions"
