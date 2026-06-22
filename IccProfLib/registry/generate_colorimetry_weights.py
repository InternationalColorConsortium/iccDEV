#!/usr/bin/env python3
"""Generate the baked-in colorimetry weighting tables in IccColorimetry.cpp.

This is the reproducible source-of-truth generator for the ICC colorimetry-data
registry's 10 nm weighting tables (registry.color.org/colorimetry-data). It
fetches the registry CSVs and injects them as static const C++ arrays into
IccProfLib/IccColorimetry.cpp, between the GENERATED WEIGHTING TABLES sentinels.
icGetColorimetryWeightingTable() then hands those arrays back to callers.

Why baked-in and not loaded at runtime: the registry values are effectively
static reference data (ISO 13655 / Li et al. 2016 "LWL" least-squares weights,
computed for ICC by T. Habib, NTNU). Baking them in keeps the live library free
of any CSV/network dependency; a maintainer reruns this script when the registry
is revised. The 10 nm numbers are provisional pending CIE TC1-101, so re-running
on a registry update is the expected maintenance path (see issue #1475).

Licensing/provenance: the 10 nm weighting tables are ICC self-calculated data
published on the ICC registry; CIE has stated no objection to ICC publishing them
(P. Green, ICC Technical Secretary, 2026-06). The raw CSV snapshot is committed
under data/ for provenance and offline, deterministic regeneration.

Usage:
    # refresh the CSV snapshot from the live registry, then regenerate:
    python3 generate_colorimetry_weights.py --fetch
    # regenerate from the committed snapshot under data/ (offline, deterministic):
    python3 generate_colorimetry_weights.py

Rerunning with the same CSVs is deterministic (no timestamp enters the arrays;
the snapshot date lives in data/FETCHED_ON and only the banner comment).
"""

import argparse
import csv
import datetime
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "data")
TARGET = os.path.normpath(os.path.join(HERE, "..", "IccColorimetry.cpp"))
BASE_URL = "https://registry.color.org/colorimetry-data/data"

BEGIN = "// >>>BEGIN GENERATED WEIGHTING TABLES (do not edit by hand)"
END = "// <<<END GENERATED WEIGHTING TABLES"

# Registry grid: 380-780 nm @ 10 nm = 41 samples.
EXPECT_START, EXPECT_STEP, EXPECT_N = 380, 10, 41

# (filename token, IccProfLib enum, C identifier tag) -- the registry's two observers.
OBSERVERS = [
    ("1931", "icStdObs1931TwoDegrees", "Obs1931"),
    ("1964", "icStdObs1964TenDegrees", "Obs1964"),
]
# (filename token, IccColorimetry enum, C identifier tag) -- the registry's five
# illuminants. LED-B1 has no ICC wire-enum, hence the IccColorimetry-local enum.
ILLUMINANTS = [
    ("D50",    "icWtIllumD50",    "D50"),
    ("D65",    "icWtIllumD65",    "D65"),
    ("A",      "icWtIllumA",      "A"),
    ("LED-B1", "icWtIllumLED_B1", "LED_B1"),
    ("F11",    "icWtIllumF11",    "F11"),
]


def filename(obs_tok, illum_tok):
    return "Wts-%s-%s-380-10-780nm.csv" % (illum_tok, obs_tok)


def fetch():
    import urllib.request
    os.makedirs(DATA, exist_ok=True)
    for obs_tok, _, _ in OBSERVERS:
        for illum_tok, _, _ in ILLUMINANTS:
            fn = filename(obs_tok, illum_tok)
            # registry.color.org returns 403 to the default Python-urllib agent;
            # send a conventional User-Agent (as curl/a browser would).
            req = urllib.request.Request(
                "%s/%s" % (BASE_URL, fn),
                headers={"User-Agent": "Mozilla/5.0 (iccDEV colorimetry-weights generator)"})
            with urllib.request.urlopen(req, timeout=60) as resp:
                data = resp.read()
            with open(os.path.join(DATA, fn), "wb") as fh:
                fh.write(data)
            print("fetched %-32s %6d bytes" % (fn, len(data)))
    with open(os.path.join(DATA, "FETCHED_ON"), "w") as fh:
        fh.write(datetime.date.today().isoformat() + "\n")


