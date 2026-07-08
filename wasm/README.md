# iccDEV WASM Web Tools

Browser-based ICC color profile tools compiled to WebAssembly. All processing
runs locally in the browser -- no server, no file uploads leave your machine.

## Tools (16)

### Profile Inspection
- **iccDumpProfile** -- dump header, tag table, and tag contents
- **iccRoundTrip** -- validate PCS round-trip fidelity

### Conversion
- **iccToXml** -- binary ICC to human-readable XML
- **iccFromXml** -- build ICC profile from XML specification
- **iccFromCube** -- create ICC profile from .cube 3D LUT

### JSON
- **iccToJson** -- binary ICC to structured JSON
- **iccFromJson** -- build ICC profile from JSON specification

### Color Management
- **iccApplyProfiles** -- apply ICC profiles to transform pixel data
- **iccApplyNamedCmm** -- named color management transforms
- **iccApplyToLink** -- create device link profiles
- **iccApplySearch** -- search for optimal transform paths

### Image Analysis
- **iccTiffDump** -- dump ICC profile from TIFF images
- **iccPngDump** -- dump ICC profile from PNG images
- **iccJpegDump** -- dump ICC profile from JPEG images

### Advanced
- **iccSpecSepToTiff** -- combine single-sample spectral TIFF inputs into one TIFF
- **iccV5DspObsToV4Dsp** -- convert v5 display+observer to v4

## Architecture

Each tool page loads a pair of files:
- `toolName.js` -- Emscripten module loader (~63 KB)
- `toolName.wasm` -- compiled binary (~136 KB to 1.8 MB)

The WASM modules use `MODULARIZE=1` with:
- `FS` -- Emscripten virtual filesystem for file I/O
- `callMain(args)` -- run the tool with CLI-style arguments

The shared `app.js` runtime handles:
1. Module loading via dynamic script injection
2. File upload via drag-and-drop or file picker
3. Virtual filesystem management (write input, read output)
4. stdout/stderr capture and display
5. Output file download (XML, ICC, TIFF)

## Deployment

### GitHub Actions validation

The `ci-pr-wasm.yml` workflow validates WASM build configuration, browser-facing
assets, and parity expectations during pull-request review.

### Local development

To test locally after a WASM build:

1. Copy or build the generated `.js` and `.wasm` modules for each tool
2. Place the generated `.js` and `.wasm` files in `wasm/`, next to the HTML pages
3. Serve with any HTTP server:

```bash
cd wasm && python3 -m http.server 8080
# Open http://localhost:8080
```

Note: WASM files must be served over HTTP (not file://). The `app.js`
runtime loads modules from `./` relative to each page URL. It intentionally
does not read a `wasmBase` query parameter because changing script origins from
the URL would weaken script-injection protections.

## Build

WASM binaries are cross-compiled using Emscripten with the same third-party
dependency stack used by the native tools (zlib, libpng, libjpeg, libtiff,
libxml2, nlohmann-json).

Key Emscripten flags:
- `INITIAL_MEMORY=128MB` -- initial heap size
- `ALLOW_MEMORY_GROWTH=1` -- dynamic heap growth for large profiles
- `FORCE_FILESYSTEM=1` -- enable MEMFS virtual filesystem
- `MODULARIZE=1` -- export factory function
- `EXPORTED_RUNTIME_METHODS=['FS','callMain']` -- expose filesystem and main()
