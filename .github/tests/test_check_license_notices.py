#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Regression checks for notice matching across checkout line endings."""

import runpy
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "check-license-notices.py"
normalized_text = runpy.run_path(str(SCRIPT))["normalized_text"]
NOTICE = "/*\n * Copyright (c) International Color Consortium.\n */\n"
PATH = Path("example.mm")


class NoticeLineEndingTests(unittest.TestCase):
    def test_existing_lf_and_crlf_notices_are_unchanged(self):
        for line_ending in ("\n", "\r\n"):
            source = (NOTICE + "int value;\n").replace("\n", line_ending)
            self.assertEqual(normalized_text(source, NOTICE, PATH), source)

    def test_crlf_insertion_preserves_line_endings(self):
        source = "int value;\r\n"
        result = normalized_text(source, NOTICE, PATH)
        self.assertEqual(result, NOTICE.replace("\n", "\r\n") + source)

    def test_crlf_legacy_replacement_does_not_duplicate_notice(self):
        source = (
            "/*\r\n"
            " * Copyright (c) International Color Consortium.\r\n"
            " * Redistribution and use in source and binary forms\r\n"
            " */\r\n\r\n"
            "int value;\r\n"
        )
        result = normalized_text(source, NOTICE, PATH)
        self.assertEqual(result, NOTICE.replace("\n", "\r\n") + "int value;\r\n")
        self.assertEqual(normalized_text(result, NOTICE, PATH), result)


if __name__ == "__main__":
    unittest.main()
