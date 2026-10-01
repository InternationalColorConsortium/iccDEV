#!/usr/bin/env python3
################################################################################
# .github/ci/quality-assurance/scripts/icc_apply_profiles_matrix.py
# Copyright (C) 2026 The International Color Consortium.
#                                        All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
################################################################################
"""Run the deterministic iccApplyProfiles evidence smoke matrix."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys
import time


SCHEMA = "iccdev-applyprofiles-result/v1"
OUTCOMES = {
    "PASS",
    "EXPECTED_REJECTION",
    "PROFILE_QUALITY_ISSUE",
    "OUTPUT_CONTRACT_FAILURE",
    "TOOL_FAILURE",
    "TIMEOUT",
    "SANITIZER_FINDING",
    "MEMORY_TOOL_FINDING",
    "CRASH",
}
SANITIZER_MARKERS = (
    "AddressSanitizer",
    "UndefinedBehaviorSanitizer",
    "runtime error:",
    "LeakSanitizer",
    "MemorySanitizer",
    "ThreadSanitizer",
    "DEADLYSIGNAL",
)
MEMORY_TOOL_MARKERS = (
    "ERROR SUMMARY:",
    "Helgrind, a thread error detector",
    "Memcheck, a memory error detector",
)
TIFF_TYPE_SIZES = {
    1: 1,
    2: 1,
    3: 2,
    4: 4,
    5: 8,
    6: 1,
    7: 1,
    8: 2,
    9: 4,
    10: 8,
    11: 4,
    12: 8,
}


class MatrixError(Exception):
    pass


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n",
        encoding="ascii",
    )


def run_process(argv, timeout_seconds, stdout_path, stderr_path):
    started = time.monotonic()
    timed_out = False
    return_code = None
    try:
        with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
            completed = subprocess.run(
                argv,
                stdin=subprocess.DEVNULL,
                stdout=stdout,
                stderr=stderr,
                timeout=timeout_seconds,
                check=False,
            )
        return_code = completed.returncode
    except subprocess.TimeoutExpired:
        timed_out = True
    return return_code, timed_out, round(time.monotonic() - started, 6)


def read_log_text(*paths):
    return "\n".join(
        path.read_text(encoding="utf-8", errors="replace") for path in paths
    )


def classify_process(return_code, timed_out, log_text):
    if any(marker in log_text for marker in SANITIZER_MARKERS):
        return "SANITIZER_FINDING"
    if any(marker in log_text for marker in MEMORY_TOOL_MARKERS):
        error_counts = re.findall(r"ERROR SUMMARY:\s*(\d+)\s+errors?", log_text)
        if not error_counts or any(int(count) for count in error_counts):
            return "MEMORY_TOOL_FINDING"
    if timed_out:
        return "TIMEOUT"
    if return_code is not None and return_code < 0:
        return "CRASH"
    if return_code is not None and 129 <= return_code <= 192:
        return "CRASH"
    if return_code != 0:
        return "TOOL_FAILURE"
    return None


def unpack_values(data, endian, value_type, count):
    formats = {
        1: "B",
        3: "H",
        4: "I",
        5: "II",
        6: "b",
        7: "B",
        8: "h",
        9: "i",
        10: "ii",
        11: "f",
        12: "d",
    }
    if value_type == 2:
        return data[:count].rstrip(b"\0").decode("ascii", errors="replace")
    if value_type in (5, 10):
        pair_format = endian + formats[value_type]
        pair_size = struct.calcsize(pair_format)
        values = []
        for offset in range(0, pair_size * count, pair_size):
            numerator, denominator = struct.unpack_from(pair_format, data, offset)
            values.append(None if denominator == 0 else numerator / denominator)
        return values
    value_format = endian + str(count) + formats[value_type]
    return list(struct.unpack_from(value_format, data, 0))


def parse_tiff(path, include_pixels=False):
    data = path.read_bytes()
    if len(data) < 8:
        raise MatrixError("TIFF header is truncated")
    if data[:2] == b"II":
        endian = "<"
    elif data[:2] == b"MM":
        endian = ">"
    else:
        raise MatrixError("TIFF byte order marker is invalid")
    if struct.unpack_from(endian + "H", data, 2)[0] != 42:
        raise MatrixError("only classic TIFF version 42 is supported")

    page_count = 0
    first_tags = None
    ifd_offset = struct.unpack_from(endian + "I", data, 4)[0]
    visited = set()
    while ifd_offset:
        if ifd_offset in visited:
            raise MatrixError("TIFF IFD chain contains a cycle")
        visited.add(ifd_offset)
        if ifd_offset > len(data) - 2:
            raise MatrixError("TIFF IFD offset is outside the file")
        entry_count = struct.unpack_from(endian + "H", data, ifd_offset)[0]
        entries_end = ifd_offset + 2 + entry_count * 12
        if entries_end > len(data) - 4:
            raise MatrixError("TIFF IFD entries are truncated")
        tags = {}
        for index in range(entry_count):
            entry_offset = ifd_offset + 2 + index * 12
            tag, value_type, count = struct.unpack_from(
                endian + "HHI", data, entry_offset
            )
            if tag in tags:
                raise MatrixError("TIFF IFD contains duplicate tag %d" % tag)
            type_size = TIFF_TYPE_SIZES.get(value_type)
            if type_size is None or count > len(data):
                raise MatrixError("TIFF tag has an unsupported type or count")
            byte_count = type_size * count
            if byte_count <= 4:
                value_data = data[entry_offset + 8 : entry_offset + 8 + byte_count]
            else:
                value_offset = struct.unpack_from(endian + "I", data, entry_offset + 8)[0]
                if value_offset > len(data) or byte_count > len(data) - value_offset:
                    raise MatrixError("TIFF tag payload is outside the file")
                value_data = data[value_offset : value_offset + byte_count]
            tags[tag] = unpack_values(value_data, endian, value_type, count)
        if first_tags is None:
            first_tags = tags
        page_count += 1
        if page_count > 64:
            raise MatrixError("TIFF has more than 64 pages")
        ifd_offset = struct.unpack_from(endian + "I", data, entries_end)[0]

    if first_tags is None:
        raise MatrixError("TIFF contains no image file directory")

    def values(tag, default=None):
        value = first_tags.get(tag, default)
        if value is None or isinstance(value, str):
            return value
        return list(value)

    def scalar(tag, default=None):
        value = values(tag)
        return default if not value else value[0]

    samples = int(scalar(277, 1))
    bits = values(258, [1])
    sample_format = values(339, [1])
    if len(bits) == 1:
        bits *= samples
    if len(sample_format) == 1:
        sample_format *= samples
    icc_values = values(34675)
    icc_bytes = bytes(icc_values) if icc_values is not None else None
    stored_x_resolution = scalar(282)
    stored_y_resolution = scalar(283)
    metadata = {
        "bits_per_sample": bits,
        "compression": int(scalar(259, 1)),
        "embedded_profile": icc_bytes is not None,
        "embedded_profile_sha256": (
            hashlib.sha256(icc_bytes).hexdigest() if icc_bytes is not None else None
        ),
        "extra_samples": values(338, []),
        "height": int(scalar(257, 0)),
        "orientation": int(scalar(274, 1)),
        "pages": page_count,
        "photometric": int(scalar(262, 0)),
        "planar_configuration": int(scalar(284, 1)),
        "resolution_unit": int(scalar(296, 2)),
        "sample_format": sample_format,
        "samples_per_pixel": samples,
        "stored_x_resolution": stored_x_resolution,
        "stored_y_resolution": stored_y_resolution,
        "width": int(scalar(256, 0)),
        # Match TiffImg::Open(): absent or non-positive resolution is treated as
        # 96 dpi before iccApplyProfiles creates the destination. Keep the raw
        # tag values above so evidence still distinguishes defaulting from a
        # value explicitly stored by the file.
        "x_resolution": (
            stored_x_resolution
            if stored_x_resolution is not None and stored_x_resolution > 0
            else 96.0
        ),
        "y_resolution": (
            stored_y_resolution
            if stored_y_resolution is not None and stored_y_resolution > 0
            else 96.0
        ),
    }
    if include_pixels:
        metadata["pixel_bytes"] = extract_tiff_pixels(data, first_tags, metadata)
        metadata["pixel_sha256"] = hashlib.sha256(metadata["pixel_bytes"]).hexdigest()
    return metadata, icc_bytes


def extract_tiff_pixels(data, tags, metadata):
    if metadata["compression"] != 1:
        raise MatrixError("channel metrics require uncompressed TIFF data")
    if metadata["planar_configuration"] != 1:
        raise MatrixError("channel metrics require contiguous TIFF data")
    if any(value != 8 for value in metadata["bits_per_sample"]):
        raise MatrixError("channel metrics require 8-bit TIFF data")
    if any(value != 1 for value in metadata["sample_format"]):
        raise MatrixError("channel metrics require unsigned TIFF data")
    offsets = tags.get(273)
    counts = tags.get(279)
    if not offsets or not counts or len(offsets) != len(counts):
        raise MatrixError("TIFF strip offsets and byte counts are inconsistent")
    pixels = bytearray()
    for offset, count in zip(offsets, counts):
        offset = int(offset)
        count = int(count)
        if offset > len(data) or count > len(data) - offset:
            raise MatrixError("TIFF strip is outside the file")
        pixels.extend(data[offset : offset + count])
    expected = (
        metadata["width"] * metadata["height"] * metadata["samples_per_pixel"]
    )
    if len(pixels) != expected:
        raise MatrixError(
            "TIFF pixel byte count is %d, expected %d" % (len(pixels), expected)
        )
    return bytes(pixels)


def public_tiff_metadata(metadata):
    return {key: value for key, value in metadata.items() if key != "pixel_bytes"}


def values_equal(left, right):
    if isinstance(left, float) or isinstance(right, float):
        if left is None or right is None:
            return left is right
        return abs(float(left) - float(right)) <= 1.0e-6
    return left == right


def validate_output_contract(contract, source_metadata, output_metadata, profiles):
    differences = []
    for key, expected in contract.items():
        if key in ("source_equal", "embedded_profile_matches", "output_absent",
                   "pixel_sha256_equals_source"):
            continue
        actual = output_metadata.get(key)
        if not values_equal(actual, expected):
            differences.append({"field": key, "expected": expected, "actual": actual})
    for key in contract.get("source_equal", []):
        expected = source_metadata.get(key)
        actual = output_metadata.get(key)
        if not values_equal(actual, expected):
            differences.append({"field": key, "expected": expected, "actual": actual})
    role = contract.get("embedded_profile_matches")
    if role:
        matching = [profile for profile in profiles if profile["role"] == role]
        expected = matching[-1]["sha256"] if matching else None
        actual = output_metadata.get("embedded_profile_sha256")
        if actual != expected:
            differences.append(
                {
                    "field": "embedded_profile_sha256",
                    "expected": expected,
                    "actual": actual,
                }
            )
    if contract.get("pixel_sha256_equals_source"):
        expected = source_metadata.get("pixel_sha256")
        actual = output_metadata.get("pixel_sha256")
        if expected is None or actual != expected:
            differences.append({
                "field": "pixel_sha256",
                "expected": expected,
                "actual": actual,
            })
    return differences


def measure_channel_error(source_metadata, output_metadata):
    source_pixels = source_metadata.get("pixel_bytes")
    output_pixels = output_metadata.get("pixel_bytes")
    if source_pixels is None or output_pixels is None:
        return {"status": "not_measured", "reason": "pixel data unavailable"}
    if len(source_pixels) != len(output_pixels):
        return {
            "status": "not_measured",
            "reason": "source and output sample counts differ",
        }
    channels = source_metadata["samples_per_pixel"]
    if channels != output_metadata["samples_per_pixel"]:
        return {"status": "not_measured", "reason": "channel counts differ"}
    totals = [0] * channels
    maxima = [0] * channels
    samples = len(source_pixels) // channels
    for index, (source_value, output_value) in enumerate(
        zip(source_pixels, output_pixels)
    ):
        difference = abs(source_value - output_value)
        channel = index % channels
        totals[channel] += difference
        maxima[channel] = max(maxima[channel], difference)
    return {
        "status": "measured",
        "unit": "8_bit_code_value",
        "samples_per_channel": samples,
        "mean_absolute_error": [value / samples for value in totals],
        "maximum_absolute_error": maxima,
        "enforcement": "measure_only",
    }


def safe_repo_path(repo_root, relative_path):
    path = (repo_root / relative_path).resolve()
    try:
        path.relative_to(repo_root)
    except ValueError as error:
        raise MatrixError("path escapes repository root: %s" % relative_path) from error
    return path


def assess_profile(profile_path, role, pawg_tool, evidence_dir, timeout_seconds, cache):
    cache_key = str(profile_path.resolve())
    if cache_key in cache:
        cached = dict(cache[cache_key])
        cached["role"] = role
        return cached
    output_path = evidence_dir / (hashlib.sha256(cache_key.encode("utf-8")).hexdigest() + ".json")
    stderr_path = output_path.with_suffix(".stderr")
    return_code, timed_out, elapsed = run_process(
        [str(pawg_tool), "--json", str(profile_path)],
        timeout_seconds,
        output_path,
        stderr_path,
    )
    assessment = {
        "elapsed_seconds": elapsed,
        "json": str(output_path),
        "return_code": return_code,
        "role": role,
        "status": "unavailable",
        "stderr": str(stderr_path),
    }
    # PAWG returns 1 for a checklist failure but still emits its full JSON.
    # Keep that verdict instead of losing it as an unavailable assessment.
    if not timed_out and return_code in (0, 1):
        try:
            report = json.loads(output_path.read_text(encoding="utf-8"))
            summary = report.get("summary", {})
            assessment["summary"] = summary
            assessment["checklist_warn_ids"] = sorted(
                item["id"] for item in report.get("items", [])
                if item.get("verdict") == "WARN"
            )
            if summary.get("fail", 0):
                assessment["status"] = "fail"
            elif summary.get("warn", 0) or summary.get("gap", 0):
                assessment["status"] = "warn"
            else:
                assessment["status"] = "pass"
            if return_code != 0 and assessment["status"] != "fail":
                assessment["status"] = "unavailable"
                assessment["error"] = "nonzero exit without a PAWG failure"
        except (json.JSONDecodeError, OSError) as error:
            assessment["error"] = str(error)
    elif timed_out:
        assessment["error"] = "timeout"
    cache[cache_key] = dict(assessment)
    return assessment


def validate_manifest(manifest):
    if manifest.get("schema") != "iccdev-applyprofiles-matrix/v1":
        raise MatrixError("unsupported manifest schema")
    rows = manifest.get("rows")
    if not isinstance(rows, list) or not rows:
        raise MatrixError("manifest rows must be a nonempty list")
    known_warnings = manifest.get("known_profile_warnings", {})
    if not isinstance(known_warnings, dict):
        raise MatrixError("known_profile_warnings must be an object")
    for digest, declaration in known_warnings.items():
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise MatrixError("known profile warning key must be a SHA-256 digest")
        if (not isinstance(declaration, dict)
                or not isinstance(declaration.get("owner"), str)
                or not declaration["owner"].strip()
                or not isinstance(declaration.get("purpose"), str)
                or not declaration["purpose"].strip()
                or not isinstance(declaration.get("expected_checklist_warn_ids"), list)
                or not declaration["expected_checklist_warn_ids"]
                or not all(isinstance(item, str) and item
                           for item in declaration["expected_checklist_warn_ids"])):
            raise MatrixError("known warning %s requires owner, purpose, and checklist ids" % digest)
    seen = set()
    for row in rows:
        row_id = row.get("id")
        if not isinstance(row_id, str) or not row_id:
            raise MatrixError("every row requires a nonempty id")
        if row_id in seen:
            raise MatrixError("duplicate row id: %s" % row_id)
        seen.add(row_id)
        if row.get("expected_outcome") not in ("PASS", "EXPECTED_REJECTION"):
            raise MatrixError("unsupported expected outcome in row %s" % row_id)
        if not isinstance(row.get("profiles"), list) or not row["profiles"]:
            raise MatrixError("row %s requires at least one profile stage" % row_id)
        if row["expected_outcome"] == "EXPECTED_REJECTION":
            if (row.get("missing_precondition") != "source_embedded_profile"
                    or not isinstance(row.get("expected_message"), str)
                    or not row["expected_message"]
                    or row.get("output_contract", {}).get("output_absent") is not True):
                raise MatrixError("row %s needs a declared absent precondition, message, and output contract" % row_id)
    return rows


def profile_quality_differences(profiles, known_warnings):
    differences = []
    for profile in profiles:
        digest = profile["sha256"]
        if digest is None:
            continue
        assessment = profile["pawg"]
        status = assessment["status"]
        if status == "pass":
            continue
        declaration = known_warnings.get(digest)
        actual_warn_ids = assessment.get("checklist_warn_ids", [])
        if (status != "warn" or declaration is None
                or actual_warn_ids != sorted(declaration["expected_checklist_warn_ids"])
                or assessment.get("summary", {}).get("gap", 0)):
            differences.append({
                "role": profile["role"],
                "profile_sha256": digest,
                "status": status,
                "checklist_warn_ids": actual_warn_ids,
                "expected_checklist_warn_ids": (
                    declaration["expected_checklist_warn_ids"] if declaration else None
                ),
            })
    return differences


def resolve_source(source_spec, repo_root, artifacts):
    prefix = "artifact:"
    if not source_spec.startswith(prefix):
        return safe_repo_path(repo_root, source_spec)
    reference = source_spec[len(prefix) :]
    parts = reference.split("/")
    if len(parts) != 2 or parts[1] != "output" or parts[0] not in artifacts:
        raise MatrixError("invalid or forward artifact reference: %s" % source_spec)
    return artifacts[parts[0]]["output"]


def run_matrix(arguments):
    repo_root = arguments.repo_root.resolve()
    manifest_path = arguments.manifest.resolve()
    out_dir = arguments.out_dir.resolve()
    if out_dir.exists() and any(out_dir.iterdir()):
        raise MatrixError("output directory is not empty: %s" % out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for child in ("artifacts", "logs", "profiles", "receipts", "telemetry", "tiffdump"):
        (out_dir / child).mkdir()

    manifest = json.loads(manifest_path.read_text(encoding="ascii"))
    rows = validate_manifest(manifest)
    manifest_sha256 = sha256_file(manifest_path)
    revision = subprocess.run(
        ["git", "-C", str(repo_root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    binary = arguments.apply_tool.resolve()
    binary_sha256 = sha256_file(binary)
    artifacts = {}
    pawg_cache = {}
    results = []

    for row in rows:
        row_id = row["id"]
        source = resolve_source(row["source"], repo_root, artifacts)
        if not source.is_file():
            raise MatrixError("row %s source is missing: %s" % (row_id, source))
        output = out_dir / "artifacts" / (row_id + ".tif")
        stdout_path = out_dir / "logs" / (row_id + ".stdout")
        stderr_path = out_dir / "logs" / (row_id + ".stderr")
        telemetry_path = out_dir / "telemetry" / (row_id + ".jsonl")
        receipt_path = out_dir / "receipts" / (row_id + ".json")

        source_metadata, source_icc = parse_tiff(source, include_pixels=True)
        if (row.get("missing_precondition") == "source_embedded_profile"
                and source_icc is not None):
            raise MatrixError("row %s requires a source without an embedded profile" % row_id)
        profile_records = []
        command_profiles = []
        for stage in row["profiles"]:
            profile_value = stage["path"]
            if not profile_value:
                command_profiles.extend(["", str(stage["intent"])])
                continue
            profile_path = safe_repo_path(repo_root, profile_value)
            if not profile_path.is_file():
                raise MatrixError(
                    "row %s profile is missing: %s" % (row_id, profile_path)
                )
            command_profiles.extend([str(profile_path), str(stage["intent"])])
            profile_records.append(
                {
                    "path": str(profile_path),
                    "role": stage["role"],
                    "sha256": sha256_file(profile_path),
                    "pawg": assess_profile(
                        profile_path,
                        stage["role"],
                        arguments.pawg_tool,
                        out_dir / "profiles",
                        arguments.timeout,
                        pawg_cache,
                    ),
                }
            )

        source_profile_record = {
            "path": None,
            "role": "source_embedded",
            "sha256": None,
            "pawg": {"status": "not_run", "reason": "source has no embedded profile"},
        }
        if source_icc is not None:
            embedded_path = out_dir / "profiles" / (row_id + "-source-embedded.icc")
            embedded_path.write_bytes(source_icc)
            source_profile_record = {
                "path": str(embedded_path),
                "role": "source_embedded",
                "sha256": hashlib.sha256(source_icc).hexdigest(),
                "pawg": assess_profile(
                    embedded_path,
                    "source_embedded",
                    arguments.pawg_tool,
                    out_dir / "profiles",
                    arguments.timeout,
                    pawg_cache,
                ),
            }

        options = row["options"]
        command = [
            str(binary),
            "--threads",
            str(options["threads"]),
            "--telemetry=jsonl",
            "--telemetry-file",
            str(telemetry_path),
            "--evidence-file",
            str(receipt_path),
            "--quiet",
            str(source),
            str(output),
            str(options["encoding"]),
            str(options["compression"]),
            str(options["planar"]),
            str(options["embed_icc"]),
            str(options["interpolation"]),
        ] + command_profiles
        return_code, timed_out, elapsed = run_process(
            command, arguments.timeout, stdout_path, stderr_path
        )
        log_text = read_log_text(stdout_path, stderr_path)
        process_outcome = classify_process(return_code, timed_out, log_text)
        expected = row["expected_outcome"]
        outcome = process_outcome
        contract_differences = []
        quality_differences = []
        output_metadata = None
        color_metrics = {
            "delta_e_2000": {
                "status": "not_measured",
                "reason": "manifest has no profile-aware reference conversion",
                "enforcement": "measure_only",
            }
        }

        if process_outcome is None and expected == "EXPECTED_REJECTION":
            outcome = "TOOL_FAILURE"
        elif process_outcome == "TOOL_FAILURE" and expected == "EXPECTED_REJECTION":
            expected_message = row.get("expected_message", "")
            if output.exists():
                outcome = "TOOL_FAILURE"
            elif expected_message and expected_message not in log_text:
                outcome = "TOOL_FAILURE"
            else:
                outcome = "EXPECTED_REJECTION"
        elif process_outcome is None:
            if not output.is_file() or output.stat().st_size == 0:
                outcome = "OUTPUT_CONTRACT_FAILURE"
                contract_differences.append(
                    {"field": "output", "expected": "nonempty", "actual": "missing"}
                )
            else:
                try:
                    output_metadata, _ = parse_tiff(output, include_pixels=True)
                    contract_differences = validate_output_contract(
                        row["output_contract"],
                        source_metadata,
                        output_metadata,
                        profile_records,
                    )
                except MatrixError as error:
                    contract_differences.append(
                        {"field": "tiff", "expected": "valid", "actual": str(error)}
                    )
                outcome = "OUTPUT_CONTRACT_FAILURE" if contract_differences else "PASS"
                if output_metadata is not None:
                    if row.get("color_measurement") == "same_channel_error":
                        color_metrics["channel_error"] = measure_channel_error(
                            source_metadata, output_metadata
                        )
                    else:
                        color_metrics["channel_error"] = {
                            "status": "not_measured",
                            "reason": "not requested for this row",
                        }

        tiffdump_record = {"status": "not_run"}
        if output.is_file() and output.stat().st_size:
            dump_stdout = out_dir / "tiffdump" / (row_id + ".stdout")
            dump_stderr = out_dir / "tiffdump" / (row_id + ".stderr")
            dump_rc, dump_timeout, dump_elapsed = run_process(
                [str(arguments.tiffdump_tool), str(output)],
                arguments.timeout,
                dump_stdout,
                dump_stderr,
            )
            tiffdump_record = {
                "elapsed_seconds": dump_elapsed,
                "return_code": dump_rc,
                "status": "timeout" if dump_timeout else ("pass" if dump_rc == 0 else "fail"),
                "stderr": str(dump_stderr),
                "stdout": str(dump_stdout),
            }
            if outcome == "PASS" and tiffdump_record["status"] != "pass":
                outcome = "OUTPUT_CONTRACT_FAILURE"
                contract_differences.append(
                    {
                        "field": "iccTiffDump",
                        "expected": "pass",
                        "actual": tiffdump_record["status"],
                    }
                )

        if outcome == "PASS":
            quality_differences = profile_quality_differences(
                [source_profile_record] + profile_records,
                manifest.get("known_profile_warnings", {}),
            )
            if quality_differences:
                outcome = "PROFILE_QUALITY_ISSUE"

        result = {
            "actual_outcome": outcome,
            "binary": {"path": str(binary), "sha256": binary_sha256},
            "blocking": outcome not in ("PASS", "EXPECTED_REJECTION"),
            "color_metrics": color_metrics,
            "command": {"argv": command, "shell": shlex.join(command)},
            "contract_differences": contract_differences,
            "elapsed_seconds": elapsed,
            "expected_outcome": expected,
            "id": row_id,
            "manifest_sha256": manifest_sha256,
            "output": {
                "path": str(output),
                "sha256": sha256_file(output) if output.is_file() else None,
                "tiff": public_tiff_metadata(output_metadata) if output_metadata else None,
            },
            "policy": manifest["policy"],
            "profile_quality_differences": quality_differences,
            "profiles": [source_profile_record] + profile_records,
            "receipt": str(receipt_path) if receipt_path.is_file() else None,
            "repository_revision": revision,
            "return_code": return_code,
            "schema": SCHEMA,
            "source": {
                "path": str(source),
                "sha256": sha256_file(source),
                "tiff": public_tiff_metadata(source_metadata),
            },
            "stderr": str(stderr_path),
            "stdout": str(stdout_path),
            "telemetry": str(telemetry_path) if telemetry_path.is_file() else None,
            "tiffdump": tiffdump_record,
            "timed_out": timed_out,
        }
        results.append(result)
        artifacts[row_id] = {"output": output}
        print("[%s] %s" % ("PASS" if not result["blocking"] else "FAIL", row_id))

    results_path = out_dir / "results.jsonl"
    with results_path.open("w", encoding="ascii", newline="\n") as stream:
        for result in results:
            stream.write(json.dumps(result, sort_keys=True, ensure_ascii=True) + "\n")
    counts = {outcome: 0 for outcome in sorted(OUTCOMES)}
    for result in results:
        counts[result["actual_outcome"]] += 1
    summary = {
        "blocking_failures": sum(result["blocking"] for result in results),
        "counts": counts,
        "manifest": str(manifest_path),
        "manifest_sha256": manifest_sha256,
        "policy": manifest["policy"],
        "repository_revision": revision,
        "results": str(results_path),
        "schema": "iccdev-applyprofiles-summary/v1",
        "total": len(results),
    }
    write_json(out_dir / "summary.json", summary)
    print(
        "ApplyProfiles matrix: total=%d blocking_failures=%d evidence=%s"
        % (summary["total"], summary["blocking_failures"], out_dir)
    )
    return 1 if summary["blocking_failures"] else 0


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--apply-tool", required=True, type=Path)
    parser.add_argument("--tiffdump-tool", required=True, type=Path)
    parser.add_argument("--pawg-tool", required=True, type=Path)
    parser.add_argument("--timeout", type=int, default=30)
    arguments = parser.parse_args()
    if arguments.timeout < 1:
        parser.error("--timeout must be a positive integer")
    return arguments


def main():
    try:
        return run_matrix(parse_arguments())
    except (MatrixError, OSError, ValueError, json.JSONDecodeError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
