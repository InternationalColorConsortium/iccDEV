#!/bin/bash
###############################################################
#
# Copyright (c) 2025-2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
## Intent: Parse CodeQL SARIF files and generate a text report
#
## Usage:
##   .github/scripts/codeql-report.sh [sarif_file ...]
##   .github/scripts/codeql-report.sh --fingerprints-out FILE [sarif_file ...]
##   .github/scripts/codeql-report.sh codeql-results/*.sarif
##
## Output: Text report to stdout
##
## Last Updated: 2026-04-09 by David Hoyt
#
###############################################################
set -euo pipefail

FINGERPRINTS_OUT=""
if [ "${1:-}" = "--fingerprints-out" ]; then
  if [ $# -lt 3 ] || [ -z "${2:-}" ]; then
    echo "ERROR: --fingerprints-out requires a path and at least one SARIF file" >&2
    exit 1
  fi
  FINGERPRINTS_OUT="$2"
  shift 2
fi

if [ $# -eq 0 ]; then
  echo "Usage: $0 [--fingerprints-out FILE] <sarif_file> [sarif_file ...]" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required but not found" >&2
  exit 1
fi

python3 - "$FINGERPRINTS_OUT" "$@" << 'PYTHON_SCRIPT'
import hashlib
import json
import sys
import os
from collections import defaultdict

def category_for_run(run, path):
    automation_id = run.get("automationDetails", {}).get("id", "")
    if automation_id:
        return automation_id.rstrip("/").rsplit("/", 1)[0] if not automation_id.endswith("/") else automation_id.rstrip("/")
    return {
        "cpp-security-and-quality.sarif": "iccdev-standard-cpp-security-and-quality",
        "iccdev-security.sarif": "iccdev-custom-security",
        "iccdev-jsonlib-security.sarif": "iccdev-jsonlib-security",
    }.get(os.path.basename(path), os.path.basename(path))


def clean_field(value):
    return str(value).replace("\t", " ").replace("\r", " ").replace("\n", " ")


def parse_sarif(path):
    """Parse a SARIF file and return structured results."""
    try:
        with open(path) as f:
            data = json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        raise RuntimeError("Failed to parse %s: %s" % (path, e)) from e
    if not isinstance(data, dict) or not isinstance(data.get("runs"), list):
        raise RuntimeError("Invalid SARIF document in %s: missing runs array" % path)
    results = []
    for run in data.get("runs", []):
        # Build rule lookup
        rules = {}
        tool = run.get("tool", {}).get("driver", {})
        tool_name = tool.get("name", "unknown")
        tool_version = tool.get("semanticVersion", tool.get("version", "unknown"))
        category = category_for_run(run, path)
        for ext in run.get("tool", {}).get("extensions", []):
            for rule in ext.get("rules", []):
                rules[rule["id"]] = rule
        for rule in tool.get("rules", []):
            rules[rule["id"]] = rule

        for result in run.get("results", []):
            rule_id = result.get("ruleId", "unknown")
            msg = result.get("message", {}).get("text", "")
            locs = result.get("locations", [])
            loc_str = ""
            file_path = ""
            line = 0
            if locs:
                pl = locs[0].get("physicalLocation", {})
                art = pl.get("artifactLocation", {}).get("uri", "")
                region = pl.get("region", {})
                line = region.get("startLine", 0)
                loc_str = "{}:{}".format(art, line)
                file_path = art

            # Get rule metadata
            rule_meta = rules.get(rule_id, {})
            level = result.get("level") or rule_meta.get("defaultConfiguration", {}).get("level", "warning")
            severity = rule_meta.get("properties", {}).get("problem.severity", level)
            tags = rule_meta.get("properties", {}).get("tags", [])
            cwe_tags = [t for t in tags if t.startswith("external/cwe/")]
            cwe_str = ", ".join(t.replace("external/cwe/", "").upper() for t in cwe_tags)

            results.append({
                "rule_id": rule_id,
                "level": level,
                "severity": severity,
                "message": msg,
                "location": loc_str,
                "file": file_path,
                "line": line,
                "cwe": cwe_str,
                "tool": tool_name,
                "tool_version": tool_version,
                "category": category,
                "fingerprint": result.get("partialFingerprints", {}).get("primaryLocationLineHash", ""),
            })
    return results

def severity_rank(s):
    """Sort key: error > warning > note > recommendation."""
    order = {"error": 0, "warning": 1, "note": 2, "recommendation": 3}
    return order.get(s.lower(), 9)

def main():
    fingerprints_out = sys.argv[1]
    sarif_files = sys.argv[2:]
    all_results = []

    for path in sarif_files:
        if not os.path.isfile(path):
            raise RuntimeError("SARIF input is missing or is not a regular file: {}".format(path))
        results = parse_sarif(path)
        all_results.extend(results)

    fingerprint_rows = sorted(
        "\t".join([
            clean_field(r["category"]),
            clean_field(r["rule_id"]),
            clean_field(r["file"]),
            str(r["line"]),
            clean_field(r["fingerprint"]),
        ])
        for r in all_results
    )
    fingerprint_payload = "\n".join(fingerprint_rows)
    if fingerprint_payload:
        fingerprint_payload += "\n"
    fingerprint_digest = hashlib.sha256(fingerprint_payload.encode("utf-8")).hexdigest()
    if fingerprints_out:
        with open(fingerprints_out, "w", encoding="utf-8", newline="\n") as stream:
            stream.write("category\trule_id\tpath\tline\tprimary_location_line_hash\n")
            stream.write(fingerprint_payload)

    # Header
    print("=" * 72)
    print("iccDEV CodeQL Security Report")
    print("=" * 72)
    print()
    print("SARIF files analyzed: {}".format(len(sarif_files)))
    print("Total findings: {}".format(len(all_results)))
    print("Fingerprint rows: {}".format(len(fingerprint_rows)))
    print("Fingerprint SHA-256: {}".format(fingerprint_digest))
    if fingerprints_out:
        print("Fingerprint manifest: {}".format(fingerprints_out))
    print()

    if not all_results:
        print("[OK] No findings detected.")
        return

    # Summary by severity
    by_level = defaultdict(int)
    for r in all_results:
        by_level[r["level"]] += 1
    print("--- Severity Summary ---")
    for level in sorted(by_level, key=severity_rank):
        print("  {}: {}".format(level.upper(), by_level[level]))
    print()

    # Summary by SARIF category. These raw counts remain visible even when the
    # corresponding GitHub code-scanning alerts have been dismissed.
    by_category = defaultdict(list)
    for r in all_results:
        by_category[r["category"]].append(r)
    print("--- Raw Findings by SARIF Category ---")
    for category, results in sorted(by_category.items()):
        rows = sorted(
            "\t".join([
                clean_field(r["category"]), clean_field(r["rule_id"]),
                clean_field(r["file"]), str(r["line"]), clean_field(r["fingerprint"]),
            ])
            for r in results
        )
        payload = "\n".join(rows) + ("\n" if rows else "")
        digest = hashlib.sha256(payload.encode("utf-8")).hexdigest()
        versions = sorted(set(r["tool_version"] for r in results))
        print("  {:45s} {:>5d}  {}  {}".format(
            category, len(results), ",".join(versions), digest
        ))
    print()

    # Summary by rule
    by_rule = defaultdict(int)
    for r in all_results:
        by_rule[r["rule_id"]] += 1
    print("--- Findings by Rule ---")
    for rule_id, count in sorted(by_rule.items(), key=lambda x: -x[1]):
        print("  {:60s} {:>5d}".format(rule_id, count))
    print()

    # Summary by file (top 20)
    by_file = defaultdict(int)
    for r in all_results:
        if r["file"]:
            by_file[r["file"]] += 1
    print("--- Top 20 Files by Finding Count ---")
    for fpath, count in sorted(by_file.items(), key=lambda x: -x[1])[:20]:
        print("  {:60s} {:>5d}".format(fpath, count))
    print()

    # Summary by component
    by_component = defaultdict(int)
    for r in all_results:
        f = r["file"]
        if f.startswith("IccProfLib/"):
            by_component["IccProfLib"] += 1
        elif f.startswith("IccXML/IccLibXML/"):
            by_component["IccLibXML"] += 1
        elif f.startswith("IccJSON/IccLibJSON/"):
            by_component["IccLibJSON"] += 1
        elif f.startswith("IccConnect/IccLibConnect/"):
            by_component["IccLibConnect"] += 1
        elif f.startswith("IccXML/CmdLine/"):
            by_component["IccXML Tools"] += 1
        elif f.startswith("Tools/CmdLine/"):
            by_component["CLI Tools"] += 1
        elif f.startswith("Tools/Winnt/"):
            by_component["Windows Tools"] += 1
        elif f.startswith("Tools/wxWidget/"):
            by_component["wxWidget Tools"] += 1
        elif f.startswith("examples/"):
            by_component["Examples"] += 1
        else:
            by_component["Other"] += 1
    print("--- Findings by Component ---")
    for comp, count in sorted(by_component.items(), key=lambda x: -x[1]):
        print("  {:60s} {:>5d}".format(comp, count))
    print()

    # CWE summary
    by_cwe = defaultdict(int)
    for r in all_results:
        if r["cwe"]:
            for cwe in r["cwe"].split(", "):
                by_cwe[cwe] += 1
    if by_cwe:
        print("--- Findings by CWE ---")
        for cwe, count in sorted(by_cwe.items(), key=lambda x: -x[1]):
            print("  {:60s} {:>5d}".format(cwe, count))
        print()

    # Detailed error-level findings
    errors = [r for r in all_results if r["level"] == "error"]
    if errors:
        print("=" * 72)
        print("ERROR-LEVEL FINDINGS (require attention)")
        print("=" * 72)
        for r in sorted(errors, key=lambda x: (x["rule_id"], x["file"], x["line"])):
            print()
            print("  Rule: {}".format(r["rule_id"]))
            print("  Location: {}".format(r["location"]))
            if r["cwe"]:
                print("  CWE: {}".format(r["cwe"]))
            print("  {}".format(r["message"][:200]))
        print()

    # Footer
    print("=" * 72)
    print("Report complete. {} total findings ({} error, {} warning, {} note)".format(
        len(all_results),
        by_level.get("error", 0),
        by_level.get("warning", 0),
        by_level.get("note", 0),
    ))
    print("=" * 72)

if __name__ == "__main__":
    try:
        main()
    except RuntimeError as error:
        print("ERROR: {}".format(error), file=sys.stderr)
        sys.exit(1)
PYTHON_SCRIPT
