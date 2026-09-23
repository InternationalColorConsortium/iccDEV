# OpenImageIO Build Bootstrap Patch

`openimageio-yaml-cpp-cstdint.patch` applies only to OpenImageIO's dependency
bootstrap. It makes OpenImageIO install the pystring package required by its
static OpenColorIO export and makes pinned yaml-cpp 0.8.0 include the standard
`cstdint` declarations required by modern GCC and Clang.

Apply this patch before configuring OpenImageIO. It does not alter OpenImageIO
runtime behavior and is intentionally separate from the vulnerable-to-fixed
candidate patch stack in `../patches`.
