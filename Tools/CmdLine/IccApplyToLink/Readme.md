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
| `option` with `link_type=0` | `0` writes a version 4.3 profile with a 2-byte mAB CLUT; `1` writes a version 5 profile. |
| `option` with `link_type=1` | Digits after the decimal point for `.cube` LUT samples, from `0` through `20`. Non-default `DOMAIN_MIN`/`DOMAIN_MAX` metadata is written with enough significant digits to preserve its values. |
| `first_transform` | `0` uses the destination transform from the first profile; `1` uses the source transform from the first profile. |
| `interp` | `0` linear interpolation; `1` tetrahedral interpolation while sampling the input profile transforms. It does not prescribe interpolation by a later consumer of the generated link. |
| `rendering_intent` | Base intent `0`-`3` plus decimal-coded modifiers, read as independent columns: the tens digit is the transform-lookup type (`icXformLutType`, reachable values `0`-`9`; `+10` drops the D2Bx/B2Dx tags and `+40` adds black-point compensation), `+100` requests luminance matching, and `+1000` uses a V5 sub-profile if present. A negative code is rejected (#2268). |

All stages used to create a DeviceLink must select the same base intent from
`0` through `3`. The tool records that intent in the generated profile header,
as required by ICC.1:2022 clauses 7.2.15 and 8.6 and ICC.2:2023 clauses 7.2.17
and 8.6. A mixed-intent chain is rejected for DeviceLink output because its
single header field cannot describe that chain accurately. Cube output has no
ICC profile header and continues to permit mixed base intents.

`+100` and `+1000` are the canonical modifier spellings. For compatibility
with existing command corpora, any non-zero hundreds digit enables luminance
matching and any positive value above the hundreds column enables V5-subprofile
selection; the final two digits retain the transform type and base intent.
`iccApplyProfiles`, `iccApplyNamedCmm`, and `iccApplySearch` use a different,
wider positional encoding through `CIccCfgProfileSequence`; do not transfer
their modifier weights to this tool.

For a CMYK output profile to RGB color-space profile chain, use the first
profile as a source transform and write a DeviceLink profile:

```sh
iccApplyToLink GRACoL_to_sRGB.icc 0 2 1 "GRACoL_to_sRGB" 0 1 1 0 GRACoL2006_Coated1v2.icc 1 sRGB_v4_ICC_preference.icc 1
```

`.cube` output requires both source and destination spaces to have exactly three
channels. It follows Adobe Cube LUT Specification 1.0 for its three-component
`DOMAIN_MIN`/`DOMAIN_MAX` keywords and table order: red varies fastest, then
green, then blue. A quote in the requested title is represented as `\x22`
because the Cube format excludes the quote delimiter from title text and
defines no escape sequence for it.

Each transform stage has the following shape. At least one stage is required,
and an environment signature is exactly four characters:

```text
[-ENV:cccc value ...] profile_file_path rendering_intent [-PCC connection_conditions_path]
```

Repeat the complete stage to build a multi-profile chain. Environment controls
apply to the profile that follows them; a PCC applies to the profile immediately
before it. Environment values must be finite numbers. They are supplied to the
profile as 32-bit CMM environment values keyed by the exact four-character
signature; calculator profiles can therefore change their results in response
to controls such as `-ENV:ambL 30`.

The DeviceLink structures follow ICC.1:2022 clauses 8.6 and 10.12 for the V4
mAB output, and ICC.2:2023 clauses 8.6, 9.2.1, 11.2.2.2, and 11.2.3 for the V5
MPE output. The tool currently labels its V4 output as version 4.3; ICC.1:2022
itself identifies version 4.4 for that specification edition. Cube text output
is not defined by either ICC specification.
For V4 DeviceLinks with xCLR endpoints, the endpoint profiles must provide the
corresponding colorant tables required by ICC.1:2022 clause 8.6. The tool
refuses V4 output if a required table is absent, including when the source is
a V5 profile without a legacy colorant table. Its `profileSequenceDescTag`
uses aligned `multiLocalizedUnicodeType` descriptions; missing source
descriptions become the empty `mluc` placeholders specified by ICC.1:2022
clause 10.19.

Malformed command-line arguments are diagnosed on standard error. Successful
generation and legacy progress output remain on standard output.

## Telemetry and Evidence

Global options can appear before or after the positional command form:

```sh
iccApplyToLink output.cube 1 17 6 "sRGB cube" 0 1 0 0 sRGB.icc 1 \
  --telemetry=jsonl --telemetry-file link.jsonl \
  --evidence-file link-evidence.json
```

Configure `ICCDEV_ENABLE_TELEMETRY=ON` to use
`--telemetry=off|human|jsonl`, `--telemetry-file FILE`, and
`--evidence-file FILE`; legacy single-dash spellings remain accepted. Human
telemetry uses UTC timestamps and is written to stderr. JSONL records
`run_started`, `transform_ready`,
and completion or failure; the evidence sidecar includes the link output, grid
size/node count, elapsed time, and node throughput. Human telemetry reports
timestamped LUT progress at the first and last node and about once per second
during long runs. It replaces the legacy
percentage display so a combined console log has one progress view.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccFromCube](../IccFromCube/Readme.md)
