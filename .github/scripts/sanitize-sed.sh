#!/usr/bin/env bash
#------------------------------------------------------------------------------
# @file
# File:       sanitize-sed.sh
#
# Contains:   Implementation of sanitizer for BASH Shell.
#
# Version:    V3
#
# Copyright:  (c) see Software License
#------------------------------------------------------------------------------
#
# Copyright (c) International Color Consortium.
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
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
# ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# ====================================================================
#
# This software consists of voluntary contributions made by many
# individuals on behalf of the International Color Consortium.
#
# Membership in the ICC is encouraged when this software is used for
# commercial purposes.
#
# For more information on The International Color Consortium, please
# see http://www.color.org/.
#------------------------------------------------------------------------------

###############################################################
# Copyright (c) 2024-2025 David H Hoyt. All rights reserved.
###############################################################
#                 https://srd.cx
#
# Last Updated: 02-MAR-2026 0045Z by David Hoyt
#
# Intent: Try Sanitizing User Controllable Inputs
#
# File: .github/scripts/sanitize-sed.sh
#
#
# Comment: Sanitizing User Controllable Input
#          - is a Moving Target
#          - needs ongoing updates
#          - needs additional unit tests
#          - v3: Unicode control/bidi/ZWJ stripping
#
#
#
###############################################################

# --- Configuration ---
# Maximum lengths
SANITIZE_LINE_MAXLEN=${SANITIZE_LINE_MAXLEN:-1000}   # single-line max
SANITIZE_PRINT_MAXLEN=${SANITIZE_PRINT_MAXLEN:-8000} # multi-line max

# --- Low-level helpers -------------------------------------------------------

# escape_html STRING
# Replace &, <, >, " and ' with HTML entities.
# Uses sed to avoid bash parameter expansion issues with & in replacement.
escape_html() {
  local s="$1"
  # Order matters: escape & first
  # Use multiple sed passes to avoid quoting complexity
  s=$(printf '%s' "$s" | \
    sed 's/&/\&amp;/g' | \
    sed 's/</\&lt;/g' | \
    sed 's/>/\&gt;/g' | \
    sed 's/"/\&quot;/g' | \
    sed "s/'/\&#39;/g")
  printf '%s' "$s"
}

# ANSI escape rules, built from real bytes for the same reason the Unicode fallback
# below is: CSI (ESC [ ... final), OSC (ESC ] ... BEL), then any remaining bare ESC.
_SAN_ANSI_RE=$'s/\x1b\[[0-9;]*[A-Za-z]//g; s/\x1b\][^\x07]*\x07//g; s/\x1b//g'

