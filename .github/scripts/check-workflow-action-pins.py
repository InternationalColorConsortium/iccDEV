#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Validate workflow action pins without duplicating mutable SHAs."""

from __future__ import annotations

import argparse
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable

import yaml


ACTION_CHARS = frozenset(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./-"
)
ACTION_SEGMENT_CHARS = ACTION_CHARS - {"/"}
LOWER_HEX = frozenset("0123456789abcdef")


def is_action_name(value: str) -> bool:
    segments = value.split("/")
    return (
        len(segments) >= 2
        and all(segment for segment in segments)
        and all(set(segment) <= ACTION_SEGMENT_CHARS for segment in segments)
    )


def parse_remote_action(value: str) -> tuple[str, str] | None:
    action, separator, sha = value.rpartition("@")
    if (
        not separator
        or not is_action_name(action)
        or len(sha) != 40
        or not set(sha) <= LOWER_HEX
    ):
        return None
    return action, sha


def is_digest_pinned_docker_action(value: str) -> bool:
    image, separator, digest = value.rpartition("@sha256:")
    return (
        bool(separator)
        and image.startswith("docker://")
        and len(image) > len("docker://")
        and "@" not in image
        and not any(char.isspace() for char in image)
        and len(digest) == 64
        and set(digest) <= LOWER_HEX
    )


def mirrored_actions(line: str) -> Iterable[str]:
    search_from = 0
    while True:
        at = line.find("@", search_from)
        if at < 0:
            return
        search_from = at + 1
        sha = line[at + 1:at + 41]
        if len(sha) != 40 or not set(sha) <= LOWER_HEX:
            continue
        if at + 41 < len(line) and line[at + 41] in LOWER_HEX:
            continue

        start = at
        while start > 0 and line[start - 1] in ACTION_CHARS:
            start -= 1
        action = line[start:at]
        if is_action_name(action):
            yield action + "@" + sha


def workflow_files(root: Path) -> list[Path]:
    workflows = root / ".github" / "workflows"
    return sorted([*workflows.glob("*.yml"), *workflows.glob("*.yaml")])


def uses_values(node: Any, location: str = "") -> Iterable[tuple[str, str]]:
    if isinstance(node, dict):
        for key, value in node.items():
            child = f"{location}.{key}" if location else str(key)
            if key == "uses" and isinstance(value, str):
                yield child, value
            yield from uses_values(value, child)
    elif isinstance(node, list):
        for index, value in enumerate(node):
            child = f"{location}[{index}]"
            yield from uses_values(value, child)


def repository_files(root: Path) -> list[Path]:
    command = [
        "git",
        "-C",
        str(root),
        "ls-files",
        "-z",
        "--cached",
        "--others",
        "--exclude-standard",
    ]
    result = subprocess.run(command, capture_output=True, check=False)
    if result.returncode == 0:
        relative_paths = {
            Path(item.decode("utf-8"))
            for item in result.stdout.split(b"\0")
            if item
        }
        return sorted(root / path for path in relative_paths)

    return sorted(
        path for path in root.rglob("*")
        if path.is_file() and ".git" not in path.parts
    )


def mirrored_pin_errors(root: Path) -> list[str]:
    errors = []
    workflows = root / ".github" / "workflows"

    for path in repository_files(root):
        if not path.is_file() or path.is_relative_to(workflows):
            continue
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeDecodeError):
            continue

        for number, line in enumerate(lines, start=1):
            for action_ref in mirrored_actions(line):
                relative = path.relative_to(root)
                errors.append(
                    f"{relative}:{number}: exact workflow action pin is mirrored "
                    f"outside .github/workflows: {action_ref}"
                )

    return errors


def check_repository(root: Path) -> tuple[list[str], dict[str, int]]:
    errors = []
    pins: dict[str, dict[str, list[str]]] = defaultdict(
        lambda: defaultdict(list)
    )
    action_occurrences = 0
    docker_occurrences = 0
    workflows = workflow_files(root)

    if not workflows:
        errors.append("no workflow files found under .github/workflows")

    for path in workflows:
        relative = path.relative_to(root)
        try:
            data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
        except (OSError, UnicodeDecodeError, yaml.YAMLError) as error:
            errors.append(f"{relative}: unable to parse workflow: {error}")
            continue

        for location, action_ref in uses_values(data):
            label = f"{relative}:{location}"
            if action_ref.startswith("./"):
                continue
            if action_ref.startswith("docker://"):
                docker_occurrences += 1
                if not is_digest_pinned_docker_action(action_ref):
                    errors.append(
                        f"{label}: Docker action is not digest-pinned: "
                        f"{action_ref}"
                    )
                continue

            parsed = parse_remote_action(action_ref)
            if not parsed:
                errors.append(
                    f"{label}: remote action is not pinned to a lowercase "
                    f"40-character commit SHA: {action_ref}"
                )
                continue

            action_occurrences += 1
            action, sha = parsed
            repository = "/".join(action.split("/")[:2]).lower()
            pins[repository][sha].append(label)

    for repository, repository_pins in sorted(pins.items()):
        if len(repository_pins) <= 1:
            continue
        details = []
        for sha, locations in sorted(repository_pins.items()):
            details.append(f"{sha} at {', '.join(locations)}")
        errors.append(
            f"{repository}: inconsistent workflow action pins: "
            f"{'; '.join(details)}"
        )

    errors.extend(mirrored_pin_errors(root))
    counts = {
        "workflow_files": len(workflows),
        "action_occurrences": action_occurrences,
        "action_repositories": len(pins),
        "docker_occurrences": docker_occurrences,
    }
    return errors, counts


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="repository root to inspect",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    errors, counts = check_repository(root)

    if errors:
        for error in errors:
            print(f"[FAIL] {error}", file=sys.stderr)
        return 1

    print(
        "[OK] workflow action pin policy: "
        f"{counts['action_occurrences']} remote action references, "
        f"{counts['action_repositories']} repositories, "
        f"{counts['docker_occurrences']} Docker action references, "
        "0 mirrored pins"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
