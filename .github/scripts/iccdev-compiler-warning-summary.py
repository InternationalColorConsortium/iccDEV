#!/usr/bin/env python3
"""Summarize compiler-warning signal from CI build logs."""

from __future__ import annotations

import argparse
import html
import re
import sys
from collections import OrderedDict
from pathlib import Path


ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
CONTROL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")
WARNING_RE = re.compile(r"(?i)(\bwarning\s*(?:[A-Z]+\d+)?\s*:|\[-W[^\]]+\])")

CATEGORIES = OrderedDict(
    [
        (
            "format-signal",
            {
                "title": "Format mismatch signal",
                "kind": "source signal",
                "patterns": [
                    r"\[-Wformat\]",
                    r"format specifies type",
                    r"format string",
                ],
            },
        ),
        (
            "control-flow-signal",
            {
                "title": "Control-flow signal",
                "kind": "source signal",
                "patterns": [
                    r"\[-Wmisleading-indentation\]",
                    r"misleading indentation",
                ],
            },
        ),
        (
            "enum-switch-signal",
            {
                "title": "Enum/switch signal",
                "kind": "source signal",
                "patterns": [
                    r"\[-Wenum-compare-switch\]",
                    r"\[-Wswitch\]",
                    r"case value .*not in enumerated type",
                    r"enumeration value .*not handled in switch",
                ],
            },
        ),
        (
            "unused-signal",
            {
                "title": "Unused-code signal",
                "kind": "source signal",
                "patterns": [
                    r"\[-Wunused-(?:variable|const-variable|function)\]",
                    r"\bunused (?:variable|const variable|function)\b",
                ],
            },
        ),
        (
            "crt-deprecation-noise",
            {
                "title": "Windows CRT deprecation noise",
                "kind": "toolchain noise",
                "patterns": [
                    r"\[-Wdeprecated-declarations\]",
                    r"\bC4996\b",
                    r"_CRT_SECURE",
                    r"\bdeprecated (?:declaration|function|API)\b",
                    r"may be unsafe",
                ],
            },
        ),
        (
            "toolchain-header-noise",
            {
                "title": "Windows/toolchain header noise",
                "kind": "toolchain noise",
                "patterns": [
                    r"Windows Kits",
                    r"\\ucrt\\",
                    r"\\VC\\Tools\\MSVC\\",
                    r"Program Files.*Windows",
                ],
            },
        ),
        (
            "unclassified-signal",
            {
                "title": "Unclassified warning signal",
                "kind": "source signal",
                "patterns": [],
            },
        ),
    ]
)


def ascii_safe(value: str) -> str:
    return value.encode("ascii", "xmlcharrefreplace").decode("ascii")


def sanitize_line(value: str, max_len: int = 600) -> str:
    value = ANSI_RE.sub("", value)
    value = value.replace("\r", " ").replace("\n", " ")
    value = CONTROL_RE.sub("", value)
    value = " ".join(value.split())
    if len(value) > max_len:
        value = value[: max_len - 3] + "..."
    value = html.escape(value, quote=True).replace("`", "&#96;")
    return ascii_safe(value)


def read_log(path_text: str) -> tuple[str, list[str]]:
    if path_text == "-":
        return "stdin", sys.stdin.read().splitlines()

    path = Path(path_text)
    if not path.exists():
        return str(path), []

    data = path.read_text(encoding="utf-8", errors="replace")
    return str(path), data.splitlines()


def classify_warning(line: str) -> str:
    for category, meta in CATEGORIES.items():
        if category == "unclassified-signal":
            continue
        for pattern in meta["patterns"]:
            if re.search(pattern, line, re.IGNORECASE):
                return category
    return "unclassified-signal"


