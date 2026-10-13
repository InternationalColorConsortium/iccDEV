# iccHeifDump

`iccHeifDump` validates HEIF, HEIC, and AVIF files with libheif, extracts every
ICC `colr` property association from the file's `iprp` boxes, writes each raw
blob unchanged, and reports whether IccProfLib can parse it. This includes
profiles on non-top-level items. It is built with the default native tools
when `ENABLE_TOOLS=ON`. Files larger than 256 MiB are rejected before the
property parser buffers the carrier. The parser also limits property
associations to 4096 and cumulative extracted ICC bytes to 256 MiB; profile
views share the carrier buffer rather than copying each association's payload.

The Nokia reader A/B research tool remains under `.github/ci/tooling/heif/`.
Its pinned reader is isolated in the standalone CTest wrapper and manually
dispatched workflow, where it is compared with this release implementation.

## Usage

```sh
iccHeifDump input.heic [existing-output-directory]
```

When an output directory is supplied, the tool writes profiles as:

```text
item-<item-id>-property-<property-index>-<prof-or-rICC>.icc
```

Extraction never overwrites an existing path. Choose an empty output directory;
this prevents a carrier from being clobbered when its name matches an extraction
filename.
Existing regular files, hard links, symlinks (including dangling symlinks),
and directories are rejected with exit status 2. Concurrent extractions to
the same filename allow only one writer to create the output. If extraction
fails partway through a multi-profile carrier, earlier outputs remain; use
a fresh directory when retrying.

The focused `iccdev.heif-output-safety` CTest checks this contract on Windows
and Unix using the tracked carrier and its pinned ICC digest. It needs Python 3
and the native tool, but does not need the Nokia reader. Windows symlink cases
report a skip if the account lacks symlink creation privileges.

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
