# iccDEV CLI Tool Reference

Run any tool without arguments to print its built-in usage. This page provides a
single index for common command shapes and shared option tables.

## Conversion and Inspection

| Tool | Purpose | Example |
|------|---------|---------|
| `iccToXml` | Convert ICC binary to XML | `iccToXml input.icc output.xml` |
| `iccFromXml` | Convert XML to ICC binary | `iccFromXml input.xml output.icc -v=SampleIccRELAX.rng` |
| `iccToJson` | Convert ICC binary to JSON | `iccToJson input.icc output.json -indent=2` |
| `iccFromJson` | Convert JSON to ICC binary | `iccFromJson input.json output.icc` |
| `iccDumpProfile` | Dump and validate ICC profile contents | `iccDumpProfile -v profile.icc ALL` |
| `iccDumpProfile` | Emit gated QA evidence flags | `iccDumpProfile --qa-flags --evidence-json --diag -v 100 profile.icc` |
| `iccProfilePlot` | Emit data-first profile graphs and CLUT rasters | `iccProfilePlot profile.icc list` |
| `iccProfileVisualize` | Dump profile LUT data as images and PDF graphs | `iccProfileVisualize profile.icc` |
| `iccProfileVisualizePlot` | Render profile visualizations to PDF and TIFF | `iccProfileVisualizePlot profile.icc` |

## Applying Profiles

| Tool | Purpose | Example |
|------|---------|---------|
| `iccApplyNamedCmm` | Apply named CMM profile chains to text data | `iccApplyNamedCmm -cfg config.json` |
| `iccApplyProfiles` | Apply profile chains to TIFF images | `iccApplyProfiles -cfg config.json` |
| `iccApplySearch` | Apply a profile sequence using inverse search | `iccApplySearch -cfg config.json` |
| `iccApplyToLink` | Build DeviceLink profiles or `.cube` LUTs | `iccApplyToLink output.icc 0 33 1 "Link" 0.0 1.0 1 1 src.icc 1 dst.icc 1` |
| `iccRoundTrip` | Evaluate round-trip behavior | `iccRoundTrip profile.icc` |

For `iccApplyToLink`, `link_type=0` writes an ICC DeviceLink and `option`
selects profile version (`0` for v4, `1` for v5). `link_type=1` writes a
`.cube` text LUT and `option` is the precision (`0` through `20`). Other
`link_type` values are rejected. `lut_size` must be `2` through `255`.
`first_transform=1` uses the source transform from the first profile; `0` uses
its destination transform.

## Image and Specialty Tools

| Tool | Purpose | Example |
|------|---------|---------|
| `iccTiffDump` | Inspect TIFF metadata and embedded ICC | `iccTiffDump image.tif` |
| `iccPngDump` | Inspect PNG metadata and embedded ICC | `iccPngDump image.png` |
| `iccJpegDump` | Inspect JPEG metadata and embedded ICC | `iccJpegDump image.jpg` |
| `iccPawgReport` | Emit an ICC PAWG profile assessment checklist report for security, conformance, and quality review | `iccPawgReport profile.icc` |
| `iccPawgReport` | Emit gated QA evidence flags | `iccPawgReport --qa-flags --evidence-json profile.icc` |
| `iccSpecSepToTiff` | Combine spectral separation TIFFs | `iccSpecSepToTiff output.tif 0 0 spectral/spec_ 1 10 1` |
| `iccV5DspObsToV4Dsp` | Convert v5 display/observer profiles to v4 display | `iccV5DspObsToV4Dsp display.icc observer.icc output.icc` |
| `iccFromCube` | Convert `.cube` 3D LUT to ICC.2 DeviceLink | `iccFromCube input.cube output.icc` |
| `iccBenchApply` | Measure profile-chain apply throughput and checksums | `iccBenchApply -suite -csv -threads 1,2,8` |

`iccSpecSepToTiff` treats the input argument as a filename prefix and appends
each channel number from `start` through `end`. For a single input named
`spec_3`, pass prefix `spec_` with `start=3` and `end=3`. The prefix is literal,
not a `printf` pattern: `spec_%03d.tif` opens `spec_%03d.tif1`, not `spec_001.tif`.

When `{profile}` is provided, the file is parsed and validated as an ICC profile
before any output is written. ICC.2 spectral PCS profiles must have spectral PCS
channels and spectral range steps equal to the generated TIFF `SamplesPerPixel`;
non-spectral profiles must have a data color-space sample count equal to
`SamplesPerPixel`. Non-ICC bytes, empty files, non-compliant profiles, and
sample-count mismatches are rejected.

Sweep the optional profile argument across a directory while holding the
checked-in separation inputs constant:

```bash
ICCDEV_TOOLS_DIR="$PWD/Build/Tools" \
  .github/scripts/iccdev-specsep-profile-sweep.sh \
  --profile-dir /path/to/profiles --channels 3
```

