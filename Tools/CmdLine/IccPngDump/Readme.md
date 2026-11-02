# IccPngDump

`iccPngDump` prints PNG metadata and embedded ICC profile information. It can
also export a profile or inject one into a separate PNG.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccPngDump
```

Inspect or extract the embedded profile:

```sh
iccPngDump image.png
iccPngDump image.png extracted.icc
```

Inject an ICC profile into a separate PNG:

```sh
iccPngDump image.png --write-icc profile.icc --output tagged.png
```

Injection rejects animated PNG (APNG) before creating output because the
current writer handles only one image frame. The tool limits a decoded raster
to 256 MiB and an ICC profile or PNG chunk to 64 MiB. A requested extraction
can currently report success without writing a file when no iCCP profile is
present; the correction is tracked in the image-tool audit.

`--version` reports the IccProfLib and libpng build versions plus the linked
libpng runtime version. `--help` puts that banner before the usage text.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccJpegDump](../IccJpegDump/Readme.md)
- [IccTiffDump](../IccTiffDump/Readme.md)
- [Image-tool QA review](../../../docs/analysis/image-tool-tiff-qa-2026-11-01.md)
