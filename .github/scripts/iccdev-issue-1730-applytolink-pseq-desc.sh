#!/bin/bash
###############################################################################
# iccApplyToLink issue-1730 profileSequenceDesc device description placement
###############################################################################
#
# The ICC Software License, Version 0.2
#
# Copyright (c) 2003-2026 The International Color Consortium. All rights
# reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# 3. In the absence of prior written permission, the names "ICC" and "The
#    International Color Consortium" must not be used to imply that the
#    ICC organization endorses or promotes products derived from this
#    software.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
# ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
###############################################################################
#
# When iccApplyToLink builds a V4 device link it copies each source profile's
# deviceMfgDesc ('dmnd') and deviceModelDesc ('dmdd') into that profile's
# profileSequenceDesc ('pseq') entry. The deviceModelDesc block wrote the model
# text into psd.m_deviceMfgDesc -- a copy/paste of the manufacturer block above
# -- so the manufacturer description was clobbered with the model text and the
# model description was left at its default.
#
# The tool still exits 0 in that state, and the sibling CTest
# iccdev.applytolink-v4-missing-device-descriptions uses a source profile with
# no dmnd/dmdd at all, so those copy blocks never execute there. Neither exit
# status nor that test can observe this bug, so this gate asserts on the
# generated pseq CONTENT instead:
#
#   fixed:  manufacturer = <MFG sentinel>,   model = <MODEL sentinel>
#   broken: manufacturer = <MODEL sentinel>, model = empty
#
# There is no tracked profile carrying distinct dmnd/dmdd, so the source
# fixture is derived at run time from the tracked sRGB_v4_ICC_preference.icc:
# iccToXml -> inject the two description tags -> iccFromXml. The injection is
# verified before use, so an anchor change fails the gate loudly rather than
# silently reducing it to a no-op.
#
# Environment variables:
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools or build/Tools
#   ICCDEV_TESTING_DIR -- path to the Testing/ tree
#   ICCDEV_TEST_OUTDIR -- output directory for temporary files and logs
#
# Exit codes: 0 pass or clean skip; 2 regression.
###############################################################################

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$REPO_ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$REPO_ROOT/Testing}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-issue-1730-applytolink-pseq-desc}"
mkdir -p "$OUTDIR"

if [ ! -d "$TOOLS_DIR" ]; then
  for candidate in "$REPO_ROOT/build/Tools" "$REPO_ROOT/Build/Tools"; do
    if [ -d "$candidate" ]; then
      TOOLS_DIR="$candidate"
      break
    fi
  done
fi

BUILD_ROOT="$(cd "$TOOLS_DIR/.." 2>/dev/null && pwd -P)"
if [ -n "$BUILD_ROOT" ]; then
  export LD_LIBRARY_PATH="$BUILD_ROOT/IccProfLib:$BUILD_ROOT/IccXML:$BUILD_ROOT/IccJSON:$BUILD_ROOT/IccConnect${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-print_scariness=1:halt_on_error=0:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0:print_stacktrace=1}"

TOXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccToXml -type f 2>/dev/null | head -1)"
FROMXML="$(find "$TOOLS_DIR" -maxdepth 2 -name iccFromXml -type f 2>/dev/null | head -1)"
APPLYLINK="$(find "$TOOLS_DIR" -maxdepth 2 -name iccApplyToLink -type f 2>/dev/null | head -1)"
DUMP="$(find "$TOOLS_DIR" -maxdepth 2 -name iccDumpProfile -type f 2>/dev/null | head -1)"

MFG_SENTINEL="ICCDEV1730 MFG SENTINEL"
MODEL_SENTINEL="ICCDEV1730 MODEL SENTINEL"

regress() {
  echo "  [FAIL] issue-1730-applytolink-pseq-desc -- $1"
  exit 2
}

# A sanitizer diagnostic under halt_on_error=0 is printed but does not change
# the exit code, so scan each captured log explicitly.
SAN_MARKERS='runtime error:|ERROR: (Address|Leak|Memory|Thread)Sanitizer|SUMMARY: (Address|Leak|Memory|Thread|Undefined)'
scan_sanitizer() {
  if grep -Eq "$SAN_MARKERS" "$1"; then
    grep -En "$SAN_MARKERS" "$1" | sed -n '1,6p'
    regress "sanitizer diagnostic while $2"
  fi
}

echo "=== iccApplyToLink issue-1730 pseq device description placement regression ==="

for tool in "$TOXML" "$FROMXML" "$APPLYLINK" "$DUMP"; do
  if [ -z "$tool" ] || [ ! -x "$tool" ]; then
    echo "  [SKIP] iccToXml, iccFromXml, iccApplyToLink or iccDumpProfile not built under $TOOLS_DIR"
    exit 0
  fi
done

SRC_PROFILE="$TESTING_DIR/sRGB_v4_ICC_preference.icc"
[ -f "$SRC_PROFILE" ] || SRC_PROFILE="$REPO_ROOT/Testing/sRGB_v4_ICC_preference.icc"
if [ ! -f "$SRC_PROFILE" ]; then
  regress "tracked source profile missing: Testing/sRGB_v4_ICC_preference.icc"
fi

base_xml="$OUTDIR/source-base.xml"
fixture_xml="$OUTDIR/source-with-descriptions.xml"
fixture_icc="$OUTDIR/source-with-descriptions.icc"
link_icc="$OUTDIR/link-with-descriptions.icc"
toxml_log="$OUTDIR/toxml.log"
fromxml_log="$OUTDIR/fromxml.log"
link_log="$OUTDIR/applytolink.log"
dump_log="$OUTDIR/dump.log"

