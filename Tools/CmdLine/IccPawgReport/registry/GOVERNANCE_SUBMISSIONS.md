# Segment A (governance): signatures to reconcile with registry.color.org

This is the issue-#1459 **governance** track. The non-governance fix (the S3
manufacturer/creator registry correction) is handled separately; the items below
**cannot** be resolved in code — they require the ICC to add/clarify entries in
the public Manufacturer Signatures registry (registry.color.org). Until then S3
correctly continues to WARN on them (for the well-known conventions with softened
wording; see the validator's `KnownUnregisteredConventionNote`).

Spec basis: **ICC.1:2022-05 §7.2.17** — the profile header `manufacturer` and
`creator` fields shall match a signature in the device manufacturer section of
the ICC signature registry, or be zero.

## Evidence

Found by validating a 95-profile local corpus (ICC-published + vendor profiles)
with the #1459-fixed iccPawgReport. Distinct header signatures that are neither
zero nor present in the Manufacturer Signatures registry snapshot (2026-06-20):

| Signature | Hex | Occurrences | Seen in (examples) | Disposition |
|-----------|-----|-------------|--------------------|-------------|
| `XRCM` | `0x5852434D` | 16 | `CGATS21_CRPC2..7`, `GRACoL2013*`, `SWOP2013C3_CRPC5`, `APTEC_PC1x_*` | **Submit** — creator of the entire ICC CRPC/GRACoL/SWOP/APTEC characterization-profile family; clearly an ICC/X-Rite production signature missing from the public list |
| `ICC ` | `0x49434320` | 23 | `sRGB_D65_colorimetric`, `sRGB_D65_MAT` (and iccDEV regression fixtures) | **Clarify/Register** — the ICC's own reference-profile signature. Registry lists `SICC` and `iccd` for the consortium but not `'ICC '`. Either register `'ICC '` or document the canonical consortium signature |
| `none` | `0x6E6F6E65` | 25 | `AdobeRGB1998`, `ISO22028-2_ROMM-RGB`, `sRGB_v4_ICC_preference_displayclass` | **RULED 2026-07-24 (issue #1472) — not registrable.** The ICC ruled that the specification requires `00h` here, so `'none'` is a producer error rather than a placeholder awaiting registration. S3 still WARNs, and its detail must not describe `'none'` as "not yet" registered |
| `LOGO` | `0x4C4F474F` | 2 | `MCPPiPF5000Glossy` | **Investigate/Submit** — likely Logo/GretagMacbeth-era tooling; confirm owner and submit |
| `ccox` | `0x63636F78` | 1 | (corpus) | **Investigate/Submit** — probably CHROMiX (registry has `CMiX` `0x434D6958`); confirm and submit or map |

Related CMM-field observations (separate from manufacturer/creator, noted for the
maintainers; tracked in `PERMISSIVENESS_DELTAS.md` §B as an IccProfLib change):

- `APPL` (`0x4150504C`, uppercase) used in the **CMM** field of `SNAP2007`,
  `MCPPiPF5000Glossy` — this is the *platform* signature, not Apple's registered
  CMM `appl` (`0x6170706C`); the profiles appear to misuse the field.
- `appl` (`0x6170706C`, lowercase) appears once as a *manufacturer* — it is in the
  **CMM** registry but not the **Manufacturer** registry.

## Requested ICC actions

1. Register `XRCM` (and confirm its owner — X-Rite production?) so the ICC's own
   published CRPC/GRACoL/SWOP/APTEC profiles validate cleanly.
2. Register or canonically document the `'ICC '` consortium signature.
3. ~~Decide policy on the `'none'` placeholder (sanction vs. recommend zero).~~
   **Answered 2026-07-24 (issue #1472): the specification says `00h`, so `'none'`
   is not registrable and producers should emit the zero signature.**
4. Confirm and register `LOGO` and `ccox` (or provide the correct mappings).

## What the code does in the meantime

- S3 keeps WARNing on all of the above (spec-correct).
- For `'none'` and `'ICC '` the WARN wording is softened to identify them as known
  conventions rather than suspect/unknown data (per the #1459 review decision).
- No signature is silently accepted; nothing here changes validator strictness.
