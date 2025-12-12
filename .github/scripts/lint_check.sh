#!/usr/bin/env bash
set -euo pipefail

git config --global --add safe.directory "${GITHUB_WORKSPACE:-$(pwd)}"
git config --global credential.helper ""

# Clear the in-shell GITHUB_TOKEN
unset GITHUB_TOKEN || true

mkdir -p lint_reports

# Initialize/clear reports
: > lint_reports/clang_tidy.txt
: > lint_reports/clang_format.txt

# Determine files to check safely (avoid unbound variable with set -u)
# Priority:
#  - the `files` env var (or FILES)
#  - changed files vs. base ref (if available)
#  - HEAD~1..HEAD diff (fallback)
#  - all tracked files (final fallback)
files="${files:-${FILES:-}}"

if [ -z "${files}" ]; then
  if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if [ -n "${GITHUB_BASE_REF:-}" ]; then
      base_ref="$GITHUB_BASE_REF"
      # Validate base_ref against allowlist (e.g., only allow 'master' and 'main')
      case "${base_ref}" in
        master|main)
          # best-effort fetch of base (ignore failures)
          git fetch --no-tags --depth=1 origin "${base_ref}" >/dev/null 2>&1 || true
          files="$(git diff --name-only --diff-filter=ACMRTUXB "origin/${base_ref}" HEAD 2>/dev/null || true)"
          ;;
        *)
          echo "Untrusted base_ref '${base_ref}' - skipping git fetch/diff." >&2
          ;;
      esac
    fi

    if [ -z "${files}" ]; then
      files="$(git diff --name-only --diff-filter=ACMRTUXB HEAD~1 HEAD 2>/dev/null || true)"
    fi

    if [ -z "${files}" ]; then
      files="$(git ls-files 2>/dev/null || true)"
    fi

    # Keep only C/C++ source/header files
    files="$(printf '%s\n' "${files}" | grep -E '\.(c|cpp|cc|cxx|h|hpp)$' || true)"
  fi
fi

# Safely handle empty or multi-line lists; loop per-line to support spaces in filenames
if [ -z "${files}" ]; then
  echo "No files to check; skipping clang-format." >> lint_reports/clang_format.txt
else
  fail=0
  while IFS= read -r f; do
    [ -z "${f}" ] && continue

    if [ ! -f "${f}" ]; then
      echo "Skipping missing file: ${f}" >> lint_reports/clang_format.txt
      continue
    fi

    # clang-format in check mode; capture output
    if ! clang-format --dry-run --Werror "${f}" 2>&1 | tee -a lint_reports/clang_format.txt; then
      echo "${f}" >> lint_reports/clang_format.txt
      fail=1
    fi
  done <<EOF
${files}
EOF

  if [ "${fail}" -eq 0 ]; then
    echo "All files properly formatted." >> lint_reports/clang_format.txt
  else
    echo "Some files are not properly formatted. See above for details." >> lint_reports/clang_format.txt
  fi
fi