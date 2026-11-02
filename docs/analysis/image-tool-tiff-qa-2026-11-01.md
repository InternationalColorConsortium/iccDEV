# Image tool and TIFF QA review (2026-11-01)

This review uses `ci-qa-pr-tiff-parsing` from master after issue #2949 was
merged. It covers the six external-image command-line tools, their shared TIFF
reader/writer, the ApplyProfiles JSON configuration, the two visualization
TIFF writers, and the relevant CMake and CTest contracts. Findings marked
"static" still need a reachable reproducer; they are not claims of a crash.

## Multi-image policy

The common CLI ecosystem uses more than one policy. `tiffinfo` inventories TIFF
directories, `tiffcp` copies image sequences, and `libvips` TIFF loading and
OpenImageIO's `oiiotool` provide explicit page/subimage selection. These are
different operations. For iccDEV's current single-output transforms, keep the
historical main IFD 0 default, provide a zero-based `--ifd` selector, report
the selected page and ignored pages, and never silently treat SubIFDs as main
chain entries. `iccTiffDump` remains a metadata-oriented inventory tool.

Sources: <a href="https://libtiff.gitlab.io/libtiff/tools/tiffinfo.html">libtiff
tiffinfo</a>, <a href="https://libtiff.gitlab.io/libtiff/tools/tiffcp.html">libtiff
tiffcp</a>, <a href="https://www.libvips.org/API/current/ctor.Image.tiffload.html">libvips
TIFF loader</a>, and <a href="https://openimageio.readthedocs.io/en/latest/oiiotool.html#dealing-with-multi-subimage-multi-part-files">OpenImageIO
subimage handling</a>.

## Implemented on this branch

| Surface | Correction and nearest regression |
| --- | --- |
| ApplyProfiles | `--ifd N` and JSON `imageFiles.srcIfd` select a bounded main-chain IFD; CLI overrides config. Inventory and evidence identify page/count/provenance and warn about omitted main IFDs and selected-page SubIFDs. `test_tiff_tiles.py` covers default, selected, override, mixed numeric JSON forms, missing pages, and receipt fields. |
| SpecSepToTiff | `--ifd N` selects the same main-chain index from every input. The tool reports omitted directories, records aggregate and per-input inventory in telemetry, and rejects a changed tile layout or an over-budget tile band after reopening an input. `test_tiff_tiles.py` and `test_tiff_telemetry.py` cover selection and reporting. |
| TiffImg/TiffDump | The shared writer rejects out-of-range bits-per-sample and photometric values before opening a destination. TIFF tools print the linked runtime libtiff version through `TIFFGetVersion`; TiffDump also has a standalone `--version`. Direct writer and CLI regressions cover these paths. |
| PNG dump | Injection rejects APNG before opening output, avoiding silent animation flattening. Help/version show build-time IccProfLib/libpng and runtime libpng. The PNG regression uses a two-frame APNG and checks output preservation. |
| JPEG dump | Injection removes old ICC APP2 chunks before writing the new profile; existing non-ICC markers and compressed data remain. The parser reports injected read faults as read failures. Its unused direct libjpeg build/link requirement was removed, so the banner reports only the linked ICC library. JPEG inspection and issue #2162 regressions cover it. |
| HEIF dump | Help/version show build-time IccProfLib/libheif and runtime libheif. Output paths and library diagnostics are console-sanitized. `IccFileUtil.h` is the correct shared header; exclusive-create output does not call for `IccSameFile.h`. The HEIF safety regression checks control characters in output paths. |

Existing `ICCDEV_ENABLE_TELEMETRY`, `ICCDEV_ENABLE_QA_FLAGS`, sanitizer presets,
and focused CTests supply the needed build switches for this tranche. No new
general CMake toggle is justified by these corrections.

## Remaining findings and implementation order

1. **P1: Preserve existing destinations on every failure.** ApplyProfiles
   (`iccApplyProfiles.cpp`, `CTiffImg::Create`) and SpecSep
   (`iccSpecSepToTiff.cpp`, output creation) can truncate a destination before
   a late read/write failure; their cleanup can then remove the old file.
   JPEG injection/extraction and PNG raw-profile extraction also write final
   paths directly. Stage in the destination directory, validate and close,
   then replace atomically. Test a byte-identical sentinel after malformed
   input and forced late failure, success parity, and absence of temp files on
   Windows and Unix. PNG injection already has a staged-output pattern.
2. **P1: Make graceful exits 1-127.** ApplyProfiles and SpecSep still return
   `-1` from many ordinary failure branches, yielding 255 on POSIX. Use
   `EXIT_FAILURE` at the process boundary and update CLI tests to assert exact
   exit 1 for malformed arguments, missing IFDs, and late decode failures.
3. **P1: Distinguish PNG extraction states.** `iccPngDump.cpp` currently treats
   absent iCCP, an empty profile, and extraction errors too similarly; a
   requested export can report success without a file. Return explicit
   absent/success/error states, decide and document whether malformed ICC is
   exported for forensics, and test requested output preservation on failure.
