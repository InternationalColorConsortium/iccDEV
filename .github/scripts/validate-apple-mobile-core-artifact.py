#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Validate the Apple static-core artifact before latest-release packaging."""

from __future__ import annotations

import argparse
from pathlib import Path


PLATFORMS = {
    "ios": ("iphoneos", "iphonesimulator"),
    "tvos": ("appletvos", "appletvsimulator"),
    "watchos": ("watchos", "watchsimulator"),
    "visionos": ("xros", "xrsimulator"),
}
COMPONENTS = {
    "core": (("IccProfLib2-static", "IccProfLibVer.h"),),
    "extended-core": (
        ("IccProfLib2-static", "IccProfLibVer.h"),
        ("IccXML2-static", "IccLibXMLVer.h"),
        ("IccJSON2-static", "IccLibJSONVer.h"),
        ("IccConnect2-static", "IccLibConnectVer.h"),
    ),
}


def expected_presets() -> dict[str, str]:
    expected: dict[str, str] = {}
    for platform, (device_sdk, simulator_sdk) in PLATFORMS.items():
        for destination, sdk in (("device", device_sdk), ("simulator", simulator_sdk)):
            for flavor in COMPONENTS:
                expected[f"apple-{platform}-{destination}-{flavor}"] = sdk
    return expected


def validate_artifact(root: Path) -> int:
    manifest = root / "manifest.txt"
    if not manifest.is_file() or not manifest.stat().st_size:
        raise ValueError("Apple mobile core manifest.txt is missing or empty")

    expected = expected_presets()
    seen: set[str] = set()
    built_count = 0
    for line_number, line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        fields = line.split("|")
        if len(fields) != 3:
            raise ValueError(f"Invalid Apple manifest line {line_number}")
        preset, sdk, result = fields
        if preset not in expected or sdk != expected[preset] or preset in seen:
            raise ValueError(f"Unexpected or duplicate Apple preset at line {line_number}: {preset}")
        seen.add(preset)

        output_dir = root / preset
        if result == "skipped: SDK unavailable":
            if output_dir.exists():
                raise ValueError(f"Skipped Apple preset unexpectedly has an output directory: {preset}")
            continue
        if result != "built":
            raise ValueError(f"Invalid Apple preset outcome at line {line_number}: {result}")
        built_count += 1
        flavor = "extended-core" if preset.endswith("-extended-core") else "core"
        for library, header in COMPONENTS[flavor]:
            for filename in (f"lib{library}.a", header):
                path = output_dir / filename
                if not path.is_file() or not path.stat().st_size:
                    raise ValueError(f"Missing Apple mobile core payload: {preset}/{filename}")

    missing = set(expected) - seen
    if missing:
        raise ValueError(f"Apple mobile core manifest omitted presets: {', '.join(sorted(missing))}")
    if built_count == 0:
        raise ValueError("Apple mobile core artifact contains no built SDK presets")
    return built_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact_dir", type=Path)
    args = parser.parse_args()
    try:
        count = validate_artifact(args.artifact_dir)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"ERROR: {exc}\n")
    print(f"Validated {count} Apple mobile core SDK presets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
