# iccHeifDump

`iccHeifDump` is the IccProfLib-aware half of local HEIF ICC carrier A/B
testing. It uses the pinned Nokia HEIF reader to enumerate `prof` and `rICC`
`colr` properties, writes each raw ICC blob unchanged, and reports whether
IccProfLib can parse the same blob.

It is intentionally built only by the standalone HEIF QA wrapper because the
Nokia HEIF reader is an externally pinned dependency. It is not registered by
the normal `Build/Cmake` project or general pull-request CTest suite.

## Usage

```sh
iccHeifDump input.heic [existing-output-directory]
```

When an output directory is supplied, the tool writes profiles as:

```text
item-<item-id>-property-<property-index>-<prof-or-rICC>.icc
```

Each embedded ICC property reports its carrier metadata, byte count, output
path, and `proflib_parse` status:

- `ok`: IccProfLib parsed the blob.
- `failed`: the raw blob was preserved but IccProfLib rejected it.
- `empty`: the carrier has a zero-byte ICC payload.
- `too-large`: the carrier blob exceeds IccProfLib's `icUInt32Number` input
  limit.

`heifIccDump` is the carrier-only companion tool. The A/B CTest requires both
tools to emit identical raw profiles before it evaluates the native parse
status.

## Local build and test

Follow the pinned Nokia checkout and sanitizer build commands in
<a href="../../../.github/ci/tooling/heif/Readme.md">HEIF ICC Carrier QA</a>. The
standalone CTest wrapper registers:

- `iccdev.heif-carrier-qa` for carrier-only extraction regression.
- `iccdev.heif-carrier-ab-qa` for byte-for-byte comparison between
  `heifIccDump` and `iccHeifDump`.

See [CTest guidance](../../../docs/ctest.md#external-heif-carrier-ctest) for
the focused test command.