4. **P2: Finish input identity and SubIFD coverage.** SpecSep's post-open tile
   check prevents unsigned underflow, but a path replacement between metadata
   and pixel opens can briefly allocate against the wrong budget. Reuse the
   same open handle or enforce the remaining aggregate tile budget at pixel
   open regardless of the first handle's layout. ApplyProfiles inventory has
   the same provenance race. Add a SubIFD fixture with different pixels/profile
   and invalid/cyclic offsets. Count references without following them in the
   single-IFD tools. Design pyramid processing separately with depth, count,
   cycle, and aggregate decode bounds.
5. **P2, static: Bound visualization geometry and classic TIFF offsets.**
   `IccProfileVisualize/iccProfileVisualize.cpp` multiplies CLUT geometry in
   signed `int` before validating it; both `MiniTIFF.cpp` copies narrow image
   dimensions and offsets to 32 bits. Use checked arithmetic before allocation
   or output open, then add sanitizer-backed oversized negative controls and a
   small CLUT pixel-parity control. A reachable crash is not established yet.
6. **P3: Normalize all CLI version contracts.** The six external-image tools
   now have top-of-help versions. The plot/visualization tools still vary in
   banner placement, standalone version support, and one tool identity
   (`iccProfileVisualizePlot` says `iccProfileVisualize`). XML tools omit the
   linked libxml2 backend; JSON's nlohmann dependency is header-only; optional
   zlib is transitively linked through IccProfLib. Decide which transitive
   libraries belong in user-facing output, label build versus runtime
   versions, and add a tool/banner matrix CTest before normalizing the other
   CLIs. Do not claim a runtime ICC-owned version without an exported API.

## Changed-surface contract matrix

| Surface | Producer | Consumer | Build/runtime behavior | Platform boundary | CI trigger and dependency owner | Local evidence |
| --- | --- | --- | --- | --- | --- | --- |
| Main IFD selection | CLI or Apply JSON config | `CTiffImg::Open`, SpecSep inputs | Default 0; explicit 0..4095; one output IFD | libtiff directory API; Windows/Unix paths | TIFF tile and SpecSep CTests; iccDEV + libtiff | Multi-page 8/16-bit fixtures, invalid selection, telemetry ON/OFF |
| TIFF versions and provenance | `TIFFGetVersion`, selected IFD inventory | Help, `--version`, JSONL/evidence | Runtime libtiff; ICC-owned compile-time version; line-ending-independent CLI assertion | Linked libtiff ABI and Windows CRT CRLF | TIFF telemetry/directory CTests; libtiff | Version/receipt scripts in WSL Release/sanitizer and Windows MSVC, ClangCL, and MinGW |
| TIFF writer format validation | `CTiffImg::Create` arguments | TIFF output tags | Reject before opening output | 16-bit TIFF tag fields | Direct overflow CTest; iccDEV/libtiff | `iccdev.tiff-create-overflow` |
| JPEG APP2 replacement | Marker parser, ICC input | JPEG output | Replace ICC APP2; no direct libjpeg link | JPEG marker format; filesystem errors | JPEG inspection and issue #2162 CTests; iccDEV | Replacement and injected read-fault checks |
| PNG APNG policy and versions | PNG chunk preflight, libpng APIs | PNG injection and banner | Reject animation before output; runtime libpng | libpng/PNG chunk framing | PNG issue #2854 CTest; libpng | Two-frame APNG and output-preservation checks |
| HEIF diagnostics and versions | libheif API, `IccFileUtil.h` | CLI banner and path diagnostics | Exclusive create; sanitized console output | libheif, OS path encoding | HEIF output-safety CTest; libheif | Control-character path and version checks |
| JPEG CMake dependency | Native image-tool target and WASM workflow | `iccJpegDump` and TIFF JPEG codec | Dump tool has no direct libjpeg link; WASM still builds libjpeg for libtiff | CMake, vcpkg, and Emscripten link boundaries | Image-tool and WASM lanes; iccDEV/libtiff/libjpeg | Native build without JPEG discovery; WASM workflow and staged-module audit |
| TIFF CLI grammar | SpecSep/TiffDump argument parsers and usage banners | Shell users, regression scripts, Doxygen lanes | Reject mixed SpecSep help/version and unknown long options; distinct TiffDump inspection/listing grammar | Windows and Unix argument/stream behavior | TIFF CLI and output-hardening regressions; iccDEV | Focused WSL Release/plain/sanitizer plus three Windows toolchains |
| Doxygen image-tool lanes | Doxyfile, curated navigation, `.dox` pages, checked-in DOT diagrams | Generated maintainer guide | Apply, TIFF pipeline, SpecSep, and TiffDump parser/pipeline contracts | Doxygen/Graphviz rendering | Documentation build; iccDEV | Graph-light Doxygen exit 0, empty warnings log, and direct DOT renders |

Local validation uses Clang 22 WSL Release with telemetry/QA enabled, Release
with both disabled, and Debug with ASan, UBSan, integer, and float sanitizers.
Focused TIFF telemetry, directory inventory, and tile CLI scripts also passed
on Windows MSVC, ClangCL, and MinGW UCRT64 builds. The manual hosted run at
`911fed7` exposed a Windows CRLF-only test assertion; that assertion is fixed
here, and the stale run was cancelled. The user will run hosted CI manually
after this update is pushed.
