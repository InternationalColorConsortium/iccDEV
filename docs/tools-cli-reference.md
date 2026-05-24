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
| `iccProfileVisualize` | Dump profile LUT data as images and PDF graphs | `iccProfileVisualize profile.icc` |

## Applying Profiles

| Tool | Purpose | Example |
|------|---------|---------|
| `iccApplyNamedCmm` | Apply named CMM profile chains to text data | `iccApplyNamedCmm -cfg config.json` |
| `iccApplyProfiles` | Apply profile chains to TIFF images | `iccApplyProfiles -cfg config.json` |
| `iccApplySearch` | Apply a profile sequence using inverse search | `iccApplySearch -cfg config.json` |
| `iccApplyToLink` | Build DeviceLink profiles or `.cube` LUTs | `iccApplyToLink output.icc 0 33 1 "Link" 0.0 1.0 1 1 src.icc 1 dst.icc 1` |
| `iccRoundTrip` | Evaluate round-trip behavior | `iccRoundTrip profile.icc` |

## Image and Specialty Tools

| Tool | Purpose | Example |
|------|---------|---------|
| `iccTiffDump` | Inspect TIFF metadata and embedded ICC | `iccTiffDump image.tif` |
| `iccPngDump` | Inspect PNG metadata and embedded ICC | `iccPngDump image.png` |
| `iccJpegDump` | Inspect JPEG metadata and embedded ICC | `iccJpegDump image.jpg` |
| `iccPawgReport` | Emit an ICC PAWG profile assessment checklist report for security, conformance, and quality review | `iccPawgReport profile.icc` |
| `iccSpecSepToTiff` | Combine spectral separation TIFFs | `iccSpecSepToTiff output.tif 0 0 spectral/spec_ 1 10 1` |
| `iccV5DspObsToV4Dsp` | Convert v5 display/observer profiles to v4 display | `iccV5DspObsToV4Dsp display.icc observer.icc output.icc` |
| `iccFromCube` | Convert `.cube` 3D LUT to ICC.2 DeviceLink | `iccFromCube input.cube output.icc` |

`iccSpecSepToTiff` treats the input argument as a filename prefix and appends
each channel number from `start` through `end`. For a single input named
`spec_3`, pass prefix `spec_` with `start=3` and `end=3`.

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

Tool-specific details remain in each `Tools/CmdLine/*/Readme.md` file.
