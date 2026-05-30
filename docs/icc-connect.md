# IccConnect Library

`IccConnect` (target `IccConnect2`, static variant `IccConnect2-static`) is a
small library that builds fully initialized `CIccCmm` transform objects from
JSON-driven configuration. It exists so command-line tools and embedders
share one path for profile loading, hint setup, Profile Connection Conditions
(PCC) lifetime, and `Begin()`. Sources live in
[IccConnect/IccLibConnect](../IccConnect/IccLibConnect).

The library is intentionally narrow:

| Header | Purpose |
|--------|---------|
| [IccCmmConfig.h](../IccConnect/IccLibConnect/IccCmmConfig.h) | C++ config objects (`CIccCfgProfile`, `CIccCfgProfileSequence`, `CIccCfgSearchApply`, …) and JSON / argv loaders. |
| [IccJsonUtil.h](../IccConnect/IccLibConnect/IccJsonUtil.h) | nlohmann/json helpers shared by the config objects. |
| [IccConnect.h](../IccConnect/IccLibConnect/IccConnect.h) | `CIccConnectCmm` factory that turns a config into a `Begin()`-ed CMM. |

The JSON shape consumed by `IccCmmConfig` is described by
[icc-connect-config.schema.json](icc-connect-config.schema.json).

## Build and Linkage

`IccConnect2` is built from [Build/Cmake/IccConnect/CMakeLists.txt](../Build/Cmake/IccConnect/CMakeLists.txt)
and is enabled by the same `ENABLE_SHARED_LIBS` / `ENABLE_STATIC_LIBS`
options used by `IccProfLib`. It depends on:

- `IccProfLib2` (or `IccProfLib2-static`)
- `nlohmann_json::nlohmann_json`

Public headers are installed under
`<install-prefix>/include/<target-folder>/IccConnect2/` when
`ENABLE_INSTALL_RIM` is on. The version header
`IccLibConnectVer.h` is generated at configure time.

## `CIccConnectCmm` Factory

`CIccConnectCmm` owns one wrapped `CIccCmm*` and exposes typed accessors for
the underlying CMM. There is no public constructor; instances come from
factory methods that load profiles, register hints, attach PCCs, and call
`Begin()` for you.

### Factory methods

| Factory | Input | Use case |
|---------|-------|----------|
| `CreateStandard(profiles, embeddedData=null, embeddedLen=0, nThreads=1, pErrorMsg=nullptr)` | `CIccCfgProfileSequence` | Standard pixel-pipeline CMM; if `embeddedData` is supplied and the first profile config has an empty `iccFile`, that buffer is loaded as the first xform (e.g. for ICC profiles embedded in TIFF/JPEG). When `nThreads != 1`, the returned wrapper is a `CIccThreadedCmm` over the underlying CMM. |
| `CreateNamed(profiles, srcSpace=icSigUnknownData, bInputProfile=true, pErrorMsg=nullptr)` | `CIccCfgProfileSequence` | Named-color CMM (`CIccNamedColorCmm`) for named-color workflows. |
| `CreateSearch(searchApply, pErrorMsg=nullptr)` | `CIccCfgSearchApply` | Spectral inverse-search CMM (`CIccCmmSearch`) with weighted PCC attach and optional destination init profile. |
| `Attach(pCmm)` | Any `CIccCmm*` | Wraps an externally constructed CMM; the wrapper takes ownership. |

All factories return `nullptr` on any failure (profile load, hint allocation,
PCC load, `AddXform`, `Begin`). Loaded PCC profiles are released before
return whether the call succeeds or fails. Embedded raw profile bytes
passed to `CreateStandard` are not retained by the library.

### Error Reporting

The library never writes to `stderr`. To learn *why* a factory call
returned `nullptr`, pass a `std::string*` as the final `pErrorMsg`
argument. On failure it is populated with a single human-readable line
describing the first failure encountered, prefixed where applicable with
the failing stage index. Examples:

```text
stage 0: unable to open PCC profile 'ICC\Missing.icc' for embedded source profile
stage 1: unable to open ICC profile 'Data\NotThere.icc'
stage 2: AddXform failed for 'foo.icc' (status 4)
Begin() failed (status 14); profile chain is incompatible (srcSpace=0x434d594b dstSpace=0x52474220)
weighted PCC 0: unable to open or read PCC tags from 'pcc.icc'
initial-destination: unable to open ICC profile 'final.icc'
```

