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
| `CreateStandard(profiles, embeddedData=null, embeddedLen=0)` | `CIccCfgProfileSequence` | Standard pixel-pipeline CMM; if `embeddedData` is supplied and the first profile config has an empty `iccFile`, that buffer is loaded as the first xform (e.g. for ICC profiles embedded in TIFF/JPEG). |
| `CreateNamed(profiles, srcSpace=icSigUnknownData, bInputProfile=true)` | `CIccCfgProfileSequence` | Named-color CMM (`CIccNamedColorCmm`) for named-color workflows. |
| `CreateSearch(searchApply)` | `CIccCfgSearchApply` | Spectral inverse-search CMM (`CIccCmmSearch`) with weighted PCC attach and optional destination init profile. |
| `Attach(pCmm)` | Any `CIccCmm*` | Wraps an externally constructed CMM; the wrapper takes ownership. |

All factories return `nullptr` on any failure (profile load, hint allocation,
PCC load, `AddXform`, `Begin`). Loaded PCC profiles are released before
return whether the call succeeds or fails. Embedded raw profile bytes
passed to `CreateStandard` are not retained by the library.

### Typed accessors

```cpp
CIccCmm*           GetCmm()       const;   // wrapped CMM (never null after success)
CIccNamedColorCmm* GetNamedCmm()  const;   // non-null only for CreateNamed
CIccCmmSearch*     GetSearchCmm() const;   // non-null only for CreateSearch
icColorSpaceSignature GetSourceSpace() const;
icColorSpaceSignature GetDestSpace()   const;
```

`~CIccConnectCmm()` deletes the wrapped CMM, so callers own exactly one
object and free with `delete`.

### Optional Parallel Apply

After a successful factory call, threading can be enabled in place:

```cpp
bool EnableThreading(int nThreads = 0);   // 0 -> hardware_concurrency
bool IsThreaded() const;
```

`EnableThreading` wraps the underlying CMM with `CIccThreadedCmm` (see
[threading guide](icc-cmm-threading.md)) so subsequent multi-pixel
`Apply()` calls run in parallel. The wrapper assumes ownership of the
underlying CMM, so the single-owner contract of `CIccConnectCmm` is
preserved — callers still free with one `delete`.

Behaviour:

| Condition | Result |
|-----------|--------|
| `m_pCmm` is null | Returns `false`. |
| Already threaded | Returns `true`, no rewrap. |
| Attach fails | Underlying CMM is destroyed by `CIccThreadedCmm::Attach`, `m_pCmm` becomes null, returns `false`. The connect object should be deleted. |
| Success | `GetCmm()` returns the threaded wrapper; `IsThreaded()` returns `true`. |

After threading is enabled, `GetNamedCmm()` and `GetSearchCmm()` return
`nullptr` because the type-specific CMM is hidden behind the wrapper.
Striped threading is most useful for `CreateStandard` pixel pipelines; if
you need the named-color or search APIs, call them through the typed
accessor before enabling threading, or skip `EnableThreading` for those
factories.

For a worked example, see
[`Tools/CmdLine/IccApplyProfiles/iccApplyProfiles.cpp`](../Tools/CmdLine/IccApplyProfiles/iccApplyProfiles.cpp),
which builds the CMM with `CreateStandard`, snapshots the underlying
`CIccCmm*` for metadata queries the wrapper does not forward (e.g.
`GetLastParentSpace`), calls `EnableThreading(nThreads)`, then drives
row-batched `Apply(dst, src, nPixels)` calls. The `-threads` CLI flag
maps straight through to that single argument.

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

`pccFile` is opened once per profile entry, passed through as the PCC for
that xform, and deleted after `Begin()` returns.

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

CIccCfgApply cfg;
if (!cfg.fromJson(jsonText)) { /* parse error */ }

CIccConnectCmm *conn = CIccConnectCmm::CreateStandard(cfg.m_profiles);
if (!conn) { /* failed to build CMM */ }

// Optional: enable parallel apply.  Ownership stays with conn; the
// connect object's destructor frees the threaded wrapper, which in turn
// frees the underlying CMM.
if (!conn->EnableThreading(/*nThreads=*/0)) {
  delete conn;
  /* threading failed; connect is already empty */
}

conn->GetCmm()->Apply(dst, src, nPixels);
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
