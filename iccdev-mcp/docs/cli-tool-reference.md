# CLI Tool Reference

MCP tool to iccDEV CLI binary mapping. Each MCP tool wraps a CLI binary
via `subprocess.run()` with path validation, timeout enforcement, and
structured output.

## Environment Setup

```bash
# Required: point to compiled iccDEV CLI tools
export ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools

# Required: shared libraries
export LD_LIBRARY_PATH=/path/to/iccDEV/Build/IccProfLib:/path/to/iccDEV/Build/IccXML:/path/to/iccDEV/Build/IccJSON:/path/to/iccDEV/Build/IccConnect

# Optional: additional profile search directories
export ICCDEV_PROFILE_DIRS=/path/to/profiles:/another/path
export ICCDEV_TESTING_DIR=/path/to/iccDEV/Testing
```

---

## Tool Mapping

### Profile Inspection

#### dump_profile

| Property | Value |
|----------|-------|
| **MCP Tool** | `dump_profile` |
| **CLI Binary** | `iccDumpProfile` |
| **Syntax** | `iccDumpProfile {-v} {verbosity} profile {tagId\|"ALL"}` |
| **MCP Args** | `path` (required), `validate`, `verbosity`, `tag` |
| **Exit Codes** | `0` success, `255` tool error |

Dumps profile header, tag table, and tag contents as text. Supports all
profile classes (Display, Output, Input, NamedColor, ColorSpace, DeviceLink,
Abstract).

```bash
# CLI
iccDumpProfile -v test.icc ALL

# MCP equivalent
dump_profile(path="test.icc", validate=True, verbosity=100, tag="ALL")
```

---

#### pawg_report

| Property | Value |
|----------|-------|
| **MCP Tool** | `pawg_report` |
| **CLI Binary** | `iccPawgReport` |
| **Syntax** | `iccPawgReport profile.icc` |
| **MCP Args** | `path` (required) |
| **Exit Codes** | `0` success, non-zero on tool or profile error |

Generates the ICC PAWG security, conformance, and quality checklist report for
one profile. The report includes summary counts and an overall result.

```bash
# CLI
iccPawgReport sRGB.icc

# MCP equivalent
pawg_report(path="sRGB.icc")
```

---

### Format Conversion

#### profile_to_xml

| Property | Value |
|----------|-------|
| **MCP Tool** | `profile_to_xml` |
| **CLI Binary** | `iccToXml` |
| **Syntax** | `iccToXml src.icc dest.xml` |
| **MCP Args** | `input_path` (required), `output_path` (optional, auto-generated) |
| **Exit Codes** | `0` success, non-zero on error |

Converts binary ICC profile to human-readable XML. Output follows the
ICC XML schema. Use ASAN-instrumented builds for untrusted profiles.

```bash
# CLI
iccToXml sRGB.icc sRGB.xml

# MCP equivalent
profile_to_xml(input_path="sRGB.icc")
```

#### xml_to_profile

| Property | Value |
|----------|-------|
| **MCP Tool** | `xml_to_profile` |
| **CLI Binary** | `iccFromXml` |
| **Syntax** | `iccFromXml xml_file output.icc {-noid -v{=[schema.rng]}}` |
| **MCP Args** | `input_path` (required), `output_path` (optional) |
| **Exit Codes** | `0` success, non-zero on parse/creation error |

Reconstructs binary ICC profile from XML. Supports optional `-noid` (skip
MD5 Profile ID calculation) and `-v` (RelaxNG schema validation).

```bash
# CLI
iccFromXml profile.xml profile.icc

# Round-trip test
iccToXml test.icc /tmp/test.xml
iccFromXml /tmp/test.xml /tmp/test_rt.icc
diff <(xxd test.icc) <(xxd /tmp/test_rt.icc)
```

#### profile_to_json / json_to_profile

| Property | Value |
|----------|-------|
| **MCP Tool** | `profile_to_json` / `json_to_profile` |
| **CLI Binary** | `iccToJson` / `iccFromJson` |
| **MCP Args** | `input_path` (required), `output_path` (optional) |
| **Exit Codes** | `0` success, non-zero on error |

JSON serialization of ICC profiles. Same semantics as XML conversion.

#### from_cube

| Property | Value |
|----------|-------|
| **MCP Tool** | `from_cube` |
| **CLI Binary** | `iccFromCube` |
| **Syntax** | `iccFromCube input.cube output.icc` |
| **MCP Args** | `input_path` (required), `output_path` (optional) |
| **Exit Codes** | `0` success, non-zero on parse error |

Converts `.cube` 3D LUT files to ICC DeviceLink profiles. The `.cube`
format uses text-based `LUT_3D_SIZE N`, `DOMAIN_MIN`/`DOMAIN_MAX`, and
RGB triplet data lines.

```bash
# CLI
iccFromCube identity.cube identity-link.icc
```