The caller decides whether to log the message, surface it in a GUI, or
discard it. Passing `nullptr` (the default) silently drops the message.

### Typed accessors

```cpp
CIccCmm*           GetCmm()         const;   // wrapped CMM (never null after success)
CIccNamedColorCmm* GetNamedCmm()    const;   // non-null only for CreateNamed
CIccCmmSearch*     GetSearchCmm()   const;   // non-null only for CreateSearch
CIccThreadedCmm*   GetThreadedCmm() const;   // non-null when CreateStandard wrapped with nThreads != 1
bool               IsThreaded()     const;   // shorthand for GetThreadedCmm() != nullptr
icColorSpaceSignature GetSourceSpace() const;
icColorSpaceSignature GetDestSpace()   const;
```

`~CIccConnectCmm()` deletes the wrapped CMM, so callers own exactly one
object and free with `delete`.

### Parallel Apply

Threading is selected at construction time via the `nThreads` argument to
`CreateStandard`. When `nThreads == 0` the factory uses
`std::thread::hardware_concurrency()`; when `nThreads > 1` it uses that
many workers; when `nThreads == 1` the underlying CMM is returned without
the threaded wrapper. See the [threading guide](icc-cmm-threading.md)
for the apply-time semantics.

Because the threaded wrapper hides the type-specific CMM,
`GetNamedCmm()` and `GetSearchCmm()` return `nullptr` once a threaded
wrapper is in place. `GetThreadedCmm()` and `IsThreaded()` expose the
wrapper directly when needed.

For a worked example, see
[`Tools/CmdLine/IccApplyProfiles/iccApplyProfiles.cpp`](../Tools/CmdLine/IccApplyProfiles/iccApplyProfiles.cpp),
which calls `CreateStandard(cfgProfiles, pEmbedded, nEmbeddedLen,
cfgConnect.m_nThreads, &sConnectError)` and drives row-batched
`Apply(dst, src, nPixels)` calls. The `-threads` CLI flag maps straight
through to the `nThreads` argument.

## Per-profile Hints Applied

`AddXformFromConfig` translates each `CIccCfgProfile` entry to an `AddXform`
call. The following hints are registered automatically when the config
requests them:

