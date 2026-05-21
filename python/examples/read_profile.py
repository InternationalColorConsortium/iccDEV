#!/usr/bin/env python3
"""
read_profile.py - Example: read and inspect an ICC profile header.

Usage:
    python read_profile.py [profile.icc]
"""

from pathlib import Path
import sys
import iccdev


def find_example_profile() -> str:
    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "Testing" / "Display" / "sRGB_D65_MAT.icc",
        repo_root / "Testing" / "Display" / "sRGB_D65_colorimetric.icc",
        repo_root / "Testing" / "Display" / "LCDDisplay.icc",
    ]
    for path in candidates:
        if path.is_file():
            return str(path)
    raise FileNotFoundError(
        "No bundled display profile found. Run Testing/CreateAllProfiles.sh first."
    )


def main():
    path = sys.argv[1] if len(sys.argv) >= 2 else find_example_profile()

    with iccdev.IccProfile(path) as profile:
        header = profile.header

        print(f"Profile: {path}")
        print(f"  Version:      {header.version_string}")
        print(f"  Size:         {header.size} bytes")
        print(f"  Device class: {header.device_class_name}")
        print(f"  Color space:  {header.color_space_name}")
        print(f"  PCS:          {header.pcs_name}")
        print(f"  Platform:     {iccdev.sig_to_str(header.platform)}")
        print(f"  CMM:          {iccdev.sig_to_str(header.cmm_id)}")
        print(f"  Creator:      {iccdev.sig_to_str(header.creator)}")
        print(f"  Intent:       {header.rendering_intent_name}")
        print(f"  Illuminant:   X={header.illuminant['X']:.4f} "
              f"Y={header.illuminant['Y']:.4f} "
              f"Z={header.illuminant['Z']:.4f}")
        print(f"  Date:         {header.date['year']}-{header.date['month']:02d}-"
              f"{header.date['day']:02d} {header.date['hours']:02d}:"
              f"{header.date['minutes']:02d}:{header.date['seconds']:02d}")
        print(f"  Profile ID:   {header.profile_id.hex()}")


if __name__ == "__main__":
    main()
