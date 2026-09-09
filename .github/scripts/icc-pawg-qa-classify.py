#!/usr/bin/env python3
# Copyright (c) 2026 International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""Count PAWG verdicts, not words in questions, details, or zero summaries."""

import collections
import json
from pathlib import Path
import re
import sys

STATES = {"OK": "pass", "PASS": "pass", "WARN": "warn", "FAIL": "fail",
          "GAP": "gap", "NOT RUN": "notRun", "N/A": "notApplicable"}
SUMMARY_STATES = {key: value for key, value in STATES.items() if key != "OK"}


def classify(text, mode):
    """Return errors, FAIL, WARN, GAP, NOT RUN; invalid reports fail closed."""
    counts = collections.Counter()
    errors = 0
    try:
        if mode == "json":
            report = json.loads(text)
            if report["tool"] != "iccPawgReport":
                raise ValueError("wrong producer")
            items = [(item["id"], item["verdict"]) for item in report["items"]]
            summary = report["summary"]
        else:
            items = [(match[1], match[0].strip()) for match in re.findall(
                r"^  \[([^\]]+)\] ([SCQ][1-9][0-9]*)\s+.*$", text, re.M)]
            summaries = re.findall(
                r"^  (PASS|WARN|FAIL|GAP|NOT RUN|N/A):\s+([0-9]+)\s*$", text, re.M)
            summary = {SUMMARY_STATES[state]: int(value) for state, value in summaries}
            totals = re.findall(r"^  Total checklist items:\s+([0-9]+)\s*$", text, re.M)
            if len(summaries) != len(SUMMARY_STATES) or len(totals) != 1:
                errors += 1
            summary["total"] = int(totals[0]) if totals else -1
            if re.search(r"^(?:ERROR|Error|FATAL|Fatal)(?:!|:)", text, re.M):
                errors += 1
        seen = set()
        for item_id, verdict in items:
            if not re.fullmatch(r"[SCQ][1-9][0-9]*", item_id) or item_id in seen:
                errors += 1
            seen.add(item_id)
            if verdict not in STATES:
                errors += 1
            else:
                counts[STATES[verdict]] += 1
        if not items or summary["total"] != len(items):
            raise ValueError("incomplete report")
        for state in set(STATES.values()) | {"total"}:
            value = summary[state]
            expected = len(items) if state == "total" else counts[state]
            if type(value) is not int or value < 0 or value != expected:
                raise ValueError("invalid or inconsistent summary")
    except (ValueError, KeyError, TypeError, AttributeError):
        errors += 1
    return (errors, counts["fail"], counts["warn"], counts["gap"], counts["notRun"])


if __name__ == "__main__":
    print(*classify(Path(sys.argv[2]).read_text(errors="replace"), sys.argv[1]), sep="\t")
