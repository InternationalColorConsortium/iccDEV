# IccApplyToLink

`iccApplyToLink` builds an ICC DeviceLink profile or `.cube` LUT by applying a
sequence of ICC profiles. It supports rendering intent, LUT size, interpolation,
and Profile Connection Condition controls.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccApplyToLink
```

Command shape:

```sh
iccApplyToLink dst_link_file link_type lut_size option title range_min range_max first_transform interp profile_file_path rendering_intent [...]
```

Key arguments:

| Argument | Values |
|----------|--------|
| `link_type` | `0` writes an ICC DeviceLink profile; `1` writes a `.cube` text LUT. Other values are rejected. |
| `lut_size` | Integer grid size from `2` through `255`. |
| `option` with `link_type=0` | `0` writes a version 4 profile; `1` writes a version 5 profile. |
| `option` with `link_type=1` | Digits of precision for `.cube` output, from `0` through `20`. |
| `first_transform` | `0` uses the destination transform from the first profile; `1` uses the source transform from the first profile. |
| `interp` | `0` linear interpolation; `1` tetrahedral interpolation. |
| `rendering_intent` | Base intent `0`-`3` plus decimal-coded modifiers, read as independent columns: the tens digit is the transform-lookup type (`icXformLutType`, reachable values `0`-`9`; `+10` drops the D2Bx/B2Dx tags and `+40` adds black-point compensation), `+100` requests luminance matching, and `+1000` uses a V5 sub-profile if present. A negative code is rejected (#2268). |

For a CMYK output profile to RGB color-space profile chain, use the first
profile as a source transform and write a DeviceLink profile:

```sh
iccApplyToLink GRACoL_to_sRGB.icc 0 2 1 "GRACoL_to_sRGB" 0 1 1 0 GRACoL2006_Coated1v2.icc 1 sRGB_v4_ICC_preference.icc 0
```

`.cube` output requires both source and destination spaces to have exactly three
channels.

## Telemetry and Evidence

Global options can appear before or after the positional command form:

```sh
iccApplyToLink output.cube 1 17 6 "sRGB cube" 0 1 0 0 sRGB.icc 1 \
  --telemetry=jsonl --telemetry-file link.jsonl \
  --evidence-file link-evidence.json
```

Use `--telemetry=off|human|jsonl`, `--telemetry-file FILE`, and
`--evidence-file FILE`; legacy single-dash spellings remain accepted. Human
telemetry uses UTC timestamps and is written to stderr. JSONL records
`run_started`, `transform_ready`,
and completion or failure; the evidence sidecar includes the link output, grid
size/node count, elapsed time, and node throughput. Human telemetry replaces
the legacy percentage display so a combined console log has one progress view.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccFromCube](../IccFromCube/Readme.md)
