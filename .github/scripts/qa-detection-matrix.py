#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Validate and render the recent-QA detector ownership matrix.
###############################################################################

"""Render a checked QA detector matrix for CI summaries and artifacts."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REQUIRED_FIELDS = {
    "issue",
    "state",
    "defect_class",
    "primary_detector",
    "static_signal",
    "runtime_gate",
    "coverage_role",
    "evidence_paths",
}
VALID_STATES = {"closed", "open", "research"}
VALID_COVERAGE_ROLES = {"exercised", "not-covered", "semantic-gap", "research"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--markdown", type=Path, required=True)
    parser.add_argument("--json", dest="json_output", type=Path)
    return parser.parse_args()


def load_matrix(path: Path, repo_root: Path) -> tuple[int, list[dict[str, object]]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot load {path}: {error}") from error
    if not isinstance(data, dict) or not isinstance(data.get("issues"), list):
        raise ValueError("matrix must contain an issues array")
    tracking_issue = data.get("tracking_issue")
    if not isinstance(tracking_issue, int) or tracking_issue <= 0:
        raise ValueError("matrix must contain a positive tracking_issue")

    records = data["issues"]
    seen: set[int] = set()
    for index, record in enumerate(records):
        if not isinstance(record, dict) or not REQUIRED_FIELDS.issubset(record):
            raise ValueError(f"issue entry {index} is missing required fields")
        issue = record["issue"]
        if not isinstance(issue, int) or issue <= 0 or issue in seen:
            raise ValueError(f"issue entry {index} has an invalid or duplicate number")
        seen.add(issue)
        if record["state"] not in VALID_STATES:
            raise ValueError(f"issue #{issue} has an invalid state")
        if record["coverage_role"] not in VALID_COVERAGE_ROLES:
            raise ValueError(f"issue #{issue} has an invalid coverage role")
        paths = record["evidence_paths"]
        if not isinstance(paths, list) or not paths:
            raise ValueError(f"issue #{issue} needs at least one evidence path")
        for relative_path in paths:
            if not isinstance(relative_path, str) or not relative_path or not (repo_root / relative_path).exists():
                raise ValueError(f"issue #{issue} evidence path is missing: {relative_path}")
    return tracking_issue, sorted(records, key=lambda item: int(item["issue"]))


def cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def render(tracking_issue: int, records: list[dict[str, object]]) -> str:
    lines = [
        "## Recent QA detector ownership",
        "",
        "Tracking: "
        f"[#{tracking_issue}](https://github.com/InternationalColorConsortium/iccDEV/issues/{tracking_issue})",
        "",
        "Coverage proves execution only; the primary detector and regression assertion own defect detection.",
        "",
        "| Issue | State | Defect class | Primary detector | Static signal | Runtime gate | Coverage role |",
        "|------:|-------|--------------|------------------|---------------|--------------|---------------|",
    ]
    for record in records:
        issue = int(record["issue"])
        issue_link = f"[#{issue}](https://github.com/InternationalColorConsortium/iccDEV/issues/{issue})"
        lines.append(
            "| "
            + " | ".join(
                [
                    issue_link,
                    cell(record["state"]),
                    cell(record["defect_class"]),
                    cell(record["primary_detector"]),
                    cell(record["static_signal"]),
                    cell(record["runtime_gate"]),
                    cell(record["coverage_role"]),
                ]
            )
            + " |"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    try:
        tracking_issue, records = load_matrix(args.input, repo_root)
    except ValueError as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1

    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.write_text(render(tracking_issue, records), encoding="ascii")
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(
            json.dumps(
                {"issues": records, "tracking_issue": tracking_issue},
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="ascii",
        )
    print(f"QA detector matrix: {len(records)} issues validated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
