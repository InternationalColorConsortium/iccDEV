#!/usr/bin/env bash
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# See LICENSE.md in the repository root for the BSD-3-Clause license.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python="${ICCDEV_PYTHON:-python3}"
if ! command -v "$python" >/dev/null 2>&1; then
  printf '[FAIL] Python 3.9 or newer is required: %s\n' "$python" >&2
  exit 2
fi
exec "$python" "$script_dir/iccdev_xml_json_roundtrip_qa.py" "$@"
