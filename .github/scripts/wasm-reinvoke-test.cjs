// SPDX-License-Identifier: BSD-3-Clause
// WASM re-invoke regression test.
//
// Asserts that an Emscripten-generated module can:
//   1. Run callMain() once
//   2. Re-invoke callMain() after main() has exited
//   3. Read from the in-memory FS after main() has exited
//
// This pattern fails with "native function called after runtime exit"
// when the link line is missing -s EXIT_RUNTIME=0. This script is
// invoked from .github/workflows/wasm-latest-matrix.yml.
//
// Usage: node wasm-reinvoke-test.cjs <path-to-iccDumpProfile.js> [sentinel-file]

'use strict';

const fs = require('fs');
const path = require('path');

const factoryArg = process.argv[2];
const sentinelPath = process.argv[3] || '/tmp/wasm-reinvoke-result';
if (!factoryArg) {
  console.error('usage: node wasm-reinvoke-test.cjs <module.js> [sentinel-file]');
  process.exit(2);
}

// Resolve the factory path against CWD so the workflow can pass a
// build-relative path like ./Tools/IccDumpProfile/iccDumpProfile.js.
// A bare require() would resolve relative to this script's directory.
const factoryPath = path.resolve(process.cwd(), factoryArg);

// Pre-write FAIL; only the PASS path overwrites with PASS. Any abort,
// hang, or hijacked exit leaves the FAIL marker for the shell to detect.
fs.writeFileSync(sentinelPath, 'FAIL: script did not reach completion\n');

let factory;
try {
  factory = require(factoryPath);
} catch (e) {
  const msg = 'FAIL: cannot load factory ' + factoryPath + ': ' + (e && e.message ? e.message : e);
  console.error(msg);
  fs.writeFileSync(sentinelPath, msg + '\n');
  process.exit(1);
}

function makeMinimalIcc() {
  // 132-byte stub: 128-byte header with 'acsp' signature + zero tag count.
  const b = Buffer.alloc(132, 0);
  b.writeUInt32BE(132, 0);
  Buffer.from('acsp').copy(b, 36);
  b.writeUInt32LE(0, 128);
  return new Uint8Array(b);
}

(async () => {
  // Intentionally do NOT pass noExitRuntime here - the build itself must
  // bake the correct default via -s EXIT_RUNTIME=0. If we override at the
  // factory level we mask exactly the regression we are trying to catch.
  let aborted = null;
  const mod = await factory({
    noInitialRun: true,
    print:    () => {},
    printErr: () => {},
    onAbort:  (reason) => { aborted = String(reason); },
  });

  mod.FS.writeFile('t.icc', makeMinimalIcc());

  // First call: triggers main().
  try { mod.callMain(['t.icc', 'HEADER']); } catch (_) {}
  if (aborted) throw new Error('aborted on first call: ' + aborted);

  // Second call AFTER main() has exited - this is what regresses
  // when -s EXIT_RUNTIME=0 is not passed to the linker.
  try { mod.callMain(['t.icc', 'SUMMARY']); } catch (_) {}
  if (aborted) throw new Error('aborted on re-invoke: ' + aborted);

  // FS access after main() exited - same failure surface.
  const data = mod.FS.readFile('t.icc');
  if (data.length !== 132) {
    throw new Error('FS readback wrong size: ' + data.length);
  }

  console.log('PASS: re-invoke callMain() and FS access after main() exit');
  fs.writeFileSync(sentinelPath, 'PASS\n');
  process.exit(0);
})().catch((e) => {
  const msg = 'FAIL: ' + (e && e.message ? e.message : e);
  console.error(msg);
  fs.writeFileSync(sentinelPath, msg + '\n');
  process.exit(1);
});
