## HEIF ICC QA Test Files (PoC)

These files exercise `colr` properties through the Nokia HEIF reader & `iccHeifDump` tool.

- `valid-prof.heic`: HEVC HEIF with a valid unrestricted ICC profile
- `valid-rICC.heic`: Same carrier and profile with restricted ICC colour type
- `valid-prof.avif`: AV1 AVIF with the same valid ICC profile
- `malformed-prof.heic`: HEVC HEIF containing `dtob-zero-channels.icc`
- `short-prof.hif`: HEVC HEIF with a seven-byte `prof` payload
- `nclx-only.heif`: HEVC HEIF with `nclx` colour information and no ICC data
- `duplicate-prof.heic`: Two associations to byte-identical `prof` properties
- `conflicting-prof.heic`: Valid and malformed `prof` properties on one item
- `conflicting-nclx.heic`: `prof` and `nclx` properties on one item
- `invalid-ipma-index.heic`: `ipma` associates a nonexistent property index
- `truncated-colr.heic`: The `colr` box declares a size smaller than its header
- `oversized-colr.heic`: The `colr` box declares a size beyond its container
- `truncated-sps.heic`: Valid HEIF structure with an exhausted HEVC SPS bitstream
- `empty-prof.heic`: A `prof` property with a zero-byte ICC payload
- `truncated-nclx.heic`: An `nclx` property without its seven-byte fields
- `overflow-meta-largesize.heic`: ICC carrier whose `meta` largesize is `INT64_MAX`
