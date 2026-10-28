# Native HEIF Regression Fixtures

These carriers exercise the libheif and IccProfLib-backed `iccHeifDump`.

| File | Expected native behavior |
|------|--------------------------|
| `valid-prof.heic` | One unrestricted ICC profile on item 1, property 1; `profile_bytes=540`, `proflib_parse=ok`, and `icc_profiles=1`. |
| `nclx-only.heif` | HEVC HEIF with `nclx` colour information and no ICC data; `icc_profiles=0`. |
| `invalid-ipma-index.heic` | `ipma` associates a nonexistent property index; exit 2 with `unable to read HEIF colour properties`. |

The valid carrier embeds a 540-byte profile with SHA-256
`de9b36ccc513fdc79860d5dff523aed9cf67411789e4c20ae866146cbaf75a4e`.

`.github/scripts/test_heif_output_safety.py` uses the valid carrier for the
native `iccdev.heif-output-safety` CTest. Python's
`python/tests/test_heif_cli.py` and MATLAB's
`matlab/tests/test_heif_dump_native.m` exercise all three carriers.
Release bundles preserve this repository-relative directory so the staged
MATLAB smoke test does not depend on the source checkout.