def read_table(obs_tok, illum_tok):
    """Return (wx, wy, wz) as 41 literal strings each; validate the grid layout."""
    path = os.path.join(DATA, filename(obs_tok, illum_tok))
    rows = [r for r in csv.reader(open(path, encoding="utf-8-sig"))
            if r and r[0].strip()]
    if len(rows) != EXPECT_N:
        raise SystemExit("%s: expected %d rows, got %d" % (path, EXPECT_N, len(rows)))
    wx, wy, wz = [], [], []
    for i, r in enumerate(rows):
        if len(r) < 4:
            raise SystemExit("%s row %d: expected 4 columns, got %d" % (path, i, len(r)))
        wl = int(round(float(r[0])))
        if wl != EXPECT_START + i * EXPECT_STEP:
            raise SystemExit("%s row %d: wavelength %d off the 380/10/780 grid" % (path, i, wl))
        wx.append(r[1].strip())
        wy.append(r[2].strip())
        wz.append(r[3].strip())
    return wx, wy, wz


def lit(s):
    """A registry numeric string -> C float literal (faithful: keep the digits)."""
    float(s)            # validate it parses
    return s + "f"


def emit_array(name, wx, wy, wz):
    out = ["static const icFloatNumber %s[%d] = {" % (name, 3 * EXPECT_N)]
    for label, col in (("Wx", wx), ("Wy", wy), ("Wz", wz)):
        out.append("  // %s (%d)" % (label, EXPECT_N))
        for i in range(0, EXPECT_N, 6):
            out.append("  " + " ".join(lit(v) + "," for v in col[i:i + 6]))
    out.append("};")
    return "\n".join(out)


def generate(fetched_on):
    out = []
    out.append("// Snapshot: registry.color.org/colorimetry-data, fetched %s." % fetched_on)
    out.append("// CIE Y=100 scale (a perfect diffuser sums to the illuminant XYZ with Y=100);")
    out.append("// 380-780 nm @ 10 nm = 41 samples; consecutive Wx,Wy,Wz blocks (3*41 = 123).")
    out.append("")
    entries = []
    for obs_tok, obs_enum, obs_tag in OBSERVERS:
        for illum_tok, illum_enum, illum_tag in ILLUMINANTS:
            name = "kWts%s%s" % (obs_tag, illum_tag)
            wx, wy, wz = read_table(obs_tok, illum_tok)
            out.append(emit_array(name, wx, wy, wz))
            out.append("")
            entries.append((obs_enum, illum_enum, name))
    out.append("struct IccColorimetryWtEntry {")
    out.append("  icStandardObserver               obs;")
    out.append("  icColorimetryWeightingIlluminant illum;")
    out.append("  const icFloatNumber             *pWeights;   // 3*41 Wx,Wy,Wz blocks")
    out.append("};")
    out.append("")
    out.append("static const IccColorimetryWtEntry kColorimetryWtTables[] = {")
    iw = max(len(e[1]) for e in entries) + 1
    for obs_enum, illum_enum, name in entries:
        out.append("  { %-22s %-*s %s }," % (obs_enum + ",", iw, illum_enum + ",", name))
    out.append("};")
    out.append("static const int            kColorimetryWtTableCount = %d;" % len(entries))
    out.append("static const icUInt16Number kColorimetryWtSteps      = %d;" % EXPECT_N)
    return "\n".join(out)


def inject(generated):
    src = open(TARGET, encoding="utf-8").read()
    b = src.find(BEGIN)
    e = src.find(END)
    if b < 0 or e < 0 or e < b:
        raise SystemExit("GENERATED WEIGHTING TABLES sentinels not found in %s" % TARGET)
    new = src[:b + len(BEGIN)] + "\n" + generated + "\n" + src[e:]
    with open(TARGET, "w", encoding="utf-8") as fh:
        fh.write(new)


def main():
    ap = argparse.ArgumentParser(description="Generate baked-in colorimetry weighting tables.")
    ap.add_argument("--fetch", action="store_true",
                    help="re-download the CSV snapshot from the registry before generating")
    args = ap.parse_args()

    if args.fetch:
        fetch()

    fpath = os.path.join(DATA, "FETCHED_ON")
    fetched_on = "unknown"
    if os.path.exists(fpath):
        fetched_on = open(fpath).read().strip() or "unknown"

    generated = generate(fetched_on)
    inject(generated)
    print("injected %d tables (snapshot %s) into %s"
          % (len(OBSERVERS) * len(ILLUMINANTS), fetched_on, TARGET))
    return 0


if __name__ == "__main__":
    sys.exit(main())
