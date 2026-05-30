# IccApplyNamedCmm

`iccApplyNamedCmm` applies one or more ICC/iccMAX profiles to text-based sample
data and writes transformed values to stdout.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccApplyNamedCmm
```

Example input data files are under `Testing/ApplyDataFiles/`.

## Named Color Overprint Variants

The JSON `transform` field for a NamedColor stage selects on two
axes that mirror the non-named `color` / `colorimetric` / `spectral`
triad, plus a `namedDevice` stem that reads the device-side member.
The **output-side stem** pins which array member the lookup reads;
the **overprint suffix** picks the spectral substrate variant
(applies only when the stem is `named` or `namedSpectral`):

| Surface | Over white | Over black | Over gray |
|---------|------------|------------|-----------|
| `transform` stem `named` — output side auto (legacy heuristic) | `"named"` | `"namedOnBlack"` | `"namedOnGray"` |
| `transform` stem `namedColorimetric` — pinned to `nmclPcsDataMbr` (suffix is a no-op) | `"namedColorimetric"` | `"namedColorimetricOnBlack"` | `"namedColorimetricOnGray"` |
| `transform` stem `namedSpectral` — pinned to the overprint spectral member | `"namedSpectral"` | `"namedSpectralOnBlack"` | `"namedSpectralOnGray"` |
| `transform` stem `namedDevice` — pinned to `nmclDeviceDataMbr` (no overprint suffix accepted) | `"namedDevice"` | — | — |
| Legacy CLI intent code (overprint only — stem implicit via `useD2BxB2Dx`) | base intent | `intent + 1000000` | `intent + 2000000` |

`named` is the auto / legacy stem: if `useD2BxB2Dx` is true and the
profile declares a `spectralPCS`, the named transform's output side is
the spectralPCS (reads the overprint-correct spectral member);
otherwise it is the profile's colorimetric PCS (reads `nmclPcsDataMbr`).
`namedColorimetric` pins reads to `nmclPcsDataMbr` and fails with
`icCmmStatBadTintXform` when an entry does not carry that member.
`namedSpectral` pins reads to the spectral array member selected by the
overprint suffix and fails with `icCmmStatBadTintXform` when that
member is absent. `namedDevice` pins reads to `nmclDeviceDataMbr` (or
the legacy `deviceCoords` struct field on a v4 `CIccTagNamedColor2`
profile) and sets the xform's destination to the profile's
`colorSpace`; it fails with `icCmmStatBadTintXform` when the device
member is absent. The overprint suffix is a no-op on
`namedColorimetric*` and not accepted on `namedDevice`, because neither
the PCS nor the device member varies by substrate.

Examples — three transforms that exercise the over-black variant of a
named color, differing in PCS-side stem:

```jsonc
// PCS-side auto (legacy heuristic; over-black selected via the
// overprint suffix).  Whether the lookup reads the spectral member
// or nmclPcsDataMbr depends on useD2BxB2Dx and whether spectralPCS
// is declared in the profile header.
{
  "iccFile": "Named/NamedColor.icc",
  "intent": "absolute",
  "transform": "namedOnBlack"
}

// Pinned colorimetric -- reads nmclPcsDataMbr only.  Fails if the
// matched entry has no nmclPcsDataMbr (e.g. the over-black 'spcb'
// member alone is not sufficient -- there is no spectral integration
// fall-back).
{
  "iccFile": "Named/NamedColor.icc",
  "intent": "absolute",
  "transform": "namedColorimetricOnBlack"
}

// Pinned spectral -- reads icSigNmclSpectralOverBlackMbr ('spcb').
// Equivalent to "namedOnBlack" + "useD2BxB2Dx": true but does not
// depend on the legacy heuristic.  Fails if 'spcb' is absent for
// the matched entry.
{
  "iccFile": "Named/NamedColor.icc",
  "intent": "absolute",
  "transform": "namedSpectralOnBlack"
}
```

```sh
# Legacy CLI: absolute (intent=3) + over-black (+1000000) = 1000003
iccApplyNamedCmm <data> 0 1 Named/NamedColor.icc 1000003
```

If the profile lacks the requested array member for any named color the
apply hits, `Apply` fails with `icCmmStatBadTintXform` rather than
silently falling back to a different member; this surfaces the
misconfiguration so it can be corrected.

Library API: direct CMM users can set the overprint by attaching a
`CIccCreateNamedColorXformHint` (with `nOverprintType` set to
`icNamedColorOverWhite`, `icNamedColorOverBlack`, or
`icNamedColorOverGray`) to the hint manager before calling
`CIccCmm::AddXform`, and choose the output-side axis via the `nLutType`
argument (`icXformLutNamedColor`, `icXformLutNamedColorimetric`,
`icXformLutNamedSpectral`, or `icXformLutNamedDevice`).

## Legacy Dump Format

When the destination is a pixel space, each input data row produces one
output line:

```
<applied values...>   ;{ "<source name>" }   <source tint>
```

The trailing `;{ "name" } tint` comment echoes the input back so the
mapping between source and applied value is unambiguous. The tint
token is the value supplied in the colorData `"v"` field (or the
trailing number on a legacy `{ "Silver" } 0.5`-style data row). An
absent `"v"` leaves the token off entirely and the apply path uses
tint=1.0 by default. Tint values of exactly 1.0 are still echoed
verbatim -- they used to be suppressed, which made the S5/S6/S7
overprint scenarios for the *Hybrid Printer With Overprint Simulation*
ICS look as though no tint had been supplied.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [Testing profiles](../../../Testing/Readme.md)
- [IccConnect library](../../../docs/icc-connect.md) — JSON-driven setup
  used by the `-cfg` form.