# _strip_stray_c1 STRING
# Remove raw 0x80-0x9F bytes that are NOT part of a well-formed UTF-8 sequence.
#
# These cannot be removed with tr: the same byte values are legitimate UTF-8
# continuation bytes, so blanket deletion corrupts ordinary text -- the suite's
# own UTF-8 case is E4 B8 96 E7 95 8C, which contains 0x96.  Telling the two
# apart needs an actual decode, so this walks the byte stream the way
# icDecodeUtf8() (IccProfLib/IccFileUtil.h) does and drops a 0x80-0x9F byte only
# where no well-formed sequence claims it.  U+009B is CSI -- the same introducer
# the ANSI rule strips in its ESC [ form -- and a terminal in a single-byte mode
# acts on the raw byte, so leaving it reopened log spoofing from a third side.
#
# Deliberately narrow: a malformed byte OUTSIDE 0x80-0x9F is still passed
# through, which is what the "overlong UTF-8 bytes preserved (no decode)" case in
# test_sanitization.sh asserts.  Widening this to drop every ill-formed byte
# would reverse that decision silently -- that test only checks no ASCII '<'/'>'
# is synthesized, which dropping satisfies just as well as preserving.
#
# awk rather than perl: the perl-less fallback is the path #2463 found stripping
# nothing at all, so this rule must not depend on perl being installed.  awk is
# not guarded here because it is already an unguarded hard dependency of this
# file -- _trim_whitespace() below pipes through it, and sanitize_line() calls
# that.  A `command -v awk` fallback would therefore rescue only sanitize_print(),
# and would rescue it by silently returning the input unfiltered, which is the
# raw-CSI residue this function exists to remove.  Failing loudly is correct.
_strip_stray_c1() {
  local s="$1"
  printf '%s' "$s" | LC_ALL=C awk '
    BEGIN {
      for (i = 0; i < 256; i++) ord[sprintf("%c", i)] = i
      # Second-byte bounds mirror icDecodeUtf8(): E0 A0 rejects the overlong
      # 3-byte forms, ED 9F the surrogates, F0 90 the overlong 4-byte forms,
      # and F4 8F caps at U+10FFFF.
      lo[224] = 160; hi[224] = 191
      lo[237] = 128; hi[237] = 159
      lo[240] = 144; hi[240] = 191
      lo[244] = 128; hi[244] = 143
    }
    {
      out = ""; n = length($0); i = 1
      while (i <= n) {
        c = ord[substr($0, i, 1)]
        len = 0
        if (c < 128)                   len = 1
        else if (c >= 194 && c <= 223) len = 2
        else if (c >= 224 && c <= 239) len = 3
        else if (c >= 240 && c <= 244) len = 4
        if (len == 1) { out = out substr($0, i, 1); i++; continue }
        if (len > 1) {
          L = (c in lo) ? lo[c] : 128
          H = (c in hi) ? hi[c] : 191
          ok = (i + len - 1 <= n)
          for (k = 2; ok && k <= len; k++) {
            b = ord[substr($0, i + k - 1, 1)]
            if (k == 2) { if (b < L   || b > H)   ok = 0 }
            else        { if (b < 128 || b > 191) ok = 0 }
          }
          if (ok) { out = out substr($0, i, len); i += len; continue }
        }
        # Not the start of a well-formed sequence: drop only a stray C1.
        if (c < 128 || c > 159) out = out substr($0, i, 1)
        i++
      }
      print out
    }
  '
}

