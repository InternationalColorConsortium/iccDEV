# OpenImageIO Candidate Patch

`openimageio-icc-exif-jpeg2000.patch` applies to pinned OpenImageIO commit
`8004015ace460bf7e9019514f6d8c6c677e6e7ae` and is exercised by the #2657
external compatibility CTest.

The EXIF alignment and Canon MakerNote portions preserve the two locally
validated OpenImageIO commits from the 2026-09-22 investigation. The remaining
hunks propagate strict JPEG ICC and JPEG2000 encoder failures, restrict `mluc`
decoding to the declared tag span, read `jpeg2000:ProgressionOrder` through the
string accessor, and correct the CPRL comparison.

Keep the patch dry-run, vulnerable-contract proof, patched rebuild, and fixed
CTest together when changing the pinned upstream revision. Upstream issue and
pull-request references are intentionally deferred until maintainers publish
them.
