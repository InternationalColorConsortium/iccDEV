# IccApplyProfiles

`iccApplyProfiles` applies a sequence of ICC/iccMAX profiles to a TIFF image and
writes a destination TIFF image. The destination profile can optionally be
embedded in the output image.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccApplyProfiles
```

## TIFF Pixel Encoding

`iccApplyProfiles` treats TIFF pixel values as a *device encoding* regardless
of color space or TIFF photometric tag. The rule applies symmetrically on
read and write:

| Pixel format | Decode (read) | Encode (write) |
|--------------|---------------|----------------|
| 8-bit integer | `value = pixel / 255` | `pixel = clamp01(value) * 255` |
| 16-bit integer | `value = pixel / 65535` | `pixel = clamp01(value) * 65535` |
| 32-bit float | `value = pixel` (pass-through) | `pixel = value` (pass-through) |

No bit-bias adjustments are applied — for example, a/b channels of a 16-bit
Lab destination are *not* offset by `0x8000` even when the TIFF
PhotometricInterpretation is `CIELAB` or `ICCLAB`. Consumers of the output
TIFFs should use the *embedded ICC profile* to interpret pixel values,
since that profile (not the photometric tag alone) describes the actual
encoding produced by the final transform.

Any PCS-encoding bridging is the CMM's responsibility: profile xforms whose
PCS endpoint is `Lab` or `XYZ` apply `icLabToPcs` / `icLabFromPcs` (or the
XYZ equivalents) at their own entry/exit, so values entering and leaving
the boundary code here are always in the form the *next* transform stage
or the destination TIFF expects.

Practical consequences:

- A chain ending in a v5 MPE PCC profile that maps standard CIELAB to a
  `[0, 1]` device-Lab encoding (e.g. `L_dev = L*/100`,
  `a_dev = (a*+128)/256`) will produce a 16-bit Lab TIFF where each channel
  is the device-encoded value scaled to `[0, 65535]`. Round-tripping via
  the same profile recovers the original colors.
- A chain ending in a plain Lab PCS output produces values in the CMM's
  internal PCS-Lab encoding (normalized `[0, 1]`). Writing 32-bit float
  passes those through unchanged; writing 16-bit scales them to
  `[0, 65535]`.
- A TIFF whose `PhotometricInterpretation` is `CIELAB` (TIFF spec) without
  an embedded ICC profile cannot be read back to its original colors,
  because the tool does not apply the spec's `±128`-biased decoding. Embed
  the ICC profile (set `dstEmbedIcc: true` in the JSON config) so the
  reverse chain can recover the values.

## Resolution

The destination TIFF carries the source's `XResolution`, `YResolution` and
`ResolutionUnit`. The unit tag is always written, so the output states its unit
rather than relying on the reader defaulting an absent tag to inches.

## Output Destination Safety

The destination must be a regular non-symlink file when it already exists.
Directories, devices, POSIX symlinks, and Windows reparse points are rejected
before libtiff opens the destination. Naming a symlink as the output is
therefore an error rather than a write through to its target; point the tool at
the target path directly.

Two limits are deliberate rather than oversights:

- A **hard** link is accepted. It is another name for the regular file it links
  to, and nothing in the file's metadata distinguishes it from the original.
- The destination is checked and then opened, so a path swapped between those
  two steps is not covered. The check guards against writing through a symlink
  that is already there, not against a race for the path.

## Performance Diagnostics

Set `ICC_APPLY_PROFILES_TIMING=1` to print bounded apply-loop timing:

```text
[TIMING] Loop ms: 28997.128
[TIMING] Apply ms: 27611.920
[TIMING] Apply pct: 95.223
[TIMING] Apply calls: 1048576
[TIMING] Async worker strips: 0
```

`Async worker strips` is the cumulative number of strips queued to background
workers by the active apply object. It is zero for scalar and short-row calls
that run entirely on the caller thread.

Timing and trace modes suppress the carriage-return progress display so logs
remain line-oriented and safe to copy or parse.

Set `ICC_APPLY_TRACE=1` for phase, image geometry, profile-chain, buffer, and
throughput records through `IccSignatureUtils.h`. Level 2 also logs each row;
level 3 logs each pixel and is intended only for tiny diagnostic crops. Keep
`ICC_VERBOSE_CALC_APPLY=OFF` for timing because it dumps the calculator program
for every pixel and changes the workload into an output-throughput test.

For the MCS overprint command, the bounded profiling helper constructs 1x1,
64x64, and 512x512 crops, calibrates with the original 1024x1024 TIFF, compares
Debug sanitizer and Release output checksums, and repeats the full sanitizer
case to target 300 seconds without exceeding 600 seconds:

```sh
.github/scripts/iccdev-mcs-applyprofiles-profile.sh \
  out/debug-asan-ubsan-trace out/release-compare /tmp/iccdev-mcs-profile
```

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [Build documentation](../../../docs/build.md)
