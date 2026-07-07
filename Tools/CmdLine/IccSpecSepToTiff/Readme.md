# IccSpecSepToTiff

`iccSpecSepToTiff` combines separate TIFF images for spectral wavelengths into a
single multi-sample TIFF image. It can optionally validate and embed an ICC
profile whose channel model matches the generated TIFF.

## Usage

Run without arguments to print the current command syntax and supported options.
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
cannot describe the image samples.

```sh
iccSpecSepToTiff
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

- Input TIFFs must have identical dimensions, resolution, sample format,
  photometric interpretation, and `BitsPerSample`.
- Each input TIFF must have exactly one sample per pixel.
- Photometric interpretation must be `MINISBLACK` or `MINISWHITE`; palette,
  RGB, CMYK, and unknown photometrics are rejected.
- `BitsPerSample` must be byte-aligned. Uncompressed output supports any
  byte-aligned sample size accepted by the TIFF helper. LZW compression is
  limited to 8-, 16-, or 32-bit samples.
- Output uses explicit `ExtraSamples` metadata for every sample beyond the
  first. Very high channel-count spectral TIFFs can be valid but may exceed
  limits in display tools such as ImageMagick.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccTiffDump](../IccTiffDump/Readme.md)
