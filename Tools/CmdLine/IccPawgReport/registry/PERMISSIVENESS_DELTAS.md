# IccPawgReport signature-validator permissiveness deltas

Snapshot of registry.color.org fetched **2026-06-20**. This report flags every
place where the iccPawgReport signature validator and the underlying IccProfLib
signature tables diverge from the public ICC registries. Per the issue #1459
plan these are **flag-and-wait**: nothing here is tightened automatically. Items
are grouped by whether they were resolved by the #1459 S3 fix or are deferred
for a maintainer decision (and, for the CMM items, a separate IccProfLib PR).

Generator/scope: see `generate_signature_registry.py`. Out of scope and not
encoded: patent registry, CMYK characterization data, ICC profile registry, the
three-component (RGB) color encoding registry, colorimetry data, profile library.

---

## A. Manufacturer / creator header fields - RESOLVED by the #1459 S3 fix

**Defect:** S3 validated the header `manufacturer` and `creator` fields against
the registered *private tag-signature ranges* (`FindRegisteredPrivateTag`) - the
wrong registry. ICC.1:2022-05 section 7.2.17 requires these fields to match a signature
in the **device manufacturer** section of the ICC signature registry (the
Manufacturer Signatures registry), or be zero.

**Magnitude:** of the **279** signatures in the Manufacturer Signatures registry,
**271** fall in *no* private tag-signature range - i.e. the old code would have
emitted a false "unregistered" S3 warning for 271 of 279 legitimately registered
manufacturers. Worked example from the issue: `KODA` (Kodak, `0x4B4F4441`) is in
the Manufacturer Signatures registry but in no private-tag range -> false warn
before, OK after.

**Fix:** new exact-match `kIccManufacturerSignatures[]` table + a dedicated
`IsRegisteredManufacturerOrZero()`; S3 now routes `manufacturer`/`creator`
through it. The private-tag table is unchanged and still (correctly) attributes
private tags found in a profile *body* (`AssessPrivateTags`, checks S11/C6).

**Permissiveness direction:** the fix makes S3 *more correct*, not blanket more
permissive - it accepts the 271 genuinely-registered manufacturers it used to
reject, while still warning on signatures absent from the registry (see section D).

---

## B. CMM signature field - RESOLVED by the issue-1724 IccProfLib update

S3's CMM check uses `CIccInfo::GetCmmSigName()` (IccProfLib/IccUtil.cpp) and
warns when it returns an "Unknown..." name. Issue #1724 carries the follow-up
IccProfLib update requested by the #1459 review: the core CMM enum, name table,
and profile validation allow-list now match the live CMM registry rows described
below. No PawgReport-local CMM table was added; S3 continues to consume the core
IccProfLib lookup.

### B1. Enum entries with no `GetCmmSigName()` case - fixed
The `icCmmSignature` enum (icProfileHeader.h, "as of Mar 6, 2018") defines these,
but `GetCmmSigName()` had no `case` for them, so S3 reported them Unknown. The
issue-1724 update adds both name-table cases and keeps the existing enum values:

| Enum | Value | ASCII |
|------|-------|-------|
| `icSigWindowsCMS`   | `0x57435320` | `WCS ` |
| `icSigOnyxGraphics` | `0x4F4E5958` | `ONYX` |

### B2. Live CMM registry entries absent from the IccProfLib enum - fixed
These registry.color.org/cmm-signatures entries were present in the live CMM
registry but absent from the enum snapshot. The issue-1724 update adds enum and
`GetCmmSigName()` coverage for all three, and `CIccProfile::Validate()` now
accepts `repr` and `iccd` as registered CMM signatures:

| Value | ASCII | Vendor |
|-------|-------|--------|
| `0x52494D58` | `RIMX` | ICC (RefIccMAX) |
| `0x69636364` | `iccd` | ICC |
| `0x72657072` | `repr` | Reprointelligence |

### B3. Latent value/comment mismatch - fixed
`icSigRefIccMAX = 0x52494343` (= ASCII `RICC`) but its comment says `/* 'RIMX' */`,
and the registry lists RefIccMAX's CMM signature as `RIMX = 0x52494D58`. The enum
*value* and the registry disagreed. The issue-1724 update changes
`icSigRefIccMAX` to `0x52494D58` (RIMX), matching the registry and the existing
comment.

**Permissiveness direction:** this is a targeted correction for registered CMM
signatures. Unknown or unregistered CMM signatures still warn.

---

## C. Private tag-signature ranges - refreshed (data only)

The private-tag range table was regenerated from registry.color.org/tag-signatures
(non-ICC rows). Prior in-source table: **171** ranges; live snapshot: **174**.
Differences are minor and data-only (the code path is unchanged):

- Adobe `AS00` narrowed from range `0x41533030-0x41533939` to the single
  signature `0x41533030` (registry now lists only `AS00`).
- X-Rite `gbd0` expanded from range `0x67626430-0x67626433` to four explicit
  single signatures `gbd0`,`gbd1`,`gbd2`,`gbd3`.

Net effect on attribution is negligible; no behavioral flag.

---

## D. Known still-warns (by design) - governance, not code

Some signatures appear in ICC-*published* and widely-used profiles yet are absent
from the online Manufacturer Signatures registry, so S3 correctly continues to
warn until the ICC adds/clarifies them. These require registry submission or a
policy decision (ICC governance), not a strictness change - tracked as **Segment A**
of #1459 (see `GOVERNANCE_SUBMISSIONS.md`). Found by validating a 95-profile local
corpus with the #1459-fixed tool:

| Signature | Value | Occurrences | Seen in | Status |
|-----------|-------|-------------|---------|--------|
| `XRCM` | `0x5852434D` | 16 | CRPC2..7 / GRACoL2013 / SWOP2013 / APTEC | submit to ICC |
| `ICC ` | `0x49434320` | 23 | ICC reference profiles (sRGB_D65_*), iccDEV fixtures | register/clarify consortium sig |
| `none` | `0x6E6F6E65` | 25 | AdobeRGB1998, ROMM-RGB, sRGB displayclass | policy: sanction placeholder vs. require zero |
| `LOGO` | `0x4C4F474F` | 2 | MCPPiPF5000Glossy | investigate/submit |
| `ccox` | `0x63636F78` | 1 | corpus | investigate/submit (likely CHROMiX) |

(`XRIT` = X-Rite `0x58524954` *is* registered and validates OK; `XRCM` is the
distinct unregistered one.)

**Code behavior (per #1459 review decision):** S3 still WARNs on all of the above
(spec-correct under section 7.2.17). For the two pervasive conventions `'none'` and
`'ICC '`, the WARN wording is *softened* (via `KnownUnregisteredConventionNote`)
to identify them as known placeholders/consortium signatures rather than suspect
data - but the verdict stays WARN and nothing is silently accepted. The other
sigs keep the plain "not in Manufacturer Signatures registry" wording.

---

## E. Phase P2 (live registry check) - noted, not implemented

A pluggable live check against registry.color.org is recorded as a possible
future direction only. The registries change extremely slowly, and a live query
imposes a network dependency on every report run and on CI; the design/operational
cost is judged not worth it now. Any move in this direction needs maintainer +
ICC debate. Documented here and in the PR for completeness; intentionally no code.
