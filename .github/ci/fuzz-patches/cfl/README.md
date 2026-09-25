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
| `001-issue-2686-curve-gamma.patch` | #2686 | Derive the one-entry curve gamma from initialized curve storage. |
| `002-issue-2688-colorant-table-pcs.patch` | #2688 | Initialize and propagate colorant-table PCS state. |
| `003-issue-2699-mpe-buffer-channels.patch` | #2699 | Copy the MPE scratch-channel count. |
| `004-issue-2703-mpe-buffer-channels.patch` | #2703 | Same root fix as #2699, kept separate so either issue patch can retire independently. |
| `005-issue-2704-pixel-buffer-initialization.patch` | #2704 | Zero-initialize heap-backed `CIccPixelBuf` storage. |
| `006-issue-2705-apply-scratch-initialization.patch` | #2705 | Zero-initialize CMM apply scratch and chunk buffers. |
| `007-mpe-curve-position-bounds.patch` | CFL follow-up | Reject MPE curve positions outside their enclosing element. |

When a normal source fix lands, remove only its issue patch. If #2699 or #2703
lands first, the duplicate shared-root patch remains independently applicable;
if the common fix lands, both report as already integrated until they are
removed. Run these checks after changing the inventory:

```bash
.github/scripts/iccdev-fuzz-patch-check-tests.sh
.github/scripts/check-fuzz-patches.sh
.github/scripts/iccdev-clusterfuzzlite-config-tests.sh
```
