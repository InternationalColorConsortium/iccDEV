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
| `link_type` | `0` writes an ICC DeviceLink profile; `1` writes a `.cube` text LUT. |
| `option` with `link_type=0` | `0` writes a version 4 profile; `1` writes a version 5 profile. |
| `option` with `link_type=1` | Digits of precision for `.cube` output. |
| `first_transform` | `0` uses the destination transform from the first profile; `1` uses the source transform from the first profile. |
| `interp` | `0` linear interpolation; `1` tetrahedral interpolation. |

For a CMYK output profile to RGB color-space profile chain, use the first
profile as a source transform and write a DeviceLink profile:

```sh
iccApplyToLink GRACoL_to_sRGB.icc 0 2 1 "GRACoL_to_sRGB" 0 1 1 0 GRACoL2006_Coated1v2.icc 1 sRGB_v4_ICC_preference.icc 0
```

`.cube` output requires both source and destination spaces to have exactly three
channels.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccFromCube](../IccFromCube/Readme.md)
