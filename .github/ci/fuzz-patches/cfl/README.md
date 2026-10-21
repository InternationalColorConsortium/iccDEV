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
| `007-mpe-curve-position-bounds.patch` | CFL follow-up | Reject MPE curve positions outside their enclosing element. |
| `008-pawg-report-detail-bounds.patch` | PAWG slow unit `dd41634224392c7e0e1a23a899a201df6a249978` | Replace the quadratic shared-tag-region scan with a keyed lookup and bound repeated private/compression evidence to 16 examples while retaining exact counts. |
| `009-pawg-calculator-quality-budget.patch` | PAWG slow unit `ed262b33075a5afbb00d79b122e7861fdf92a914` | Estimate selector-width calculator work and classify Q1/Q3 as not applicable before repeated CMM evaluation would exceed the quality-metric budget. |

When a normal source fix lands, remove only its issue patch. Run these checks
after changing the inventory:

```bash
.github/scripts/iccdev-fuzz-patch-check-tests.sh
.github/scripts/check-fuzz-patches.sh
.github/scripts/iccdev-clusterfuzzlite-config-tests.sh
```
