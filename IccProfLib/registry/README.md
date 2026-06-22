# Colorimetry weighting-table generator

`generate_colorimetry_weights.py` is the reproducible source-of-truth generator for
the ICC colorimetry-data registry's 10 nm spectral weighting tables. It fetches the
registry CSVs and **injects them as `static const` C++ arrays directly into**
[`../IccColorimetry.cpp`](../IccColorimetry.cpp), between the
`>>>BEGIN GENERATED WEIGHTING TABLES` / `<<<END GENERATED WEIGHTING TABLES` sentinels.
The library then hands those arrays back through `icGetColorimetryWeightingTable()` —
there is **no runtime CSV or registry ingestion** anywhere in the live library.

It mirrors the sibling generator at
`Tools/CmdLine/IccPawgReport/registry/generate_signature_registry.py` (issue #1459).

## Why baked-in (and not loaded at runtime)

The registry weighting tables are effectively static reference data, so they belong
compiled into the library as program data rather than ingested through a setter. A
maintainer reruns this script when the registry is revised; the generated arrays *and*
the CSV snapshot are committed, so an ordinary build needs neither Python nor network.

The 10 nm numbers are **provisional** pending CIE TC1-101 (the ICC may republish with
Li's revised method). Rerunning this generator on a registry update is exactly the
intended maintenance path — which is why the data is generated, not hand-edited.

## Usage

```sh
# refresh the CSV snapshot from the live registry, then regenerate:
python3 generate_colorimetry_weights.py --fetch

# regenerate from the committed snapshot under data/ (offline, deterministic):
python3 generate_colorimetry_weights.py
```

Rerunning with the same CSVs is byte-for-byte deterministic: no timestamp enters the
arrays; the snapshot date lives in `data/FETCHED_ON` and the generated banner comment
only. (`--fetch` sends a browser `User-Agent`; the registry returns HTTP 403 to the
default `Python-urllib` agent.)

After regenerating, rebuild and run the regression test that anchors the data against
canonical CIE white points:

```sh
ctest --test-dir <build> -R iccdev.colorimetry-methods --output-on-failure
```

## The data

|              |                                                                    |
| ------------ | ------------------------------------------------------------------ |
| Source       | <https://registry.color.org/colorimetry-data/>                     |
| Tables       | 10 = {CIE 1931 2°, CIE 1964 10°} × {D50, D65, A, LED-B1, F11}       |
| Grid         | 380–780 nm @ 10 nm = 41 samples                                    |
| Layout       | consecutive Wx, Wy, Wz blocks (3 × 41 = 123 floats per table)       |
| Scale        | **CIE Y = 100** (a perfect diffuser sums to the illuminant XYZ, Y = 100) |
| Method       | ISO 13655 / Li et al. 2016 least-squares "LWL" weights             |

`data/` holds the exact registry CSVs. Unlike the signature generator's gitignored
`_raw/` (which carries registrant PII), these are pure numbers, so they are committed
for provenance and offline regeneration. `icGetColorimetryWeightingTable()` returns
the arrays unchanged; apply one to a 10 nm reflectance vector with
`icApplyWeightingTable()`. The `icColorimetryWeightingIlluminant` enum names the five
illuminants (LED-B1 has no ICC wire-enum `icIlluminant` assignment).

## Provenance & licensing

The 10 nm weighting tables are ICC self-calculated data published on the ICC registry
(computed for ICC by Tanzima Habib, NTNU). CIE has stated no objection to ICC
publishing them (P. Green, ICC Technical Secretary, 2026-06). See issue #1475 for the
full method/provenance discussion.
