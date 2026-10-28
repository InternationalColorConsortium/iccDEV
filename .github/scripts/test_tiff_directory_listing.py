#!/usr/bin/env python3
"""Check TIFF diagnostic reporting, directory inventory and its 4096-IFD bound."""

import argparse
import json
import pathlib
import struct
import subprocess
import tempfile


def make_chain(path, count):
    tags = ((256, 3, 1), (257, 3, 1), (258, 3, 8), (259, 3, 1),
            (262, 3, 1), (273, 4, 8), (277, 3, 1), (278, 4, 1),
            (279, 4, 1))
    first = 10
    size = 2 + len(tags) * 12 + 4
    data = bytearray(struct.pack("<2sHI", b"II", 42, first) + b"\x00\x00")
    for index in range(count):
        data.extend(struct.pack("<H", len(tags)))
        for tag, kind, value in tags:
            data.extend(struct.pack("<HHII", tag, kind, 1, value))
        data.extend(struct.pack("<I", first + (index + 1) * size
                                if index + 1 < count else 0))
    path.write_bytes(data)


def inventory(tool, path, evidence):
    options = ["--evidence-json", "--list-directories"] if evidence else ["--list-directories"]
    return subprocess.run([tool, *options, str(path)], capture_output=True, timeout=60)


def make_metadata_chain(path, repairs, *, endian="<", big=False, tiled=False,
                        invalid_custom_tag=False, broken_next=False):
    """Write scalar IFD entries without relying on libtiff to repair the fixture."""
    header_size = 16 if big else 8
    inline_size = 8 if big else 4
    count_format = "Q" if big else "H"
    offset_format = "Q" if big else "I"
    value_count_format = "Q" if big else "I"
    tags = [(256, 3, 16), (257, 3, 16), (258, 3, 8), (259, 3, 1),
            (262, 3, 1), (277, 3, 1), (284, 3, 1)]
    offset_tag, bytecount_tag = (324, 325) if tiled else (273, 279)
    tags += [(322, 4, 16), (323, 4, 16)] if tiled else [(278, 4, 16)]
    tags += [(offset_tag, 4, 0), (bytecount_tag, 4, 256)]
    if invalid_custom_tag:
        tags.append((65000, 65532, 0))
    tags.sort()
    ifd_size = (8 if big else 2) + len(tags) * (20 if big else 12) + inline_size
    pixels = header_size + len(repairs) * ifd_size
    order = b"II" if endian == "<" else b"MM"
    data = bytearray(struct.pack(endian + "2sHHHQ", order, 43, 8, 0, header_size)
                     if big else struct.pack(endian + "2sHI", order, 42, header_size))
    for index, repaired in enumerate(repairs):
        data.extend(struct.pack(endian + count_format, len(tags)))
        for tag, kind, value in tags:
            if tag == offset_tag:
                value = pixels + index * 256
            elif tag == bytecount_tag and repaired:
                value = 0
            count = 0 if tag == bytecount_tag and tiled and repaired else 1
            data.extend(struct.pack(endian + "HH" + value_count_format, tag, kind, count))
            data.extend(struct.pack(endian + ("H" if kind == 3 else "I"), value)
                        .ljust(inline_size, b"\x00"))
        next_ifd = header_size + (index + 1) * ifd_size if index + 1 < len(repairs) else 0
        if broken_next and index + 1 == len(repairs):
            next_ifd = 0x7ffffff0
        data.extend(struct.pack(endian + offset_format, next_ifd))
    data.extend(b"\x00" * (256 * len(repairs)))
    path.write_bytes(data)


def check_diagnostics(result, *, evidence, warnings, errors=False, listing=False):
    if result.returncode:
        raise AssertionError(f"Metadata inspection failed: {result.stderr!r}")
    stderr_lines = result.stderr.decode("ascii").splitlines()
    warning_count = sum(": Warning, " in line for line in stderr_lines)
    error_count = len(stderr_lines) - warning_count
    if (warnings is not None and bool(warning_count) != warnings) or bool(error_count) != errors:
        raise AssertionError(f"Unexpected TIFF diagnostics for {result.args}: {result.stderr!r}")
    if evidence:
        report = json.loads(result.stdout)
        if report["metadataSource"] != "libtiff" or \
                report["tiffValidation"] != "not-performed" or \
                report["tiffDiagnostics"] != {
                    "warningCount": warning_count, "errorCount": error_count}:
            raise AssertionError("JSON lost the libtiff interpretation/diagnostic contract")
    else:
        text = result.stdout.decode("ascii")
        if "libtiff interpretation" not in text or \
                (not listing and "not a TIFF validity check" not in text):
            raise AssertionError("Plain report does not qualify interpreted metadata")
        if warning_count or error_count:
            summary = f"TIFF diagnostics:  {warning_count} warning(s), {error_count} error(s)"
            if summary not in text or "not a raw IFD dump" not in text:
                raise AssertionError("Plain report lost TIFF diagnostics")
        elif "TIFF diagnostics:" in text or "Metadata caveat:" in text:
            raise AssertionError("Clean metadata was incorrectly marked as repaired")


