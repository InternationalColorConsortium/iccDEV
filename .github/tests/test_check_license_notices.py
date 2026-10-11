#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Regression checks for notice matching across checkout line endings."""

import runpy
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "check-license-notices.py"
CHECKER = runpy.run_path(str(SCRIPT))
normalized_text = CHECKER["normalized_text"]
has_legacy_v02_marker = CHECKER["has_legacy_v02_marker"]
canonical_notice = CHECKER["canonical_notice"]
NOTICE = "/*\n * Copyright (c) International Color Consortium.\n */\n"
PATH = Path("example.mm")
ROOT = Path(__file__).resolve().parents[2]


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

    def test_legacy_v02_marker_after_canonical_notice_is_rejected(self):
        source = NOTICE + "int value;\n/* ICC Software License, Version " + "0.2 */\n"
        self.assertTrue(has_legacy_v02_marker(source))

    def test_new_tool_template_uses_one_complete_notice_before_file_doc(self):
        template = (ROOT / ".github/prompts/add-new-tool.prompt.md").read_text(
            encoding="ascii"
        )
        source = template.split("```cpp\n", 1)[1].split("```", 1)[0].lstrip("\n")
        notice = canonical_notice(ROOT)
        self.assertTrue(source.startswith(notice + "/** @file"))
        self.assertEqual(source.count("SPDX-License-Identifier: BSD-3-Clause"), 1)
        self.assertEqual(normalized_text(source, notice, PATH), source)


if __name__ == "__main__":
    unittest.main()
