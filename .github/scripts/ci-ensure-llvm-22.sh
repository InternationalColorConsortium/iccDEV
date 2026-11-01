#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2026 International Color Consortium.
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# https://color.org
#
# Ensure LLVM 22 packages are available on the rolling ubuntu-latest runner.
# Call after ci-safe-apt-update.sh, before apt-get install.
###############################################################################

set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 clang-22 [llvm-22|clang-tools-22|libclang-rt-22-dev ...]" >&2
  exit 2
fi

missing=()
for package in "$@"; do
  case "$package" in
    clang-22|clang-tools-22|llvm-22|libclang-rt-22-dev) ;;
    *) echo "Unsupported LLVM 22 package: $package" >&2; exit 2 ;;
  esac
  if ! apt-cache show "$package" 2>/dev/null |
      grep -Fx "Package: $package" >/dev/null; then
    missing+=("$package")
  fi
done

if [ "${#missing[@]}" -eq 0 ]; then
  echo "[OK] LLVM 22 packages available from runner APT sources: $*"
  exit 0
fi

# shellcheck disable=SC1091
. /etc/os-release
if [ "$ID" != ubuntu ] || [ "${VERSION_CODENAME:-}" != noble ]; then
  echo "LLVM 22 packages unavailable for Ubuntu ${VERSION_CODENAME:-unknown}: ${missing[*]}" >&2
  exit 1
fi

llvm_tmp="${RUNNER_TEMP:-/tmp}/iccdev-llvm-22"
mkdir -p "$llvm_tmp"
llvm_key="$llvm_tmp/llvm-snapshot.gpg.key"
llvm_keyring="$llvm_tmp/llvm-snapshot.gpg"
curl --fail --location --silent --show-error \
  https://apt.llvm.org/llvm-snapshot.gpg.key --output "$llvm_key"
llvm_fingerprint="$(gpg --show-keys --with-colons "$llvm_key" |
  awk -F: '$1 == "fpr" {print $10; exit}')"
if [ "$llvm_fingerprint" != "6084F3CF814B57C1CF12EFD515CF4D18AF4F7421" ]; then
  echo "Unexpected LLVM APT key fingerprint: $llvm_fingerprint" >&2
  exit 1
fi
gpg --dearmor --yes --output "$llvm_keyring" "$llvm_key"
sudo install -m 0644 "$llvm_keyring" /usr/share/keyrings/llvm-snapshot.gpg
printf '%s\n' \
  'deb [signed-by=/usr/share/keyrings/llvm-snapshot.gpg] https://apt.llvm.org/noble/ llvm-toolchain-noble-22 main' |
  sudo tee /etc/apt/sources.list.d/llvm-toolchain-22.list >/dev/null
bash "$(dirname "$0")/ci-safe-apt-update.sh"

for package in "${missing[@]}"; do
  if ! apt-cache show "$package" 2>/dev/null |
      grep -Fx "Package: $package" >/dev/null; then
    echo "LLVM 22 package unavailable after adding signed Noble source: $package" >&2
    exit 1
  fi
done
echo "[OK] LLVM 22 packages available from signed Noble source: ${missing[*]}"
