# IccApplyProfiles

`iccApplyProfiles` applies a sequence of ICC/iccMAX profiles to a TIFF image and
writes a destination TIFF image. The destination profile can optionally be
embedded in the output image.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccApplyProfiles
```

## TIFF Pixel Encoding

`iccApplyProfiles` treats TIFF pixel values as a *device encoding* regardless
of color space or TIFF photometric tag. The rule applies symmetrically on
read and write:

| Pixel format | Decode (read) | Encode (write) |
|--------------|---------------|----------------|
| 8-bit integer | `value = pixel / 255` | `pixel = clamp01(value) * 255` |
| 16-bit integer | `value = pixel / 65535` | `pixel = clamp01(value) * 65535` |
| 32-bit float | `value = pixel` (pass-through) | `pixel = value` (pass-through) |

No bit-bias adjustments are applied — for example, a/b channels of a 16-bit
Lab destination are *not* offset by `0x8000` even when the TIFF
PhotometricInterpretation is `CIELAB` or `ICCLAB`. Consumers of the output
TIFFs should use the *embedded ICC profile* to interpret pixel values,
since that profile (not the photometric tag alone) describes the actual
encoding produced by the final transform.

Any PCS-encoding bridging is the CMM's responsibility: profile xforms whose
PCS endpoint is `Lab` or `XYZ` apply `icLabToPcs` / `icLabFromPcs` (or the
XYZ equivalents) at their own entry/exit, so values entering and leaving
the boundary code here are always in the form the *next* transform stage
or the destination TIFF expects.

Practical consequences:

- A chain ending in a v5 MPE PCC profile that maps standard CIELAB to a
  `[0, 1]` device-Lab encoding (e.g. `L_dev = L*/100`,
  `a_dev = (a*+128)/256`) will produce a 16-bit Lab TIFF where each channel
  is the device-encoded value scaled to `[0, 65535]`. Round-tripping via
  the same profile recovers the original colors.
- A chain ending in a plain Lab PCS output produces values in the CMM's
  internal PCS-Lab encoding (normalized `[0, 1]`). Writing 32-bit float
  passes those through unchanged; writing 16-bit scales them to
  `[0, 65535]`.
- A TIFF whose `PhotometricInterpretation` is `CIELAB` (TIFF spec) without
  an embedded ICC profile cannot be read back to its original colors,
  because the tool does not apply the spec's `±128`-biased decoding. Embed
  the ICC profile (set `dstEmbedIcc: true` in the JSON config) so the
  reverse chain can recover the values.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [Build documentation](../../../docs/build.md)
