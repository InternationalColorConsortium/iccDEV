#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# See LICENSE.md in the repository root for the BSD-3-Clause license.

"""Measure ICC XML/JSON fidelity while preserving native process evidence."""

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import csv
from dataclasses import dataclass
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
TOOLS = {
    "to_xml": ("ICC_TO_XML", "IccToXml", "iccToXml"),
    "from_xml": ("ICC_FROM_XML", "IccFromXml", "iccFromXml"),
    "to_json": ("ICC_TO_JSON", "IccToJson", "iccToJson"),
    "from_json": ("ICC_FROM_JSON", "IccFromJson", "iccFromJson"),
    "dump": ("ICC_DUMP_PROFILE", "IccDumpProfile", "iccDumpProfile"),
}
DIAGNOSTIC = re.compile(
    r"NonCompliant!|Warning!|Critical Error!|Error!|"
    r"(^|[^A-Za-z])(ERROR|Error:|Unable to|[Ff]ailed)"
)
SANITIZER = re.compile(
    r"^(==[0-9]+==)?(ERROR|SUMMARY|WARNING): "
    r"(AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|"
    r"MemorySanitizer|ThreadSanitizer)|"
    r"^AddressSanitizer:DEADLYSIGNAL|^MemorySanitizer:|^runtime error:|"
    r"^.+:[0-9]+(:[0-9]+)?: runtime error:"
)
LOADER_FAILURE = re.compile(r"error while loading shared libraries:")
QUOTED_OPERAND = re.compile(r"'[^']*'|\"[^\"]*\"")
ABNORMAL = {"timeout", "signal", "sanitizer", "launch-error"}
WINDOWS_TOOL_REJECTIONS = {0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD}
FIELDS = [
    "profile", "input_sha256", "input_bytes", "source_validation_status",
    "to_xml_status", "from_xml_status", "from_xml_bytes", "from_xml_size_delta",
    "from_xml_identical", "from_xml_validation_status", "from_xml_source_fidelity",
    "to_json_status", "from_json_status", "from_json_bytes", "from_json_size_delta",
    "from_json_identical", "from_json_validation_status", "from_json_source_fidelity",
    "xml_json_diagnostic_parity", "overall",
]


def ascii_text(value):
    return str(value).encode("ascii", "backslashreplace").decode("ascii")


def tsv_text(value):
    return json.dumps(str(value), ensure_ascii=True)[1:-1]


def write_text(path, text):
    with path.open("w", encoding="ascii", newline="\n") as stream:
        stream.write(ascii_text(text))


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def number(value, minimum=0, maximum=2147483647):
    if not re.fullmatch(r"[0-9]+", value) or len(value) > 10:
        raise argparse.ArgumentTypeError("expected a bounded decimal integer")
    result = int(value, 10)
    if not minimum <= result <= maximum:
        raise argparse.ArgumentTypeError(f"expected {minimum} through {maximum}")
    return result


def parser():
    result = argparse.ArgumentParser(
        description=__doc__,
        epilog="Exit: 0 measured without failures/untriaged diagnostics; "
        "1 profile/process failure; 2 setup error; 3 untriaged diagnostics only. "
        "Requires Python 3.9+. Tool overrides: ICCDEV_TOOLS_DIR, ICC_TO_XML, "
        "ICC_FROM_XML, ICC_TO_JSON, ICC_FROM_JSON, ICC_DUMP_PROFILE.",
    )
    result.add_argument("--jobs", type=lambda x: number(x, 1, 32), default=1, metavar="N")
    result.add_argument("--max-profiles", type=number, default=0, metavar="N",
                        help="maximum sorted profiles; 0 selects all")
    result.add_argument("--timeout", type=lambda x: number(x, 1, 86400), default=60,
                        metavar="SECONDS", help="per-tool time limit; default 60")
    result.add_argument("--compare-baseline", type=Path, metavar="DIR")
    result.add_argument("icc_path", type=Path)
    result.add_argument("output_directory", type=Path,
                        help="unused evidence directory outside the input tree")
    return result


