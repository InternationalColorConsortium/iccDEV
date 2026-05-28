#!/usr/bin/env bash
set -euo pipefail

ROOT="${ICCDEV_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
TOOLS_DIR="${ICCDEV_TOOLS_DIR:-$ROOT/Build/Tools}"
TESTING_DIR="${ICCDEV_TESTING_DIR:-$ROOT/Testing}"
FROMXML="$TOOLS_DIR/IccFromXml/iccFromXml"
BASE_XML="$TESTING_DIR/ICS/Lab_float-D65_2deg-Part1.xml"

if [ ! -x "$FROMXML" ]; then
  echo "ERROR: IccFromXml is required at $FROMXML" >&2
  exit 1
fi

if [ ! -f "$BASE_XML" ]; then
  echo "ERROR: Base XML fixture is required at $BASE_XML" >&2
  exit 1
fi

if ! command -v strace >/dev/null 2>&1; then
  echo "ERROR: strace is required for XXE local-file-open regression" >&2
  exit 1
fi

tmp="$(mktemp -d)"
cleanup() {
  rm -rf "$tmp"
}
trap cleanup EXIT

secret="$tmp/secret.txt"
xxe_xml="$tmp/xxe.xml"
out_icc="$tmp/out.icc"
trace_log="$tmp/strace.log"

printf 'ICCDEV_XXE_LOCAL_SECRET\n' > "$secret"
awk -v secret="$secret" '
  NR == 2 {
    print "<!DOCTYPE IccProfile [ <!ENTITY xxe SYSTEM \"file://" secret "\"> ]>"
  }
  /<\/Header>/ {
    print "    <UnknownHeaderField>&xxe;</UnknownHeaderField>"
  }
  { print }
' "$BASE_XML" > "$xxe_xml"

if ! ASAN_OPTIONS=detect_leaks=0 \
  strace -f -e trace=openat -o "$trace_log" \
  "$FROMXML" "$xxe_xml" "$out_icc" >/dev/null 2>&1; then
  echo "FAIL: IccFromXml did not process the XXE regression input cleanly" >&2
  exit 1
fi

if grep -F "$secret" "$trace_log" >/dev/null; then
  echo "FAIL: XXE local file open observed: $secret" >&2
  exit 1
fi

echo "PASS: XXE local file open not observed"
