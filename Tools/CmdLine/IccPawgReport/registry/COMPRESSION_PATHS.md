# Compression-paths cross-check (issue #1723)

Cross-check of every zlib-backed "compression path" in iccDEV — the compressed
ICC tag types and the CLI tools / PAWG report that touch them — prompted by the
#1722 diagnostic scan (which surfaced `zip` / `compressed` / `created` messages
across the profile corpus) and xsscx's #1723 notes:

> 1. CLI Tools need cross-check for Compression Paths
> 2. Pawg Code Path — Missing — Rev.A didn't contemplate Compression

Two concrete defects were found and fixed (section C); the PAWG gap in note 2 is
substantiated and recorded as **flag-and-wait** (section D) because what the
report *should say* about a compressed tag is a maintainer design decision, not a
mechanical correction.

---

## A. The compression surface

iccDEV has exactly three compressed tag representations, all zlib/DEFLATE and all
compiled behind `ICC_USE_ZLIB`:

| Type | Signature | Class | Payload |
|------|-----------|-------|---------|
| zipUtf8Text | `zut8` `0x7a757438` | `CIccTagZipUtf8Text` | DEFLATE-compressed UTF-8 text |
| zipXml | `zxml` `0x7a786d6c`, `ZXML` `0x5a584d4c` (X-Rite CxF) | `CIccTagZipXml` (subclass of the above) | DEFLATE-compressed XML |
| data (compressed) | `data` `0x64617461` + `icCompressedData` flag `0x00010000` | `CIccTagData` | DEFLATE-compressed byte block |

`CIccTagZipXml` adds no compression code of its own — it inherits the whole
inflate/deflate implementation from `CIccTagZipUtf8Text` and only overrides the
type signature. XML/JSON mirrors (`CIccTagXmlZipUtf8Text` etc.) round-trip the raw
compressed bytes as hex and need **no** zlib.

---

## B. Build split — the source of two-mode behaviour

`ICC_USE_ZLIB` has two independent defaults:

- **Header default: OFF** — `IccProfLib/IccProfLibConf.h:187` ships
  `//#define ICC_USE_ZLIB` commented out. Any consumer that compiles IccProfLib
  *without* CMake (raw source include) gets the no-zlib path.
- **CMake option: ON** — `Build/Cmake/CMakeLists.txt:514`
  `option(ICC_USE_ZLIB "Enable zlib-backed compressed ICC text tags" ON)`, applied
  as a **PUBLIC** compile definition to both IccProfLib targets
  (`Build/Cmake/IccProfLib/CMakeLists.txt:183`,`234`). Every CLI tool and
  regression binary built through CMake therefore inherits `ICC_USE_ZLIB=1`.

Behaviour of each path with zlib **on** vs **off**:

| Operation | zlib on | zlib off |
|-----------|---------|----------|
| `zut8`/`zxml` Read/Write | raw compressed bytes stored/written verbatim (no inflate on Read) | identical — passthrough, lossless |
| `zut8`/`zxml` `GetText`/`SetText` | inflate / deflate | return `false` (no text available) |
| `zut8`/`zxml` `Describe` | decompressed string | hex dump wrapped in `BEGIN_COMPRESSED_DATA["<n>"]` … `END_COMPRESSED_DATA` |
| `zut8`/`zxml` `Validate` | flags corrupt / faulty-XML-encoded data | Warning "Zip compression not supported by CMM" |
| `data` (compressed) Read/Write | inflate in place / re-deflate (256 MB cap, CWE-400 guard) | passthrough — raw compressed bytes retained, lossless |
| `data` `Describe`/`Validate` | labelled/validated (256 MB cap) | same, minus decompression |

Net: profile *bytes* survive a no-zlib build losslessly (Read/Write are
passthrough); only the *decoded view* (`GetText`/`Describe` content) is
unavailable without zlib.

---

## C. CLI-tool cross-check + defects fixed

**The CLI tools are type-agnostic.** No tool (`iccDumpProfile`, `IccToXml`,
`iccApplyProfiles`, `IccPawgReport`) switches on `zut8`/`zxml`/`data` signatures;
they dispatch through the tag factory to the virtual `Describe()`/`Validate()`.
`iccDumpProfile` prints the type name via `GetTagTypeSigName()` and dumps content
via `Describe()`, so a compressed tag is decompressed-and-dumped when built with
zlib and hex-dumped otherwise — there is no "Unknown tag type" fallthrough for
these registered types. (The only `compress` references in `iccApplyProfiles` /
`TiffImg` are TIFF LZW *image* compression, unrelated to zlib tag compression.)

The two defects were therefore in the **library dump code**, not the tools:

