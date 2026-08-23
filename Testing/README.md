# Testing Script and Batch File Guide

This directory contains portable, manually runnable ICC profile-generation and
application lanes. Run a script from the directory that contains it unless its
command line below says otherwise. The normal automated entry point is CTest;
these files are also useful for focused developer and quality-assurance work.

Shell scripts use the tools on `PATH`. `CreateAllProfiles.sh` and
`RunTests.sh` source `Testing/path.sh` when it is present. Their matching batch
files call `path.bat` when it is present. The CTest Windows wrapper supplies
the build-tree tool and runtime paths itself. The hybrid batch lane also accepts
tools one directory above its working directory through `TOOLDIR`.

## Command-Line Conventions

The application tools accept a data or TIFF input, encoding and interpolation
controls, then a profile sequence. A profile is always followed by its integer
rendering intent. `-pcc` introduces the connection-conditions profile that
precedes the following destination profile. `-ENV:name value` inserts a named
environment value into the current transform. The active scripts use:

| Value | Meaning in these lanes |
| --- | --- |
| `0` | encoded-value data representation or linear interpolation, depending on position |
| `1` | percent data representation, tetrahedral interpolation, or relative intent |
| `3` | float data representation or absolute intent |
| `80` | material-color-space connection intent |
| `10000 + intent` | select the v5 sub-profile before applying the base intent |
| `-embedded N` | read the embedded TIFF profile with intent `N` |

`iccApplyNamedCmm` takes `data_file encoding interpolation` before the profile
sequence. `iccApplyProfiles` takes `input.tif output.tif
output_encoding compression planar embed interpolation` before the sequence.
`iccApplySearch` uses the same data and profile conventions, followed by
`-INIT` and its initial profile list. `iccTiffDump` takes one TIFF path, and
`iccV5DspObsToV4Dsp` takes exactly `display-v5.icc observer-v5.icc output-v4.icc`.

## Primary Generation and Application Lanes

### `CreateAllProfiles.sh` and `CreateAllProfiles.bat`

Run from `Testing/`:

```sh
./CreateAllProfiles.sh
./CreateAllProfiles.sh clean
```

```cmd
CreateAllProfiles.bat
CreateAllProfiles.bat clean
```

The default action converts the XML fixtures in Calc, Display, Encoding, ICS,
Named, Overprint, MCS, PCC, SpecRef, and V2 into generated ICC profiles. The
`clean` argument removes only generated profiles in directories that contain
only generated outputs; it deliberately preserves the checked-in invalid
Calculator fixtures in `CalcTest`. The V2 fixtures exercise `mft1`, `mft2`,
matrix/TRC, gray-TRC, and the legacy Lab/XYZ PCS paths. No other positional
argument is supported by the shell form.

### `RunTests.sh` and `RunTests.bat`

Run after profile generation from `Testing/`:

```sh
./RunTests.sh
```

```cmd
RunTests.bat
```

This is the broad `iccApplyNamedCmm` suite. It covers Calculator MPEs,
NamedColor, display links with ambient luminance, fluorescent colors,
six-channel reflectance, and two spectral-reflectance-to-XYZ applications. The
NamedColor baseline explicitly applies the D50 Lab PCC before the v4 sRGB
destination; this now matches on Unix and Windows. The spectral variants select
named D50, D65, D93, Illuminant A, and observer profiles using `-pcc`.

The final section performs an optional JSON round trip and invokes
`iccProfileVisualize`. Missing JSON tools are a skip unless
`ICCDEV_REQUIRE_JSON_ROUNDTRIP=1` in the shell lane. A failed command produces
a failing script status: the shell stops at the failed command, while the batch
lane records failures and completes the remaining independent cases.

### `RunJsonTests.sh`

Run from any directory with an optional build or tools directory:

```sh
./Testing/RunJsonTests.sh
./Testing/RunJsonTests.sh build-qa
./Testing/RunJsonTests.sh build-qa/Tools
```

This Bash-only exhaustive JSON parity lane finds every `Testing/**/*.icc`,
writes temporary JSON and ICC round-trip files, and compares their byte sizes.
Known malformed Calculator under-stack fixtures are reported as expected
failures. A profile rejected by either JSON tool is a skip; size differences
outside `CalcTest` fail the script.

## Specialized Profile Lanes

| Scripts | Working directory | Purpose |
| --- | --- | --- |
| `CalcTest/checkInvalidProfiles.sh`, `CalcTest/checkInvalidProfiles.bat` | `Testing/CalcTest` | `iccDumpProfile -v` must reject each Calculator stack-overflow and stack-underflow fixture. Both preflight the tool so a missing executable cannot be mistaken for correct rejection. |
| `CalcTest/runtests.sh`, `CalcTest/runtests.bat` | `Testing/CalcTest` | Run the invalid-profile check, build `calcCheckInit.icc` and `calcExercizeOps.icc`, then exercise the two valid Calculator MPE execution cases with `-debugcalc`. |
| `Display/RunProtoTests.sh`, `Display/RunProtoTests.bat` | `Testing/Display` | Applies Rec. 2020 colorimetric and spectral display profiles to the RGB data. The four `-pcc` calls select D65 age-dependent observers (20, 40, 60, and 80 years). |
| `HDR/mkprofiles.sh`, `HDR/mkprofiles.bat` | `Testing/HDR` | Builds HLG and PQ BT.2100 scene and display profiles plus their scene-to-display links. |
| `Overprint/RunTests.sh`, `Overprint/RunTests.bat` | `Testing/Overprint` | Applies the 17-channel overprint profile to its 17-channel data. |
| `mcs/updateprev.sh`, `mcs/updateprev.bat` | `Testing/mcs` | Builds the six-channel selector and 18-channel spot MCS profiles, then renders `CMYKSS-Numbered-Overprint.tif` to `prev.tif`. |
| `mcs/updateprevWithBkgd.sh`, `mcs/updateprevWithBkgd.bat` | `Testing/mcs` | Same MCS preview with D50-relative background XYZ values (`0.4014`, `0.2391`, `0.0272`) injected before the 18-channel profile. |

