#!/usr/bin/env python3
"""Exercise libpng iCCP stream-completion and CRC-policy behavior."""

# Copyright (c) International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# 3. In the absence of prior written permission, the names "ICC" and "The
#    International Color Consortium" must not be used to imply that the ICC
#    organization endorses or promotes products derived from this software.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import base64
import binascii
import hashlib
import pathlib
import struct
import subprocess
import sys
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PROFILE_SHA256 = "42e12f4d6cb959f0242fa740c2547cd371ca60ec72f2ecd0926da035a2bd2f11"


def chunk(kind, payload, corrupt_crc=False):
    crc = binascii.crc32(kind + payload) & 0xFFFFFFFF
    if corrupt_crc:
        crc ^= 1
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def make_png(profile, compression, corrupt_crc=False):
    ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
    iccp = b"icc\x00\x00" + compression
    scanline = zlib.compress(b"\x00\x00\x00\x00")
    return b"".join(
        (
            PNG_SIGNATURE,
            chunk(b"IHDR", ihdr),
            chunk(b"iCCP", iccp, corrupt_crc),
            chunk(b"IDAT", scanline),
            chunk(b"IEND", b""),
        )
    )


def read_iccp(path):
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"not a PNG file: {path}")
    offset = len(PNG_SIGNATURE)
    while offset < len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"iCCP":
            name_end = payload.index(0)
            if payload[name_end + 1] != 0:
                raise ValueError(f"unsupported iCCP compression method: {path}")
            return zlib.decompress(payload[name_end + 2 :])
        if kind == b"IEND":
            break
    return None


def write_fixtures(work_dir, profile):
    compressed = zlib.compress(profile, level=9)
    fixtures = {
        "valid": make_png(profile, compressed),
        "extra-output": make_png(profile, zlib.compress(profile + b"\x00", level=9)),
        "bad-adler": make_png(profile, compressed[:-1] + bytes((compressed[-1] ^ 1,))),
        "missing-adler": make_png(profile, compressed[:-4]),
        "bad-crc": make_png(profile, compressed, corrupt_crc=True),
    }
    fixture_dir = work_dir / "fixtures"
    fixture_dir.mkdir(parents=True, exist_ok=True)
    for name, payload in fixtures.items():
        (fixture_dir / f"{name}.png").write_bytes(payload)
    return fixture_dir


def run_pngtest(pngtest, input_path, output_path, relaxed=False):
    output_path.unlink(missing_ok=True)
    if relaxed:
        (output_path.parent / "pngout.png").unlink(missing_ok=True)
    command = [str(pngtest)]
    if relaxed:
        command.extend(("--relaxed", str(input_path)))
    else:
        command.extend((str(input_path), str(output_path)))
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        cwd=output_path.parent,
    )
    log = result.stdout + result.stderr
    if result.returncode != 0:
        raise RuntimeError(
            f"pngtest failed for {input_path.name} ({result.returncode}):\n{log}"
        )
    if "AddressSanitizer" in log or "runtime error:" in log:
        raise RuntimeError(f"sanitizer diagnostic for {input_path.name}:\n{log}")
    if relaxed:
        (output_path.parent / "pngout.png").replace(output_path)
    return log


def expect_profile(path, expected, context):
    actual = read_iccp(path)
    if actual != expected:
        actual_size = 0 if actual is None else len(actual)
        raise AssertionError(
            f"{context}: expected {len(expected)} profile bytes, got {actual_size}"
        )


def expect_absent(path, context):
    actual = read_iccp(path)
    if actual is not None:
        raise AssertionError(f"{context}: retained {len(actual)} profile bytes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("fixed", "vulnerable"), required=True)
    parser.add_argument("--pngtest", type=pathlib.Path, required=True)
    parser.add_argument("--profile", type=pathlib.Path, required=True)
    parser.add_argument("--work-dir", type=pathlib.Path, required=True)
    args = parser.parse_args()

    encoded_profile = b"".join(args.profile.read_bytes().split())
    profile = base64.b64decode(encoded_profile, validate=True)
    digest = hashlib.sha256(profile).hexdigest()
    if digest != PROFILE_SHA256:
        raise AssertionError(f"profile digest mismatch: {digest}")
    if len(profile) != struct.unpack(">I", profile[:4])[0]:
        raise AssertionError("profile header size does not match fixture length")

    args.work_dir.mkdir(parents=True, exist_ok=True)
    fixture_dir = write_fixtures(args.work_dir, profile)

    valid_output = args.work_dir / "valid-output.png"
    run_pngtest(args.pngtest, fixture_dir / "valid.png", valid_output)
    expect_profile(valid_output, profile, "valid control")

    invalid_names = ("extra-output", "bad-adler", "missing-adler", "bad-crc")
    for name in invalid_names:
        output = args.work_dir / f"{name}-output.png"
        log = run_pngtest(args.pngtest, fixture_dir / f"{name}.png", output)
        if args.mode == "vulnerable":
            expect_profile(output, profile, f"vulnerable {name}")
        else:
            expect_absent(output, f"fixed {name}")
            if "iCCP:" not in log:
                raise AssertionError(f"fixed {name}: missing iCCP diagnostic")

    relaxed_output = args.work_dir / "bad-crc-relaxed-output.png"
    run_pngtest(
        args.pngtest,
        fixture_dir / "bad-crc.png",
        relaxed_output,
        relaxed=True,
    )
    expect_profile(relaxed_output, profile, "relaxed bad-crc policy")

    print(
        f"PASS: libpng iCCP {args.mode} contract; "
        f"profile={len(profile)} sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, OSError, RuntimeError, ValueError, zlib.error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
