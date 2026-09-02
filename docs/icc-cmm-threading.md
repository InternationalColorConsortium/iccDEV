# CIccThreadedCmm — Parallel CMM Apply

`CIccThreadedCmm` is a decorator over an existing `CIccCmm` that runs
multi-pixel `Apply()` calls in parallel by splitting the pixel buffer into
contiguous strips and processing them through persistent workers. It lives in
[IccProfLib/IccCmmThread.h](../IccProfLib/IccCmmThread.h) and is part of
`IccProfLib2`.

It is a drop-in wrapper: callers keep the `CIccCmm*` API; the threading is
internal.

## When to Use

- Bulk pixel processing where a single `Apply(dst, src, nPixels)` call covers
  many independent pixels (TIFF rows, full images, batched CGATS data, etc.).
- The wrapped CMM has already been fully configured (`AddXform` + `Begin()`).
- Single-pixel `Apply()` does not benefit; it is forwarded to one worker.

The requested thread count is a maximum. Calls below 1024 pixels use no more
than one active worker per 256 pixels; larger calls use one per 128 pixels.
This keeps short TIFF rows from paying more synchronization overhead than
transform work while retaining host-wide parallelism for wide rows.

## Construction Model

`CIccThreadedCmm` cannot be built incrementally. It wraps a CMM that is
already valid:

```cpp
CIccCmm *cmm = new CIccCmm(srcSpace, dstSpace, bInputProfile);
cmm->AddXform(...);
cmm->AddXform(...);
if (cmm->Begin() != icCmmStatOk) { delete cmm; /* handle error */ }

// nThreads = 0 -> std::thread::hardware_concurrency()
CIccThreadedCmm *tcmm = CIccThreadedCmm::Attach(cmm, /*nThreads=*/0,
                                                /*bDeleteCmm=*/true);
if (!tcmm) { /* cmm has already been deleted on failure */ }

tcmm->Apply(dst, src, nPixels);

delete tcmm;   // deletes the wrapped cmm when bDeleteCmm=true
```

`Attach()` parameters:

| Parameter | Meaning |
|-----------|---------|
| `pCmm` | A `Begin()`-ed `CIccCmm`. `Valid()` must return true. |
| `nThreads` | `0` selects `std::thread::hardware_concurrency()`; clamped to `>= 1`. |
| `bDeleteCmm` | Take ownership of `pCmm`. Also deletes `pCmm` if `Attach` fails. |

`AddXform()` is disabled on the wrapper — every overload returns
`icCmmStatBad`. Build the transform chain on the wrapped CMM before calling
`Attach()`.

## Apply Semantics

`CIccApplyThreadedCmm::Apply(dst, src, nPixels)` does the following:

1. Caps active workers by call size: one per 256 pixels below 1024 pixels,
   otherwise one per 128 pixels, never exceeding `nThreads`.
2. Splits the buffer into `nActive` contiguous strips, with the first
   `nPixels % nActive` strips receiving one extra pixel.
3. Queues `nActive - 1` strips to persistent worker threads; runs the final
   strip on the calling thread.
4. Collects results and returns the first non-OK status, if any.

`Apply(dst, src)` (single pixel) is forwarded to worker `[0]` without any
thread launches.

## Concurrency Rules

- **Per-apply-object**: a single `CIccApplyThreadedCmm` instance must not be
  used from more than one thread at a time. Internally each strip uses its
  own private `CIccApplyCmm`, but the apply object itself is not reentrant.
- **Across apply objects**: to apply from several threads simultaneously,
  call `tcmm->GetNewApplyCmm(status)` once per caller. Each returned object
  owns its own pool of worker apply objects.
- **No shared mutable xform state**: every worker allocates its own
  `CIccApplyCmm` from the wrapped CMM, so transforms that maintain
  per-apply scratch state (e.g. calculator stacks) stay isolated.

## Buffer Requirements