def collect_warnings(log_paths: list[str]) -> tuple[list[str], dict[str, list[str]], list[str]]:
    buckets: dict[str, list[str]] = {category: [] for category in CATEGORIES}
    missing_logs: list[str] = []
    warning_lines: list[str] = []

    for log_path in log_paths:
        label, lines = read_log(log_path)
        if not lines and log_path != "-":
            missing_logs.append(label)
            continue
        for line in lines:
            if not WARNING_RE.search(line):
                continue
            warning_lines.append(line)
            buckets[classify_warning(line)].append(line)

    return warning_lines, buckets, missing_logs


def make_markdown(
    context: str,
    issues: str,
    warning_lines: list[str],
    buckets: dict[str, list[str]],
    missing_logs: list[str],
    max_lines: int,
) -> str:
    signal_count = sum(
        len(lines)
        for category, lines in buckets.items()
        if CATEGORIES[category]["kind"] == "source signal"
    )
    noise_count = sum(
        len(lines)
        for category, lines in buckets.items()
        if CATEGORIES[category]["kind"] == "toolchain noise"
    )
    unclassified_count = len(buckets["unclassified-signal"])

    lines = [
        f"### {sanitize_line(context)} compiler warning signal",
        "",
        f"- Issues: `{sanitize_line(issues or 'n/a')}`",
        f"- Warning log lines: `{len(warning_lines)}`",
        f"- Source signal lines: `{signal_count}`",
        f"- Known toolchain/deprecation noise lines: `{noise_count}`",
        f"- Unclassified signal lines: `{unclassified_count}`",
        "- ClangCL noise-control policy: `_CRT_SECURE_NO_WARNINGS` and "
        "`-Wno-deprecated-declarations` are applied only in the ClangCL smoke job.",
        "",
        "| Category | Count | Classification |",
        "|----------|------:|----------------|",
    ]

    for category, meta in CATEGORIES.items():
        lines.append(
            "| `{}` | `{}` | `{}` |".format(
                sanitize_line(meta["title"]),
                len(buckets[category]),
                sanitize_line(meta["kind"]),
            )
        )

    if missing_logs:
        lines.extend(["", "Missing or empty logs:"])
        for log_path in missing_logs:
            lines.append(f"- `{sanitize_line(log_path)}`")

    for category, meta in CATEGORIES.items():
        entries = buckets[category]
        if not entries:
            continue
        lines.extend(
            [
                "",
                f"<details><summary>{sanitize_line(meta['title'])} ({len(entries)})</summary>",
                "",
            ]
        )
        for entry in entries[:max_lines]:
            lines.append(f"- `{sanitize_line(entry)}`")
        if len(entries) > max_lines:
            lines.append(f"- ... {len(entries) - max_lines} additional line(s) omitted")
        lines.extend(["", "</details>"])

    lines.append("")
    return "\n".join(lines)


def write_text(path_text: str, body: str, append: bool) -> None:
    path = Path(path_text)
    path.parent.mkdir(parents=True, exist_ok=True)
    mode = "a" if append else "w"
    with path.open(mode, encoding="utf-8", newline="\n") as handle:
        handle.write(body)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", action="append", required=True, help="Build log path, or '-' for stdin")
    parser.add_argument("--summary", help="Append sanitized markdown to this summary file")
    parser.add_argument("--details", help="Write sanitized detailed markdown to this file")
    parser.add_argument("--context", default="Compiler", help="Compiler/job label for the report")
    parser.add_argument("--issues", default="", help="Issue references for traceability")
    parser.add_argument("--max-lines", type=int, default=40, help="Maximum examples per category")
    args = parser.parse_args()

    warning_lines, buckets, missing_logs = collect_warnings(args.log)
    markdown = make_markdown(
        context=args.context,
        issues=args.issues,
        warning_lines=warning_lines,
        buckets=buckets,
        missing_logs=missing_logs,
        max_lines=max(1, args.max_lines),
    )

    if args.summary:
        write_text(args.summary, markdown, append=True)
    else:
        print(markdown)

    if args.details:
        write_text(args.details, markdown, append=False)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
