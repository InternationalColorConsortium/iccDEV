#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################
"""Validate the ClusterFuzzLite prepared-local-corpus runner overlay."""

from __future__ import annotations

import os
import runpy
import sys
import tempfile
import types
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
OVERLAY = SCRIPT_DIR.parent / "ci" / "cfl" / "runner-overlay" / "sitecustomize.py"
MARKER_ENV = "ICCDEV_CFL_LOCAL_CORPUS_MARKER_ROOT"
CORPUS_ENV = "ICCDEV_CFL_LOCAL_CORPUS_ROOT"
TARGET = "icc_profileparse_fuzzer"


class FakeClusterFuzzLite:
    def __init__(self) -> None:
        self.remote_downloads = 0

    def download_corpus(self, target_name: str, corpus_dir: str) -> str:
        self.remote_downloads += 1
        return f"remote:{target_name}:{corpus_dir}"


def expect_runtime_error(callback, expected: str) -> None:
    try:
        callback()
    except RuntimeError as error:
        if expected not in str(error):
            raise AssertionError(f"Unexpected error: {error}") from error
    else:
        raise AssertionError(f"Expected RuntimeError containing: {expected}")


def main() -> None:
    fake_module = types.ModuleType("clusterfuzz_deployment")
    fake_module.ClusterFuzzLite = FakeClusterFuzzLite
    previous_module = sys.modules.get("clusterfuzz_deployment")
    previous_marker = os.environ.get(MARKER_ENV)
    previous_corpus = os.environ.get(CORPUS_ENV)

    try:
        sys.modules["clusterfuzz_deployment"] = fake_module
        runpy.run_path(str(OVERLAY))

        with tempfile.TemporaryDirectory(prefix="iccdev-cfl-overlay-test.") as temp:
            root = Path(temp)
            corpus_root = root / "corpora"
            marker_root = root / "markers"
            corpus_dir = corpus_root / TARGET
            marker = marker_root / TARGET
            corpus_dir.mkdir(parents=True)
            marker_root.mkdir(parents=True)
            marker.write_text("ready\n", encoding="ascii")
            os.environ[CORPUS_ENV] = str(corpus_root)
            os.environ[MARKER_ENV] = str(marker_root)

            runner = FakeClusterFuzzLite()
            result = runner.download_corpus(TARGET, str(corpus_dir))
            assert result == str(corpus_dir)
            assert runner.remote_downloads == 0

            marker.unlink()
            expect_runtime_error(
                lambda: runner.download_corpus(TARGET, str(corpus_dir)),
                "Missing local corpus marker",
            )
            assert runner.remote_downloads == 0

            marker.write_text("ready\n", encoding="ascii")
            expect_runtime_error(
                lambda: runner.download_corpus(TARGET, str(root / "wrong")),
                "Unexpected local corpus path",
            )
            assert runner.remote_downloads == 0

            os.environ.pop(CORPUS_ENV)
            os.environ.pop(MARKER_ENV)
            result = runner.download_corpus(TARGET, str(corpus_dir))
            assert result.startswith("remote:")
            assert runner.remote_downloads == 1
    finally:
        if previous_module is None:
            sys.modules.pop("clusterfuzz_deployment", None)
        else:
            sys.modules["clusterfuzz_deployment"] = previous_module
        if previous_marker is None:
            os.environ.pop(MARKER_ENV, None)
        else:
            os.environ[MARKER_ENV] = previous_marker
        if previous_corpus is None:
            os.environ.pop(CORPUS_ENV, None)
        else:
            os.environ[CORPUS_ENV] = previous_corpus

    print("[PASS] ClusterFuzzLite local corpus overlay contract")


if __name__ == "__main__":
    main()
