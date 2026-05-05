#!/usr/bin/env bash
#------------------------------------------------------------------------------
# Generate a report-only binary artifact inventory for staged iccDEV releases.
#------------------------------------------------------------------------------

set -euo pipefail

usage() {
  printf 'Usage: %s <input-directory> <output-directory>\n' "$(basename "$0")" >&2
}

if [ "$#" -ne 2 ]; then
  usage
  exit 2
fi

input_dir="$1"
output_dir="$2"
strict="${BINARY_INVENTORY_STRICT:-0}"

if [ ! -d "$input_dir" ]; then
  printf 'ERROR: input directory not found: %s\n' "$input_dir" >&2
  exit 1
fi

mkdir -p "$output_dir"
input_dir="$(cd "$input_dir" && pwd -P)"
output_dir="$(cd "$output_dir" && pwd -P)"

inventory_csv="$output_dir/binary-inventory.csv"
summary_md="$output_dir/binary-hardening.md"
needed_txt="$output_dir/needed-libraries.txt"
sha256_txt="$output_dir/sha256sum.txt"
hygiene_txt="$output_dir/strings-hygiene.txt"
failures_txt="$output_dir/failures.txt"

: > "$needed_txt"
: > "$sha256_txt"
: > "$hygiene_txt"
: > "$failures_txt"

tmp_hygiene="$(mktemp)"
cleanup() {
  rm -f "$tmp_hygiene"
}
trap cleanup EXIT

csv_escape() {
  local value="$1"
  value="${value//$'\r'/ }"
  value="${value//$'\n'/ }"
  value="${value//\"/\"\"}"
  printf '"%s"' "$value"
}

csv_row() {
  local separator=""
  local value
  for value in "$@"; do
    printf '%s' "$separator" >> "$inventory_csv"
    csv_escape "$value" >> "$inventory_csv"
    separator=","
  done
  printf '\n' >> "$inventory_csv"
}

record_failure() {
  local path="$1"
  local message="$2"
  failure_count=$((failure_count + 1))
  printf '%s: %s\n' "$path" "$message" >> "$failures_txt"
}

