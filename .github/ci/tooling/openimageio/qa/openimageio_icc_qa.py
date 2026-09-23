#!/usr/bin/env python3
"""Reproduce and verify pinned OpenImageIO ICC/EXIF/JPEG2000 faults."""

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
import hashlib
import os
import pathlib
import struct
import subprocess
import sys


BASE_JPEG = bytes.fromhex(
    "ffd8ffe000104a46494600010100000100010000ffdb004300010101010101010101"
    "01010101010102010101010102010101020202020202020202030304030303030302"
    "020304030304040404040203050504040504040404ffc0000b080001000101011100"
    "ffc4001f0000010501010101010100000000000000000102030405060708090a0bff"
    "c400b5100002010303020403050504040000017d0102030004110512213141061351"
    "6107227114328191a1082342b1c11552d1f02433627282090a161718191a25262728"
    "292a3435363738393a434445464748494a535455565758595a636465666768696a73"
    "7475767778797a838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2"
    "b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8"
    "e9eaf1f2f3f4f5f6f7f8f9faffda0008010100003f00ff003ffaffd9"
)
SANITIZER_MARKERS = ("AddressSanitizer", "runtime error:")
PROGRESSION_ORDERS = ("LRCP", "RLCP", "RPCL", "PCRL", "CPRL")


def icc_header(profile_size):
    header = bytearray(128)
    struct.pack_into(">I", header, 0, profile_size & 0xFFFFFFFF)
    header[36:40] = b"acsp"
    return header


def icc_tag_table(tags):
    table = bytearray(struct.pack(">I", len(tags)))
    for signature, offset, size in tags:
        table += signature + struct.pack(">II", offset, size)
    return table


def jpeg_with_icc(profile):
    body = b"ICC_PROFILE\0" + bytes((1, 1)) + profile
    app2 = b"\xff\xe2" + struct.pack(">H", len(body) + 2) + body
    return BASE_JPEG[:2] + app2 + BASE_JPEG[2:]


def make_oversized_profile():
    return bytes(icc_header(0x7FFFFFFF) + icc_tag_table([]))


def make_cross_tag_profile():
    table_size = 4 + 2 * 12
    mluc_offset = 128 + table_size
    text = "Leak".encode("utf-16-be")
    mluc = bytearray(b"mluc\0\0\0\0")
    mluc += struct.pack(">II", 1, 12)
    mluc += b"en\0\0" + struct.pack(">II", len(text), 28) + text
    total = mluc_offset + len(mluc)
    tags = (
        (b"desc", mluc_offset, 4),
        (b"targ", mluc_offset + 4, len(mluc) - 4),
    )
    profile = icc_header(total) + icc_tag_table(tags) + mluc
    if len(profile) != total:
        raise AssertionError("cross-tag profile size mismatch")
    return bytes(profile)


def run(command, env=None):
    result = subprocess.run(
        [str(part) for part in command],
        check=False,
        capture_output=True,
        text=True,
        env=env,
    )
    return result.returncode, result.stdout + result.stderr


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def require_clean(log, context):
    for marker in SANITIZER_MARKERS:
        require(marker not in log, f"{context}: sanitizer diagnostic: {marker}")


def require_metadata(iinfo, image, expected, env, context):
    code, log = run((iinfo, "-v", image), env)
    require(code == 0, f"{context}: iinfo failed ({code}):\n{log}")
    require_clean(log, context)
    for value in expected:
        require(value in log, f"{context}: missing metadata {value!r}:\n{log}")
    return log


def write_fixtures(work_dir):
    oversized = work_dir / "corrupt-icc-oversized.jpg"
    cross_tag = work_dir / "corrupt-icc-mluc-cross-tag.jpg"
    oversized.write_bytes(jpeg_with_icc(make_oversized_profile()))
    cross_tag.write_bytes(jpeg_with_icc(make_cross_tag_profile()))
    return oversized, cross_tag