def resolve_tools():
    directory = Path(os.environ.get("ICCDEV_TOOLS_DIR", ROOT / "Build/Tools"))
    suffix = ".exe" if os.name == "nt" else ""
    tools = {}
    for name, (variable, subdirectory, executable) in TOOLS.items():
        if variable in os.environ:
            candidates = [Path(os.environ[variable])]
        else:
            candidates = [directory / subdirectory / (executable + suffix),
                          directory / (executable + suffix)]
        for candidate in candidates:
            if candidate.is_file() and os.access(candidate, os.X_OK):
                tools[name] = candidate.resolve()
                break
        else:
            raise ValueError(f"Tool is not executable: {variable} ({candidates})")
    return tools


@dataclass
class Stage:
    status: str
    returncode: object
    diagnostics: tuple
    log: Path
    signal: bool = False
    sanitizer: bool = False
    timed_out: bool = False


def is_signal_exit(code, platform_name=None):
    if code is None:
        return False
    if code < 0:
        return True
    if (platform_name or os.name) != "nt":
        return False
    return code >= 0x80000000 and code not in WINDOWS_TOOL_REJECTIONS


def normalized_diagnostic(line, artifact_root):
    if re.match(r"^\s*Profile:\s+", line):
        return None
    message = " ".join(line.split())
    if artifact_root is not None:
        root = str(artifact_root)
        for spelling in {root, root.replace("\\", "/")}:
            message = message.replace(spelling, "<evidence>")
    return message


def run_stage(command, log, timeout, output=None, artifact_root=None):
    code = None
    timed_out = False
    launch_error = False
    # Stream to disk: large profile dumps must not accumulate in parent memory.
    with log.open("wb") as stream:
        try:
            completed = subprocess.run(
                [str(part) for part in command], stdout=stream, stderr=subprocess.STDOUT,
                timeout=timeout, check=False,
            )
            code = completed.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
        except OSError as error:
            launch_error = True
            stream.write(ascii_text(f"[FAIL] Unable to launch tool: {error}\n").encode("ascii"))
    diagnostics = set()
    sanitizer = False
    loader_failure = False
    with log.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            message = normalized_diagnostic(line, artifact_root)
            if message is None:
                continue
            classification = QUOTED_OPERAND.sub("<operand>", message)
            sanitizer |= bool(SANITIZER.search(classification))
            loader_failure |= bool(LOADER_FAILURE.search(classification))
            if DIAGNOSTIC.search(message) or LOADER_FAILURE.search(message):
                diagnostics.add(message)
    launch_error |= code is not None and code != 0 and loader_failure
    signal = is_signal_exit(code)
    if timed_out:
        status = "timeout"
    elif signal:
        status = "signal"
    elif launch_error:
        status = "launch-error"
    elif sanitizer:
        status = "sanitizer"
    elif output is None:
        status = "success" if code == 0 else "tool-rejection"
    elif output.is_file() and output.stat().st_size > 0:
        status = "success" if code == 0 else "warning-output"
    else:
        status = "missing-output" if code == 0 else "error"
    write_text(Path(str(log) + ".exit"), "timeout\n" if timed_out else f"{code}\n")
    write_text(Path(str(log) + ".command.json"), json.dumps(
        {"argv": [str(part) for part in command], "returncode": code,
         "status": status, "timed_out": timed_out, "sanitizer": sanitizer,
         "signal": signal}, ensure_ascii=True, indent=2) + "\n")
    return Stage(status, code, tuple(sorted(diagnostics)), log, signal, sanitizer, timed_out)


def fidelity(source, returned, usable):
    if not usable or source.status != "success" or returned.status != "success":
        return "n/a"
    if source.diagnostics == returned.diagnostics:
        return "same"
    if source.diagnostics and not returned.diagnostics:
        return "masked"
    if not source.diagnostics:
        return "added"
    return "changed"


def triage(message):
    if message == (
        "NonCompliant! - Unknown 'aapy' = 61617079: - "
        "Invalid data flag encoding (reserved high order bits)."
    ):
        return "InternationalColorConsortium/iccDEV#2846"
    if 'Unable to Parse "curveType"' in message:
        return "InternationalColorConsortium/iccDEV#2806"
    if "cicpTag cicpType: Invalid tag type" in message:
        return "known-cicp-type"
    if "Negative Z value" in message:
        return "InternationalColorConsortium/iccDEV#1808-policy"
    return "untriaged"


