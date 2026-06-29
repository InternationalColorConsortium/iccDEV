# IccSpecSepToTiff

`iccSpecSepToTiff` combines separate TIFF images for spectral wavelengths into a
single multi-sample TIFF image. It can optionally embed an iccMAX profile.

## Usage

Run without arguments to print the current command syntax and supported options.
The fourth argument is a filename prefix. `iccSpecSepToTiff` appends the channel
number to that prefix, so `spec_` with `start=1` opens `spec_1`, then `spec_2`,
and so on.

`compress` and `sep` must be literal boolean values (`0` or `1`). The channel
range must be exact: repeatedly adding `incr` to `start` must land on `end`.
When the optional profile argument is provided, the profile file must be
readable or the command fails without writing an output TIFF.

```sh
iccSpecSepToTiff
```

Example:

```sh
iccSpecSepToTiff out.tif 0 0 .github/ci/test-data/spectral/spec_ 1 10 1
```

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccTiffDump](../IccTiffDump/Readme.md)
