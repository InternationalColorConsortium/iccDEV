#!/usr/bin/env bash
###############################################################
#
# Copyright (c) 2025-2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Synchronize GitHub labels from .github/labels.yml.
#
###############################################################

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: .github/scripts/sync-labels.sh [--dry-run] [--repo OWNER/REPO] [--file PATH]

Synchronize the repository labels declared in .github/labels.yml. The GitHub
CLI must be authenticated through GH_TOKEN, GITHUB_TOKEN, or gh auth login.
USAGE
}

dry_run=false
labels_file=".github/labels.yml"
repo="${GH_REPOSITORY:-${GITHUB_REPOSITORY:-}}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run)
      dry_run=true
      shift
      ;;
    --file)
      if [[ $# -lt 2 ]]; then
        echo "[FAIL] --file requires a path" >&2
        exit 2
      fi
      labels_file="$2"
      shift 2
      ;;
    --repo)
      if [[ $# -lt 2 ]]; then
        echo "[FAIL] --repo requires OWNER/REPO" >&2
        exit 2
      fi
      repo="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[FAIL] Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "$repo" ]]; then
  echo "[FAIL] Set GH_REPOSITORY, GITHUB_REPOSITORY, or pass --repo OWNER/REPO" >&2
  exit 2
fi

if [[ ! "$repo" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]]; then
  echo "[FAIL] Invalid repository name: $repo" >&2
  exit 2
fi

if [[ ! -f "$labels_file" ]]; then
  echo "[FAIL] Label inventory not found: $labels_file" >&2
  exit 2
fi

if ! $dry_run && ! command -v gh >/dev/null 2>&1; then
  echo "[FAIL] gh is required unless --dry-run is used" >&2
  exit 2
fi

parse_labels() {
  if command -v ruby >/dev/null 2>&1; then
    ruby -e '
    require "yaml"

    file = ARGV.fetch(0)
    labels = YAML.safe_load_file(file, permitted_classes: [], aliases: false)
    abort("[FAIL] #{file} must contain a YAML list") unless labels.is_a?(Array)

    seen = {}
    labels.each_with_index do |entry, index|
      abort("[FAIL] label ##{index + 1} must be a mapping") unless entry.is_a?(Hash)

      name = entry.fetch("name", "").to_s
      color = entry.fetch("color", "").to_s.delete_prefix("#")
      description = entry.fetch("description", "").to_s

      abort("[FAIL] label ##{index + 1} has an empty name") if name.empty?
      abort("[FAIL] duplicate label: #{name}") if seen[name]
      seen[name] = true

      if name.length > 50 || name.match?(/[\r\n\t]/)
        abort("[FAIL] invalid label name: #{name.inspect}")
      end
      unless color.match?(/\A[0-9A-Fa-f]{6}\z/)
        abort("[FAIL] invalid color for #{name}: #{color.inspect}")
      end
      if description.length > 100 || description.match?(/[\r\n\t]/)
        abort("[FAIL] invalid description for #{name}")
      end

      puts [name, color.downcase, description].join("\t")
    end
    ' "$labels_file"
    return
  fi

  if command -v python3 >/dev/null 2>&1 &&
     python3 -c 'import yaml' >/dev/null 2>&1; then
    python3 -c '
import re
import sys
import yaml

file = sys.argv[1]
with open(file, "r", encoding="utf-8") as handle:
    labels = yaml.safe_load(handle)
if not isinstance(labels, list):
    raise SystemExit(f"[FAIL] {file} must contain a YAML list")

seen = set()
for index, entry in enumerate(labels, 1):
    if not isinstance(entry, dict):
        raise SystemExit(f"[FAIL] label #{index} must be a mapping")

    name = str(entry.get("name", ""))
    color = str(entry.get("color", "")).removeprefix("#")
    description = str(entry.get("description", ""))

    if not name:
        raise SystemExit(f"[FAIL] label #{index} has an empty name")
    if name in seen:
        raise SystemExit(f"[FAIL] duplicate label: {name}")
    seen.add(name)

    if len(name) > 50 or re.search(r"[\r\n\t]", name):
        raise SystemExit(f"[FAIL] invalid label name: {name!r}")
    if not re.fullmatch(r"[0-9A-Fa-f]{6}", color):
        raise SystemExit(f"[FAIL] invalid color for {name}: {color!r}")
    if len(description) > 100 or re.search(r"[\r\n\t]", description):
        raise SystemExit(f"[FAIL] invalid description for {name}")

    print("\t".join([name, color.lower(), description]))
    ' "$labels_file"
    return
  fi

  echo "[FAIL] ruby or python3 with PyYAML is required to parse $labels_file" >&2
  exit 2
}

count=0
while IFS=$'\t' read -r name color description; do
  [[ -n "$name" ]] || continue

  if $dry_run; then
    printf '[DRY-RUN] %s #%s %s\n' "$name" "$color" "$description"
  else
    gh label create "$name" \
      --repo "$repo" \
      --color "$color" \
      --description "$description" \
      --force >/dev/null
    printf '[OK] %s\n' "$name"
  fi

  count=$((count + 1))
done < <(parse_labels)

if $dry_run; then
  printf '[DRY-RUN] validated %d labels from %s for %s\n' "$count" "$labels_file" "$repo"
else
  printf '[OK] synced %d labels from %s for %s\n' "$count" "$labels_file" "$repo"
fi
