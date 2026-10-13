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

Inspection is metadata-only: tiled or stripped storage, unsupported pixel
orientation/sample formats, and an unavailable codec do not require image
decoding or allocation of pixel buffers to inspect/extract ICC metadata.
The tool reports classic TIFF/BigTIFF, storage layout, tile geometry, block
count, and codec availability; codec availability is not proof of successful
pixel decoding.

Select a zero-based main-chain IFD, or inventory all main-chain directories:

```sh
iccTiffDump --ifd 1 image.tif embedded-ifd1.icc
iccTiffDump --list-directories image.tif
iccTiffDump --evidence-json --list-directories image.tif
```

`--ifd` accepts indexes 0..4095 and defaults to 0. Inventory is limited to 4096
directories, includes each directory's ICC byte count, sample/storage metadata,
and SubIFD offsets, and does not parse/validate profiles or extract files.
SubIFD offsets are reported only, not recursively followed or selectable.
Listing cannot be combined with `--ifd` or an extraction destination. Extract
each desired main-chain directory with a separate invocation and destination.
Stored ExtraSamples counts are read from the selected classic or BigTIFF IFD,
not borrowed from the first directory.

The existing `iccdev-qa-evidence/v1` extraction receipt has additive
`directory`, `bigTiff`, `tiled`, `tileWidth`, and `tileHeight` fields.
Inventory uses `iccdev-tiff-inventory/v1` with a `directories` array.
Metadata reporting does not establish SVS pixel-decoding compatibility or an
ICC inheritance policy across pyramid levels and associated images.

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

## Optional telemetry

Telemetry is compiled out by default. Configure `ICCDEV_ENABLE_TELEMETRY=ON`,
or use the composed `linux-clang-tiff-telemetry-qa` or
`vs2022-clangcl-x64-tiff-telemetry-qa` preset, to enable these opt-in diagnostics:

```sh
iccTiffDump --telemetry=human image.tif
iccTiffDump --telemetry=jsonl --telemetry-file run.jsonl image.tif embedded.icc
iccTiffDump --telemetry=jsonl --telemetry-file inventory.jsonl --list-directories image.tif
```

Human events and JSON fields go to stderr; JSONL events go to a separate,
exclusively created file that must not already exist or alias the input or
extraction destination. Telemetry remains off unless requested. Default stdout
and stderr, extraction evidence, and inventory schemas are unchanged; builds
without telemetry reject telemetry options explicitly. `--help` and `-h` also
print help successfully.

Events measure metadata opening, bounded main-chain inventory preflight and
forward traversal, and embedded-profile parsing, tag reading, validation, and
byte-for-byte extraction. Inventory reports each visited IFD's geometry and
aggregates visited IFDs and profile bytes; it never follows SubIFDs. Inspection
reports the selected IFD's dimensions, sample/planar and tile layout,
classic/BigTIFF container, resolution, orientation, and profile bytes.
Profile counters count successful embedded-profile phases; extraction bytes
count a completed atomic write, even if later validation fails. The additional
extracted-file parse used by `--evidence-json` has its own duration event.
Decoded pixels and decoded bytes are explicitly zero: no codec is exercised.
Early failures report the last stage and never report successful completion.
A diagnostic-write failure returns status 1 without removing a forensic
extraction artifact or skipping the normal extraction/validation work.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccJpegDump](../IccJpegDump/Readme.md)
- [IccPngDump](../IccPngDump/Readme.md)
