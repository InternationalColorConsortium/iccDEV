# Debug WASM Build Failures

Use this prompt when a WASM CI workflow fails (ci-wasm-build-test.yml or wasm-latest-matrix.yml).

## Quick Diagnosis

### 1. Check if emsdk fetch failed
```
# Look for: "fatal: unable to access 'https://chromium.googlesource.com/'"
# Fix: Retry logic already built into workflows (3 attempts, 10s backoff)
# If persistent: GitLab/Chromium CDN outage — wait and re-trigger
```

### 2. Check if libxml2 build failed
```
# WASM builds use cmake to build libxml2 from source (no pkg-config in Emscripten)
# Common error: "Could NOT find LibXml2" in cmake output
# Fix: Ensure LIBXML2_ROOT is set to the local build prefix
```

### 3. Check if iccDEV cmake failed
```
# Common: "wasm branch not found" — the wasm branch was consolidated into master
# All WASM CMake guards are in Build/Cmake/CMakeLists.txt under if(EMSCRIPTEN)
# WASM builds MUST use: -DENABLE_TESTS=OFF -DENABLE_TOOLS=ON -DENABLE_SHARED_LIBS=OFF
```

## WASM Build Architecture

### Build Configs (3-matrix)
- **Debug**: `-O0 -g -sASSERTIONS=2` (256MB initial memory)
- **Release**: `-O3` (128MB initial memory)
- **RelWithDebInfo**: `-O2 -g` (128MB initial memory)

### Critical CMake Flags
```bash
emcmake cmake ../Build/Cmake \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DENABLE_TESTS=OFF \
  -DENABLE_TOOLS=ON \
  -DENABLE_SHARED_LIBS=OFF \
  -DENABLE_STATIC_LIBS=ON \
  -DLIBXML2_ROOT=/path/to/libxml2-prefix
```

### Critical Emscripten Link Flags (set in CMakeLists.txt)
- `-sINVOKE_RUN=0` — MANDATORY with `-sMODULARIZE=1`
- `-sEXPORTED_RUNTIME_METHODS=callMain,FS` — for Module.callMain() and Module.FS
- `-sALLOW_MEMORY_GROWTH=1` — dynamic memory for large profiles
- CMakeLists.txt has `if(EMSCRIPTEN)` guards that skip ELF-only flags automatically

### Expected Artifacts (per config)
- **13 JS modules** (one per CLI tool, excluding IccDumpProfileGui)
- **13 WASM binaries** (matching .wasm files)
- Third-party: `libIccProfLib2.a`, `libIccXML2.a`, `libxml2.a`

### Validation Step
```bash
# Verify JS modules are valid (not truncated)
for js in Build/Tools/*/icc*.js; do
  node -e "require('$js')" 2>/dev/null && echo "OK: $js" || echo "FAIL: $js"
done
```

## Common Pitfalls

1. **Stale wasm branch reference** — The `wasm` branch was consolidated. Build from
   the checked-out branch directly. Never `git checkout wasm`.

2. **emsdk version pinning** — emsdk git tags are emsdk releases (e.g., `5.0.3`),
   NOT Emscripten SDK versions (`3.1.78`). Use `./emsdk install latest`.

3. **ASan and SAFE_HEAP** — Mutually exclusive in Emscripten. Debug config uses
   ASSERTIONS+SAFE_HEAP. ASan config uses `-fsanitize=address` WITHOUT SAFE_HEAP.

4. **GitLab clone transient failures** — `chromium.googlesource.com` has intermittent
   503s. Workflows include retry logic (3 attempts, 10s exponential backoff).

5. **Summary formatting** — Multiline file lists must iterate line-by-line through
   `sanitize_line` to avoid collapsing to a single line in GITHUB_STEP_SUMMARY.
