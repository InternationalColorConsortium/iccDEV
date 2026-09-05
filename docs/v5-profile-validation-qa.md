# ICC v5 Profile Validation QA

The `iccdev.v5-profile-validation` CTest exercises the public XML-to-profile
validation path with compact ICC v5/iccMAX spectral profiles. It complements
the in-memory IccProfLib regression executables by testing `iccFromXml`, the
serialized header, `iccDumpProfile -v 100`, and the resulting diagnostics.
Invalid probes must produce a nonzero exit and the expected diagnostic from
both tools; `iccFromXml` must still preserve a nonempty artifact for independent
checking.

The XML inputs are focused validation probes, not complete example profiles.
In particular, the accepted probes intentionally omit the profile connection
condition tags needed by a complete spectral workflow. Do not use them as
templates for production profiles or cite CTest acceptance as full ICC.2
conformance.

## Fixture Matrix

| Fixture | Expected validator result | Boundary covered |
|---|---|---|
| `v5-sref-two-step-boundary.xml` | Accept | Smallest spectral range currently accepted by IccProfLib |
| `v5-sref-direct-36-channel.xml` | Accept | `nc0024`, `rs0024`, and 36 wavelength steps |
| `v5-sref-direct-81-channel.xml` | Accept | `nc0051`, `rs0051`, and 81 wavelength steps |
| `v5-sref-spectral-range-mismatch.xml` | Reject | 36 spectral channels paired with 35 range steps |
| `v5-sref-one-step-review.xml` | Reject | One-channel signature paired with a one-step range |
| `v4-illegal-nchannel-space.xml` | Reject | ICC.2 `ncXXXX` data space in an ICC.1 v4 profile |

Generated `.icc` files and logs belong in the build or temporary output tree;
only the XML sources are tracked.

## Specification Correlation

The expectations above were reviewed against ICC.2:2023 and ICC.1:2022.

| Behavior | Specification basis | QA interpretation |
|---|---|---|
| Extended device channel signature | ICC.2:2023, 6.2 and Table 9 | `ncXXXX` encodes 1 through 65,535 channels in the low 16 bits. |
| Spectral signature channel count | ICC.2:2023, 6.3.3.2 and 7.2.21 | The channel count in `rsXXXX`, `tsXXXX`, or `esXXXX` must equal the corresponding spectral range step count. |
| Uniform wavelength interval | ICC.2:2023, 6.3.3.2 and 7.2.22 | The interval is `(end - start) / (steps - 1)`. IccProfLib therefore treats fewer than two steps as invalid. |
| ICC v4 data space | ICC.1:2022, 7.2.6 and Table 19 | ICC.1 enumerates the v4 data spaces through `FCLR`; it does not define the ICC.2 binary `ncXXXX` family. |
| Spectral PCC tags | ICC.2:2023, 6.3.2 | A complete spectral workflow requires spectral viewing conditions and, as applicable, transforms between custom and standard PCCs. These focused probes do not test that contract. |

There is a specification-review edge at one step. ICC.2 spectral signature
tables permit a one-channel signature, while the canonical wavelength interval
formula is undefined when `steps` is one. The validator's two-step minimum has
existed since the initial 2015 iccMAX source import. Keep the one-step fixture as
a rejection probe until the ICC interpretation is formally resolved; do not
change the `(steps - 1)` interval formula to make that case pass.

## Run the Test

After configuring with `ENABLE_TOOLS=ON` and `ENABLE_TESTS=ON`:

```bash
cmake --build build --target iccFromXml iccDumpProfile
ctest --test-dir build -R '^iccdev\.v5-profile-validation$' \
  --output-on-failure --no-tests=error
```

The CTest runner writes six generated profiles below
`build/Testing/ctest-output/iccdev-v5-profile-validation`. It fails if a tool
returns the wrong success class, an expected diagnostic disappears, or common
ASan/UBSan/LSan signatures appear.

For source coverage, use a separate `linux-clang-coverage` build and set
`LLVM_PROFILE_FILE` to a temporary pattern before running the focused CTest.
Do not combine coverage instrumentation with sanitizer bug reproduction.