- **C1 — `CIccTagData::Describe()` dropped the "Compressed" label**
  (`IccProfLib/IccTagBasic.cpp`). The method appended `"Compressed "` for a
  compressed-flagged tag, then every data-kind branch reassigned the string with
  `=` (e.g. `sDescription = "Binary Data:\n";`), overwriting the marker. A
  compressed data tag was thus **never** labelled as compressed in any dump, on
  either build. Fixed by appending (`+=`) the data-kind label so the
  `"\n"`/`"Compressed "` prefix survives. Build-independent.

- **C2 — no-zlib `CIccTagZipUtf8Text::Describe()` marker malformed**
  (`IccProfLib/IccTagBasic.cpp`). The opening marker read
  `BEGIN_COMPESSED_DATA["` (missing the R, and an unbalanced `["…]` delimiter)
  while the closing marker correctly read `END_COMPRESSED_DATA`. Fixed to
  `BEGIN_COMPRESSED_DATA["<n>"]` (correct spelling, balanced quotes). This branch
  is compiled out under `ICC_USE_ZLIB`.

Both are output/formatting only — no change to parsing, validation verdicts, or
on-disk bytes.

**Regression:** `.github/ci/regression/compression-describe-labels.cpp`
(CTest `iccdev.compression-describe-labels`) builds compressed `data` tags in
memory and asserts the `"Compressed"` label survives (C1, plus an uncompressed
control that must *not* be labelled), and asserts the corrected no-zlib marker
spelling/balance (C2, self-gated to no-zlib builds). It needs no deflate/inflate,
so — unlike the zlib-gated #1521/#1743 tests — it runs in **every** configuration.

---

## D. PAWG code path — compression measurement (S14, implemented for #1775)

xsscx's note 2 was **substantiated** and, per the #1775 handoff ("add any needed
compression-specific code paths then will add to overnight CI Fuzzing Harness"),
is now closed with a dedicated measurement. Before it, `IccPawgReport` had no
compression-specific logic: a compressed tag reached the report only indirectly —
`TagTypeAllowedVerdict()`/`TagValueEncodingVerdict()` delegate to
`CIccProfile::Validate()` (which already *accepts* `zut8`/`zxml`), and the
known-vs-unknown classification (`IsSpecTag`, `AssessPrivateTags`) keys off tag
**signature** names, never tag **type**, so a tag's compressed nature never
entered PAWG's own classification.

**What was added (`PawgReport.cpp`):**

- **`CompressionPathVerdict()`** — a new measurement wired into `EvaluatePawg()` as
  security item **S14**. It enumerates compressed tags from the **raw bytes** (a
  tag's 4-byte TYPE signature at its data offset, plus the `icCompressedData` flag
  word at `data+8` for a `data` tag), so it runs even when IccProfLib could not
  parse the profile. The measurement is **build-independent** — it reports the
  compression state (On/Off) regardless of whether zlib is linked, deliberately
  distinct from the gzip-*malware* header scan in `ValidateGzipHeader()` (xsscx:
  "Compression On or Off, not gzip").
- **Verdict policy** (the decision §D previously deferred): no compressed tags →
  `Ok` ("none present"); compressed tags **with** zlib linked → `Ok` (content is
  decodable via `Describe()`/`GetText()`, so presence is not itself a finding);
  compressed tags **without** zlib → **`Gap`** — the bytes are retained losslessly
  (Read/Write are passthrough) but their content cannot be decoded in this build,
  so the report must not imply it examined content it never inflated.
- **Assessment-only entries** (`PawgReport.h`) for the overnight fuzzing harness —
  "up to the point of Output, not for Output": `AssessPawgFromMemory(data, size)`
  runs the full evaluation over an in-memory image and returns fail/ok with no
  report emission (libFuzzer-shaped); `PawgCompressionVerdict(data, size, detail)`
  runs only the S14 measurement. Both reuse `LoadRawProfileFromMemory()`, the
  in-memory twin of `LoadRawProfile()` factored out via `ParseRawProfile()` so the
  CLI and the harness apply identical header/bounds handling.

The library-side dump correctness (C1/C2) remains fixed regardless, so a
compressed tag both *labels itself* correctly under `Describe()` and is now
*measured* by the report.

---

## E. Existing coverage

- `iccdev.tagdata-zlib-roundtrip` (#1521) — `CIccTagData` deflate/inflate
  round-trip; zlib-gated.
- `iccdev.ziputf8-settext-contract` (#1743) — `CIccTagZipUtf8Text::SetText`
  success/failure contract; zlib-gated.
- `iccdev.compression-describe-labels` (#1723) — dump labelling for C1/C2;
  runs in all configurations.
- `iccdev.pawg-compression-paths` (#1775, **new**) — the S14 measurement via the
  in-memory `PawgCompressionVerdict()` entry: `zut8`/`zxml`/flagged-`data` tags are
  measured as compressed (`Ok` with zlib, `Gap` without), uncompressed controls
  read as the empty case, and a truncated buffer does not crash. Detection is
  build-independent so it runs in **every** configuration; only the verdict value
  self-gates on `ICC_USE_ZLIB`.