# 1. Serialize the tracked profile to XML so the two description tags can be added.
if ! "$TOXML" "$SRC_PROFILE" "$base_xml" > "$toxml_log" 2>&1; then
  sed -n '1,8p' "$toxml_log"
  regress "iccToXml failed on the tracked source profile"
fi
scan_sanitizer "$toxml_log" "iccToXml serialized the source profile"

# 2. Inject distinct deviceMfgDesc/deviceModelDesc after the profile description
#    tag. Distinct sentinels are what make the swap observable: the bug puts the
#    MODEL sentinel into the manufacturer slot.
awk -v mfg="$MFG_SENTINEL" -v model="$MODEL_SENTINEL" '
  { print }
  !done && /<\/multiLocalizedUnicodeType> <\/profileDescriptionTag>/ {
    print ""
    print "    <deviceMfgDescTag> <multiLocalizedUnicodeType>"
    print "      <LocalizedText LanguageCountry=\"enUS\"><![CDATA[" mfg "]]></LocalizedText>"
    print "    </multiLocalizedUnicodeType> </deviceMfgDescTag>"
    print ""
    print "    <deviceModelDescTag> <multiLocalizedUnicodeType>"
    print "      <LocalizedText LanguageCountry=\"enUS\"><![CDATA[" model "]]></LocalizedText>"
    print "    </multiLocalizedUnicodeType> </deviceModelDescTag>"
    done = 1
  }
' "$base_xml" > "$fixture_xml"

# The anchor must have matched, or the fixture would silently lack the tags and
# the gate would pass without exercising anything.
if ! grep -q "deviceMfgDescTag" "$fixture_xml" || ! grep -q "deviceModelDescTag" "$fixture_xml"; then
  regress "failed to inject deviceMfgDesc/deviceModelDesc into the fixture XML (anchor changed?)"
fi

# 3. Rebuild the fixture profile carrying both descriptions.
if ! "$FROMXML" "$fixture_xml" "$fixture_icc" > "$fromxml_log" 2>&1; then
  sed -n '1,8p' "$fromxml_log"
  regress "iccFromXml failed to build the description fixture"
fi
scan_sanitizer "$fromxml_log" "iccFromXml built the description fixture"

# 4. Build a V4 device link (type 0, granularity 2) from that fixture. V4 is the
#    path that populates pseq; a V5 link uses a different structure.
"$APPLYLINK" "$link_icc" 0 2 0 "Issue1730DescProbe" 0 1 1 0 "$fixture_icc" 1 > "$link_log" 2>&1
link_rc=$?
if [ "$link_rc" -ne 0 ]; then
  sed -n '1,12p' "$link_log"
  regress "iccApplyToLink failed to write the V4 link (rc=$link_rc)"
fi
scan_sanitizer "$link_log" "iccApplyToLink wrote the V4 link"

# 5. Dump the link and read back the two pseq description fields. iccDumpProfile
#    prints the label, then (for an mluc description) a "Language = ..." line,
#    then the quoted value; a textDescription omits the language line. Extract
#    the whole block belonging to each label rather than assuming a fixed
#    offset, and bound the manufacturer block at the model label so the two
#    sentinels can never be confused for one another.
"$DUMP" "$link_icc" ALL > "$dump_log" 2>&1
dump_rc=$?
if [ "$dump_rc" -ne 0 ]; then
  sed -n '1,12p' "$dump_log"
  regress "iccDumpProfile failed on the generated link (rc=$dump_rc)"
fi
scan_sanitizer "$dump_log" "iccDumpProfile dumped the generated link"

mfg_value="$(awk '
  /Description of device manufacturer:/ { f = 1; next }
  /Description of device model:/        { f = 0 }
  f' "$dump_log")"
model_value="$(awk '
  /Description of device model:/ { f = 1; next }
  f && (/^[[:space:]]*$/ || /^Contents of/) { exit }
  f' "$dump_log")"

if [ -z "$mfg_value" ] || [ -z "$model_value" ]; then
  regress "could not read the pseq device description fields from the dump"
fi

# Regression 1: the manufacturer slot must hold the manufacturer text. Holding
# the MODEL sentinel is the exact signature of the copy/paste bug.
if printf '%s' "$mfg_value" | grep -qF "$MODEL_SENTINEL"; then
  echo "    manufacturer slot: $mfg_value"
  echo "    model slot:        $model_value"
  regress "model description was written into the pseq manufacturer slot (deviceModelDesc copied to m_deviceMfgDesc)"
fi
if ! printf '%s' "$mfg_value" | grep -qF "$MFG_SENTINEL"; then
  echo "    manufacturer slot: $mfg_value"
  regress "pseq manufacturer description does not carry the source deviceMfgDesc"
fi

# Regression 2: the model slot must hold the model text (the bug leaves it at
# its pre-populated default instead).
if ! printf '%s' "$model_value" | grep -qF "$MODEL_SENTINEL"; then
  echo "    model slot: $model_value"
  regress "pseq model description does not carry the source deviceModelDesc"
fi

echo "  [PASS] issue-1730-applytolink-pseq-desc -- pseq carries deviceMfgDesc and deviceModelDesc in their own slots"
exit 0
