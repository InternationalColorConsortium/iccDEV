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

## B. CMM signature field

S3's CMM check uses `CIccInfo::GetCmmSigName()` (IccProfLib/IccUtil.cpp) and
warns when it returns an "Unknown..." name. Two rounds of IccProfLib work bring
the core CMM enum, name table, and profile-validation allow-list into agreement
with the live CMM registry. No PawgReport-local CMM table was added; S3 continues
to consume the core IccProfLib lookup.

### B1/B2/B3 - RESOLVED by PR #1473 (enum + name table)

PR #1473 ("Reconcile icCmmSignature enum + GetCmmSigName with the CMM registry")
already reconciled the enum values and `GetCmmSigName()` name-table cases with
registry.color.org/cmm-signatures. Recorded here for completeness; **no code for
these rows ships in the issue-1724 change** - they were verified present in
`master` before this report:

- **B1 - name-table cases added.** `icSigWindowsCMS` (`0x57435320` `WCS `) and
  `icSigOnyxGraphics` (`0x4F4E5958` `ONYX`) had enum entries but no
  `GetCmmSigName()` `case`; #1473 added the names.
- **B2 - registry entries added to the enum + names.** `RIMX` (`0x52494D58`,
  ICC/RefIccMAX), `iccd` (`0x69636364`, ICC) and `repr` (`0x72657072`,
  Reprointelligence) were absent from the enum snapshot; #1473 added enum values
  and `GetCmmSigName()` coverage for all three.
- **B3 - latent value/comment mismatch corrected.** `icSigRefIccMAX` had value
  `0x52494343` (ASCII `RICC`) while its comment and the registry said
  `RIMX = 0x52494D58`; #1473 changed the value to `0x52494D58` to match.

### B4. Validation allow-list catch-up - the issue-1724 change

#1473 named `repr`/`iccd` but did **not** add them to the CMM allow-list in
`CIccProfile::CheckHeader()`, so `CIccProfile::Validate()` still emitted
"Unregistered CMM signature" for the two (now-named) sigs. The issue-1724 change
is exactly this two-line catch-up: it adds `case icSigReprointelligence:` and
`case icSigICC:` to that switch so `repr` and `iccd` validate cleanly, matching
the names #1473 gave them. (`WCS `/`ONYX`/`RIMX`/`ICCD` were already allow-listed.)
All four registry memberships were re-verified against
registry.color.org/cmm-signatures on 2026-07-21: `repr`, `iccd`, `RIMX`, `WCS `
and `ONYX` are registered; `MSFT` is **not** a registered CMM (it is a platform
signature), so `icSigMicrosoftCMM` is deliberately left out of the allow-list and
still warns.

**Permissiveness direction:** this is a targeted correction for registered CMM
signatures. Unknown or unregistered CMM signatures still warn.

### B5. Two more value/comment mismatches - the issue-2098 change

B3 was not the only instance of its class. Decoding every `0x`-literal signature
enum in `icProfileHeader.h` against its own quoted comment (437 enums) found two
further CMM rows whose value disagreed with the comment and with the registry:

- `icSigLogoSync` held `0x44676F53` (ASCII `DgoS`); registry: `LgoS = 0x4C676F53`
  (GretagMacbeth).
- `icSigKonicaMinolta` held `0x4D434D44` (ASCII `MCMD`); registry:
  `MCML = 0x4D434D4C` (Konica Minolta). That line also carried an inline question
  - "actually this is 'MCMD' - which is right? Sent email to Dr. Phil Green" -
  which the live registry answers.

Both were corrected the same way as B3: the value changed, the comment's
registered string kept. Re-verified against registry.color.org/cmm-signatures on
2026-08-11.

A third row, `icSigVivo`, needed no value change - `0x7669766F` is correct - but
its comment read `'VIVO'`. Corrected to `'vivo'`. Note the **manufacturer**
registry separately lists Vivo as `VIVO = 0x5649564F`; that is a different
registry and a different header field, so the two do not conflict, and only the
lower-case spelling applies to the CMM enum.

**Permissiveness direction:** two-way, and this is the point. Before the change a
profile carrying the *registered* `LgoS`/`MCML` was reported "Unregistered CMM
signature", while one carrying the unregistered `DgoS`/`MCMD` validated clean and
was given the registered CMM's name. Both directions are now pinned by
`iccdev.cmm-registry-allowlist`, which drives the allow-list with literal
registered hex plus `static_assert`s on the enum constants - the pre-existing
checks in that test reach the allow-list through enum *names* and so could not
detect a wrong value.

No corpus impact: all 105 tracked `.icc`/`.icm` profiles were scanned and none
carry any of the four values.

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
| `none` | `0x6E6F6E65` | 25 | AdobeRGB1998, ROMM-RGB, sRGB displayclass | **ruled 2026-07-24 (#1472): spec says `00h`, not registrable** |
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

**Amended 2026-07-24 by the ICC ruling on #1472:** the two softened cases are no
longer worded alike, because they differ in whether registration is even possible.
`'none'` is **not registrable** (the spec requires `00h`), so its detail states that
and must not say "not yet in the registry", which would imply a pending submission.
`'ICC '` is undecided rather than refused, so it keeps the "not yet in ... snapshot"
wording. Each case therefore supplies its own complete parenthetical.

---

## E. Phase P2 (live registry check) - noted, not implemented

A pluggable live check against registry.color.org is recorded as a possible
future direction only. The registries change extremely slowly, and a live query
imposes a network dependency on every report run and on CI; the design/operational
cost is judged not worth it now. Any move in this direction needs maintainer +
ICC debate. Documented here and in the PR for completeness; intentionally no code.
