# WASM Build and Test

Build iccDEV tools as WebAssembly modules and validate the staged module
package. The legacy browser UI under `wasm/` has been removed, so this workflow
does not assemble HTML pages or run browser/Playwright checks.

## Steps

1. **Configure Emscripten build**
   ```bash
   source /path/to/emsdk/emsdk_env.sh
   mkdir -p build-wasm && cd build-wasm
   emcmake cmake ../Build/Cmake \
     -DCMAKE_BUILD_TYPE=Release \
     -DENABLE_TOOLS=ON \
     -DENABLE_STATIC_LIBS=ON \
     -DENABLE_SHARED_LIBS=OFF
   ```

2. **Build the WASM tool modules**
   ```bash
   emmake make -j$(nproc) 2>&1 | tee build.log
   ```
   Expected: Emscripten-generated `.js` loader modules and matching `.wasm`
   binaries under `Tools/` in the current `build-wasm` directory.

3. **Verify artifacts**
   ```bash
   find Tools -name '*.wasm' -type f | sort
   find Tools -name '*.js' -type f | sort
   ```

4. **Stage the npm-shaped package**
   ```bash
   cd ..
   bash Build/Cmake/wasm-package/stage.sh build-wasm wasm-stage 0.0.0-local
   ```
   The staged package contains the generated module loaders, binaries, package
   metadata, and Node-based regression scripts.

5. **Run module smoke tests**
   ```bash
   cd wasm-stage
   node test_all.js
   ```
   Expected: every staged WASM tool can be loaded and invoked with its usage
   path.

6. **Run profile parity regression**
   ```bash
   node regression.js
   ```
   Expected: the staged WASM tools reproduce the reference
   `Testing/CreateAllProfiles.sh` profile outputs.

## Validation Checklist

- [ ] WASM binaries are present under `build-wasm/Tools/`
- [ ] JS loader modules are present under `build-wasm/Tools/`
- [ ] `Build/Cmake/wasm-package/stage.sh` creates `wasm-stage/`
- [ ] `wasm-stage/test_all.js` passes
- [ ] `wasm-stage/regression.js` passes
- [ ] No legacy `wasm/*.html`, `wasm/*.css`, or `wasm/*.js` browser assets are required
