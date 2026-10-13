# IccSpecSepToTiff

`iccSpecSepToTiff` combines separate TIFF images for spectral wavelengths into a
single multi-sample TIFF image. It can optionally validate and embed an ICC
profile whose channel model matches the generated TIFF.

## Usage

Use `-h` or `--help` for the command syntax and `--version` for the build
version. Both exit with status 0 and print to standard output. Running without
arguments, or with an incomplete or over-long command, names the problem and
prints the syntax to standard error, then exits with status 255. A conversion is
`argc=8` without a profile or `argc=9` with one:

Optional `--tiles WIDTH HEIGHT` and `--bigtiff` switches are removed before
the positional arguments below are parsed. Tile dimensions must be positive
multiples of 16. Without these switches, output stays classic stripped TIFF.

```sh
iccSpecSepToTiff --tiles 256 256 --bigtiff out.tif 1 1 spectral/spec_ 1 10 1
```

| `argv` | Name | Meaning |
| --- | --- | --- |
| `argv[0]` | program | Path or name used to invoke `iccSpecSepToTiff` |
| `argv[1]` | output | TIFF file to create |
| `argv[2]` | compress | `0` for none or `1` for LZW |
| `argv[3]` | sep | `0` for interleaved or `1` for separate planes |
| `argv[4]` | infile_prefix | Literal input filename prefix |
| `argv[5]` | start | First channel number |
| `argv[6]` | end | Last channel number, inclusive |
| `argv[7]` | incr | Nonzero channel increment |
| `argv[8]` | profile | Optional ICC profile to validate and embed |

The fourth argument is a literal filename prefix. `iccSpecSepToTiff` appends the
channel number to that prefix, so `spec_` with `start=1` opens `spec_1`, then
`spec_2`, and so on. It is not a `printf` format string.

`compress` and `sep` must be literal boolean values (`0` or `1`). `start`,
`end`, and `incr` must be plain decimal integers with an optional leading minus
sign only; whitespace, `+`, `NaN`, floats, hex, and out-of-range values are
rejected. The channel range must be exact: repeatedly adding `incr` to `start`
must land on `end`.
When the optional profile argument is provided, the profile file must be
readable, non-empty, parse as ICC, pass ICC validation without non-compliance,
and match the output sample count:

- If the profile has an ICC.2 spectral PCS, the spectral PCS channel count and
  spectral range steps must equal the generated TIFF `SamplesPerPixel`.
- Otherwise, the profile data color-space sample count must equal the generated
  TIFF `SamplesPerPixel`.

The command fails before creating output when these checks fail. This prevents
TIFF files from carrying mislabeled `ICC Profile` tag data or profiles that
cannot describe the image samples. Diagnostics for every rejection go to
standard error; the accepted-profile line and the conversion summary go to
standard output. Every error path exits with status 255.

```sh
iccSpecSepToTiff --help
iccSpecSepToTiff --version
```

Example:

```sh
iccSpecSepToTiff out.tif 0 0 .github/ci/test-data/spectral/spec_ 1 10 1
```

The checked-in CI spectral fixtures are intentionally tiny and may appear very
small in image viewers. For manual display review, use the larger
`.github/ci/test-data/specsep-harvest/gray300/spec_` sequence or create larger
single-channel TIFF inputs.

## Input and Output Notes

- Stripped and tiled single-channel inputs may be combined. Tiled input/output
  supports 8-bit unsigned, 16-bit unsigned, and 32-bit float samples. Interleaved
  and separated output preserve every spectral sample; edge-tile padding is zero.
  All concurrently open tiled inputs share a 256 MiB tile-band budget. The
  next input is rejected before its tile band is allocated if it would exceed
  the remaining budget. The output tile band has a separate 256 MiB cap; row
  and stripped-input buffers are outside these tile-band limits. These are not
  total-process memory guarantees.
- Tiled output preserves the existing unspecified-extra-sample policy, profile
  checks, resolution metadata, and partial-output cleanup. It does not generate
  pyramids or vendor-specific SVS files.

- Input TIFFs must have identical dimensions, resolution, resolution unit,
  sample format, photometric interpretation, and `BitsPerSample`. The unit is
  part of the resolution: 300 pixels/inch and 300 pixels/cm are the same two
  numbers describing images of different physical size.
- The output carries the inputs' `ResolutionUnit`, and their `XResolution` and
  `YResolution` with them. Two long-standing substitutions can still make the
  output resolution values differ from the input's, both in the unit the input
  declared: an absent or non-positive resolution becomes 96 when the input is
  read, and a resolution below 1 that survives that becomes 72.
- `start`, `end`, and `incr` select filename suffixes; they are not wavelength
  values. The TIFF container records neither wavelengths, units, bandwidths,
  nor channel names. A compatible embedded spectral ICC profile can provide
  colour semantics to applications that support ICC.2, but it does not make
  filename suffixes into wavelength metadata.
- Each input TIFF must have exactly one sample per pixel.
- Photometric interpretation must be `MINISBLACK` or `MINISWHITE`; palette,
  RGB, CMYK, and unknown photometrics are rejected. Floating-point
  `MINISWHITE` is rejected because the bytewise inversion it needs is not a
  valid numeric conversion for IEEE floating-point samples.
- `BitsPerSample` must be byte-aligned. Uncompressed output supports any
  byte-aligned sample size accepted by the TIFF helper. LZW compression is
  limited to 8-, 16-, or 32-bit samples.
- Output uses explicit `ExtraSamples` metadata for every sample beyond the
  first. Each is `EXTRASAMPLE_UNSPECIFIED` (TIFF value 0), identifying it as
  auxiliary spectral data rather than associated or unassociated alpha. Very
  high channel-count spectral TIFFs can be valid but may exceed limits in
  display tools such as ImageMagick. Some editors, including Photoshop, expose
  unspecified extras as channels labelled `Alpha 1`, `Alpha 2`, and so on;
  those labels do not make the samples transparency data.
