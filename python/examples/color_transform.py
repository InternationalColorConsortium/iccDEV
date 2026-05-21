#!/usr/bin/env python3
"""
color_transform.py - Example: apply a color transform between two profiles.

Usage:
    python color_transform.py [input.icc output.icc] [r g b]

If no pixel is given, transforms a small set of test pixels.
"""

from pathlib import Path
import sys
import iccdev


def find_example_profiles() -> tuple[str, str]:
    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "Testing" / "Display" / "sRGB_D65_MAT.icc",
        repo_root / "Testing" / "Display" / "sRGB_D65_colorimetric.icc",
        repo_root / "Testing" / "Display" / "LCDDisplay.icc",
    ]
    found = [str(path) for path in candidates if path.is_file()]
    if not found:
        raise FileNotFoundError(
            "No bundled display profile found. Run Testing/CreateAllProfiles.sh first."
        )
    if len(found) == 1:
        return found[0], found[0]
    return found[0], found[1]


def main():
    if len(sys.argv) == 1:
        input_profile, output_profile = find_example_profiles()
    elif len(sys.argv) >= 3:
        input_profile = sys.argv[1]
        output_profile = sys.argv[2]
    else:
        print("Usage: python color_transform.py [input.icc output.icc] [r g b]")
        sys.exit(1)

    with iccdev.IccCmm() as cmm:
        cmm.attach_profile(
            input_profile,
            intent=iccdev.RenderingIntent.Perceptual,
        )
        cmm.attach_profile(output_profile)
        cmm.begin()

        print(f"Transform: {cmm.src_space!r} ({cmm.src_channels}ch) "
              f"-> {cmm.dst_space!r} ({cmm.dst_channels}ch)")

        if len(sys.argv) >= 6:
            # Single pixel from command line
            pixel = [float(sys.argv[i]) for i in range(3, 3 + cmm.src_channels)]
            result = cmm.apply(pixel)
            print(f"  Input:  {pixel}")
            print(f"  Output: {[round(v, 6) for v in result]}")
        else:
            # Default test pixels for RGB
            test_pixels = [
                [0.0, 0.0, 0.0],  # Black
                [1.0, 1.0, 1.0],  # White
                [1.0, 0.0, 0.0],  # Red
                [0.0, 1.0, 0.0],  # Green
                [0.0, 0.0, 1.0],  # Blue
                [0.5, 0.5, 0.5],  # Mid-gray
            ]

            # Trim to actual source channels
            test_pixels = [p[:cmm.src_channels] for p in test_pixels]

            results = cmm.apply_multi(test_pixels)
            for pixel, result in zip(test_pixels, results):
                print(f"  {pixel} -> {[round(v, 6) for v in result]}")


if __name__ == "__main__":
    main()
