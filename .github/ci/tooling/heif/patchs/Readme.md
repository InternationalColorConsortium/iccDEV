## CI Patch Area

- `nokia-heif-meta-largesize-overflow.patch` applies to Nokia HEIF commit
  `503194eb85e13434b54797bab9d82ad7f88fd35b`. It rejects a box whose start
  offset plus declared 64-bit size would overflow `int64_t`.
- `nokia-heif-nalutil-cstdint.patch` applies to the same commit. It gives
  `nalutil.hpp` the direct `<cstdint>` dependency required for `uint8_t` by
  newer GCC/libstdc++ combinations.
- `nokia-heif-timestamp-sign-conversion.patch` rejects a decoded time span that
  cannot fit the reader's signed timestamp type, then keeps repeated edit-list
  arithmetic in that type. This resolves GCC 15 sign-conversion warnings
  without silently wrapping an out-of-range duration.
- Validate every patch with `git apply --check` against the pinned commit and
  rerun the sanitizer-backed `iccdev.heif-carrier-qa` CTest.
