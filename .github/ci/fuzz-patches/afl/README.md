# AFL Local Patch Stack

Place maintainer-local AFL fuzzing patches here as ordered `*.patch` files.

These patches are experimental fuzz-run stabilizers, not source changes being
proposed for direct merge from this directory. They let maintainers compare raw
and patched fuzz behavior on branches such as `ci-afl-cfl`; durable fixes still
need normal review as source changes in the affected component.

`.github/scripts/iccdev-afl-smoke.sh --patches` applies this directory by
default through `.github/scripts/iccdev-apply-fuzz-patches.sh --mode afl`.

Keep patches small, documented in the commit message, and suitable for local
validation only. Upstream source fixes should be landed as normal source
changes once they are stable.

Current patches:

- `001-json-config-parser-no-sanitize.patch` rejects structurally unbalanced
  JSON config inputs before dependency parsing and keeps parser inputs from
  turning dependency-internal integer sanitizer reports into AFL crashes.
- `002-jpegdump-segment-bounds.patch` uses subtraction-based JPEG segment
  bounds checks before reading marker length bytes.
- `003-applyprofiles-cam-encoding-div-zero.patch` guards malformed CAM inverse
  and encoding surround-ratio denominators found by `iccApplyProfiles` AFL
  replays.
- `004-applyprofiles-tiff-sample-count-bounds.patch` constrains malformed TIFF
  sample counts before `iccApplyProfiles` replay paths can allocate or iterate
  over unbounded sample planes.
- `005-applytolink-bpc-degenerate-lrange.patch` rejects degenerate BPC
  destination L* ranges before `iccApplyToLink` can divide by `MaxL - MinL`
  during quadratic black-point normalization.
