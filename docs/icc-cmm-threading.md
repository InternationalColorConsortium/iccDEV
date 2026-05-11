# CIccThreadedCmm — Parallel CMM Apply

`CIccThreadedCmm` is a decorator over an existing `CIccCmm` that runs
multi-pixel `Apply()` calls in parallel by splitting the pixel buffer into
contiguous strips and processing each strip on its own worker. It lives in
[IccProfLib/IccCmmThread.h](../IccProfLib/IccCmmThread.h) and is part of
`IccProfLib2`.

It is a drop-in wrapper: callers keep the `CIccCmm*` API; the threading is
internal.

## When to Use

- Bulk pixel processing where a single `Apply(dst, src, nPixels)` call covers
  many independent pixels (TIFF rows, full images, batched CGATS data, etc.).
- The wrapped CMM has already been fully configured (`AddXform` + `Begin()`).
- Single-pixel `Apply()` does not benefit; it is forwarded to one worker.

Threading is unhelpful (and skipped automatically) when `nPixels` is smaller
than the requested thread count.

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

1. Caps the active worker count at `min(nPixels, nThreads)`.
2. Splits the buffer into `nActive` contiguous strips, with the first
   `nPixels % nActive` strips receiving one extra pixel.
3. Launches `nActive - 1` strips via `std::async(std::launch::async)`; runs
   the final strip on the calling thread to overlap with the launches.
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
exposes the wrapper through a `-threads` flag. Internally it builds the
CMM with [`CIccConnectCmm::CreateStandard`](icc-connect.md) and then calls
`pConnect->EnableThreading(nThreads)` — this is the canonical example of
the connect-side threading pattern, including the row-batched float
buffer that feeds `Apply(dst, src, nPixels)`.

```text
iccApplyProfiles -threads N -cfg config.json
```

| Value | Behaviour |
|-------|-----------|
| omitted | Defaults to `nThreads = 0` (hardware concurrency). |
| `-threads 0` | Use `std::thread::hardware_concurrency()`. |
| `-threads 1` | Threaded code path with a single worker (row-batched apply, no concurrency). |
| `-threads N` (N > 1) | Use exactly `N` workers. |
| `-threads -1` | Disable threading; per-pixel apply on the unwrapped CMM. |

The flag is parsed before `-cfg`, so it must come first.

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

When a CMM has been built through [`CIccConnectCmm`](icc-connect.md),
call `EnableThreading(nThreads)` instead of attaching directly. The
connect object keeps single ownership and rewraps `m_pCmm` for you, so
there is no ownership coordination between two wrappers:

```cpp
CIccConnectCmm *conn = CIccConnectCmm::CreateStandard(cfg.m_profiles);
if (!conn) { /* error */ }
if (!conn->EnableThreading(0)) { delete conn; /* error */ }
conn->GetCmm()->Apply(dst, src, nPixels);
delete conn;   // tears down the threaded wrapper and the underlying CMM
```

`conn->IsThreaded()` reports whether the wrapper has been installed.
After threading is on, the type-specific accessors `GetNamedCmm()` and
`GetSearchCmm()` return null — striped apply only fits the standard
pixel-pipeline use case.

## See Also

- [IccConnect library](icc-connect.md) — factory that constructs a
  `Begin()`-ed CMM from JSON config, with `EnableThreading()` for
  in-place parallel apply.
- [CLI tool reference](tools-cli-reference.md) — shared option tables for
  the `iccApply*` tools.
