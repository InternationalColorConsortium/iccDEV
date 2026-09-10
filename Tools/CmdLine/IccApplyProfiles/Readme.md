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

## Inverse-Search Chains (`connect.useSearch`)

Setting `useSearch` to `true` inside the `connect` block builds a
`CIccCmmSearch` instead of the usual forward `CIccCmm`, so an image can be run
through a spectral inverse search rather than a forward profile chain. It is
available in the JSON configuration only; the positional command-line form has
no room for the search-only settings and `-exportcfg` output is unchanged.

```json
{
  "imageFiles": { "srcImageFile": "in.tif", "dstImageFile": "out.tif", "dstEmbedIcc": true },
  "connect":    { "threads": 0, "useSearch": true },
  "searchApply": {
    "profileSequence": [ { "iccFile": "src.icc", "intent": "relative" },
                         { "iccFile": "dst.icc", "intent": "relative" } ],
    "initial":    { "intent": "relative" },
    "pccWeights": [ { "pccFile": "obs2deg.icc",  "weight": 1.0 },
                    { "pccFile": "obs10deg.icc", "weight": 0.5 } ]
  }
}
```

The chain lives in `searchApply`, spelled exactly as `iccApplySearch` spells it
-- `profileSequence`, `initial` and `pccWeights` inside one object -- so the
block moves between the two tools verbatim. There is only ever one place the
chain is written: a top-level `profileSequence` is the forward-mode spelling and
is not consulted under `useSearch`. Naming a `searchApply` block with
`useSearch` off is an error rather than a silent forward run.

`initial` is optional; without it the search starts from the destination
profile itself rather than a seeded starting point.

Rules the tool enforces before building the CMM:

- **2 or 3 profiles.** `CIccCmmSearch` accepts a source, an optional mid, and a
  destination profile. Anything else is rejected by name:
  `useSearch requires 2 or 3 profiles in profileSequence (found N)`.
- **3 profiles require `pccWeights`.** A three-stage search connects
  source->mid and mid->destination through the weighted PCC set, so at least one
  entry is required.

Two further behaviors are worth knowing:

- **Embedded source profile.** An empty first `iccFile` still means "use the ICC
  profile embedded in the source TIFF", exactly as in forward mode.
- **Per-stage `pccFile` is inert.** `CIccCmmSearch::AddXform` discards its `pPcc`
  argument, so a `pccFile` on a chain entry does nothing -- except on the last
  entry, whose `pccFile` is read for the initial-destination chain. The tool
  warns on stderr for the entries where it is ignored. Use `pccWeights` to give
  the search the PCCs it optimises across.

### Threading

`connect.threads` works for a search chain exactly as it does for a forward one.
Each `CIccThreadedCmm` worker gets its own `CIccApplyCmmSearch`, which owns a
private `CIccApplyCmm` for every sub-chain (`m_srcToMidApply`,
`m_dstToMidApply`, `m_pMidToDstApply`), and the per-pixel search carries nothing
between pixels, so strip partitioning is safe and the threaded result is
identical to the scalar one.

Threading matters far more here than for a forward chain, because the per-pixel
cost is orders of magnitude higher -- see below.

### Performance

A search chain is orders of magnitude slower per pixel than a forward chain:
every pixel runs a Nelder-Mead search rather than a fixed sequence of xforms.
Memory grows with both the worker count and the size of `pccWeights`, since each
worker's apply object holds one reverse sub-chain per weighted PCC. Measured on
a 3-profile chain with four weighted PCCs, a search apply costs roughly 0.5 ms
per pixel against about 0.4 us for the equivalent forward chain -- so prefer
`"threads": 0` for anything larger than a test swatch, and expect image-sized
runs to take minutes rather than seconds even then.

## Resolution

The destination TIFF carries the source's `XResolution`, `YResolution` and
`ResolutionUnit`. The unit tag is always written, so the output states its unit
rather than relying on the reader defaulting an absent tag to inches.

## Output Destination Safety

The destination must be a regular non-symlink file when it already exists.
Directories, devices, POSIX symlinks, and Windows reparse points are rejected
before libtiff opens the destination. Naming a symlink as the output is
therefore an error rather than a write through to its target; point the tool at
the target path directly.

Two limits are deliberate rather than oversights:

- A **hard** link is accepted. It is another name for the regular file it links
  to, and nothing in the file's metadata distinguishes it from the original.
- The destination is checked and then opened, so a path swapped between those
  two steps is not covered. The check guards against writing through a symlink
  that is already there, not against a race for the path.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [Build documentation](../../../docs/build.md)
