"""Shared pytest guards for Python package validation.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.
"""

from __future__ import annotations

import os
from pathlib import Path


def pytest_sessionstart(session):
    if os.environ.get("ICCDEV_REQUIRE_INSTALLED_PACKAGE") != "1":
        return

    import iccdev

    repo_root = Path(__file__).resolve().parents[2]
    source_package = (repo_root / "python" / "iccdev").resolve()
    imported_from = Path(iccdev.__file__).resolve()
    if imported_from.is_relative_to(source_package):
        raise AssertionError(
            "iccdev imported from the source checkout during installed-package "
            f"validation: {imported_from}"
        )
