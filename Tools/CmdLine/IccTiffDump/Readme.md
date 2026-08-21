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

Extraction copies the TIFF ICC profile field byte-for-byte before profile
parsing and validation. This preserves malformed profiles for diagnostics and
does not rewrite tag offsets, padding, or the profile ID. A later parse or
validation failure is still reported with a nonzero exit status, but does not
remove the extracted forensic artifact. Output is written to a sibling
temporary file and atomically renamed only after the complete write succeeds.
Existing regular files may be replaced atomically; device files, directories,
and symbolic links are rejected as extraction destinations.

The `Resolution:` line names the unit the file declares -- `pixels per/inch`,
`pixels per/centimeter`, or `(relative, no absolute unit)` for `RESUNIT_NONE`,
where the resolution values fix an aspect ratio and no absolute size exists. The
`Size:` line reports the physical size in inches, converted from centimeters when
needed, and omits it entirely under `RESUNIT_NONE`.

The no-argument form is a help/syntax path and exits successfully. Other
malformed invocations fail: extra trailing arguments are rejected, missing input
files fail, and export requests fail when the TIFF has no embedded ICC profile.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccJpegDump](../IccJpegDump/Readme.md)
- [IccPngDump](../IccPngDump/Readme.md)