# _strip_unicode_control STRING
# Remove Unicode control/formatting characters that enable Trojan Source,
# invisible padding, and homoglyph attacks:
#   - C1 controls (U+0080-U+009F), in their well-formed UTF-8 spelling C2 80..C2 9F.
#     U+009B is CSI, the same introducer the ANSI rule strips in its ESC [ form, so
#     leaving it reopens log spoofing from the other side.  sanitize.ps1 already
#     drops this block (keep-set 0x20..0x7E plus >= 0xA0), as does the library's
#     icSanitizeConsoleText().
#     A RAW 0x80-0x9F byte -- malformed UTF-8, not a codepoint -- is handled by
#     _strip_stray_c1() below rather than here, because neither branch of this
#     function can see one: perl works on decoded codepoints and the sed rules
#     match well-formed spellings.  sanitize_ref() is unaffected either way
#     because it whitelists to ASCII.
#   - Bidi overrides/embeddings (U+202A-202E, U+2066-2069)
#   - Zero-width chars (U+200B-200F, U+2060, U+FEFF)
#   - Tag characters (U+E0001-E007F) - used in emoji but abusable
# Uses perl for reliable multi-byte removal; falls back to sed byte patterns.
_strip_unicode_control() {
  local s="$1"
  # Stray C1 bytes first, for the reason given above: neither branch below can
  # see a byte that is not a codepoint and not a well-formed spelling.
  s="$(_strip_stray_c1 "$s")"
  if command -v perl >/dev/null 2>&1; then
    s="$(printf '%s' "$s" | perl -CS -pe '
      s/[\x{0080}-\x{009F}]//g;
      s/[\x{200B}-\x{200F}]//g;
      s/[\x{2028}-\x{202F}]//g;
      s/[\x{2060}-\x{2069}]//g;
      s/[\x{E0001}-\x{E007F}]//g;
      s/[\x{FEFF}]//g;
      s/[\x{FFF9}-\x{FFFB}]//g;
    ')"
  else
    # Fallback: strip known UTF-8 byte sequences for the most dangerous chars.
    #
    # LC_ALL=C is the load-bearing part, and it is why five of these six rules used
    # to be dead.  In a UTF-8 locale a bracket expression like [\xa8-\xaf] is a
    # CHARACTER range whose endpoints are not characters, so it matches nothing;
    # only the bracket-free U+FEFF rule fired, and a U+202E filename came through
    # sanitize_line() byte-identical on a perl-less runner.  Under LC_ALL=C the same
    # ranges are byte ranges and match -- which is exactly why sanitize_ref() below
    # already sets it.  (GNU sed does honour \xNN inside a bracket expression; the
    # locale, not the escape, was the bug.)
    #
    # The $'...' expansion is belt-and-braces on top of that: it hands sed real bytes
    # rather than escape text, so the rules also work on BSD sed, which does not
    # interpret \xNN at all.  macOS runners source this file.
    local re
    re=$'s/\xc2[\x80-\x9f]//g;'                  # U+0080-U+009F C1 controls
    re+=$'s/\xe2\x80[\x8b-\x8f]//g;'             # U+200B-U+200F zero-width
    re+=$'s/\xe2\x80[\xa8-\xaf]//g;'             # U+2028-U+202F separators, bidi overrides
    re+=$'s/\xe2\x81[\xa0-\xa9]//g;'             # U+2060-U+2069 word joiner, bidi isolates
    re+=$'s/\xf3\xa0[\x80-\x81][\x80-\xbf]//g;'  # U+E0001-U+E007F tag characters
    re+=$'s/\xef\xbb\xbf//g;'                    # U+FEFF BOM
    re+=$'s/\xef\xbf[\xb9-\xbb]//g;'             # U+FFF9-U+FFFB interlinear annotation
    s="$(printf '%s' "$s" | LC_ALL=C sed -E "$re")"
  fi
  printf '%s' "$s"
}

# _strip_ctrl_keep_newlines STRING
# Remove control characters except newline (0x0A). Also remove NUL.
# Strips ANSI escape sequences (CSI, OSC, etc.) to prevent log spoofing.
# Strips Unicode control/formatting characters to prevent Trojan Source attacks.
_strip_ctrl_keep_newlines() {
  local s="$1"
  # remove CRs explicitly
  s="${s//$'\r'/}"
  # strip ANSI escape sequences: CSI (\x1b[...m), OSC (\x1b]...\x07), then any remaining bare ESC
  # Same escape/locale care as _strip_unicode_control: built from real bytes and
  # matched under LC_ALL=C, so these rules also fire on BSD sed (macOS runners),
  # which does not interpret \xNN.  Without that they matched the literal text
  # "x1b" there and only the tr below removed the ESC, leaving "[31m" in the summary.
  s="$(printf '%s' "$s" | LC_ALL=C sed -E "$_SAN_ANSI_RE")"
  # strip Unicode bidi overrides, zero-width chars, and formatting controls.
  # This is also what removes the C1 controls: the tr below works on BYTES and
  # cannot see them, because UTF-8 spells U+0080-U+009F as C2 80..C2 9F.
  s="$(_strip_unicode_control "$s")"
  # remove NUL and other C0 control chars except LF (0x0A), plus DEL (0x7F)
  s="$(printf '%s' "$s" | tr -d '\000-\011\013\014\016-\037\177')"
  printf '%s' "$s"
}

