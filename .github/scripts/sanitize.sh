#!/usr/bin/env bash
#
set -euo pipefail

# canonical sanitize_line
sanitize_line() {
  local input="$1"
  local out orig_len san_len max
  max=512

  orig_len=${#input}

  # 1) Strip explicit NULs
  out="${input//$'\0'/}"

  # 2) Drop ASCII control chars except TAB
  out="$(printf '%s' "$out" | tr -d '\001-\010\013\014\016-\037\177')"

  # 3) Remove ANSI escapes and zero-width / bidi ranges using perl (portable on GH runners)
  out="$(printf '%s' "$out" | perl -CSD -pe 's/\e(\[[0-9;]*[A-Za-z]|\][^\a]*\a)//g; s/[\x{200B}-\x{200F}\x{2028}\x{2029}\x{202A}-\x{202E}\x{2066}-\x{2069}\x{FEFF}]//g')"

  # 4) Normalize CR/LF to spaces (single-line summaries)
  out="${out//$'\r'/ }"
  out="${out//$'\n'/ }"

  # 5) HTML escape (first)
  out="${out//&/&amp;}"
  out="${out//</&lt;}"
  out="${out//>/&gt;}"
  out="${out//\"/&quot;}"
  out="${out//\'/&#39;}"
  out="${out//\`/&#96;}"

  # 6) Neutralize common markdown punctuation to avoid layout/formatting abuse
  out="$(printf '%s' "$out" | sed \
    -e 's/\\/\\\\/g' \
    -e 's/\*/\\*/g' \
    -e 's/_/\\_/g' \
    -e 's/\[/\\[/g' \
    -e 's/\]/\\]/g' \
    -e 's/(/\\(/g' \
    -e 's/)/\\)/g' \
    -e 's/!/\\!/g')"

  # 7) Enforce UTF-8 validity
  if ! printf '%s' "$out" | iconv -f UTF-8 -t UTF-8 >/dev/null 2>&1; then
    out="[invalid utf-8]"
  fi

  # 8) Final length cap (character-safe)
  [[ ${#out} -gt $max ]] && out="${out:0:max}…"

  san_len=${#out}
  if [[ -n "${SANITIZE_DEBUG:-}" ]]; then
    {
      printf '[sanitize] orig_len=%d san_len=%d\n' "$orig_len" "$san_len" >&2
      printf '[sanitize] orig_hex='
      printf '%s' "$input" | od -An -tx1 | tr -d ' \n' >&2
      printf '\n[sanitize] san_hex='
      printf '%s' "$out" | od -An -tx1 | tr -d ' \n' >&2
      printf '\n' >&2
    }
  fi

  printf '%s' "$out"
}

# sanitize_print: alias for sanitize_line (keeps previous API)
sanitize_print() {
  printf '%s' "$(sanitize_line "$1")"
}

# esc_md: markdown-specific escape wrapper around canonical sanitize_line
esc_md() {
  local input="$1"
  local out max orig_len san_len
  max=512
  orig_len=${#input}

  out="$(sanitize_line "$input")"
  # additional markdown-focused escapes (backtick)
  out="$(printf '%s' "$out" | sed \
    -e 's/`/\\`/g')"
  if (( ${#out} > max )); then
    out="${out:0:max}…"
  fi

  san_len=${#out}
  if [[ -n "${SANITIZE_DEBUG:-}" ]]; then
    {
      printf '[esc_md] orig_len=%d san_len=%d\n' "$orig_len" "$san_len" >&2
      printf '[esc_md] orig_hex='
      printf '%s' "$input" | od -An -tx1 | tr -d ' \n' >&2
      printf '\n[esc_md] san_hex='
      printf '%s' "$out" | od -An -tx1 | tr -d ' \n' >&2
      printf '\n' >&2
    }
  fi

  printf '%s' "$out"
}

# Export functions for use when script is sourced in bash
export -f sanitize_line sanitize_print esc_md >/dev/null 2>&1 || true
