#!/usr/bin/env python3
###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Enforce the required Bash workflow security prologue.
#
###############################################################
"""Reject changed Bash workflow steps that omit credential and token hardening."""

import argparse
import subprocess
import sys

import yaml
from yaml.nodes import MappingNode, SequenceNode


REQUIRED_PROLOGUE = (
    "set -euo pipefail",
    'git config --global credential.helper ""',
    "unset GITHUB_TOKEN || true",
)


def as_mapping(value):
    return value if isinstance(value, dict) else {}


def configured_shell(workflow, job, step):
    for owner in (step, job, workflow):
        shell = owner.get("shell")
        if shell:
            return str(shell)
        defaults = as_mapping(owner.get("defaults"))
        run_defaults = as_mapping(defaults.get("run"))
        shell = run_defaults.get("shell")
        if shell:
            return str(shell)
    return ""


def is_bash_step(workflow, job, step):
    shell = configured_shell(workflow, job, step).lower()
    if shell:
        return "bash" in shell

    runner = str(job.get("runs-on", "")).lower()
    return "windows" not in runner


def executable_lines(run):
    return [
        (number, line.strip())
        for number, line in enumerate(str(run).splitlines(), start=1)
        if line.strip() and not line.lstrip().startswith("#")
    ]


def prologue_issues(run):
    lines = executable_lines(run)
    issues = []

    for index, required in enumerate(REQUIRED_PROLOGUE):
        if index >= len(lines) or lines[index][1] != required:
            found = lines[index][1] if index < len(lines) else "end of run block"
            issues.append(f"expected `{required}` before `{found}`")

    return issues


def changed_lines(path, base_ref):
    commands = (
        ("git", "diff", "--unified=0", f"{base_ref}...HEAD", "--", path),
        ("git", "diff", "--cached", "--unified=0", "--", path),
        ("git", "diff", "--unified=0", "--", path),
    )
    lines = set()

    for command in commands:
        result = subprocess.run(command, check=False, capture_output=True, text=True)
        for line in result.stdout.splitlines():
            if not line.startswith("@@"):
                continue
            marker = line.split("+", 1)[1].split(" ", 1)[0]
            start, separator, count = marker.partition(",")
            line_start = int(start)
            line_count = int(count) if separator else 1
            if line_count:
                lines.update(range(line_start, line_start + line_count))
            else:
                lines.add(line_start)

    if lines:
        return lines

    tracked = subprocess.run(
        ("git", "ls-files", "--error-unmatch", "--", path),
        check=False,
        capture_output=True,
        text=True,
    )
    if tracked.returncode:
        with open(path, "r", encoding="utf-8") as handle:
            return set(range(1, len(handle.readlines()) + 1))
    return lines


def mapping_value(node, key):
    if not isinstance(node, MappingNode):
        return None
    for key_node, value_node in node.value:
        if key_node.value == key:
            return value_node
    return None


def step_line_ranges(path):
    with open(path, "r", encoding="utf-8") as handle:
        root = yaml.compose(handle)
    jobs = mapping_value(root, "jobs")
    if not isinstance(jobs, MappingNode):
        return {}

    ranges = {}
    for job_name, job_node in jobs.value:
        steps = mapping_value(job_node, "steps")
        if not isinstance(steps, SequenceNode):
            continue
        for index, step in enumerate(steps.value, start=1):
            ranges[(str(job_name.value), index)] = (
                step.start_mark.line + 1,
                step.end_mark.line + 1,
            )
    return ranges


def check_workflow(path, workflow, changed=None, ranges=None):
    findings = []
    jobs = as_mapping(workflow.get("jobs"))

    for job_name, job_value in jobs.items():
        job = as_mapping(job_value)
        steps = job.get("steps", [])
        if not isinstance(steps, list):
            continue

        for index, step_value in enumerate(steps, start=1):
            step = as_mapping(step_value)
            if "run" not in step or not is_bash_step(workflow, job, step):
                continue
            if changed is not None:
                start, end = ranges.get((str(job_name), index), (0, 0))
                if not any(start <= line <= end for line in changed):
                    continue

            step_name = str(step.get("name", f"step {index}"))
            for issue in prologue_issues(step["run"]):
                findings.append(f"{path}: job {job_name}, step {index} ({step_name}): {issue}")

    return findings


def run_self_test():
    valid = {
        "jobs": {
            "check": {
                "runs-on": "ubuntu-latest",
                "steps": [{"run": "\n".join(REQUIRED_PROLOGUE)}],
            }
        }
    }
    cases = (
        (
            "missing-credential-helper",
            "set -euo pipefail\nunset GITHUB_TOKEN || true",
        ),
        (
            "missing-token-unset",
            'set -euo pipefail\ngit config --global credential.helper ""',
        ),
        (
            "out-of-order-prologue",
            'git config --global credential.helper ""\nset -euo pipefail\nunset GITHUB_TOKEN || true',
        ),
    )

    if check_workflow("valid.yml", valid):
        raise AssertionError("valid fixture reported a finding")

    for name, run in cases:
        invalid = {
            "jobs": {
                "check": {
                    "runs-on": "ubuntu-latest",
                    "steps": [{"run": run}],
                }
            }
        }
        if not check_workflow(f"{name}.yml", invalid):
            raise AssertionError(f"{name} fixture did not report a finding")

    print("[OK] Bash prologue checker fixtures: 4")


def main(arguments):
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--changed", action="store_true")
    parser.add_argument("--base", default="HEAD")
    parser.add_argument("workflows", nargs="*")
    options = parser.parse_args(arguments)

    if options.self_test:
        if options.changed or options.workflows:
            parser.error("--self-test cannot be combined with workflow paths")
        run_self_test()
        return 0

    if not options.workflows:
        parser.error("at least one workflow path is required")

    findings = []
    for path in options.workflows:
        try:
            with open(path, "r", encoding="utf-8") as handle:
                workflow = yaml.safe_load(handle) or {}
        except (OSError, UnicodeDecodeError, yaml.YAMLError) as error:
            findings.append(f"{path}: unable to parse workflow: {error}")
            continue

        if not isinstance(workflow, dict):
            findings.append(f"{path}: workflow root must be a mapping")
            continue
        changed = changed_lines(path, options.base) if options.changed else None
        ranges = step_line_ranges(path) if options.changed else None
        findings.extend(check_workflow(path, workflow, changed, ranges))

    for finding in findings:
        print(f"[FAIL] {finding}", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
