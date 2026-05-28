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

When a v5 NamedColor profile carries the optional over-black or over-gray
spectral array members (`icSigNmclSpectralOverBlackMbr 'spcb'` and
`icSigNmclSpectralOverGrayMbr 'spcg'`), iccApplyNamedCmm can read those
alternates in place of the default over-white member (`'spec'`). This
exposes the metameric difference between the named color printed over a
white substrate, over a black substrate, or over a gray substrate.

Two ways to select the variant:

| Surface | Default (over white) | Over black | Over gray |
|---------|---------------------|------------|-----------|
| JSON `transform` field | `"named"` | `"namedOnBlack"` | `"namedOnGray"` |
| Legacy CLI intent code | base intent | `intent + 1000000` | `intent + 2000000` |

Examples — both yield the same chain stage:

```jsonc
// JSON config (CIccCfgProfile entry)
{
  "iccFile": "Named/NamedColorWithOverPrints.icc",
  "intent": "absolute",
  "transform": "namedOnBlack"
}
```

```sh
# Legacy CLI: absolute (intent=3) + over-black (+1000000) = 1000003
iccApplyNamedCmm <data> 0 1 Named/NamedColorWithOverPrints.icc 1000003
```

The selection only affects the spectral apply path
(`CIccArrayNamedColor::GetSpectralTint`). If the profile lacks the
requested member the apply fails with `icCmmStatBadTintXform` rather than
silently falling back to the over-white member; this surfaces the
misconfiguration so it can be corrected. Library API: direct CMM users
can set the overprint by attaching a `CIccCreateNamedColorXformHint`
(with `nOverprintType` set to `icNamedColorOverWhite`,
`icNamedColorOverBlack`, or `icNamedColorOverGray`) to the hint manager
before calling `CIccCmm::AddXform`.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [Testing profiles](../../../Testing/Readme.md)
- [IccConnect library](../../../docs/icc-connect.md) — JSON-driven setup
  used by the `-cfg` form.