def check_metadata_reporting(tool, root):
    for endian in ("<", ">"):
        for big in (False, True):
            for tiled in (False, True):
                for repaired in (False, True):
                    path = root / "metadata.tif"
                    make_metadata_chain(path, [repaired], endian=endian, big=big, tiled=tiled)
                    for evidence in (False, True):
                        for listing in (False, True):
                            options = (["--evidence-json"] if evidence else []) + \
                                      (["--list-directories"] if listing else [])
                            result = subprocess.run([tool, *options, str(path)],
                                                    capture_output=True, timeout=60)
                            check_diagnostics(result, evidence=evidence, warnings=repaired,
                                              listing=listing)
                            if repaired and b'ByteCounts' not in result.stderr:
                                raise AssertionError("Fixture did not exercise byte-count diagnostics")

            mixed = root / "mixed.tif"
            make_metadata_chain(mixed, [False, True], endian=endian, big=big)
            for evidence in (False, True):
                check_diagnostics(inventory(tool, mixed, evidence), evidence=evidence,
                                  warnings=True, listing=True)
                options = ["--evidence-json"] if evidence else []
                selected = subprocess.run([tool, *options, "--ifd", "1", str(mixed)],
                                          capture_output=True, timeout=60)
                check_diagnostics(selected, evidence=evidence, warnings=True)
                first = subprocess.run([tool, *options, str(mixed)],
                                       capture_output=True, timeout=60)
                check_diagnostics(first, evidence=evidence, warnings=False)

    custom = root / "invalid-custom-tag.tif"
    make_metadata_chain(custom, [False], invalid_custom_tag=True)
    for evidence in (False, True):
        for listing in (False, True):
            options = (["--evidence-json"] if evidence else []) + \
                      (["--list-directories"] if listing else [])
            result = subprocess.run([tool, *options, str(custom)], capture_output=True, timeout=60)
            # Some libtiff versions also warn about the unknown tag before the error.
            check_diagnostics(result, evidence=evidence, warnings=None, errors=True, listing=listing)

    broken = root / "broken-next.tif"
    make_metadata_chain(broken, [False], broken_next=True)
    selected = subprocess.run([tool, "--evidence-json", str(broken)],
                              capture_output=True, timeout=60)
    check_diagnostics(selected, evidence=True, warnings=False)
    for evidence in (False, True):
        rejected = inventory(tool, broken, evidence)
        if not 0 < rejected.returncode < 128 or \
                b"Cannot inventory TIFF directories" not in rejected.stderr or rejected.stdout:
            raise AssertionError("Unreadable next IFD produced a successful inventory")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dump", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="iccdev-tiff-ifd-list-") as temp:
        root = pathlib.Path(temp)
        check_metadata_reporting(args.dump, root)
        allowed = root / "chain-4096.tif"
        make_chain(allowed, 4096)
        plain = inventory(args.dump, allowed, False)
        if plain.returncode:
            raise AssertionError(f"Plain listing failed: {plain.stderr!r}")
        lines = plain.stdout.decode("ascii").splitlines()
        if len(lines) != 4096 or not lines[0].startswith("IFD 0: 1 x 1,") or \
                "photometric 1 (Min Is Black)" not in lines[0] or \
                not lines[-1].startswith("IFD 4095: 1 x 1,"):
            raise AssertionError("Plain inventory missed a main IFD")

        evidence = inventory(args.dump, allowed, True)
        if evidence.returncode:
            raise AssertionError(f"JSON listing failed: {evidence.stderr!r}")
        check_diagnostics(evidence, evidence=True, warnings=False, listing=True)
        directories = json.loads(evidence.stdout)["directories"]
        if len(directories) != 4096 or directories[0]["directory"] != 0 or \
                directories[-1]["directory"] != 4095 or \
                any(item["width"] != 1 or item["height"] != 1 or
                    item["blockCount"] != 1 or item["photometric"] != 1 or
                    item["photometricName"] != "Min Is Black"
                    for item in directories):
            raise AssertionError("JSON inventory lost main-IFD metadata")

        excessive = root / "chain-4097.tif"
        make_chain(excessive, 4097)
        rejected = inventory(args.dump, excessive, True)
        if rejected.returncode == 0 or b"more than 4096 directories" not in rejected.stderr:
            raise AssertionError("4097-IFD chain was not rejected")
    print("[OK] classic/BigTIFF, both byte orders, stripped/tiled byte-count diagnostics, "
          "warning/error counts, selected/mixed IFDs and broken-chain rejection")
    print("[OK] plain/JSON 4096-IFD inventory and 4097-IFD rejection")


if __name__ == "__main__":
    main()
