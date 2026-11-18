#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Audit high-signal cppcheck and clang-tidy findings against a reviewed baseline.
###############################################################################

"""Classify high-signal lint findings and reject unreviewed baseline drift."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


CPPCHECK_RE = re.compile(
    r"^(?P<path>.+?):(?P<line>[0-9]+):(?P<column>[0-9]+): "
    r"(?P<severity>error|warning|style|performance|portability|information): "
    r"(?P<message>.*) \[(?P<checker>[A-Za-z][A-Za-z0-9_]*)\]$"
)
CLANG_TIDY_RE = re.compile(
    r"^(?P<path>.+?):(?P<line>[0-9]+):(?P<column>[0-9]+): "
    r"(?P<severity>warning|error): (?P<message>.*) "
    r"\[(?P<checker>clang-analyzer-[A-Za-z0-9.]+)\]$"
)
PRODUCT_PREFIXES = ("IccProfLib/", "IccXML/", "IccJSON/", "IccConnect/", "Tools/")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cppcheck", type=Path, required=True)
    parser.add_argument("--clang-tidy", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--scope-files",
        type=Path,
        help="optional NUL- or newline-delimited source list for stale checks",
    )
    return parser.parse_args()


def normalize_path(raw_path: str, repo_root: Path) -> str:
    path = Path(raw_path)
    if path.is_absolute():
        try:
            return path.resolve().relative_to(repo_root.resolve()).as_posix()
        except ValueError:
            normalized = raw_path.replace("\\", "/")
            for prefix in PRODUCT_PREFIXES:
                marker = "/" + prefix
                if marker in normalized:
                    return prefix + normalized.split(marker, 1)[1]
    return raw_path.replace("\\", "/").lstrip("./")


def source_hash(repo_root: Path, relative_path: str, line_number: int) -> str:
    path = repo_root / relative_path
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError):
        return ""
    if line_number < 1 or line_number > len(lines):
        return ""
    return hashlib.sha256(lines[line_number - 1].encode("utf-8")).hexdigest()


def is_high_signal(finding: dict[str, object]) -> bool:
    tool = finding["tool"]
    checker = str(finding["checker"])
    if tool == "cppcheck":
        return finding["severity"] == "error"
    return (
        checker == "clang-analyzer-security.ArrayBound"
        or checker.startswith("clang-analyzer-core.")
        or checker.startswith("clang-analyzer-alpha.core.")
        or checker.startswith("clang-analyzer-cplusplus.NewDelete")
        or checker == "clang-analyzer-unix.Malloc"
    )


def parse_report(path: Path, tool: str, pattern: re.Pattern[str], repo_root: Path) -> list[dict[str, object]]:
    findings: list[dict[str, object]] = []
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as error:
        raise ValueError(f"cannot read {path}: {error}") from error

    for text in lines:
        match = pattern.match(text)
        if not match:
            continue
        finding: dict[str, object] = match.groupdict()
        finding["tool"] = tool
        finding["line"] = int(str(finding["line"]))
        finding["column"] = int(str(finding["column"]))
        finding["path"] = normalize_path(str(finding["path"]), repo_root)
        finding["source_sha256"] = source_hash(
            repo_root, str(finding["path"]), int(finding["line"])
        )
        if is_high_signal(finding):
            findings.append(finding)

    unique: dict[tuple[object, ...], dict[str, object]] = {}
    for finding in findings:
        key = (
            finding["tool"],
            finding["path"],
            finding["line"],
            finding["column"],
            finding["checker"],
            finding["message"],
        )
        unique[key] = finding
    return list(unique.values())


def baseline_key(record: dict[str, object]) -> tuple[object, ...]:
    return (
        record.get("tool"),
        record.get("path"),
        record.get("line"),
        record.get("checker"),
        record.get("source_sha256"),
    )


def finding_key(record: dict[str, object]) -> tuple[object, ...]:
    return (
        record["tool"],
        record["path"],
        record["line"],
        record["checker"],
        record["source_sha256"],
    )


def load_baseline(path: Path) -> list[dict[str, object]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot load baseline {path}: {error}") from error
    if not isinstance(data, dict) or not isinstance(data.get("reviewed_findings"), list):
        raise ValueError("baseline must contain a reviewed_findings array")
    records = data["reviewed_findings"]
    required = {"tool", "path", "line", "checker", "source_sha256", "issue", "rationale"}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or not required.issubset(record):
            raise ValueError(f"baseline entry {index} is missing required fields")
    keys = [baseline_key(record) for record in records]
    if len(keys) != len(set(keys)):
        raise ValueError("baseline contains duplicate finding identities")
    return records


def load_scope(path: Path | None) -> set[str] | None:
    if path is None:
        return None
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise ValueError(f"cannot read scope file {path}: {error}") from error
    separator = b"\0" if b"\0" in raw else b"\n"
    try:
        entries = [item.decode("utf-8") for item in raw.split(separator) if item]
    except UnicodeError as error:
        raise ValueError(f"scope file {path} is not UTF-8: {error}") from error
    return {entry.replace("\\", "/").lstrip("./") for entry in entries}


def describe(finding: dict[str, object]) -> str:
    return (
        f"{finding['path']}:{finding['line']}:{finding['column']}: "
        f"{finding['checker']}: {finding['message']}"
    )


def write_outputs(
    output_dir: Path,
    accepted: list[dict[str, object]],
    actionable: list[dict[str, object]],
    stale: list[dict[str, object]],
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        "accepted_count": len(accepted),
        "actionable_count": len(actionable),
        "stale_baseline_count": len(stale),
        "accepted": accepted,
        "actionable": actionable,
        "stale_baseline": stale,
    }
    (output_dir / "static-analysis-audit.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )

    accepted_lines = [describe(finding) for finding in accepted]
    actionable_lines = [describe(finding) for finding in actionable]
    stale_lines = [
        f"{record['path']}:{record['line']}: {record['checker']} ({record['issue']})"
        for record in stale
    ]
    (output_dir / "static-analysis-accepted.txt").write_text(
        "\n".join(accepted_lines) + ("\n" if accepted_lines else ""), encoding="ascii"
    )
    (output_dir / "static-analysis-actionable.txt").write_text(
        "\n".join(actionable_lines) + ("\n" if actionable_lines else ""), encoding="ascii"
    )

    summary = [
        "High-signal static-analysis audit",
        "=================================",
        "",
        f"Reviewed baseline matches: {len(accepted)}",
        f"Unreviewed findings: {len(actionable)}",
        f"Stale baseline entries: {len(stale)}",
    ]
    if actionable_lines:
        summary.extend(["", "Unreviewed findings:", *actionable_lines])
    if stale_lines:
        summary.extend(["", "Stale baseline entries:", *stale_lines])
    (output_dir / "static-analysis-audit.txt").write_text(
        "\n".join(summary) + "\n", encoding="ascii"
    )


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    try:
        findings = parse_report(args.cppcheck, "cppcheck", CPPCHECK_RE, repo_root)
        findings.extend(parse_report(args.clang_tidy, "clang-tidy", CLANG_TIDY_RE, repo_root))
        baseline = load_baseline(args.baseline)
        scope = load_scope(args.scope_files)
    except ValueError as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 2

    baseline_by_key = {baseline_key(record): record for record in baseline}
    finding_by_key = {finding_key(record): record for record in findings}
    accepted = [finding for key, finding in finding_by_key.items() if key in baseline_by_key]
    actionable = [finding for key, finding in finding_by_key.items() if key not in baseline_by_key]
    stale = [
        record
        for key, record in baseline_by_key.items()
        if key not in finding_by_key
        and (scope is None or str(record["path"]) in scope)
    ]

    accepted.sort(key=lambda item: (str(item["path"]), int(item["line"])))
    actionable.sort(key=lambda item: (str(item["path"]), int(item["line"])))
    stale.sort(key=lambda item: (str(item["path"]), int(item["line"])))
    write_outputs(args.output_dir, accepted, actionable, stale)

    print(
        "High-signal lint audit: "
        f"{len(accepted)} reviewed, {len(actionable)} unreviewed, {len(stale)} stale"
    )
    if actionable:
        for finding in actionable:
            print(f"[FAIL] {describe(finding)}", file=sys.stderr)
    if stale:
        for record in stale:
            print(
                f"[FAIL] stale baseline: {record['path']}:{record['line']} "
                f"{record['checker']} ({record['issue']})",
                file=sys.stderr,
            )
    return 1 if actionable or stale else 0


if __name__ == "__main__":
    raise SystemExit(main())
