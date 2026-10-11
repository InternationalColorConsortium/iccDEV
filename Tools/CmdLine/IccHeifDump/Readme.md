# iccHeifDump

`iccHeifDump` extracts ICC profiles from top-level HEIF, HEIC, and AVIF images
through libheif, writes each raw blob unchanged, and reports whether
IccProfLib can parse it. It is built with the default native tools when
`ENABLE_TOOLS=ON`.

The Nokia reader A/B research tool remains under `.github/ci/tooling/heif/`.
It uses a pinned, incompatible API and remains isolated in its standalone
CTest wrapper and manually dispatched workflow.

## Usage

```sh
iccHeifDump input.heic [existing-output-directory]
```

When an output directory is supplied, the tool writes profiles as:

```text
item-<item-id>-<prof-or-rICC>.icc
```

Each embedded ICC property reports its carrier metadata, byte count, output
path, and `proflib_parse` status:

- `ok`: IccProfLib parsed the blob.
- `failed`: the raw blob was preserved but IccProfLib rejected it.
- `empty`: the carrier has a zero-byte ICC payload.
- `too-large`: the carrier blob exceeds IccProfLib's `icUInt32Number` input
  limit.

## Local build and test

The normal native build requires the libheif development package:

```sh
cmake -S Build/Cmake -B build -DENABLE_TOOLS=ON
cmake --build build --target iccHeifDump --parallel
build/Tools/IccHeifDump/iccHeifDump input.heic [existing-output-directory]
```

For the separate Nokia reader carrier A/B research workflow, see
<a href="../../../.github/ci/tooling/heif/Readme.md">HEIF ICC Carrier QA</a>.
