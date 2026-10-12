#!/usr/bin/env python3
################################################################################
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
################################################################################
"""Check Apply tool failure receipts and future output path aliases."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(arguments, success):
    result = subprocess.run(arguments, capture_output=True, text=True,
                            timeout=30, check=False)
    if (result.returncode == 0) != success:
        raise RuntimeError("unexpected tool status for %s: %s %s" %
                           (arguments[0], result.stdout, result.stderr))
    return result


def check_failure_stream(path, reason):
    with path.open(encoding="ascii") as stream:
        events = [json.loads(line) for line in stream if line.strip()]
    terminal = [event for event in events if event.get("event") in
                ("run_completed", "run_failed")]
    if (not events or events[0].get("event") != "run_started" or
            len(terminal) != 1 or terminal[0] is not events[-1] or
            terminal[0].get("event") != "run_failed" or
            terminal[0].get("reason") != reason):
        raise RuntimeError("invalid failure lifecycle in %s: %s" %
                           (path, events))


def dot_alias(path):
    return str(path.parent) + os.sep + "." + os.sep + path.name


def check_case_alias(path, result):
    if "alias" not in result.stderr.lower():
        raise RuntimeError("case alias was not rejected: %s" % result.stderr)
    if path.exists() and path.stat().st_size != 0:
        raise RuntimeError("case alias was overwritten: %s" % path)


def check_config_tool(tool, root, data, profile, search, case_insensitive):
    label = "search" if search else "named"
    exported = root / (label + "-export.json")
    command = [str(tool), "-exportcfganddata", str(exported), str(data),
               "0", "0", str(profile), "1"]
    if search:
        command += [str(profile), "1", "-INIT", "1"]
    run(command, True)
    config = json.loads(exported.read_text(encoding="ascii"))

    if not search:
        bad_profile = json.loads(json.dumps(config))
        bad_profile["profileSequence"][0]["iccFile"] = str(root / "missing.icc")
        bad_profile_config = root / "named-bad-profile.json"
        bad_profile_config.write_text(json.dumps(bad_profile), encoding="ascii")
        bad_profile_telemetry = root / "named-bad-profile.jsonl"
        run([str(tool), "--telemetry=jsonl", "--telemetry-file",
             str(bad_profile_telemetry), "--cfg", str(bad_profile_config)], False)
        check_failure_stream(bad_profile_telemetry, "early_exit")

    missing_output = root / (label + "-missing") / "out.txt"
    config["dataFiles"]["dstFile"] = str(missing_output)
    failure_config = root / (label + "-failure.json")
    failure_config.write_text(json.dumps(config), encoding="ascii")
    failure_telemetry = root / (label + "-failure.jsonl")
    run([str(tool), "--telemetry=jsonl", "--telemetry-file",
         str(failure_telemetry), "--cfg", str(failure_config)], False)
    check_failure_stream(failure_telemetry, "output_write_failed")

    alias_output = root / (label + "-alias.txt")
    config["dataFiles"]["dstFile"] = str(alias_output)
    alias_config = root / (label + "-alias.json")
    alias_config.write_text(json.dumps(config), encoding="ascii")
    run([str(tool), "--telemetry=jsonl", "--telemetry-file",
         dot_alias(alias_output), "--cfg", str(alias_config)], False)
    if alias_output.exists():
        raise RuntimeError("output/telemetry alias was opened: %s" % alias_output)

    if case_insensitive:
        case_output = root / (label + "-case-output.txt")
        config["dataFiles"]["dstFile"] = str(case_output)
        case_config = root / (label + "-case.json")
        case_config.write_text(json.dumps(config), encoding="ascii")
        result = run([str(tool), "--telemetry=jsonl", "--telemetry-file",
                      str(case_output).upper(), "--cfg", str(case_config)],
                     False)
        check_case_alias(case_output, result)

    if search:
        evidence_output = root / "search-evidence-out.txt"
        config["dataFiles"]["dstFile"] = str(evidence_output)
        evidence_config = root / "search-evidence.json"
        evidence_config.write_text(json.dumps(config), encoding="ascii")
        evidence_telemetry = root / "search-evidence.jsonl"
        run([str(tool), "--telemetry=jsonl", "--telemetry-file",
             str(evidence_telemetry), "--evidence-file",
             str(root / "missing-evidence" / "receipt.json"),
             "--cfg", str(evidence_config)], False)
        check_failure_stream(evidence_telemetry, "evidence_write_failed")


def check_link_tool(tool, root, profile, case_insensitive):
    output = root / "link.cube"
    telemetry = root / "link.jsonl"
    missing_evidence = root / "link-missing" / "receipt.json"
    link_args = ["1", "2", "4", "QA link", "0", "1", "0", "0",
                 str(profile), "1"]
    run([str(tool), str(output)] + link_args +
        ["--telemetry=jsonl", "--telemetry-file", str(telemetry),
         "--evidence-file", str(missing_evidence)], False)
    check_failure_stream(telemetry, "evidence_write_failed")

    alias_output = root / "link-alias.cube"
    run([str(tool), str(alias_output)] + link_args +
        ["--telemetry=jsonl", "--telemetry-file", dot_alias(alias_output)],
        False)
    if alias_output.exists():
        raise RuntimeError("link/telemetry alias was opened")

    evidence_alias = root / "link-evidence-alias.cube"
    run([str(tool), str(evidence_alias)] + link_args +
        ["--evidence-file", dot_alias(evidence_alias)], False)
    if evidence_alias.exists():
        raise RuntimeError("link/evidence alias was opened")

    if case_insensitive:
        case_alias = root / "link-case-alias.cube"
        result = run([str(tool), str(case_alias)] + link_args +
                     ["--telemetry=jsonl", "--telemetry-file",
                      str(case_alias).upper()], False)
        check_case_alias(case_alias, result)


def check_profiles_tool(tool, root, source, profile, case_insensitive):
    tail = ["1", "0", "0", "1", "0", str(profile), "1"]
    missing_output = root / "profiles-missing" / "out.tif"
    telemetry = root / "profiles-failure.jsonl"
    run([str(tool), "--quiet", "--telemetry=jsonl", "--telemetry-file",
         str(telemetry), str(source), str(missing_output)] + tail, False)
    check_failure_stream(telemetry, "early_exit")

    alias_output = root / "profiles-alias.tif"
    run([str(tool), "--quiet", "--telemetry=jsonl", "--telemetry-file",
         dot_alias(alias_output), str(source), str(alias_output)] + tail, False)
    if alias_output.exists():
        raise RuntimeError("TIFF/telemetry alias was opened")

    if case_insensitive:
        case_output = root / "profiles-case-output.tif"
        result = run([str(tool), "--quiet", "--telemetry=jsonl",
                      "--telemetry-file", str(case_output).upper(),
                      str(source), str(case_output)] + tail, False)
        check_case_alias(case_output, result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("profiles", "named", "search", "link"):
        parser.add_argument("--" + name + "-tool", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--profile", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="iccdev-apply-failures-") as directory:
        root = Path(directory)
        probe = root / "CaseProbe"
        probe.write_text("probe", encoding="ascii")
        case_insensitive = (root / "caseprobe").exists()
        probe.unlink()
        check_profiles_tool(args.profiles_tool, root, args.source, args.profile,
                            case_insensitive)
        check_config_tool(args.named_tool, root, args.data, args.profile, False,
                          case_insensitive)
        check_config_tool(args.search_tool, root, args.data, args.profile, True,
                          case_insensitive)
        check_link_tool(args.link_tool, root, args.profile, case_insensitive)
    print("Apply telemetry failure and path-alias checks passed")


if __name__ == "__main__":
    main()