---

### Image ICC Extraction

#### tiff_dump

| Property | Value |
|----------|-------|
| **MCP Tool** | `tiff_dump` |
| **CLI Binary** | `iccTiffDump` |
| **Syntax** | `iccTiffDump tiff_file {exported.icc}` |
| **MCP Args** | `path` (required), `intent`, `use_mpe` |
| **Exit Codes** | `0` success, non-zero on TIFF/metadata error |

Reports TIFF image metadata (dimensions, bits/sample, samples/pixel,
photometric, compression) and extracts embedded ICC profiles.

```bash
# CLI -- extract embedded ICC
iccTiffDump image.tiff extracted.icc
```

#### jpeg_dump

| Property | Value |
|----------|-------|
| **MCP Tool** | `jpeg_dump` |
| **CLI Binary** | `iccJpegDump` |
| **Syntax** | `iccJpegDump input.jpg [output.icc]` |
| **MCP Args** | `path` (required), `intent`, `use_mpe` |
| **Exit Codes** | `0` success, `1` no ICC found (NOT a crash) |

Extracts ICC profiles from JPEG APP2 markers. Supports multi-segment
reassembly for profiles larger than 64KB. Also supports ICC injection
via `--write-icc` and `--output` flags.

```bash
# CLI -- extract
iccJpegDump photo.jpg photo-profile.icc

# CLI -- inject
iccJpegDump input.jpg --write-icc sRGB.icc --output output.jpg
```

#### png_dump

| Property | Value |
|----------|-------|
| **MCP Tool** | `png_dump` |
| **CLI Binary** | `iccPngDump` |
| **Syntax** | `iccPngDump input.png [output.icc]` |
| **MCP Args** | `path` (required) |
| **Exit Codes** | `0` success, `1` no ICC found (NOT a crash) |

Extracts ICC profiles from PNG iCCP chunks (zlib-compressed). Also
supports ICC injection via `--write-icc` and `--output` flags.

---

### Color Transforms

#### round_trip_test

| Property | Value |
|----------|-------|
| **MCP Tool** | `round_trip_test` |
| **CLI Binary** | `iccRoundTrip` |
| **Syntax** | `iccRoundTrip profile {intent=1 {use_mpe=0}}` |
| **MCP Args** | `path` (required) |
| **Exit Codes** | `0` success, non-zero if missing AToB/BToA tables |

Tests bidirectional transform fidelity. Requires profiles with both
AToB and BToA tag pairs. Rendering intents: 0=Perceptual, 1=Relative
Colorimetric (default), 2=Saturation, 3=Absolute Colorimetric.

```bash
# CLI
iccRoundTrip sRGB.icc 1
iccRoundTrip sRGB.icc 1 1   # use MPE instead of LUT
```

#### apply_profiles

| Property | Value |
|----------|-------|
| **MCP Tool** | `apply_profiles` |
| **CLI Binary** | `iccApplyProfiles` |
| **Syntax** | `iccApplyProfiles src.tiff dst.tiff encoding compress planar embed interp {{-ENV:sig val} profile intent {-PCC pcc}}` |
| **MCP Args** | `config_args` (list), or structured `input_tiff`, `profiles`, `intents`, `encoding`, `compress`, `planar`, `embed`, `interpolation` |
| **Exit Codes** | Non-zero on error |

Applies multi-profile color transforms to TIFF images. Parameters:

| Arg | Values |
|-----|--------|
| `encoding` | 0=Same, 1=8-bit, 2=16-bit, 4=Float |
| `compress` | 0=None, 1=LZW |
| `planar` | 0=Contiguous, 1=Separate |
| `embed` | 0=No, 1=Embed output ICC |
| `interp` | 0=Linear, 1=Tetrahedral |

```bash
# CLI
iccApplyProfiles in.tiff out.tiff 1 0 0 1 0 sRGB.icc 1

# JSON config mode
iccApplyProfiles -cfg transform.json
```

#### apply_named_cmm

| Property | Value |
|----------|-------|
| **MCP Tool** | `apply_named_cmm` |
| **CLI Binary** | `iccApplyNamedCmm` |
| **MCP Args** | `config_args` (list) or `-cfg config.json` |
| **Exit Codes** | `0` success, `255` transform error |

Named color CMM with data file input. Encoding values:
0=Value (Lab), 1=Percent, 2=UnitFloat, 3=Float, 4=8Bit, 5=16Bit, 6=16BitV2.

Data file format: 4-char color space signature, encoding enum, blank line,
then space-separated color values per line.

#### apply_search

| Property | Value |
|----------|-------|
| **MCP Tool** | `apply_search` |
| **CLI Binary** | `iccApplySearch` |
| **MCP Args** | `config_args` (list) or `-cfg config.json` |
| **Exit Codes** | `0` success, `255` incompatible profiles |

