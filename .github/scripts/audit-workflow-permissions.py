#!/usr/bin/env python3
"""Audit GitHub Actions workflow permissions blocks.

This intentionally uses a small indentation-aware parser instead of a YAML
dependency so it can run in early CI governance checks on all GitHub runners.
It tracks workflow-level and job-level permissions blocks and reports every
scope granted write access.
"""

from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path
from typing import Any


PERMISSIONS_RE = re.compile(r"^(?P<indent>\s*)permissions\s*:\s*(?P<value>[^#\n]*)")
SCOPE_RE = re.compile(r"^\s*(?P<scope>[A-Za-z0-9_-]+)\s*:\s*(?P<value>[A-Za-z0-9_-]+)\s*(?:#.*)?$")
KEY_RE = re.compile(r"^(?P<indent>\s*)(?P<key>[A-Za-z0-9_.-]+)\s*:\s*(?:#.*)?$")


def clean_inline_value(value: str) -> str:
    return value.split("#", 1)[0].strip().strip("\"'")


def workflow_files(workflows_dir: Path) -> list[Path]:
    files = []
    for pattern in ("*.yml", "*.yaml"):
        files.extend(workflows_dir.glob(pattern))
    return sorted(files)


def find_job_name(lines: list[str], line_index: int, permissions_indent: int) -> str | None:
    if permissions_indent < 4:
        return None

    saw_jobs = False
    for prev in range(line_index - 1, -1, -1):
        line = lines[prev]
        if not line.strip() or line.lstrip().startswith("#"):
            continue

        key_match = KEY_RE.match(line)
        if not key_match:
            continue

        indent = len(key_match.group("indent"))
        key = key_match.group("key")

        if indent == 0 and key == "jobs":
            saw_jobs = True
            break

        if indent == 0:
            break

    if not saw_jobs:
        return None

    for prev in range(line_index - 1, -1, -1):
        line = lines[prev]
        if not line.strip() or line.lstrip().startswith("#"):
            continue

        key_match = KEY_RE.match(line)
        if not key_match:
            continue

        indent = len(key_match.group("indent"))
        key = key_match.group("key")

        if indent == 2:
            return key
        if indent == 0:
            return None

    return None


def read_permissions_block(lines: list[str], index: int, base_indent: int, inline_value: str) -> dict[str, str]:
    permissions: dict[str, str] = {}
    inline = clean_inline_value(inline_value)

    if inline:
        permissions["*"] = inline
        return permissions

    for line in lines[index + 1:]:
        if not line.strip() or line.lstrip().startswith("#"):
            continue

        indent = len(line) - len(line.lstrip(" "))
        if indent <= base_indent:
            break

        scope_match = SCOPE_RE.match(line)
        if scope_match:
            permissions[scope_match.group("scope")] = clean_inline_value(scope_match.group("value"))

    return permissions


def audit_file(path: Path) -> dict[str, Any]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    blocks = []

    for index, line in enumerate(lines):
        if line.lstrip().startswith("#"):
            continue

        match = PERMISSIONS_RE.match(line)
        if not match:
            continue

        indent = len(match.group("indent"))
        job_name = find_job_name(lines, index, indent)
        context = "workflow" if indent == 0 else "job:%s" % job_name if job_name else "nested"
        permissions = read_permissions_block(lines, index, indent, match.group("value"))
        write_scopes = sorted(
            scope for scope, value in permissions.items()
            if value == "write" or value == "write-all"
        )
        if permissions.get("*") == "write-all":
            write_scopes = ["write-all"]

        blocks.append({
            "line": index + 1,
            "context": context,
            "permissions": permissions,
            "write_scopes": write_scopes,
        })

    return {
        "file": path.name,
        "path": str(path),
        "blocks": blocks,
        "has_permissions": bool(blocks),
        "has_write": any(block["write_scopes"] for block in blocks),
    }


def audit(workflows_dir: Path) -> dict[str, Any]:
    files = [audit_file(path) for path in workflow_files(workflows_dir)]
    write_workflows = [item for item in files if item["has_write"]]
    missing = [item for item in files if not item["has_permissions"]]

    return {
        "total_workflows": len(files),
        "with_permissions": len(files) - len(missing),
        "without_permissions": len(missing),
        "with_write": len(write_workflows),
        "write_blocks": sum(
            1 for item in files for block in item["blocks"] if block["write_scopes"]
        ),
        "missing": [item["file"] for item in missing],
        "write_workflows": write_workflows,
    }


def render_markdown(result: dict[str, Any]) -> str:
    lines = [
        "## 7. Permissions Minimization Audit",
        "",
        "| Metric | Count |",
        "|--------|-------|",
        "| Total workflows | %s |" % result["total_workflows"],
        "| With explicit permissions | %s |" % result["with_permissions"],
        "| Missing permissions block | %s |" % result["without_permissions"],
        "| With any write permission | %s |" % result["with_write"],
        "| Write permission blocks | %s |" % result["write_blocks"],
        "",
    ]

    if result["missing"]:
        lines.extend([
            "<details><summary>[HIGH] Missing permissions block (%s)</summary>" % len(result["missing"]),
            "",
        ])
        for item in result["missing"]:
            lines.append("- `%s`" % html.escape(item))
        lines.extend(["</details>", ""])
    else:
        lines.extend(["[OK] All workflows have explicit workflow-level or job-level permissions.", ""])

    if result["write_workflows"]:
        lines.extend([
            "<details><summary>[MEDIUM] Workflows with write access (%s)</summary>" % result["with_write"],
            "",
        ])
        for item in result["write_workflows"]:
            for block in item["blocks"]:
                if not block["write_scopes"]:
                    continue
                scopes = ",".join(block["write_scopes"])
                lines.append(
                    "- `%s:%s` `%s` (%s)"
                    % (
                        html.escape(item["file"]),
                        block["line"],
                        html.escape(block["context"]),
                        html.escape(scopes),
                    )
                )
        lines.extend(["</details>", ""])
    else:
        lines.extend(["[OK] No workflow grants write permissions.", ""])

    return "\n".join(lines)


def render_shell(result: dict[str, Any]) -> str:
    return "\n".join([
        "WF_TOTAL=%s" % result["total_workflows"],
        "WF_WITH_PERMS=%s" % result["with_permissions"],
        "WF_NO_PERMS=%s" % result["without_permissions"],
        "WF_WITH_WRITE=%s" % result["with_write"],
        "WF_WRITE_BLOCKS=%s" % result["write_blocks"],
    ])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflows-dir", default=".github/workflows")
    parser.add_argument("--format", choices=("markdown", "json", "shell"), default="markdown")
    args = parser.parse_args()

    result = audit(Path(args.workflows_dir))
    if args.format == "json":
        print(json.dumps(result, indent=2, sort_keys=True))
    elif args.format == "shell":
        print(render_shell(result))
    else:
        print(render_markdown(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
