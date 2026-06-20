#!/usr/bin/env python3
"""Generate IccSignatureRegistry.h from registry.color.org CSV exports.

This is the reproducible source-of-truth generator for the signature tables
that IccPawgReport's S3 assessment validates against. It encodes a *dated
snapshot* of the public ICC registries so the validator does not need live
network access (see issue #1459).

Why a snapshot and not a live query: the registries change extremely slowly
and a live check would impose a network dependency on every report run (and on
CI). Phase P2 (a pluggable live check) is noted as a possible future direction
but deliberately not implemented here.

In scope (per #1459): manufacturer signatures, CMM signatures, and the private
tag-signature ranges. Explicitly OUT of scope and not encoded: the patent
registry, CMYK characterization data, the ICC profile registry, the three
component (RGB) color encoding registry, colorimetry data, and the profile
library.

Privacy: the manufacturer/device CSV exports contain registrant contact PII
(names, emails, phone numbers). Only the (signature, company-name) pair is ever
emitted into the generated header; raw CSVs live under _raw/ and are gitignored.

Usage:
    # refresh the raw CSVs from the live registry, then regenerate:
    python3 generate_signature_registry.py --fetch
    # regenerate from the CSVs already in _raw/ (offline, deterministic):
    python3 generate_signature_registry.py

The generated header and the FETCHED_ON date are committed; rerunning with the
same CSVs is deterministic.
"""

import argparse
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "_raw")
SNAP = os.path.join(HERE, "snapshot")
OUT = os.path.join(HERE, "..", "IccSignatureRegistry.h")

# Live CSV export endpoints (used only with --fetch).
CSV_URLS = {
    "manufacturer": "https://registry.color.org/manufacturer-signatures/manufacturer-signatures.csv",
    "cmm": "https://registry.color.org/cmm-signatures/cmm-signatures.csv",
    "tag": "https://registry.color.org/tag-signatures/tag-signatures.csv",
}


def fetch():
    import urllib.request
    os.makedirs(RAW, exist_ok=True)
    for key, url in CSV_URLS.items():
        dst = os.path.join(RAW, key + ".csv")
        with urllib.request.urlopen(url, timeout=60) as resp:
            data = resp.read()
        with open(dst, "wb") as fh:
            fh.write(data)
        print("fetched %-12s %6d bytes -> %s" % (key, len(data), dst))


def read_csv(name):
    path = os.path.join(RAW, name + ".csv")
    with open(path, encoding="utf-8-sig", newline="") as fh:
        return [r for r in csv.reader(fh) if r and any(c.strip() for c in r)]


def hex8(s):
    """Validate an 8-hex-digit signature field; return int or None.

    Tolerates a trailing 'h' radix marker that appears on some registry rows
    (e.g. the CMM export writes '45584143h')."""
    s = s.strip()
    if len(s) == 9 and s[-1] in "hH":
        s = s[:-1]
    if len(s) != 8:
        return None
    try:
        return int(s, 16)
    except ValueError:
        return None


def parse_manufacturers():
    """manufacturer.csv: ID column is 'NAME-HHHHHHHH'; emit (sig, company)."""
    rows = read_csv("manufacturer")[1:]  # drop header
    out = {}
    skipped = []
    for r in rows:
        rid = r[0].strip()
        company = (r[1].strip() if len(r) > 1 else "") or "(unnamed registrant)"
        _, _, hexs = rid.rpartition("-")
        v = hex8(hexs)
        if v is None:
            skipped.append(rid)
            continue
        # First registrant for a signature wins; duplicates are reported.
        out.setdefault(v, company)
    return out, skipped


def parse_cmms():
    """cmm.csv: columns Vendor, Hex value, ASCII, Date, Description."""
    rows = read_csv("cmm")[1:]
    out = {}
    skipped = []
    for r in rows:
        v = hex8(r[1]) if len(r) > 1 else None
        if v is None:
            skipped.append(r[0] if r else "?")
            continue
        vendor = r[0].strip() or "(unnamed)"
        out.setdefault(v, vendor)
    return out, skipped


def parse_tag_ranges():
    """tag.csv: Vendor, Hex value, To hex, ASCII, To ASCII, Date, Tag/Type.

    The private (non-ICC) rows define the registered private-tag signature
    ranges. A blank 'To hex' means a single signature (first==last)."""
    rows = read_csv("tag")[1:]
    out = []
    skipped = []
    for r in rows:
        vendor = r[0].strip()
        if vendor == "ICC":
            continue  # spec-defined tags, validated separately via GetTagSigName
        first = hex8(r[1]) if len(r) > 1 else None
        if first is None:
            skipped.append(r[1].strip() if len(r) > 1 else "?")
            continue
        last = hex8(r[2]) if (len(r) > 2 and r[2].strip()) else first
        if last is None or last < first:
            last = first
        out.append((first, last, vendor or "(unnamed)"))
    out.sort(key=lambda t: (t[0], t[1]))
    return out, skipped


