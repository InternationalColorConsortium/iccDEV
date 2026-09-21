# libpng Patch Stack

Apply patches in lexical order to the exact libpng revision documented in the
parent Readme. Every consumer must run `git apply --check` before applying a
patch.

`libpng-1.6.58-iccp-completion-crc.patch` requires successful iCCP stream
completion, probes for decompressed data beyond the declared ICC size, accounts
for zlib input already buffered by libpng, and discards a profile when the PNG
CRC policy reports an ancillary-chunk error. Explicit CRC-use policy remains
unchanged.

The patch is a review artifact for upstream libpng. It is not compiled into
iccDEV or the unified runtime image.