`--channels` sets how many inputs are combined, and therefore the sample count a
profile must match to be embedded; it defaults to 8. An incompatible profile is
an expected rejection only when the tool emits a profile diagnostic and leaves no
output TIFF. Timeouts, sanitizer findings, unexplained exits, and incomplete
successes fail the sweep.

## Text Data Encoding Values

These values are used by `iccApplyNamedCmm` and `iccApplySearch`.

| Value | Meaning |
|-------|---------|
| `0` | Lab/XYZ value |
| `1` | Percent |
| `2` | Unit float |
| `3` | Raw float |
| `4` | 8-bit |
| `5` | 16-bit |
| `6` | 16-bit ICCv2 style |

## Common Rendering Intents

| Value | Intent |
|-------|--------|
| `0` | Perceptual |
| `1` | Media-relative colorimetric |
| `2` | Saturation |
| `3` | ICC-absolute colorimetric |

Variant flags add to the base intent value as decimal-coded high digits.
Each tool's `Readme.md` documents which flags it surfaces; the full set
parsed by the shared `CIccCfgProfile::fromArgs` is:

| Add | Effect |
|----:|--------|
| `+10`-`+13` | Base intent without D2Bx/B2Dx tags |
| `+1000` | Use luminance-based PCS adjustment |
| `+10000` | Use V5 sub-profile if present |
| `+100000` | Use HToS tag if present |
| `+1000000` | NamedColor over-black (`icSigNmclSpectralOverBlackMbr`, `'spcb'`) |
| `+2000000` | NamedColor over-gray (`icSigNmclSpectralOverGrayMbr`, `'spcg'`) |

The over-black / over-gray flags only affect chains that include a v5
NamedColor profile. JSON callers prefer the `transform` field values,
which combine an output-side stem (`named` / `namedColorimetric` /
`namedSpectral` / `namedDevice`) with an overprint suffix
(`OnBlack` / `OnGray`, only meaningful on the spectral path) - see
[`docs/icc-connect-config.schema.json`](icc-connect-config.schema.json)
and [`Tools/CmdLine/IccApplyNamedCmm/Readme.md`](../Tools/CmdLine/IccApplyNamedCmm/Readme.md).

Tool-specific details remain in each `Tools/CmdLine/*/Readme.md` file.

## Maintainer QA evidence flags

`iccDumpProfile` and `iccPawgReport` expose `--qa-flags --evidence-json` only
when the build enables `-DICCDEV_ENABLE_QA_FLAGS=ON`. `iccApplyNamedCmm` and
`iccTiffDump` expose `--evidence-json` for their transform and embedded-profile
write/extract evidence. The shared output schema is `iccdev-qa-evidence/v1` and
is intended for CI and manual QA evidence, not normal user output. Sanitizer-log
evidence remains a post-exit classifier in the `iccdev.qa-target-flags` CTest
harness because a crashing process cannot reliably emit its own final JSON.

```sh
iccDumpProfile --qa-flags --evidence-json --diag -v 100 profile.icc
iccPawgReport --qa-flags --evidence-json profile.icc
iccApplyNamedCmm --evidence-json -cfg apply-config-with-dstFile.json
iccTiffDump --evidence-json image.tif extracted.icc
```

The CTest `iccdev.qa-target-flags` demonstrates true-positive, negative-control,
malformed marker-only, PAWG validation, transform, embedded-profile write/extract,
and fixture-only sanitizer-log evidence. Its evidence covers:

| Flag | Required evidence |
|------|-------------------|
| `ICCDEV_FLAG_LOAD` | Load mode, profile size, and profile ID. |
| `ICCDEV_FLAG_VALIDATE` | Validation status plus tag signature, offset, and size. |
| `ICCDEV_FLAG_TAG_PAYLOAD` | Private `qaFL` marker plus generated nonce or controlled `0x41414141` bytes. |
| `ICCDEV_FLAG_TRANSFORM` | Input digest, profile ID, and output digest. |
| `ICCDEV_FLAG_WRITE` | Output digest plus embedded/extracted ICC profile ID. |
| `ICCDEV_FLAG_SANITIZER` | Exit classification, sanitizer summary, and stack frame file path. |

The CI tool-test workflow writes a sanitized Markdown table plus sanitized
NDJSON evidence lines to the GitHub Actions job summary.

## Headless maintainer QA scans

Maintainers can run broad command-line QA sweeps without adding a permanent CTest
row for every external profile:

```sh
.github/scripts/icc-pawg-qa-scan.sh --timeout 10 Testing
.github/scripts/icc-dumpprofile-qa-scan.sh --variant validate-all Testing
.github/scripts/icc-roundtrip-qa-scan.sh --variant intent-1 Testing
```

For ICC registry compatibility checks, use:

```sh
.github/scripts/iccdev-registry-profile-qa.sh --timeout 20
```

The scanners write per-run logs, `results.tsv`, `findings.txt`, and
`summary.md`. See [`docs/maintainer-qa-scans.md`](maintainer-qa-scans.md) for
failure policy, registry-profile source management, and CI reporting guidance.