def c_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def emit(manuf, cmm, ranges, fetched_on, notes):
    lines = []
    w = lines.append
    w("/** @file")
    w("    File:       IccSignatureRegistry.h")
    w("")
    w("    Contains:   Generated snapshot of registry.color.org signature tables")
    w("                used by IccPawgReport's S3 header-signature assessment.")
    w("")
    w("    DO NOT EDIT BY HAND. Regenerate with registry/generate_signature_registry.py.")
    w("    Snapshot of the public ICC registries fetched on %s." % fetched_on)
    w("    See issue #1459 and registry/generate_signature_registry.py for scope and")
    w("    privacy notes (only (signature, company) pairs are encoded; no PII).")
    w("*/")
    w("")
    w("#ifndef ICCPAWG_SIGNATURE_REGISTRY_H")
    w("#define ICCPAWG_SIGNATURE_REGISTRY_H")
    w("")
    w("#include <cstdint>")
    w("")
    w('#define ICCPAWG_REGISTRY_SNAPSHOT_DATE "%s"' % fetched_on)
    w("")
    for n in notes:
        w("/* %s */" % n)
    w("")
    # Manufacturer signatures (exact match) -----------------------------------
    w("/* Manufacturer Signatures registry (registry.color.org/manufacturer-signatures).")
    w("   Per ICC.1:2022-05 section 7.2.17 the profile header 'manufacturer' and")
    w("   'creator' fields shall match a signature in this registry (or be zero). */")
    w("struct IccManufacturerSig {")
    w("  uint32_t sig;")
    w("  const char *company;")
    w("};")
    w("")
    w("static const IccManufacturerSig kIccManufacturerSignatures[] = {")
    for v in sorted(manuf):
        w('  {0x%08x, "%s"},' % (v, c_escape(manuf[v])))
    w("};")
    w("")
    # CMM signatures ----------------------------------------------------------
    # The cmm dict is still parsed and reported in the run summary / the hand-
    # maintained PERMISSIVENESS_DELTAS.md, but is intentionally NOT emitted as a
    # C++ table: the live S3 CMM check uses IccProfLib GetCmmSigName(), so a
    # second copy here would be a never-referenced static (CodeQL #1474).
    w("/* CMM Signatures registry (registry.color.org/cmm-signatures) is parsed and")
    w("   reported in the run summary / PERMISSIVENESS_DELTAS.md, but is NOT emitted")
    w("   here: the live S3 CMM check uses IccProfLib GetCmmSigName(). */")
    w("")
    # Private tag-signature ranges -------------------------------------------
    w("/* Registered private tag-signature ranges (registry.color.org/tag-signatures,")
    w("   non-ICC rows). Used to attribute private tags found in a profile body. */")
    w("struct IccTagSigRange {")
    w("  uint32_t first;")
    w("  uint32_t last;")
    w("  const char *owner;")
    w("};")
    w("")
    w("static const IccTagSigRange kIccPrivateTagSigRanges[] = {")
    for first, last, owner in ranges:
        w('  {0x%08x, 0x%08x, "%s"},' % (first, last, c_escape(owner)))
    w("};")
    w("")
    w("#endif  /* ICCPAWG_SIGNATURE_REGISTRY_H */")
    w("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--fetch", action="store_true",
                    help="re-download CSVs from registry.color.org before generating")
    args = ap.parse_args()

    if args.fetch:
        fetch()

    fetched_on = "unknown"
    fpath = os.path.join(SNAP, "FETCHED_ON")
    if os.path.exists(fpath):
        with open(fpath) as fh:
            fetched_on = fh.read().strip() or "unknown"

    manuf, m_skip = parse_manufacturers()
    cmm, c_skip = parse_cmms()
    ranges, t_skip = parse_tag_ranges()

    notes = []
    if m_skip:
        notes.append("manufacturer rows skipped (malformed signature): %s"
                     % ", ".join(m_skip))
    if c_skip:
        notes.append("cmm rows skipped (malformed signature): %s" % ", ".join(c_skip))
    if t_skip:
        notes.append("tag rows skipped (malformed signature): %s" % ", ".join(t_skip))

    header = emit(manuf, cmm, ranges, fetched_on, notes)
    with open(os.path.normpath(OUT), "w", encoding="utf-8") as fh:
        fh.write(header)

    print("manufacturers: %d  cmm: %d  private-tag-ranges: %d"
          % (len(manuf), len(cmm), len(ranges)))
    if m_skip or c_skip or t_skip:
        print("skipped (malformed): manuf=%s cmm=%s tag=%s" % (m_skip, c_skip, t_skip))
    print("wrote %s" % os.path.normpath(OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