def process_profile(profile, input_root, output_root, tools, timeout):
    relative = profile.relative_to(input_root)
    # Retain the extension: same-stem .icc/.ICC inputs must not share artifacts.
    logs = output_root / "logs" / relative
    logs.mkdir(parents=True)
    source = run_stage([tools["dump"], "-v", profile], logs / "source-validate.log", timeout,
                       artifact_root=output_root)
    stages = {"source-validate": source}
    row = {
        "profile": tsv_text(relative.as_posix()), "input_sha256": digest(profile),
        "input_bytes": profile.stat().st_size, "source_validation_status": source.status,
    }
    returned = {}
    for side in ("xml", "json"):
        forward = output_root / side / relative / ("profile." + side)
        reverse = output_root / ("from-" + side) / relative
        forward.parent.mkdir(parents=True, exist_ok=True)
        reverse.parent.mkdir(parents=True, exist_ok=True)
        command = [tools["to_" + side], profile, forward]
        if side == "json":
            command.append("-sort")
        converted = run_stage(command, logs / ("icc-to-" + side + ".log"), timeout, forward,
                              output_root)
        stages["icc-to-" + side] = converted
        imported = Stage("not-run", None, (), logs / (side + "-to-icc.log"))
        validated = Stage("not-run", None, (), logs / ("from-" + side + "-validate.log"))
        if converted.status in {"success", "warning-output"}:
            imported = run_stage(
                [tools["from_" + side], forward, reverse],
                imported.log, timeout, reverse, output_root,
            )
        if imported.status in {"success", "warning-output"}:
            validated = run_stage([tools["dump"], "-v", reverse], validated.log, timeout,
                                  artifact_root=output_root)
        stages[side + "-to-icc"] = imported
        stages["from-" + side + "-validate"] = validated
        usable = imported.status in {"success", "warning-output"}
        size = reverse.stat().st_size if reverse.is_file() else 0
        row.update({
            "to_" + side + "_status": converted.status,
            "from_" + side + "_status": imported.status,
            "from_" + side + "_bytes": size,
            "from_" + side + "_size_delta": size - row["input_bytes"] if usable else "n/a",
            "from_" + side + "_identical": (
                "yes" if usable and digest(reverse) == row["input_sha256"] else "no"),
            "from_" + side + "_validation_status": validated.status,
            "from_" + side + "_source_fidelity": fidelity(source, validated, usable),
        })
        returned[side] = validated
    row["xml_json_diagnostic_parity"] = (
        "n/a" if any(stage.status != "success" for stage in returned.values())
        else "same" if returned["xml"].diagnostics == returned["json"].diagnostics else "differ"
    )
    failed = source.status != "success" or any(
        stage.status in ABNORMAL or stage.status in {"error", "missing-output", "not-run"}
        or stage.sanitizer for stage in stages.values()
    ) or any(stage.status != "success" for stage in returned.values())
    warning = any(stage.diagnostics or stage.status == "warning-output" for stage in stages.values())
    row["overall"] = "fail" if failed else "warning" if warning else "pass"
    return row, stages