# _strip_ctrl_remove_newlines STRING
# Remove control characters and newlines (useful for single-line outputs).
# Strips ANSI escape sequences (CSI, OSC, etc.) to prevent log spoofing.
# Strips Unicode control/formatting characters to prevent Trojan Source attacks.
_strip_ctrl_remove_newlines() {
  local s="$1"
  # remove CRs and LFs
  s="${s//$'\r'/}"
  s="${s//$'\n'/ }"
  # strip ANSI escape sequences: CSI (\x1b[...m), OSC (\x1b]...\x07), then any remaining bare ESC
  # Same escape/locale care as _strip_unicode_control: built from real bytes and
  # matched under LC_ALL=C, so these rules also fire on BSD sed (macOS runners),
  # which does not interpret \xNN.  Without that they matched the literal text
  # "x1b" there and only the tr below removed the ESC, leaving "[31m" in the summary.
  s="$(printf '%s' "$s" | LC_ALL=C sed -E "$_SAN_ANSI_RE")"
  # strip Unicode bidi overrides, zero-width chars, and formatting controls.
  # This is also what removes the C1 controls: the tr below works on BYTES and
  # cannot see them, because UTF-8 spells U+0080-U+009F as C2 80..C2 9F.
  s="$(_strip_unicode_control "$s")"
  # remove other control characters (NUL, etc.) plus DEL (0x7F)
  s="$(printf '%s' "$s" | tr -d '\000-\011\013\014\016-\037\177')"
  printf '%s' "$s"
}

# _trim_whitespace STRING -> trimmed
# Trim leading and trailing whitespace. Uses awk for portability.
_trim_whitespace() {
  local s="$1"
  # awk will treat the entire input as one record if we avoid newlines.
  printf '%s' "$s" | awk '{$1=$1; print}'
}