| Config flag | Hint installed |
|-------------|----------------|
| `useBPC` | `CIccApplyBPCHint` |
| `adjustPcsLuminance` | `CIccLuminanceMatchingHint` |
| `iccEnvVars` (non-empty) | `CIccCmmEnvVarHint` |
| `pccEnvVars` (non-empty) | `CIccCmmPccEnvVarHint` |
| `transform` ∈ any `named*` variant (`named`, `namedOnBlack`, `namedOnGray`, `namedColorimetric{,OnBlack,OnGray}`, `namedSpectral{,OnBlack,OnGray}`, `namedDevice`) *or* overprint set via the legacy CLI high-bit | `CIccCreateNamedColorXformHint` with `nOverprintType` set from the JSON `transform` name (or from the legacy CLI's `+1000000`/`+2000000` high-bit) |

`pccFile` is opened once per profile entry, passed through as the PCC for
that xform, and deleted after `Begin()` returns.

### NamedColor Transform Variants

The JSON `transform` field for a NamedColor stage has two independent
axes that mirror the non-named `color` / `colorimetric` / `spectral`
triad, plus a `namedDevice` stem for reading the device-side member.

**Output-side axis** — picks which array member the named lookup
reads (and, equivalently, which space the xform exposes downstream):

| Stem | Member read | Output-side space | Behaviour when the member is absent for an entry |
|------|-------------|-------------------|--------------------------------------------------|
| `named` | Auto-selected | Auto-selected | Heuristic: if `useD2BxB2Dx` is true and the profile declares `spectralPCS`, the xform's output space is `spectralPCS` (reads the spectral member); otherwise it is the profile's colorimetric PCS (reads `nmclPcsDataMbr`). Preserves legacy behaviour. |
| `namedColorimetric` | `nmclPcsDataMbr` | Profile's PCS (XYZ/Lab) | Fail with `icCmmStatBadTintXform`. No silent fall-back to spectral or device. |
| `namedSpectral` | Spectral member (selected by the overprint axis below) | Profile's `spectralPCS` | Fail with `icCmmStatBadTintXform`. No silent fall-back to the over-white member or to `nmclPcsDataMbr`. |
| `namedDevice` | `nmclDeviceDataMbr` (v5 array) or `deviceCoords` struct field (v4 `CIccTagNamedColor2`) | Profile's `colorSpace` | Fail with `icCmmStatBadTintXform`. No silent fall-back to PCS or spectral. |

**Overprint axis** — only meaningful on the spectral path. The suffix
selects which spectral array member is read (neither `nmclPcsDataMbr`
nor `nmclDeviceDataMbr` varies by substrate, so the suffix is a no-op
on `namedColorimetric*` and is not accepted on `namedDevice`):

| Suffix | Spectral array member | Maps to `icNamedColorOverprintType` |
|--------|-----------------------|-------------------------------------|
| (none) | `icSigNmclSpectralDataMbr` ('spec', over-white) | `icNamedColorOverWhite` |
| `OnBlack` | `icSigNmclSpectralOverBlackMbr` ('spcb') | `icNamedColorOverBlack` |
| `OnGray` | `icSigNmclSpectralOverGrayMbr` ('spcg') | `icNamedColorOverGray` |

The two axes combine into the following set of JSON `transform` values:
`named`, `namedOnBlack`, `namedOnGray`, `namedColorimetric`,
`namedColorimetricOnBlack`, `namedColorimetricOnGray`, `namedSpectral`,
`namedSpectralOnBlack`, `namedSpectralOnGray`, `namedDevice`.

The legacy argv form (`CIccCfgProfileSequence::fromArgs`) carries only
the overprint axis, in the millions digit of the intent code:
`intent + 1000000` → over-black; `intent + 2000000` → over-gray;
anything else stays at over-white. The output-side axis is JSON-only.

If the profile lacks the requested array member for any named color
encountered during apply (output-side stem or overprint suffix), `Apply`
fails with `icCmmStatBadTintXform` rather than silently falling back to
another member. This surfaces misconfigurations.
See [the iccApplyNamedCmm Readme](../Tools/CmdLine/IccApplyNamedCmm/Readme.md#named-color-overprint-variants)
for end-to-end usage examples.

## JSON Configuration

The library does not parse a top-level "connect document"; callers populate
a config object (typically `CIccCfgApply`, `CIccCfgImageApply`, or
`CIccCfgSearchApply`) from JSON or argv, then hand its profile sequence to
the appropriate factory. See:

- [icc-connect-config.schema.json](icc-connect-config.schema.json) — full
  schema for the config objects in `IccCmmConfig.h`.
- [Tools/CmdLine/IccApplyNamedCmm](../Tools/CmdLine/IccApplyNamedCmm) and
  the other `iccApply*` tools — concrete usage of `-cfg config.json` plus
  argv-based fallback.

## Typical Pipeline

```cpp
#include "IccConnect.h"
#include <string>

CIccCfgApply cfg;
if (!cfg.fromJson(jsonText)) { /* parse error */ }

std::string sError;
CIccConnectCmm *conn = CIccConnectCmm::CreateStandard(
    cfg.m_profiles,
    /*pEmbeddedData=*/nullptr, /*nEmbeddedLen=*/0,
    /*nThreads=*/0,                  // 0 = hardware_concurrency, 1 = no wrapper
    &sError);

if (!conn) {
  // sError carries a single human-readable line describing the first
  // failure (e.g. "stage 1: unable to open ICC profile 'foo.icc'").
  fprintf(stderr, "%s\n", sError.c_str());
  return -1;
}

conn->GetCmm()->Apply(dst, src, nPixels);   // threaded path when nThreads != 1
delete conn;
```

See [CIccThreadedCmm](icc-cmm-threading.md) for the parallel-apply
decorator and the ownership pattern used by `iccApplyProfiles`.

## See Also

- [icc-connect-config.schema.json](icc-connect-config.schema.json) — JSON
  Schema for IccConnect configuration objects.
- [CIccThreadedCmm](icc-cmm-threading.md) — parallel apply over a
  `Begin()`-ed CMM.
- [CLI tool reference](tools-cli-reference.md) — `iccApply*` tools that use
  this library.
