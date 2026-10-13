# IccJpegDump

`iccJpegDump` prints the JPEG file size and embedded ICC profile information.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccJpegDump
```

Inspect a JPEG without creating any output file:

```sh
iccJpegDump image.jpg
```

Inspection reports ICC APP2 chunk sizes, the reassembled profile size, color
space, PCS, and version. A JPEG without ICC APP2 markers reports that absence
and exits successfully. Malformed marker/chunk data or an unparseable embedded
ICC profile returns a nonzero exit code.

Extract the reassembled APP2 bytes unchanged, including malformed ICC payloads:

```sh
iccJpegDump image.jpg output.icc
```

Extraction requires an ICC APP2 payload but does not require IccProfLib to parse
it. It rejects an output path that aliases the input JPEG. EXIF/ad hoc ICC
search is not supported.

Inject an ICC profile into a separate JPEG:

```sh
iccJpegDump image.jpg --write-icc profile.icc --output tagged.jpg
```

The focused regression runs on Windows and Unix:

```sh
python .github/scripts/test_jpegdump_inspection.py --tool /path/to/iccJpegDump
```

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccPngDump](../IccPngDump/Readme.md)
- [IccTiffDump](../IccTiffDump/Readme.md)
