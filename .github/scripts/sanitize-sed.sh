#!/usr/bin/env bash
#------------------------------------------------------------------------------
# @file
# File:       sanitize-sed.sh
#
# Contains:   Implementation of sanitizer for BASH Shell.
#
# Version:    V1
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
# Copyright (©) 2024-2025 David H Hoyt. All rights reserved.
###############################################################
#                 https://srd.cx
#
# Last Updated: 02-JAN-2026 2100Z by David Hoyt
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

# _strip_ctrl_keep_newlines STRING
# Remove control characters except newline (0x0A). Also remove NUL.
_strip_ctrl_keep_newlines() {
  local s="$1"
  # remove CRs explicitly
  s="${s//$'\r'/}"
  # remove NUL and other C0 control chars except LF (0x0A), plus DEL (0x7F)
  # tr with octal escapes: delete \000-\011 \013 \014 \016-\037 \177
  # This keeps \n (LF) which is 012 octal.
  s="$(printf '%s' "$s" | tr -d '\000-\011\013\014\016-\037\177')"
  printf '%s' "$s"
}

# _strip_ctrl_remove_newlines STRING
# Remove control characters and newlines (useful for single-line outputs).
_strip_ctrl_remove_newlines() {
  local s="$1"
  # remove CRs and LFs
  s="${s//$'\r'/}"
  s="${s//$'\n'/ }"
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
  s="$(printf '%s' "$s" | sed -E 's#[^A-Za-z0-9._/-]#-#g')"
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

# Provide a minimal no-op marker so callers can check we're present
sanitizer_version() {
  printf 'iccDEV-sanitizer-v1\n'
}

# End of sanitize-sed.sh