def check_exif(mode, oiiotool, iinfo, work_dir, env):
    alignment = work_dir / "exif-alignment.png"
    command = (
        oiiotool,
        "--create",
        "1x1",
        "3",
        "--sattrib",
        "Make",
        "Apple",
        "--attrib:type=float",
        "ExposureTime",
        "0.010101",
        "-o",
        alignment,
    )
    code, log = run(command, env)
    if mode == "vulnerable":
        require(code != 0, "vulnerable EXIF alignment write unexpectedly passed")
        require("runtime error:" in log, "missing UBSan alignment diagnostic")
    else:
        require(code == 0, f"fixed EXIF alignment write failed ({code}):\n{log}")
        require_clean(log, "fixed EXIF alignment write")
        require_metadata(
            iinfo,
            alignment,
            ('Make: "Apple"', "ExposureTime: 0.0101"),
            env,
            "fixed EXIF alignment readback",
        )

    canon = work_dir / "canon-makernote.png"
    command = (
        oiiotool,
        "--create",
        "1x1",
        "3",
        "--sattrib",
        "Make",
        "Canon",
        "--attrib:type=int",
        "Canon:ColorTemperature",
        "6500",
        "--attrib:type=int",
        "Canon:MacroMode",
        "1",
        "--attrib:type=int[4]",
        "Canon:ThumbnailImageValidArea",
        "1,2,3,4",
        "-o",
        canon,
    )
    code, log = run(command, env)
    if mode == "vulnerable":
        require(code != 0, "vulnerable Canon scalar write unexpectedly passed")
        require("Assertion" in log, "missing Canon byte-count assertion")
    else:
        require(code == 0, f"fixed Canon write failed ({code}):\n{log}")
        require_clean(log, "fixed Canon write")
        require_metadata(
            iinfo,
            canon,
            (
                "Canon:ColorTemperature: 6500",
                "Canon:MacroMode: 1",
                "Canon:ThumbnailImageValidArea: 1, 2, 3, 4",
            ),
            env,
            "fixed Canon readback",
        )

    indexed = work_dir / "canon-indexed.png"
    code, log = run(
        (
            oiiotool,
            "--create",
            "1x1",
            "3",
            "--sattrib",
            "Make",
            "Canon",
            "--attrib:type=int",
            "Canon:MacroMode",
            "1",
            "-o",
            indexed,
        ),
        env,
    )
    require(code == 0, f"Canon indexed write failed ({code}):\n{log}")
    metadata = require_metadata(iinfo, indexed, ('Make: "Canon"',), env, "Canon indexed")
    if mode == "vulnerable":
        require("Canon:MacroMode" not in metadata, "vulnerable Canon endian fault absent")
    else:
        require("Canon:MacroMode: 1" in metadata, "fixed Canon indexed value absent")


def check_icc_decode(mode, oiiotool, iinfo, oversized, cross_tag, env):
    code, log = run(
        (
            oiiotool,
            "-oiioattrib",
            "imageinput:strict",
            "1",
            "-info",
            "-v",
            oversized,
        ),
        env,
    )
    if mode == "vulnerable":
        require(code == 0, f"vulnerable strict JPEG contract changed ({code}):\n{log}")
    else:
        require(code != 0, "fixed strict JPEG accepted malformed ICC profile")
        require("ICC profile size mismatch" in log, "missing strict ICC diagnostic")
        require_clean(log, "fixed strict JPEG")

    code, log = run((iinfo, "-v", cross_tag), env)
    require(code == 0, f"cross-tag JPEG read failed ({code}):\n{log}")
    if mode == "vulnerable":
        require(
            'ICCProfile:profile_description: "Leak"' in log,
            "vulnerable mluc cross-tag read was not reproduced",
        )
    else:
        require(
            'ICCProfile:profile_description: "Leak"' not in log,
            "fixed mluc decoder crossed the declared tag boundary",
        )
        require_clean(log, "fixed mluc boundary")


