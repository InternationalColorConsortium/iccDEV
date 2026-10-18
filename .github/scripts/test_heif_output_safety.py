#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

"""Exercise native HEIF extraction without modifying the tracked carriers."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


PROFILE_NAME = "item-1-property-1-prof.icc"
PROFILE_SHA256 = "de9b36ccc513fdc79860d5dff523aed9cf67411789e4c20ae866146cbaf75a4e"
CREATE_ERROR = "Error: unable to create extracted ICC profile:"
INPUT_ERROR = "Error: unable to open HEIF input file:"


def check_result(result, expected, expected_error=None):
    output = result.stdout + result.stderr
    if result.returncode != expected:
        raise RuntimeError(f"expected exit {expected}, got {result.returncode}: {output}")
    for diagnostic in ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:"):
        if diagnostic in output:
            raise RuntimeError(output)
    if expected == 2 and expected_error is None:
        expected_error = CREATE_ERROR
    if expected_error and expected_error not in result.stderr:
        raise RuntimeError(f"missing expected error {expected_error!r}: {output}")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument("--corpus", required=True, type=Path)
    args = parser.parse_args()
    tool = args.tool.resolve(strict=True)
    source = (args.corpus / "valid-prof.heic").resolve(strict=True)
    original = source.read_bytes()
    passed = 0
    skipped = 0

    def run(image, directory, expected=0):
        result = subprocess.run(
            [str(tool), str(image), str(directory)], capture_output=True,
            text=True, errors="replace", timeout=30,
        )
        expected_error = CREATE_ERROR if expected == 2 else None
        return check_result(result, expected, expected_error)

    def passed_case(name):
        nonlocal passed
        passed += 1
        print(f"[PASS] {name}")

    with tempfile.TemporaryDirectory(prefix="iccdev-heif-output-") as temporary:
        root = Path(temporary)
        image = root / "input.heic"
        shutil.copyfile(source, image)

        missing = root / "missing.heic"
        result = subprocess.run(
            [str(tool), str(missing)], capture_output=True,
            text=True, errors="replace", timeout=30,
        )
        check_result(result, 2, INPUT_ERROR)
        passed_case("missing input diagnostic")

        if os.name != "nt":
            unsafe_missing = root / "missing\n\x1b[31m.heic"
            result = subprocess.run(
                [str(tool), str(unsafe_missing)], capture_output=True,
                text=True, errors="replace", timeout=30,
            )
            check_result(result, 2, INPUT_ERROR)
            safe_name = str(unsafe_missing).replace("\n", "\\n").replace("\x1b", "\\x1B")
            if result.stderr != f"{INPUT_ERROR} {safe_name}\n":
                raise RuntimeError(f"unsafe input diagnostic was not sanitized: {result.stderr!r}")
            passed_case("missing input diagnostic sanitizes console controls")

        malformed = root / "malformed.heic"
        malformed.write_bytes(b"not a HEIF file")
        result = subprocess.run(
            [str(tool), str(malformed)], capture_output=True,
            text=True, errors="replace", timeout=30,
        )
        check_result(result, 2, "Error: unable to read HEIF colour properties")
        if INPUT_ERROR in result.stderr:
            raise RuntimeError("malformed input was reported as unavailable")
        passed_case("malformed input diagnostic remains distinct")

        # Exercise the platform separator, forward slashes, and spaces.
        for name, suffix in (("native path", ""), ("trailing native", os.sep),
                             ("trailing slash", "/")):
            directory = root / name
            directory.mkdir()
            output = run(image, str(directory) + suffix)
            extracted = directory / PROFILE_NAME
            payload = extracted.read_bytes()
            if len(payload) != 540 or hashlib.sha256(payload).hexdigest() != PROFILE_SHA256:
                raise RuntimeError("extracted ICC bytes differ from the pinned fixture")
            if "proflib_parse=ok" not in output or "icc_profiles=1" not in output:
                raise RuntimeError(f"incomplete parse report: {output}")
            run(image, directory, 2)
            if extracted.read_bytes() != payload:
                raise RuntimeError("repeat extraction modified its existing output")
            passed_case(name + " and repeat extraction")

        for name in ("same input", "existing file", "hard link", "directory",
                     "symlink", "dangling symlink"):
            directory = root / name
            directory.mkdir()
            destination = directory / PROFILE_NAME
            input_path = image
            sentinel = b"existing output must survive\n"
            missing = root / "missing-target.icc"
            if name == "same input":
                shutil.copyfile(source, destination)
                input_path = destination
            elif name == "existing file":
                destination.write_bytes(sentinel)
            elif name == "hard link":
                os.link(image, destination)
            elif name == "directory":
                destination.mkdir()
            else:
                try:
                    destination.symlink_to(image if name == "symlink" else missing)
                except OSError as error:
                    if os.name == "nt" and error.winerror == 1314:
                        print(f"[SKIP] {name}: Windows symlink privilege unavailable")
                        skipped += 1
                        continue
                    raise
            run(input_path, directory, 2)
            if image.read_bytes() != original or source.read_bytes() != original:
                raise RuntimeError("input carrier modified")
            if name in ("same input", "hard link", "symlink"):
                if destination.read_bytes() != original:
                    raise RuntimeError(f"{name} carrier modified")
            if name == "hard link" and not os.path.samefile(image, destination):
                raise RuntimeError("hard link replaced")
            if name == "existing file" and destination.read_bytes() != sentinel:
                raise RuntimeError("existing file modified")
            if name == "directory" and not destination.is_dir():
                raise RuntimeError("directory replaced")
            if "symlink" in name and not destination.is_symlink():
                raise RuntimeError("symlink replaced")
            if name == "dangling symlink" and missing.exists():
                raise RuntimeError("dangling symlink target created")
            passed_case(name)

        run(image, root / "absent directory", 2)
        passed_case("missing output directory")

        # All writers target the same new path. Exactly one must own creation.
        directory = root / "concurrent writers"
        directory.mkdir()
        processes = []
        try:
            for _ in range(8):
                processes.append(subprocess.Popen(
                    [str(tool), str(image), str(directory)], stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, text=True, errors="replace",
                ))
            statuses = []
            for process in processes:
                stdout, stderr = process.communicate(timeout=30)
                statuses.append(process.returncode)
                check_result(subprocess.CompletedProcess(
                    process.args, process.returncode, stdout, stderr),
                    0 if process.returncode == 0 else 2)
            if statuses.count(0) != 1:
                raise RuntimeError(f"expected one successful writer: {statuses}")
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.communicate()
        if hashlib.sha256((directory / PROFILE_NAME).read_bytes()).hexdigest() != PROFILE_SHA256:
            raise RuntimeError("concurrent extraction produced incorrect bytes")
        if image.read_bytes() != original or source.read_bytes() != original:
            raise RuntimeError("input carrier modified")
        passed_case("concurrent exclusive creation")

    print(f"[PASS] HEIF output safety: {passed} passed, {skipped} skipped")


if __name__ == "__main__":
    main()