def write_tsv(path, fields, rows):
    with path.open("w", encoding="ascii", newline="") as stream:
        writer = csv.DictWriter(stream, fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def load_baseline(directory):
    with (directory / "run.json").open(encoding="ascii") as stream:
        metadata = json.load(stream)
    if not isinstance(metadata, dict) or metadata.get("schema_version") != 1 or metadata.get("complete") is not True:
        raise ValueError("Baseline is incomplete or has an unsupported schema")
    summary = dict(line.split("=", 1) for line in
                   (directory / "summary.txt").read_text(encoding="ascii").splitlines())
    diagnostic_lines = (directory / "diagnostics.tsv").read_text(encoding="ascii").splitlines()
    with (directory / "results.tsv").open(encoding="ascii", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if reader.fieldnames != FIELDS:
            raise ValueError("Baseline result columns do not match this runner")
        rows = list(reader)
    if "profiles" not in summary or not summary["profiles"].isdigit():
        raise ValueError("Baseline summary requires a numeric profile count")
    if any(set(row) != set(FIELDS) or None in row.values() for row in rows):
        raise ValueError("Baseline has a malformed result row")
    if len(rows) != int(summary["profiles"]):
        raise ValueError("Baseline result count does not match its summary")
    return summary, diagnostic_lines, rows


def compare_baseline(baseline, output, metrics, diagnostic_lines, rows):
    old_metrics, old_diagnostics, old_rows = baseline
    lines = []
    normalized_rows = [{key: str(value) for key, value in row.items()} for row in rows]
    for name, old, new in (
        ("summary-metrics", sorted(old_metrics.items()),
         sorted((key, str(value)) for key, value in metrics.items())),
        ("diagnostics", old_diagnostics, diagnostic_lines),
        ("profile-results", old_rows, normalized_rows),
    ):
        before = json.dumps(old, ensure_ascii=True, indent=2).splitlines()
        after = json.dumps(new, ensure_ascii=True, indent=2).splitlines()
        lines += [name + ":", "status=" + ("match" if old == new else "changed")]
        lines.extend(difflib.unified_diff(before, after, fromfile="baseline", tofile="current",
                                         lineterm=""))
    write_text(output / "baseline-comparison.txt", "\n".join(lines) + "\n")


def execute(args):
    tools = resolve_tools()
    input_path = args.icc_path.resolve(strict=True)
    if not input_path.is_file() and not input_path.is_dir():
        raise ValueError(f"ICC input must be a regular file or directory: {input_path}")
    input_root = input_path.parent if input_path.is_file() else input_path
    profiles = [input_path] if input_path.is_file() else sorted(
        path for path in input_path.rglob("*") if path.is_file() and path.suffix.lower() == ".icc"
    )
    if args.max_profiles:
        profiles = profiles[:args.max_profiles]
    if not profiles:
        raise ValueError(f"No ICC profiles below: {input_path}")
    baseline = load_baseline(args.compare_baseline) if args.compare_baseline else None
    output = args.output_directory.absolute()
    resolved_output = output.resolve(strict=False)
    if resolved_output.is_relative_to(input_root) or input_root.is_relative_to(resolved_output):
        raise ValueError("Evidence directory must be outside the input tree")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.mkdir()
    for name in ("xml", "json", "from-xml", "from-json", "logs"):
        (output / name).mkdir()
    metadata = {
        "schema_version": 1, "complete": False, "input": str(input_path),
        "jobs": args.jobs, "timeout_seconds": args.timeout, "max_profiles": args.max_profiles,
        "platform": sys.platform, "python": sys.version, "cwd": os.getcwd(),
        "tools": {name: {"path": str(path), "sha256": digest(path)} for name, path in tools.items()},
    }
    write_text(output / "run.json", json.dumps(metadata, ensure_ascii=True, indent=2) + "\n")
    rows = []
    diagnostics = Counter()
    metrics = Counter()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = pool.map(lambda path: process_profile(
            path, input_root, output, tools, args.timeout), profiles)
        for row, stages in results:
            rows.append(row)
            metrics[row["overall"]] += 1
            for name, stage in stages.items():
                metrics["timeouts"] += stage.timed_out
                metrics["signal_terminations"] += stage.signal
                metrics["sanitizer_findings"] += stage.sanitizer
                metrics["launch_errors"] += stage.status == "launch-error"
                diagnostics.update((name, triage(message), tsv_text(message))
                                   for message in stage.diagnostics)
            if len(rows) % 100 == 0 or len(rows) == len(profiles):
                print(f"[PROGRESS] {len(rows)}/{len(profiles)} profiles", flush=True)
    write_tsv(output / "results.tsv", FIELDS, rows)
    diagnostic_rows = [
        {"stage": stage, "occurrences": count, "triage": tag, "diagnostic": message}
        for (stage, tag, message), count in sorted(diagnostics.items())
    ]
    write_tsv(output / "diagnostics.tsv", ["stage", "occurrences", "triage", "diagnostic"],
              diagnostic_rows)
    summary = {name: metrics[name] for name in (
        "pass", "warning", "fail", "timeouts", "signal_terminations",
        "sanitizer_findings", "launch_errors")}
    summary["profiles"] = len(rows)
    summary["untriaged_diagnostics"] = sum(tag == "untriaged" for _, tag, _ in diagnostics)
    summary["xml_json_diagnostic_parity_mismatch"] = sum(
        row["xml_json_diagnostic_parity"] == "differ" for row in rows)
    for side in ("xml", "json"):
        summary["from_" + side + "_byte_identical"] = sum(
            row["from_" + side + "_identical"] == "yes" for row in rows)
        for kind in ("same", "masked", "added", "changed", "n/a"):
            summary["from_" + side + "_source_fidelity_" + kind.replace("/", "")] = sum(
                row["from_" + side + "_source_fidelity"] == kind for row in rows)
    write_text(output / "summary.txt", "".join(f"{key}={value}\n" for key, value in summary.items()))
    if baseline is not None:
        compare_baseline(baseline, output, summary,
                         (output / "diagnostics.tsv").read_text(encoding="ascii").splitlines(), rows)
    report = [
        "ICC XML/JSON ROUND-TRIP QA DEVELOPER SUMMARY",
        " | ".join(f"{key}: {summary[key]}" for key in ("profiles", "pass", "warning", "fail")),
        " | ".join(f"{key}: {summary[key]}" for key in (
            "timeouts", "signal_terminations", "sanitizer_findings", "untriaged_diagnostics")),
        "", "Fidelity measurements (differences require interpretation, not proof of data loss):",
    ]
    for side in ("xml", "json"):
        report.append(f"  {side.upper()} byte-identical returns: " +
                      str(summary["from_" + side + "_byte_identical"]))
        for kind in ("masked", "added", "changed", "n/a"):
            report.append(f"  {side.upper()} diagnostic fidelity {kind}: " +
                          str(summary["from_" + side + "_source_fidelity_" + kind.replace("/", "")]))
    report += ["", "Known/untriaged diagnostic occurrences:"]
    tags = Counter()
    for (_, tag, _), count in diagnostics.items():
        tags[tag] += count
    report += [f"  {tag}: {count}" for tag, count in sorted(tags.items())] or ["  none"]
    report += ["", "Failed profiles and abnormal stages (first 20):"]
    failed_rows = [row for row in rows if row["overall"] == "fail"]
    report += [f"  {row['profile']}: " + ", ".join(
        f"{key}={row[key]}" for key in FIELDS if key.endswith("_status"))
        for row in failed_rows[:20]] or ["  none"]
    report += ["", "Changed returned-profile sizes (first 20):"]
    changed = [row for row in rows if any(
        row["from_" + side + "_size_delta"] not in (0, "n/a") for side in ("xml", "json"))]
    report += [f"  {row['profile']}: XML {row['from_xml_size_delta']}, JSON {row['from_json_size_delta']} bytes"
               for row in changed[:20]] or ["  none"]
    report += ["", f"Artifacts: {output}", "  results.tsv, diagnostics.tsv, summary.txt, run.json",
               "  logs/*/*.log, .exit and .command.json (exact native replay arguments)"]
    if baseline is not None:
        report.append("  baseline-comparison.txt (informational; includes per-profile input hashes)")
    write_text(output / "developer-summary.txt", "\n".join(report) + "\n")
    metadata["complete"] = True
    write_text(output / "run.json", json.dumps(metadata, ensure_ascii=True, indent=2) + "\n")
    print(ascii_text("\n".join(report)), flush=True)
    return 1 if summary["fail"] else 3 if summary["untriaged_diagnostics"] else 0


def main(argv=None):
    args = parser().parse_args(argv)
    try:
        return execute(args)
    except (OSError, ValueError, csv.Error) as error:
        print(ascii_text(f"[FAIL] QA setup or evidence error: {error}"), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
