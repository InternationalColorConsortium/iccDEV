# Temporary CFL Patch Stack

These source-only patches keep the maintainer ClusterFuzzLite lanes exploring
past known findings. They are not substitutes for normal source fixes.

The default applicator handles every patch independently. A patch that applies
is installed, a reverse-applicable patch is reported as already integrated, and
a drifted patch emits `[WARN]` and is skipped. The remaining patches and the
build continue. Use `--strict` only for local patch-stack maintenance, where a
missing, empty, or drifted stack must fail.

| Patch | Tracks | Temporary guard |
| --- | --- | --- |
| `005-issue-2704-pixel-buffer-initialization.patch` | #2704 | Zero-initialize heap-backed `CIccPixelBuf` storage. |
| `006-issue-2705-apply-scratch-initialization.patch` | #2705 | Zero-initialize CMM apply scratch and chunk buffers. |
| `007-mpe-curve-position-bounds.patch` | CFL follow-up | Reject MPE curve positions outside their enclosing element. |
| `009-issue-2707-calculator-temp-initialization.patch` | #2707 | Zero-initialize calculator temporary-channel storage. |

When a normal source fix lands, remove only its issue patch. Run these checks
after changing the inventory:

```bash
.github/scripts/iccdev-fuzz-patch-check-tests.sh
.github/scripts/check-fuzz-patches.sh
.github/scripts/iccdev-clusterfuzzlite-config-tests.sh
```
