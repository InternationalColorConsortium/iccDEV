#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

"""Issue #2685: read-only JPEG inspection and unchanged raw extraction."""

import argparse
import hashlib
import pathlib
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, type=pathlib.Path)
    args = parser.parse_args()
    tool = args.tool.resolve(strict=True)
    repo = pathlib.Path(__file__).resolve().parents[2]
    profile = (repo / "Testing" / "sRGB_v4_ICC_preference.icc").read_bytes()

    with tempfile.TemporaryDirectory(prefix="iccdev-jpeg-inspection-") as tmp:
        root = pathlib.Path(tmp)

        def run(*argv, success=True, text=None):
            result = subprocess.run(
                [str(tool), *map(str, argv)], cwd=root,
                capture_output=True, text=True, timeout=15,
            )
            output = result.stdout + result.stderr
            expected = result.returncode == 0 if success else 0 < result.returncode < 128
            assert expected, output
            if text:
                assert text in output, output
            return output

        def carrier(payload, split=False):
            parts = [payload[:100], payload[100:]] if split else [payload]
            data = bytearray(b"\xff\xd8")
            for i, part in enumerate(parts, 1):
                body = b"ICC_PROFILE\0" + bytes((i, len(parts))) + part
                data += b"\xff\xe2" + (len(body) + 2).to_bytes(2, "big") + body
            return bytes(data) + b"\xff\xd9"

        version = run("--version")
        assert re.fullmatch(r"iccJpegDump IccProfLib_build=\S+\n", version), version
        help_text = run("--help", text="Usage:")
        assert version.strip() in help_text, help_text
        assert "iccJpegDump <input.jpg>" in help_text, help_text

        for split in (False, True):
            image = root / ("split.jpg" if split else "single.jpg")
            image.write_bytes(carrier(profile, split))
            before = hashlib.sha256(image.read_bytes()).digest()
            files = set(root.iterdir())
            output = run(image, text=f"ICC profile size: {len(profile)} bytes")
            assert "Color Space:" in output and "Profile Version:" in output, output
            assert set(root.iterdir()) == files, "Inspection created output files"
            assert hashlib.sha256(image.read_bytes()).digest() == before
            extracted = root / ("split.icc" if split else "single.icc")
            run(image, extracted)
            assert extracted.read_bytes() == profile
            run(image, image, success=False)
            assert hashlib.sha256(image.read_bytes()).digest() == before

        no_icc = root / "no-icc.jpg"
        no_icc.write_bytes(b"\xff\xd8\xff\xd9")
        run(no_icc, text="No ICC_PROFILE APP2 marker found")
        absent = root / "absent.icc"
        run(no_icc, absent, success=False)
        assert not absent.exists()

        for payload in (profile, b"A" * 140000):
            icc = root / "injection.icc"
            icc.write_bytes(payload)
            injected = root / "injected.jpg"
            run(no_icc, "--write-icc", icc, "--output", injected)
            restored = root / "restored.icc"
            run(injected, restored)
            assert restored.read_bytes() == payload

        replacement = root / "replacement.icc"
        replacement.write_bytes(b"replacement ICC payload")
        unrelated_app2 = b"\xff\xe2\x00\x08other!"
        for split in (False, True):
            tagged = root / ("tagged-split.jpg" if split else "tagged-single.jpg")
            scan_tail = b"\xff\xda\x00\x02\x00\xff\x00\x11\xff\xd0\x22\xff\xd9"
            tagged.write_bytes(b"\xff\xd8" + unrelated_app2 +
                               carrier(profile, split)[2:-2] + scan_tail)
            replaced = root / ("replaced-split.jpg" if split else "replaced-single.jpg")
            output = run(tagged, "--write-icc", replacement, "--output", replaced,
                         text=f"Replaced {2 if split else 1} existing ICC_PROFILE APP2 chunk(s)")
            assert "successfully injected" in output
            replaced_data = replaced.read_bytes()
            assert replaced_data.count(b"ICC_PROFILE\0") == 1
            assert unrelated_app2 in replaced_data
            assert replaced_data.endswith(scan_tail)
            extracted = root / ("replaced-split.icc" if split else "replaced-single.icc")
            run(replaced, extracted)
            assert extracted.read_bytes() == replacement.read_bytes()

        malformed = root / "malformed.jpg"
        malformed.write_bytes(carrier(b"not an ICC profile"))
        run(malformed, success=False, text="Failed to parse embedded ICC profile")
        raw = root / "raw.icc"
        run(malformed, raw)
        assert raw.read_bytes() == b"not an ICC profile"

        malformed.write_bytes(carrier(profile).replace(b"\x01\x01", b"\x01\x02", 1))
        run(malformed, success=False, text="missing chunk")
        malformed.write_bytes(b"\xff\xd8\xff\xe2\x00\x20short")
        run(malformed, success=False, text="truncated JPEG marker payload")
        malformed.write_bytes(b"\xff\xd8")
        run(malformed, success=False, text="missing SOS or EOI")
        malformed.write_bytes(b"not a JPEG")
        run(malformed, success=False, text="missing SOI")
        run(root / "missing.jpg", success=False, text="Cannot open input JPEG")
        print("[PASS] issue-2685 inspection, no writes, raw extraction, and failures")


if __name__ == "__main__":
    main()
