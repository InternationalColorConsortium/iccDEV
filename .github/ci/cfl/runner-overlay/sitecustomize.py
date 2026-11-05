#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
###############################################################################
"""Bypass ClusterFuzzLite's remote corpus lookup for prepared local corpora."""

from __future__ import annotations

import logging
import os
from pathlib import Path

import clusterfuzz_deployment


MARKER_ROOT_ENV = "ICCDEV_CFL_LOCAL_CORPUS_MARKER_ROOT"
CORPUS_ROOT_ENV = "ICCDEV_CFL_LOCAL_CORPUS_ROOT"


def _install_local_corpus_bypass() -> None:
    original = clusterfuzz_deployment.ClusterFuzzLite.download_corpus
    if getattr(original, "_iccdev_local_corpus_bypass", False):
        return

    def download_corpus(self, target_name: str, corpus_dir: str):
        marker_root_value = os.environ.get(MARKER_ROOT_ENV)
        corpus_root_value = os.environ.get(CORPUS_ROOT_ENV)
        if not marker_root_value and not corpus_root_value:
            return original(self, target_name, corpus_dir)
        if not marker_root_value or not corpus_root_value:
            raise RuntimeError("Incomplete iccDEV local corpus bypass configuration")
        if Path(target_name).name != target_name:
            raise RuntimeError(f"Invalid ClusterFuzzLite target name: {target_name}")

        corpus_root = Path(corpus_root_value).resolve()
        expected_corpus = (corpus_root / target_name).resolve()
        actual_corpus = Path(corpus_dir).resolve()
        if actual_corpus != expected_corpus:
            raise RuntimeError(
                f"Unexpected local corpus path for {target_name}: {actual_corpus}"
            )

        marker_root = Path(marker_root_value).resolve()
        marker = (marker_root / target_name).resolve()
        if marker.parent != marker_root or not marker.is_file():
            raise RuntimeError(f"Missing local corpus marker for {target_name}")

        logging.info("Using prepared local corpus for %s at %s.", target_name, corpus_dir)
        return corpus_dir

    download_corpus._iccdev_local_corpus_bypass = True
    clusterfuzz_deployment.ClusterFuzzLite.download_corpus = download_corpus


_install_local_corpus_bypass()