# _truncate STRING MAXLEN -> truncated (with ellipsis if truncated)
_truncate() {
  local s="$1"
  local maxlen="$2"
  local len
  len=${#s}
  if (( len <= maxlen )); then
    printf '%s' "$s"
    return 0
  fi
  # keep a small tail to help debugging
  local head
  head="${s:0:((maxlen-3))}"
  printf '%s' "${head}..."
}

# --- Public sanitizers ------------------------------------------------------

# sanitize_line STRING
# Produce a single-line safe string:
# - remove CR/LF, control chars
# - trim
# - escape HTML entities
# - truncate to SANITIZE_LINE_MAXLEN
sanitize_line() {
  local input="$1"
  local s
  s="$(_strip_ctrl_remove_newlines "$input")"
  s="$(_trim_whitespace "$s")"
  s="$(escape_html "$s")"
  s="$(_truncate "$s" "$SANITIZE_LINE_MAXLEN")"
  printf '%s' "$s"
}

# sanitize_print STRING
# Produce a multi-line safe string suitable for step summaries:
# - remove CR and other dangerous control chars but preserve LF
# - escape HTML entities
# - collapse too-many-consecutive-newlines into max 3
# - truncate total length to SANITIZE_PRINT_MAXLEN
sanitize_print() {
  local input="$1"
  local s
  s="$(_strip_ctrl_keep_newlines "$input")"
  # Normalize different newline sequences to LF (already removed CR).
  # Collapse runs of more than 3 newlines to 3 to prevent giant junk.
  # Use sed to operate on the whole buffer (single-line command).
  s="$(printf '%s' "$s" | sed -E ':a;N;$!ba;s/\n{4,}/\n\n\n/g')"
  s="$(escape_html "$s")"
  s="$(_truncate "$s" "$SANITIZE_PRINT_MAXLEN")"
  printf '%s' "$s"
}

# sanitize_code_line STRING
# Produce a single-line string for fenced code blocks:
# - remove CR/LF and control chars
# - trim
# - neutralize markdown fence delimiters
# - truncate to SANITIZE_LINE_MAXLEN
#
# Unlike sanitize_line(), this intentionally does not HTML-escape quotes and
# braces. Use it only inside fenced code blocks so JSON and command output remain
# readable in GitHub summaries without rendering as HTML.
sanitize_code_line() {
  local input="$1"
  local s
  s="$(_strip_ctrl_remove_newlines "$input")"
  s="$(_trim_whitespace "$s")"
  s="${s//\`\`\`/\` \` \`}"
  s="$(_truncate "$s" "$SANITIZE_LINE_MAXLEN")"
  printf '%s' "$s"
}

# sanitize_ref STRING
# Sanitize branch, tag or ref names for use in filenames, concurrency groups, etc.
# - replace disallowed chars with '-'
# - collapse multiple '-' into single '-'
# - trim leading/trailing '-'
sanitize_ref() {
  local input="$1"
  local s
  s="$(printf '%s' "$input" | tr -d '\000')"
  # remove CR/LF
  s="${s//$'\r'/}"
  s="${s//$'\n'/}"
  # replace any character not in the allowed set [A-Za-z0-9._/-] with '-'
  # LC_ALL=C ensures byte-level matching (prevents overlong UTF-8 bypass)
  s="$(printf '%s' "$s" | LC_ALL=C sed -E 's#[^A-Za-z0-9._/-]#-#g')"
  # collapse multiple hyphens
  s="$(printf '%s' "$s" | sed -E 's/-+/-/g')"
  # trim leading/trailing hyphen
  s="$(printf '%s' "$s" | sed -E 's/^-+//; s/-+$//')"
  # fallback to sha-like short id if empty
  if [[ -z "$s" ]]; then
    s="ref-unknown"
  fi
  printf '%s' "$s"
}

# sanitize_filename STRING
# Produce a filename-safe string (no slashes)
sanitize_filename() {
  local input="$1"
  local s
  s="$(sanitize_ref "$input")"
  # replace forward slashes with underscores (do not allow directory traversal)
  s="${s//\//_}"
  printf '%s' "$s"
}

# safe_echo_for_summary STRING...
# Echo arguments after sanitizing as print (multi-line). Useful as a drop-in.
safe_echo_for_summary() {
  local joined
  # join args with spaces
  joined="$*"
  sanitize_print "$joined"
  printf '\n'
}

# --- Detection functions (detect-and-alert, not strip-and-pass) -------------

# detect_hidden_chars STRING [LABEL]
# Detect hidden Unicode characters in STRING. Returns 0 if hidden chars found
# (DANGEROUS), 1 if clean. Emits [CRITICAL] diagnostics to stderr.
# LABEL is an optional context label (e.g., "GITHUB_HEAD_REF").
#
# Detects:
#   - BOM / Zero-Width No-Break Space (U+FEFF)
#   - Bidi overrides/embeddings (U+202A-202E, U+2066-2069)
#   - Zero-width chars (U+200B-200F, U+2060)
#   - Line/paragraph separators (U+2028-2029)
#   - Interlinear annotation (U+FFF9-U+FFFB)
#   - Any non-ASCII byte in a ref name context
#
# Design: This function is a DETECTION system (like GitHub UI warnings),
# not a sanitization filter. It reports findings without modifying the input.
# Use sanitize_ref() or sanitize_line() separately for safe output.
detect_hidden_chars() {
  local input="$1"
  local label="${2:-input}"
  local found=1  # 1 = clean (shell convention: 0=true/found, 1=false/clean)
  local details=""

  # Skip empty input
  if [[ -z "$input" ]]; then
    return 1
  fi

  # Check for BOM (U+FEFF = EF BB BF in UTF-8)
  if printf '%s' "$input" | LC_ALL=C grep -qP '\xef\xbb\xbf' 2>/dev/null; then
    details="${details}  - U+FEFF (BOM / Zero-Width No-Break Space)\n"
    found=0
  fi

  # Check for C1 controls (U+0080-U+009F = C2 80-9F).  Listed so a CSI-carrying ref
  # is named rather than falling through to the "unknown category" catch-all below.
  # The raw-byte half needs the same decode _strip_stray_c1() does: a bare
  # [\x80-\x9f] grep here would mislabel every CJK ref, because the 0x96 in
  # E4 B8 96 is a continuation byte.  If the decoder removes anything, the input
  # carried a stray C1.  Both sides go through $( ) so trailing-newline handling
  # cannot make them differ spuriously.
  if printf '%s' "$input" | LC_ALL=C grep -qP '\xc2[\x80-\x9f]' 2>/dev/null ||
     [ "$(_strip_stray_c1 "$input")" != "$(printf '%s' "$input")" ]; then
    details="${details}  - U+0080-U+009F (C1 Control)\n"
    found=0
  fi

  # Check for bidi overrides (U+202A-202E = E2 80 AA-AE)
  if printf '%s' "$input" | LC_ALL=C grep -qP '[\xe2][\x80][\xaa-\xae]' 2>/dev/null; then
    details="${details}  - U+202A-202E (Bidi Override/Embedding)\n"
    found=0
  fi

  # Check for bidi isolates (U+2066-2069 = E2 81 A6-A9)
  if printf '%s' "$input" | LC_ALL=C grep -qP '[\xe2][\x81][\xa6-\xa9]' 2>/dev/null; then
    details="${details}  - U+2066-2069 (Bidi Isolate)\n"
    found=0
  fi

  # Check for zero-width chars (U+200B-200F = E2 80 8B-8F)
  if printf '%s' "$input" | LC_ALL=C grep -qP '[\xe2][\x80][\x8b-\x8f]' 2>/dev/null; then
    details="${details}  - U+200B-200F (Zero-Width Space/Joiner/Mark)\n"
    found=0
  fi

  # Check for word joiner (U+2060 = E2 81 A0)
  if printf '%s' "$input" | LC_ALL=C grep -qP '\xe2\x81\xa0' 2>/dev/null; then
    details="${details}  - U+2060 (Word Joiner)\n"
    found=0
  fi

  # Check for line/paragraph separators (U+2028-2029 = E2 80 A8-A9)
  if printf '%s' "$input" | LC_ALL=C grep -qP '[\xe2][\x80][\xa8-\xa9]' 2>/dev/null; then
    details="${details}  - U+2028-2029 (Line/Paragraph Separator)\n"
    found=0
  fi

  # Check for tag characters (U+E0001-E007F = F3 A0 80 81 through F3 A0 81 BF)
  if printf '%s' "$input" | LC_ALL=C grep -qP '\xf3\xa0[\x80-\x81][\x80-\xbf]' 2>/dev/null; then
    details="${details}  - U+E0001-E007F (Tag Character)\n"
    found=0
  fi

  # Check for interlinear annotation (U+FFF9-FFFB = EF BF B9-BB)
  if printf '%s' "$input" | LC_ALL=C grep -qP '[\xef][\xbf][\xb9-\xbb]' 2>/dev/null; then
    details="${details}  - U+FFF9-FFFB (Interlinear Annotation)\n"
    found=0
  fi

  # Broad check: any non-ASCII byte (catches novel attacks)
  if printf '%s' "$input" | LC_ALL=C grep -qP '[^\x20-\x7E]' 2>/dev/null; then
    if [[ $found -ne 0 ]]; then
      # Only flag if we haven't already identified specific chars above
      details="${details}  - Non-ASCII byte(s) detected (unknown category)\n"
      found=0
    fi
  fi

  # Report findings
  if [[ $found -eq 0 ]]; then
    {
      printf '[CRITICAL] Hidden Unicode characters detected in %s\n' "$label"
      printf '%b' "$details"
      printf '  Raw bytes: '
      printf '%s' "$input" | xxd -p | head -c 120
      printf '\n'
      printf '  Sanitized: %s\n' "$(sanitize_ref "$input")"
      printf '  GitHub UI parity: this finding matches GitHub warning\n'
      printf '    "The head ref may contain hidden characters"\n'
    } >&2
  fi

  return $found
}

# validate_ref STRING [LABEL]
# Wrapper: detect hidden chars + sanitize. Returns sanitized ref on stdout.
# Emits [CRITICAL] to stderr if hidden chars found (non-zero exit suppressed
# so callers can continue after logging the finding).
validate_ref() {
  local input="$1"
  local label="${2:-ref}"
  detect_hidden_chars "$input" "$label" || true
  sanitize_ref "$input"
}

# Provide a minimal no-op marker so callers can check we're present
sanitizer_version() {
  printf 'iccDEV-sanitizer-v4\n'
}

# End of sanitize-sed.sh
