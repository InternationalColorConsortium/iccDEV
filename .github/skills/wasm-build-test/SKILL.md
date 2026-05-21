# WASM Build and Test

Build iccDEV tools as WebAssembly modules and validate the browser-based tool suite.

## Steps

1. **Configure Emscripten build**
   ```bash
   source /path/to/emsdk/emsdk_env.sh
   mkdir -p build-wasm && cd build-wasm
   emcmake cmake ../Build/Cmake \
     -DCMAKE_BUILD_TYPE=Release \
     -DENABLE_TOOLS=ON \
     -DBUILD_SHARED_LIBS=OFF
   ```

2. **Build all 18 WASM tools**
   ```bash
   emmake make -j$(nproc) 2>&1 | tee build.log
   ```
   Expected: 16 JS modules + 18 WASM binaries + 4 static libraries.

3. **Verify artifacts**
   ```bash
   find build-wasm/Tools -name '*.wasm' | wc -l   # 18
   find build-wasm/Tools -name '*.js' | wc -l      # 16
   ```

4. **Assemble site**
   ```bash
   mkdir -p _site && cp wasm/*.html wasm/*.css wasm/*.js _site/
   for d in build-wasm/Tools/*/; do
     tool=$(basename "$d")
     cp "$d"*.js "$d"*.wasm _site/ 2>/dev/null
   done
   ```

5. **Run HTML validation tests**
   ```bash
   cd iccdev-mcp && python3 -m pytest tests/test_wasm_html.py -v
   ```
   Expected: 301 tests pass (WCAG, CSP, XSS, semantic HTML).

6. **Smoke test in browser**
   - Open `_site/index.html` in Chrome/Firefox
   - Load `_site/dump.html`, drop an ICC profile
   - Verify output renders with no console errors

## Validation Checklist

- [ ] 18 WASM binaries present
- [ ] 16 JS modules present
- [ ] 17 HTML pages present (index + 16 tools)
- [ ] All 301 tests pass
- [ ] No JS console errors in browser
- [ ] CSP headers prevent inline script execution
- [ ] Sanitizer strips ANSI/control chars from WASM output