def check_jpeg2000(mode, oiiotool, iinfo, source_dir, work_dir, env):
    small = work_dir / "small.jp2"
    small_source = source_dir / "testsuite/python-imagebuf/ref/outarray.tif"
    code, log = run((oiiotool, small_source, "-o", small), env)
    if mode == "vulnerable":
        require(code == 0, f"vulnerable small JP2 write did not report success: {code}")
        require(small.exists() and small.stat().st_size == 77, "small JP2 is not 77 bytes")
        read_code, read_log = run((iinfo, small), env)
        require(read_code != 0, "vulnerable small JP2 unexpectedly readable")
        require("Expected a SOC marker" in read_log, "missing truncated JP2 diagnostic")
    else:
        require(code != 0, "fixed small JP2 write still reports success")
        require("Failed write jpeg2000::save_image" in log, "missing encoder failure")
        require_clean(log, "fixed small JP2")

    source = source_dir / "testsuite/common/tahoe-tiny.tif"
    for order in PROGRESSION_ORDERS:
        output = work_dir / f"progression-{order}.jp2"
        code, log = run(
            (
                oiiotool,
                source,
                "--attrib",
                "jpeg2000:ProgressionOrder",
                order,
                "-o",
                output,
            ),
            env,
        )
        if mode == "vulnerable":
            require(code == 0, f"vulnerable {order} write did not report success")
            require(output.stat().st_size == 77, f"vulnerable {order} size changed")
            read_code, read_log = run((iinfo, output), env)
            require(read_code != 0, f"vulnerable {order} unexpectedly readable")
            require("Expected a SOC marker" in read_log, f"missing {order} JP2 diagnostic")
        else:
            require(code == 0, f"fixed {order} write failed ({code}):\n{log}")
            require_clean(log, f"fixed {order} write")
            require(output.stat().st_size > 77, f"fixed {order} output is truncated")
            read_code, read_log = run((iinfo, output), env)
            require(read_code == 0, f"fixed {order} is unreadable:\n{read_log}")
            require_clean(read_log, f"fixed {order} read")

    profile = source_dir / "testsuite/jpeg2000/ref/test-jp2.icc"
    roundtrip_image = work_dir / "icc-roundtrip.jp2"
    roundtrip_profile = work_dir / "icc-roundtrip.icc"
    code, log = run((oiiotool, source, "--iccread", profile, "-o", roundtrip_image), env)
    require(code == 0, f"JP2 ICC write control failed ({code}):\n{log}")
    require_clean(log, "JP2 ICC write control")
    code, log = run((oiiotool, roundtrip_image, "--iccwrite", roundtrip_profile), env)
    require(code == 0, f"JP2 ICC extract control failed ({code}):\n{log}")
    require_clean(log, "JP2 ICC extract control")
    expected = profile.read_bytes()
    actual = roundtrip_profile.read_bytes()
    require(actual == expected, "JP2 ICC round-trip is not byte-identical")
    return hashlib.sha256(actual).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("fixed", "vulnerable"), required=True)
    parser.add_argument("--oiiotool", type=pathlib.Path, required=True)
    parser.add_argument("--iinfo", type=pathlib.Path, required=True)
    parser.add_argument("--source-dir", type=pathlib.Path, required=True)
    parser.add_argument("--work-dir", type=pathlib.Path, required=True)
    args = parser.parse_args()

    require(args.oiiotool.is_file(), f"missing oiiotool: {args.oiiotool}")
    require(args.iinfo.is_file(), f"missing iinfo: {args.iinfo}")
    args.work_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "halt_on_error=1:abort_on_error=1:detect_leaks=0"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    oversized, cross_tag = write_fixtures(args.work_dir)
    check_exif(args.mode, args.oiiotool, args.iinfo, args.work_dir, env)
    check_icc_decode(args.mode, args.oiiotool, args.iinfo, oversized, cross_tag, env)
    profile_digest = check_jpeg2000(
        args.mode,
        args.oiiotool,
        args.iinfo,
        args.source_dir,
        args.work_dir,
        env,
    )
    print(
        f"PASS: OpenImageIO {args.mode} ICC/EXIF/JPEG2000 contract; "
        f"progression_orders={len(PROGRESSION_ORDERS)} "
        f"profile_sha256={profile_digest}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