- `dst` must hold `nPixels * GetDestSamples()` floats.
- `src` must hold `nPixels * GetSourceSamples()` floats.
- Strip partitioning is contiguous, so callers do not need to align on any
  boundary; `dst` and `src` must not alias the same memory region.

## CLI Example: `iccApplyProfiles -threads`

[`iccApplyProfiles`](../Tools/CmdLine/IccApplyProfiles/iccApplyProfiles.cpp)
exposes the wrapper through a `-threads` flag. The value is forwarded
into [`CIccConnectCmm::CreateStandard`](icc-connect.md)'s `nThreads`
parameter; the connect factory installs the `CIccThreadedCmm` wrapper
when `nThreads != 1` and returns the underlying CMM otherwise.

```text
iccApplyProfiles -threads N -cfg config.json
```

| Value | Behaviour |
|-------|-----------|
| omitted | Defaults to `nThreads = 1` (no wrapper; underlying CMM is used directly). |
| `-threads 0` | Use `std::thread::hardware_concurrency()`, capped at `CIccThreadedCmm::GetMaxThreads()`. |
| `-threads 1` | No threaded wrapper. |
| `-threads N` (N > 1) | Allow up to `N` workers, subject to the per-call workload cap. |

The flag is parsed before `-cfg`, so it must come first.

## TIFF Performance Benchmark

Use the cross-platform benchmark helper to compare `-threads 1`, `2`, and `0`
with interleaved runs and bit-identical output checks:

```bash
python3 .github/scripts/iccdev-iccapplyprofiles-threading-benchmark.py \
  --build-dir out/clut-portable \
  --output out/iccapplyprofiles-threading.tsv
```

On Windows, pass a Release Visual Studio build directory such as
`out/windows-thread-baseline`. Keep the build type, compiler, profile chain,
storage location, and image sizes identical when comparing revisions. TIFF
decode and encode remain serial, so report results by image size rather than
assuming that host-max threads must win every workload.

## Failure Modes

`Attach()` returns `NULL` and (when `bDeleteCmm=true`) deletes the wrapped
CMM if any of these are true:

- `pCmm` is null or `pCmm->Valid()` is false (typically `Begin()` was not
  called or failed).
- Worker allocation fails for the default apply object.

Strip workers preserve the first non-OK status across the parallel apply.
Subsequent strips still run; partial output in `dst` for failing strips is
undefined.

## Use From IccConnect

When a CMM is built through [`CIccConnectCmm`](icc-connect.md), pass
`nThreads` into `CreateStandard` directly — the factory installs the
wrapper internally and `CIccConnectCmm` keeps single ownership of the
whole stack:

```cpp
std::string sError;
CIccConnectCmm *conn = CIccConnectCmm::CreateStandard(
    cfg.m_profiles,
    /*pEmbeddedData=*/nullptr, /*nEmbeddedLen=*/0,
    /*nThreads=*/0,                  // 0 = hardware_concurrency, 1 = no wrapper
    &sError);
if (!conn) {
  fprintf(stderr, "%s\n", sError.c_str());
  return -1;
}

conn->GetCmm()->Apply(dst, src, nPixels);   // threaded when nThreads != 1
delete conn;                                 // tears down wrapper + underlying CMM
```

`conn->IsThreaded()` reports whether the wrapper was installed;
`conn->GetThreadedCmm()` returns the wrapper directly when needed.
`GetNamedCmm()` returns null through a threaded wrapper, while
`GetSearchCmm()` unwraps a threaded search CMM for search-specific
inspection. `CreateSearch(searchApply, error, nThreads)` accepts the same
`0` automatic, `1` scalar, and `2` through `CIccThreadedCmm::GetMaxThreads()`
(currently 256) threaded selection; higher values are rejected.

## See Also

- [IccConnect library](icc-connect.md) — factory that constructs a
  `Begin()`-ed CMM from JSON config, with an `nThreads` parameter on
  `CreateStandard` for parallel apply.
- [CLI tool reference](tools-cli-reference.md) — shared option tables for
  the `iccApply*` tools.
