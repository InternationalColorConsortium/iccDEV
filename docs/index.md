# iccDEV Documentation

iccDEV, formerly DemoIccMAX, provides C++ libraries and command-line tools for
working with ICC.1 and ICC.2/iccMAX color profiles.

## Start Here

- [Install](install.md): package and container quickstart
- [Build](build.md): build from source on Linux, macOS, and Windows
- [CTest tool suites](ctest.md): local CTest commands, registered suites, and add-test process
- [CLI tool reference](tools-cli-reference.md): command-line tool summary and shared options
- [IccJSON guide](iccjson.md): JSON conversion workflow
- [ICC JSON tag reference](iccjson-tag-types.md): detailed JSON tag examples
- [Bisecting regressions](bisect.md): focused debug workflow
- [Regression workflow governance](regression-workflow-governance.md): adding regression gates and tool-test workflow updates
- [CodeQL security analysis](codeql.md): custom query overview
- [Documentation maintenance](documentation-maintenance.md): canonical sources and review checklist
- [Agent skills](../.github/skills/README.md): repeatable repository workflows

## Libraries

| Library | Purpose |
|---------|---------|
| `IccProfLib` | Reference C++ library for reading, writing, applying, and validating ICC profiles. |
| `IccLibXML` | XML serialization layer for profiles and profile objects. |
| `IccLibJSON` | JSON serialization layer for profile inspection, editing, and round-tripping. |

## Tools

Run any command-line tool without arguments to print its usage. See
[CLI tool reference](tools-cli-reference.md) for signatures and common encoding
tables.

| Tool | Purpose |
|------|---------|
| `iccToXml` / `iccFromXml` | Convert profiles between binary ICC and XML. |
| `iccToJson` / `iccFromJson` | Convert profiles between binary ICC and JSON. |
| `iccApplyNamedCmm` | Apply named CMM profiles to text input data. |
| `iccApplyProfiles` | Apply a profile chain to a TIFF image. |
| `iccApplySearch` | Apply a profile sequence using inverse search. |
| `iccApplyToLink` | Create DeviceLink profiles or `.cube` LUTs from profile sequences. |
| `iccDumpProfile` | Dump and validate ICC profile structure. |
| `iccRoundTrip` | Evaluate round-trip profile behavior. |
| `iccSpecSepToTiff` | Combine spectral separation TIFFs. |
| `iccTiffDump`, `iccPngDump`, `iccJpegDump` | Inspect image metadata and embedded ICC profiles. |
| `iccV5DspObsToV4Dsp` | Convert v5 display/observer profiles to v4 display profiles. |
| `iccFromCube` | Convert `.cube` 3D LUTs to ICC.2 DeviceLink profiles. |
| `wxProfileDump` | wxWidgets GUI profile inspector. |

## Example iccMAX Profiles

XML files under `Testing/` can be converted into example ICC profiles with
`Testing/CreateAllProfiles.*`.

| Directory | Contents |
|-----------|----------|
| [`Calc`](../Testing/Calc) | Calculator MultiProcessElement profiles. |
| [`Display`](../Testing/Display) | Spectral display profiles with observer late binding. |
| [`Encoding`](../Testing/Encoding) | 3-channel encoding class profiles. |
| [`Named`](../Testing/Named) | Named color profiles with tints, spectral reflectance, and fluorescence. |
| [`PCC`](../Testing/PCC) | Profile Connection Condition examples. |
| [`SpecRef`](../Testing/SpecRef) | Spectral reflectance PCS examples. |

See [`Testing/Readme.md`](../Testing/Readme.md) for the full profile directory
overview.

## Examples

[`examples/hello-iccdev/`](../examples/hello-iccdev/) is a minimal standalone
example that links IccProfLib2 and IccXML2, prints library versions, and
round-trips an ICC profile header to XML. When IccJSON2 and nlohmann-json are
available, it also demonstrates JSON round-tripping.