is_absolute_runtime_path() {
  local value="$1"
  case "$value" in
    /*|*":/"*) return 0 ;;
    *) return 1 ;;
  esac
}

csv_row \
  "path" "size" "sha256" "file_type" "executable" "elf" "elf_type" \
  "nx" "relro" "bind_now" "canary_ref" "build_id" "runpath" "rpath"

total_files=0
executable_files=0
elf_files=0
pie_or_shared=0
nx_yes=0
relro_yes=0
bind_now_yes=0
canary_yes=0
build_id_yes=0
runpath_hits=0
rpath_hits=0
hygiene_hits=0
failure_count=0

while IFS= read -r -d '' path; do
  total_files=$((total_files + 1))
  rel="${path#"$input_dir"/}"
  size="$(wc -c < "$path" | tr -d ' ')"
  sha256="$(sha256sum "$path" | awk '{print $1}')"
  file_type="$(file -b "$path" | tr ',' ';')"
  executable="no"
  if [ -x "$path" ]; then
    executable="yes"
    executable_files=$((executable_files + 1))
  fi

  printf '%s  %s\n' "$sha256" "$rel" >> "$sha256_txt"

  elf="no"
  elf_type=""
  nx=""
  relro=""
  bind_now=""
  canary_ref=""
  build_id=""
  runpath=""
  rpath=""

  if grep -q 'ELF' <<< "$file_type"; then
    elf="yes"
    elf_files=$((elf_files + 1))
    elf_header="$(readelf -h "$path" 2>/dev/null || true)"
    program_headers="$(readelf -W -l "$path" 2>/dev/null || true)"
    dynamic_section="$(readelf -W -d "$path" 2>/dev/null || true)"
    symbol_table="$(readelf -Ws "$path" 2>/dev/null || true)"
    notes="$(readelf -n "$path" 2>/dev/null || true)"

    elf_type="$(awk '/Type:/{print $2; exit}' <<< "$elf_header")"
    if [ "$elf_type" = "DYN" ]; then
      pie_or_shared=$((pie_or_shared + 1))
    elif [ "$strict" = "1" ] && [ "$executable" = "yes" ]; then
      record_failure "$rel" "ELF executable is not PIE"
    fi

    stack_flags="$(awk '/GNU_STACK/{print $7; exit}' <<< "$program_headers")"
    if [ -z "$stack_flags" ]; then
      nx="unknown"
    elif [[ "$stack_flags" == *E* ]]; then
      nx="no"
      if [ "$strict" = "1" ]; then
        record_failure "$rel" "ELF stack is executable"
      fi
    else
      nx="yes"
      nx_yes=$((nx_yes + 1))
    fi

    if grep -q 'GNU_RELRO' <<< "$program_headers"; then
      relro="yes"
      relro_yes=$((relro_yes + 1))
    else
      relro="no"
      if [ "$strict" = "1" ]; then
        record_failure "$rel" "ELF artifact lacks GNU_RELRO"
      fi
    fi

    if grep -Eq 'BIND_NOW|FLAGS.*NOW' <<< "$dynamic_section"; then
      bind_now="yes"
      bind_now_yes=$((bind_now_yes + 1))
    else
      bind_now="no"
      if [ "$strict" = "1" ]; then
        record_failure "$rel" "ELF artifact lacks BIND_NOW"
      fi
    fi

    if grep -q '__stack_chk_fail' <<< "$symbol_table"; then
      canary_ref="yes"
      canary_yes=$((canary_yes + 1))
    else
      canary_ref="no"
    fi

    if grep -q 'Build ID' <<< "$notes"; then
      build_id="yes"
      build_id_yes=$((build_id_yes + 1))
    else
      build_id="no"
      if [ "$strict" = "1" ]; then
        record_failure "$rel" "ELF artifact lacks Build ID"
      fi
    fi

    runpath="$(awk -F'[][]' '/RUNPATH/{print $2; exit}' <<< "$dynamic_section")"
    rpath="$(awk -F'[][]' '/RPATH/{print $2; exit}' <<< "$dynamic_section")"
    if [ -n "$runpath" ]; then
      runpath_hits=$((runpath_hits + 1))
      if [ "$strict" = "1" ] && is_absolute_runtime_path "$runpath"; then
        record_failure "$rel" "ELF RUNPATH contains an absolute path"
      fi
    fi
    if [ -n "$rpath" ]; then
      rpath_hits=$((rpath_hits + 1))
      if [ "$strict" = "1" ] && is_absolute_runtime_path "$rpath"; then
        record_failure "$rel" "ELF RPATH contains an absolute path"
      fi
    fi

    awk -F'[][]' '/NEEDED/{print path ": " $2}' path="$rel" \
      <<< "$dynamic_section" >> "$needed_txt"
  fi

  if strings -a "$path" | grep -E \
    '(/home/runner/work/|/Users/runner/work/|GITHUB_TOKEN|ACTIONS_RUNTIME_TOKEN|ACTIONS_ID_TOKEN|BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY|AWS_SECRET_ACCESS_KEY|password=|secret=)' \
    > "$tmp_hygiene"; then
    hygiene_hits=$((hygiene_hits + 1))
    if [ "$strict" = "1" ] &&
      grep -Eq 'ELF|Mach-O|PE32|WebAssembly' <<< "$file_type" &&
      grep -Eq '(/home/runner/work/|/Users/runner/work/).*/Build/' "$tmp_hygiene"; then
      record_failure "$rel" "binary contains an absolute CI build path"
    fi
    {
      printf '[%s]\n' "$rel"
      sed -n '1,20p' "$tmp_hygiene"
      printf '\n'
    } >> "$hygiene_txt"
  fi

  csv_row \
    "$rel" "$size" "$sha256" "$file_type" "$executable" "$elf" "$elf_type" \
    "$nx" "$relro" "$bind_now" "$canary_ref" "$build_id" "$runpath" "$rpath"
done < <(find "$input_dir" -type f -print0 | sort -z)

sort -u "$needed_txt" -o "$needed_txt"

if [ "$total_files" -eq 0 ]; then
  printf 'ERROR: no files found under %s\n' "$input_dir" >&2
  exit 1
fi

{
  printf '# iccDEV Binary Artifact Inventory\n\n'
  printf '| Field | Value |\n'
  printf '|---|---:|\n'
  printf '| Input files | %s |\n' "$total_files"
  printf '| Executable files | %s |\n' "$executable_files"
  printf '| ELF files | %s |\n' "$elf_files"
  printf '| ELF type DYN | %s |\n' "$pie_or_shared"
  printf '| NX yes | %s |\n' "$nx_yes"
  printf '| RELRO yes | %s |\n' "$relro_yes"
  printf '| BIND_NOW yes | %s |\n' "$bind_now_yes"
  printf '| Stack canary references | %s |\n' "$canary_yes"
  printf '| Build ID yes | %s |\n' "$build_id_yes"
  printf '| RUNPATH entries | %s |\n' "$runpath_hits"
  printf '| RPATH entries | %s |\n' "$rpath_hits"
  printf '| Hygiene hits | %s |\n' "$hygiene_hits"
  printf '\n'
  printf 'Generated files:\n\n'
  printf '%s\n' "- \`binary-inventory.csv\`"
  printf '%s\n' "- \`needed-libraries.txt\`"
  printf '%s\n' "- \`sha256sum.txt\`"
  printf '%s\n' "- \`strings-hygiene.txt\`"
  printf '%s\n' "- \`failures.txt\`"
} > "$summary_md"

if [ "$failure_count" -gt 0 ]; then
  {
    printf '\n## Strict-mode failures\n\n'
    printf '```text\n'
    sed -n '1,200p' "$failures_txt"
    printf '```\n'
  } >> "$summary_md"
else
  printf 'No strict-mode failures recorded.\n' > "$failures_txt"
fi

if [ ! -s "$needed_txt" ]; then
  printf 'No ELF NEEDED library entries recorded.\n' > "$needed_txt"
fi

if [ ! -s "$hygiene_txt" ]; then
  printf 'No string hygiene findings recorded.\n' > "$hygiene_txt"
fi

printf 'Binary inventory written to %s\n' "$output_dir"
printf 'ELF files: %s, executables: %s, hygiene hits: %s\n' \
  "$elf_files" "$executable_files" "$hygiene_hits"

if [ "$strict" = "1" ] && [ "$failure_count" -gt 0 ]; then
  printf 'ERROR: strict binary inventory failures detected\n' >&2
  exit 1
fi
