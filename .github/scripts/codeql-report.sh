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
##   .github/scripts/codeql-report.sh codeql-results/*.sarif
##
## Output: Text report to stdout
##
## Last Updated: 2026-04-09 by David Hoyt
#
###############################################################
set -euo pipefail

if [ $# -eq 0 ]; then
  echo "Usage: $0 <sarif_file> [sarif_file ...]" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required but not found" >&2
  exit 1
fi

python3 - "$@" << 'PYTHON_SCRIPT'
import json
import sys
from collections import defaultdict

def parse_sarif(path):
    """Parse a SARIF file and return structured results."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict) or data.get("version") != "2.1.0":
        raise ValueError("expected a SARIF 2.1.0 object")
    runs = data.get("runs")
    if not isinstance(runs, list) or not runs:
        raise ValueError("expected a non-empty SARIF runs array")
    results = []
    for run in runs:
        if not isinstance(run, dict):
            raise ValueError("each SARIF run must be an object")
        tool = run.get("tool")
        driver = tool.get("driver") if isinstance(tool, dict) else None
        if (
            not isinstance(driver, dict)
            or not isinstance(driver.get("name"), str)
            or not driver["name"].strip()
        ):
            raise ValueError("each SARIF run must identify its tool driver")
        invocations = run.get("invocations", [])
        if not isinstance(invocations, list):
            raise ValueError("SARIF invocations must be an array")
        for invocation in invocations:
            if not isinstance(invocation, dict):
                raise ValueError("each SARIF invocation must be an object")
            if "executionSuccessful" in invocation and not isinstance(invocation["executionSuccessful"], bool):
                raise ValueError("SARIF invocation success must be a boolean")
            if invocation.get("executionSuccessful") is False:
                raise ValueError("SARIF records an unsuccessful analysis invocation")
        run_results = run.get("results", [])
        if not isinstance(run_results, list):
            raise ValueError("SARIF results must be an array")
        # Build rule lookup
        rules = {}
        tool = run.get("tool", {}).get("driver", {})
        tool_name = tool.get("name", "unknown")
        for ext in run.get("tool", {}).get("extensions", []):
            for rule in ext.get("rules", []):
                rules[rule["id"]] = rule
        for rule in tool.get("rules", []):
            rules[rule["id"]] = rule

        for result in run_results:
            if not isinstance(result, dict):
                raise ValueError("each SARIF result must be an object")
            rule_id = result.get("ruleId", "unknown")
            level = result.get("level", "warning")
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
            })
    return results

def severity_rank(s):
    """Sort key: error > warning > note > recommendation."""
    order = {"error": 0, "warning": 1, "note": 2, "recommendation": 3}
    return order.get(s.lower(), 9)

def main():
    sarif_files = sys.argv[1:]
    all_results = []

    for path in sarif_files:
        try:
            results = parse_sarif(path)
        except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
            print("ERROR: Failed to parse {}: {}".format(path, error), file=sys.stderr)
            return 1
        all_results.extend(results)

    # Header
    print("=" * 72)
    print("iccDEV CodeQL Security Report")
    print("=" * 72)
    print()
    print("SARIF files analyzed: {}".format(len(sarif_files)))
    print("Total findings: {}".format(len(all_results)))
    print()

    if not all_results:
        print("[OK] No findings detected.")
        return 0

    # Summary by severity
    by_level = defaultdict(int)
    for r in all_results:
        by_level[r["level"]] += 1
    print("--- Severity Summary ---")
    for level in sorted(by_level, key=severity_rank):
        print("  {}: {}".format(level.upper(), by_level[level]))
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
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
PYTHON_SCRIPT
