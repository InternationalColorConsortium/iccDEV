# IccTiffDump

`iccTiffDump` prints TIFF metadata and embedded ICC profile information.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccTiffDump
```

Dump TIFF metadata and embedded ICC profile metadata:

```sh
iccTiffDump image.tif
```

Extract an embedded ICC profile:

```sh
iccTiffDump image.tif embedded.icc
```

The no-argument form is a help/syntax path and exits successfully. Other
malformed invocations fail: extra trailing arguments are rejected, missing input
files fail, and export requests fail when the TIFF has no embedded ICC profile.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccJpegDump](../IccJpegDump/Readme.md)
- [IccPngDump](../IccPngDump/Readme.md)
