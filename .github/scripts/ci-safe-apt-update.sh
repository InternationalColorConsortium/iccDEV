#!/bin/bash
###############################################################################
# Disable nonessential third-party apt sources that can break GitHub-hosted
# Ubuntu runners, then run apt-get update.
###############################################################################

set -euo pipefail

disabled_dir="${RUNNER_TEMP:-/tmp}/iccdev-disabled-apt-sources"
sudo mkdir -p "$disabled_dir"

for source_file in /etc/apt/sources.list.d/*; do
  [ -e "$source_file" ] || continue
  if sudo grep -qs 'packages\.microsoft\.com' "$source_file"; then
    safe_name="$(basename "$source_file")"
    echo "[INFO] Disabling apt source with packages.microsoft.com: $safe_name"
    sudo mv "$source_file" "$disabled_dir/$safe_name.disabled"
  fi
done

sudo apt-get -o Acquire::Retries=3 -o Dpkg::Use-Pty=0 update -qq