- DeviceLink profiles are rejected because they cannot be embedded in image
  files. Other profiles must match the output's data colour-space sample count;
  the spectral PCS describes a connection space, not the TIFF input samples.
- TIFF can carry ICC.2 profiles, but applications that do not implement ICC.2
  may not read or apply them. Check target-application support before relying
  on embedded ICC.2 spectral metadata.
- An existing output destination must be a regular non-symlink file. Directories,
  devices, POSIX symlinks, and Windows reparse points are rejected before
  anything is written; a hard link is accepted, being another name for a regular
  file. If profile embedding, reading, or writing fails after the output was
  created, the incomplete output is removed, and a failed create removes the stub
  only when the destination did not already exist *and* this run had actually
  opened it -- a refused destination, including a dangling symlink, is left
  exactly as it was found.
- A successful conversion reports the accepted profile, if any, followed by the
  output path, dimensions, bit depth, sample count, planar configuration,
  compression, and embedded-profile state. Nothing is written to standard output
  until the output TIFF is complete, so an empty standard output means nothing
  was produced.
- Paths echoed in diagnostics are escaped: control characters and every byte
  outside printable ASCII are rendered as `\xNN`, matching `iccTiffDump`. A
  non-ASCII path therefore appears escaped rather than verbatim.

## Limits

- `start`, `end`, and `incr` are signed 32-bit integers.
- TIFF `SamplesPerPixel` limits the generated image to 65,535 channels.
- TIFF width and height are unsigned 32-bit values. Row-buffer and output-size
  products are checked for `size_t` overflow before allocation.
- Embedded ICC profile data is limited to 4,294,967,295 bytes by the ICC/TIFF
  profile-length field.
- LZW output supports 8, 16, or 32 bits per sample.
- All input TIFFs remain open during conversion. The operating system's
  per-process open-file limit can therefore impose a lower practical channel
  cap than the TIFF field.
- Concurrent tiled inputs share a 256 MiB decoded tile-band budget. A channel
  set that exceeds it fails before output creation.

There is no additional tool-specific cap on image dimensions, total pixels, or
prefix length. Allocation failures and arithmetic overflow fail the conversion
instead of producing a partial output.

## Maintainer QA

### Diagnostic telemetry

Telemetry is optional and off by default. It is compiled out unless CMake is
configured with `ICCDEV_ENABLE_TELEMETRY=ON`; the QA presets are
`linux-clang-tiff-telemetry-qa` and
`vs2022-clangcl-x64-tiff-telemetry-qa`. Telemetry options are advertised by
`--help` only in telemetry-enabled builds.

Use `--telemetry=human` for phase and geometry events on standard error, or
`--telemetry=jsonl --telemetry-file receipt.jsonl` for a new, exclusive-create
JSONL receipt. The receipt path must not alias any input, profile, or output.
Telemetry never changes the normal conversion summary on standard output.

```sh
iccSpecSepToTiff output.tif 0 0 spectral/spec_ 1 10 1 --telemetry=human
iccSpecSepToTiff output.tif 0 0 spectral/spec_ 1 10 1 \
  --telemetry=jsonl --telemetry-file receipt.jsonl
```

`logical_bytes_read` counts decoded row bytes from successful channel reads;
`logical_bytes_written` counts packed output row bytes accepted by successful
TIFF writes. They describe logical sample payload, not physical disk traffic,
compressed size, TIFF metadata, or filesystem allocation. `channel_rows_read`
counts successful source-row reads across all channels; `rows_written` counts
completed output rows. Phase times aggregate open/profile, read, pack, write,
and finalization work; there are no per-pixel timers. Throughput is null when
the measured read/pack/write interval is zero.

After building the tools, run the complete checked-in QA surface from any
directory:

```sh
ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools \
  /path/to/iccDEV/.github/scripts/iccdev-specsep-qa.sh
```

The wrapper verifies the repository fixtures and runs the CLI argument,
`--tiles`/`--bigtiff` output-layout, telemetry (when compiled), TIFF geometry,
usage/metadata, and numbered-fixture matrix suites plus both optional-profile
sweeps. Generated TIFFs and logs stay outside the source tree under
`ICCDEV_TEST_OUTDIR`.

To exercise the optional `profile` argument against an external or downloaded
profile directory:

```sh
ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools \
  /path/to/iccDEV/.github/scripts/iccdev-specsep-profile-sweep.sh \
  --profile-dir /path/to/profiles --channels 3
```

A profile is embedded only when its sample count equals `--channels`, so set
that to a value the corpus carries; otherwise every profile is a clean rejection
and the sweep reports that it accepted nothing. Compatible profiles must be
embedded in a nonempty TIFF; incompatible or non-conformant profiles must be
rejected with a profile diagnostic and no partial output.

For a bounded matrix covering compression, planar layout, range order, bit
depth, photometric conversion, optional profile embedding, rejection paths, and
exact output pixels and tags:

```sh
ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools \
  /path/to/iccDEV/.github/scripts/iccdev-specsep-campaign.sh --seconds 300
```

The campaign requires Python `numpy`, `tifffile`, and `imagecodecs`. Its
`--seconds` value buys repetition rather than coverage: the case set is
deterministic, so a longer run is a soak that surfaces nondeterminism, not a
wider matrix. It keeps its summary, generated inputs and TIFFs, and independent
`iccTiffDump`/`tiffinfo` inspections under `ICCDEV_TEST_OUTDIR`. Functional
failures and specification-audit findings both produce a nonzero exit.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccTiffDump](../IccTiffDump/Readme.md)