Search-based color transform with PCC (Profile Connection Conditions).
Requires `-INIT init_intent` argument. JSON mode uses `searchApply` object
with `profileSequence`, `initial.lab`, and `pccWeights`.

#### create_link

| Property | Value |
|----------|-------|
| **MCP Tool** | `create_link` |
| **CLI Binary** | `iccApplyToLink` |
| **MCP Args** | `config_args` (list) |
| **Exit Codes** | `0` success, non-zero on incompatible chain |

Creates DeviceLink profiles or `.cube` LUT files from profile chains.

| Arg | Values |
|-----|--------|
| `link_type` | 0=DeviceLink ICC, 1=.cube text |
| `lut_size` | Grid dimension (9, 17, 33, 65) |
| `option` | 0=v4 16-bit, 1=v5 (for ICC); precision digits (for .cube) |

```bash
# DeviceLink ICC
iccApplyToLink link.icc 0 17 0 "sRGB to AdobeRGB" 0.0 1.0 0 0 sRGB.icc 1

# .cube export
iccApplyToLink out.cube 1 17 6 "Title" 0.0 1.0 0 0 sRGB.icc 1
```

---

### Conversion Utilities

#### v5_to_v4

| Property | Value |
|----------|-------|
| **MCP Tool** | `v5_to_v4` |
| **CLI Binary** | `iccV5DspObsToV4Dsp` |
| **Syntax** | `iccV5DspObsToV4Dsp display.v5 observer.v5 output.v4` |
| **MCP Args** | `display_path`, `observer_path`, `output_path` |
| **Exit Codes** | `0` success, `254` bad transform matrix |

Converts ICC v5 Display profiles (with spectral data) to v4 using a
v5 ColorSpace observer profile. Input display must be `mntr` class,
observer must be `spac` class.

#### spec_sep_to_tiff

| Property | Value |
|----------|-------|
| **MCP Tool** | `spec_sep_to_tiff` |
| **CLI Binary** | `iccSpecSepToTiff` |
| **Syntax** | `iccSpecSepToTiff output compress sep fmt start end incr {profile}` |
| **MCP Args** | `config_args` (list) |
| **Exit Codes** | `0` success, `255` file/format error |

Merges single-channel spectral TIFF separations into a multi-channel
spectral TIFF. Input files must be MINISBLACK, 1 sample/pixel, 8-16 bit,
identical dimensions.

```bash
# Merge 81 channels (380-780nm, 5nm steps)
iccSpecSepToTiff out.tiff 0 0 "spec_%06d.tiff" 380 780 5
```

---

## Python-Native Tools (No CLI Required)

These 6 tools use the `iccdev` Python bindings directly and work without
iccDEV CLI binaries installed.

### inspect_header

Parses the 128-byte ICC profile header and returns 22 raw fields plus 6
computed human-readable names (device class, color space, PCS, platform,
rendering intent, version).

### profile_summary

Returns compact profile metadata for classification and routing: filename,
file size, profile size, ICC version, v5 flag, device class, color space,
PCS, rendering intent, platform, and Profile ID.

### color_transform

Applies multi-profile color transforms via the iccDEV CMM. Accepts pixel
data as a list of float values, converts through a profile chain with
specified rendering intent and interpolation method.

### roundtrip_delta

Measures round-trip color fidelity by transforming sample data through
AToB then BToA paths and computing delta-E between input and output.

### icc_sig_to_str

Converts a 4-byte ICC signature (uint32 or 4-char string) to its
human-readable name using the ICC specification enumeration tables.

### enum_spaces

Lists all 32 ICC color space identifiers with channel counts and
descriptions. Useful for understanding `colorSpace` and `pcs` header
fields.

---

## Exit Code Reference

| Code | Meaning | Action |
|------|---------|--------|
| `0` | Success | Output is valid |
| `1` | No ICC found (jpeg/png only) | Graceful, NOT a crash |
| `254` | Bad transform matrix (v5_to_v4) | Incompatible display/observer pairing |
| `255` | Tool error | Check stderr for details |
| Non-zero | General error | Varies by tool |

**Important**: Exit codes 1-127 indicate soft failures, NOT crashes.
Only exit codes 128+ indicate signal termination (crash). The tool exit
code is authoritative; fuzzer-reported `DEADLYSIGNAL` is a test artifact.

---

## JSON Configuration Mode

Three tools support `-cfg config.json` for complex multi-profile operations:

- `iccApplyNamedCmm -cfg config.json`
- `iccApplyProfiles -cfg config.json`
- `iccApplySearch -cfg config.json`

JSON config objects include `profileSequence`, `dataFiles`/`imageFiles`,
`colorData`, and optional `pccWeights`/`searchApply` sections. See the
[iccDEV Testing/ directory](https://github.com/InternationalColorConsortium/iccDEV/tree/master/Testing)
for example JSON configurations.