## Hybrid TIFF, Spectral, and V5-to-V4 Lane

`hybrid/BuildAndTest.sh` and `hybrid/BuildAndTest.bat` are the most complete
image workflow. Run them from `Testing/hybrid`:

```sh
./BuildAndTest.sh
```

```cmd
BuildAndTest.bat
```

They create `ICC/`, `Results/`, and `config/`, then execute these phases:

1. Build hybrid CMYK, multi-spectral RGB, observer, PCC, and overprint profiles
   from the local XML fixtures.
2. Inspect the source spectral TIFF with `iccTiffDump`, convert it to a
   multi-spectral TIFF through `iccApplyProfiles`, and inspect the generated
   TIFF again. The `-embedded 3` and `-embedded 10003` arguments explicitly
   select the embedded profile and, for the latter, its v5 sub-profile.
3. Render the reference and generated spectral TIFFs through Illuminant A and
   F11 PCCs into the v4 sRGB preference profile. This checks both embedded
   profile selection and spectral-to-colorimetric consistency.
4. Convert the v5 LCD display profile for the custom Cat8 observer with
   `iccV5DspObsToV4Dsp`; the three argv values are input display, input
   observer PCC, and output display in that order.
5. Apply the hybrid CMYK profile to spectral values with `iccApplyNamedCmm`,
   then estimate the values with `iccApplySearch -INIT`. Both commands export
   their parsed configuration and data to JSON for inspection.
6. Render CMYKW and KW TIFF overprint previews. The background variants use
   `-ENV:bkgX`, `-ENV:bkgY`, and `-ENV:bkgZ`; the MCS variants add the
   `-ENV:0ni? 1` control before the v5 MCS profile sequence.

The generated TIFFs and JSON configuration exports are intentional local
artifacts. Do not treat their presence as profile fixtures or commit them.

## Profile Directory Reference

- [`Calc`](Calc) demonstrates modeling with Calculator
  MultiProcessElements. `srgbCalcTest` exercises all specified calculator
  operations.
- [`Display`](Display) demonstrates spectral display modeling with observers
  selected at startup.
- [`Encoding`](Encoding) contains three-channel encoding-class profiles,
  including name-only and fully specified variants.
- [`Named`](Named) contains named-color profiles with tints, spectral
  reflectance, fluorescence, and sparse notation.
- [`PCC`](PCC) contains abstract Profile Connection Condition profiles for
  multiple observers, illuminants, and colorimetric intents.
- [`SpecRef`](SpecRef) contains profiles that convert and manipulate spectral
  reflectance, including six-channel abridged spectral encodings.

## CI Round-Trip Classification

The Linux CI sweep separates clean reconstruction from expected negative
fixtures during `iccFromXml` XML-to-ICC round-trip testing.

- Clean profiles must reconstruct and save without validation errors.
- Known invalid fixtures are listed in `expected-invalid-fromxml.tsv`.
- New parse failures, sanitizer findings, or unclassified validation
  diagnostics fail CI until fixed or classified.

`qa-profile-manifest.tsv` records the expected validation verdict for every
profile:

| Suite | Expected status | Rule |
| --- | --- | --- |
| `positive` | `valid` | Must validate clean; any new diagnostic is a regression. |
| `compatibility` | `warning` | A known diagnostic is baselined; escalation fails. |
| `negative` | `critical` | Rejection is expected; acceptance is a failure. |

A sanitizer finding or fatal signal fails every suite, including negative
fixtures. Stable SHA-256 values are recorded only for tracked profiles because
generated profiles can contain time-dependent creation data and profile IDs.
`expected_status` and `expected_exit` remain separate because noncompliant
profiles can still return exit status zero.

## Exit Status and Safe Use

An exit status of zero means the invoked script completed its intended lane.
`RunTests.sh` fails at the first failed tool invocation. `RunTests.bat`
aggregates failures to expose independent cases before returning nonzero.
Other legacy specialized scripts return their final command's status, so use
them for focused investigation and prefer CTest for a gated full run.

All paths are relative to the stated working directory. Before using an
application lane, generate the profiles required by that lane or use the CTest
fixture that runs `CreateAllProfiles`. Do not pass unrecognized positional
arguments to the profile-generation scripts, and do not substitute a profile
where an integer encoding, interpolation, or rendering intent is required.
