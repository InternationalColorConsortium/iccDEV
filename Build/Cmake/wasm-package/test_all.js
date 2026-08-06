const fs = require('fs');
const path = require('path');

async function test(name, factory, fn) {
  try {
    const mod = await factory({ noExitRuntime: true, noInitialRun: true, print: () => {}, printErr: () => {} });
    await fn(mod);
    console.log('  PASS: ' + name);
    return true;
  } catch (e) {
    console.log('  FAIL: ' + name + ': ' + e.message);
    return false;
  }
}

function getTestProfile() {
  const testPath = path.join(__dirname, 'test.icc');
  if (fs.existsSync(testPath)) {
    return new Uint8Array(fs.readFileSync(testPath));
  }
  // Generate minimal valid ICC v4 profile header (128 bytes + empty tag table)
  const buf = Buffer.alloc(132, 0);
  buf.writeUInt32BE(132, 0);         // Profile size
  buf.write('acsp', 36);             // Magic 'acsp'
  buf.writeUInt8(4, 8);              // Version 4.0
  buf.write('mntr', 12);             // Device class: monitor
  buf.write('RGB ', 16);             // Color space: RGB
  buf.write('XYZ ', 20);             // PCS: XYZ
  // PCS illuminant D50 at bytes 68-79
  buf.writeInt32BE(Math.round(0.9642 * 65536), 68);
  buf.writeInt32BE(Math.round(1.0000 * 65536), 72);
  buf.writeInt32BE(Math.round(0.8249 * 65536), 76);
  buf.writeUInt32BE(0, 128);         // Tag count: 0
  console.log('  (using generated minimal ICC profile for testing)');
  return new Uint8Array(buf);
}

function getClutTestProfile() {
  const testPath = path.join(__dirname, 'Testing', 'sRGB_v4_ICC_preference.icc');
  return new Uint8Array(fs.readFileSync(testPath));
}

(async () => {
  let pass = 0, fail = 0;
  const icc = getTestProfile();
  const clutIcc = getClutTestProfile();

  console.log('iccDEV WASM Tool Tests\n');

  // DumpProfile
  if (await test('IccDumpProfile', require('./IccDumpProfile/iccDumpProfile.js'), mod => {
    mod.FS.writeFile('t.icc', icc);
    mod.callMain(['t.icc', 'ALL']);
  })) pass++; else fail++;

  if (await test('IccDumpProfile (CLUT)', require('./IccDumpProfile/iccDumpProfile.js'), mod => {
    mod.FS.writeFile('clut.icc', clutIcc);
    mod.callMain(['clut.icc', 'ALL']);
  })) pass++; else fail++;

  // ToXml
  if (await test('IccToXml', require('./IccToXml/iccToXml.js'), mod => {
    mod.FS.writeFile('t.icc', icc);
    mod.callMain(['t.icc', 'out.xml']);
    const xml = mod.FS.readFile('out.xml', { encoding: 'utf8' });
    if (!xml.includes('IccProfile')) throw new Error('no XML output');
  })) pass++; else fail++;

  // ToJson
  if (await test('IccToJson', require('./IccToJson/iccToJson.js'), mod => {
    mod.FS.writeFile('t.icc', icc);
    mod.callMain(['t.icc', 'out.json']);
    const json = mod.FS.readFile('out.json', { encoding: 'utf8' });
    if (!json || json.length < 2) throw new Error('no JSON output');
  })) pass++; else fail++;

  // FromJson (generate JSON first, then convert back)
  if (await test('IccFromJson', require('./IccFromJson/iccFromJson.js'), async mod => {
    const toJson = await require('./IccToJson/iccToJson.js')({ noExitRuntime: true, noInitialRun: true, print: () => {}, printErr: () => {} });
    toJson.FS.writeFile('t.icc', icc);
    toJson.callMain(['t.icc', 'out.json']);
    const json = toJson.FS.readFile('out.json');
    mod.FS.writeFile('in.json', json);
    mod.callMain(['in.json', 'out.icc']);
    const result = mod.FS.readFile('out.icc');
    if (result.length < 100) throw new Error('output too small: ' + result.length);
  })) pass++; else fail++;

  // FromXml (generate XML first, then convert back)
  if (await test('IccFromXml', require('./IccFromXml/iccFromXml.js'), async mod => {
    const toXml = await require('./IccToXml/iccToXml.js')({ noExitRuntime: true, noInitialRun: true, print: () => {}, printErr: () => {} });
    toXml.FS.writeFile('t.icc', icc);
    toXml.callMain(['t.icc', 'out.xml']);
    const xml = toXml.FS.readFile('out.xml');
    mod.FS.writeFile('in.xml', xml);
    mod.callMain(['in.xml', 'out.icc']);
    const result = mod.FS.readFile('out.icc');
    if (result.length < 100) throw new Error('output too small: ' + result.length);
  })) pass++; else fail++;

  // RoundTrip
  if (await test('IccRoundTrip', require('./IccRoundTrip/iccRoundTrip.js'), mod => {
    mod.FS.writeFile('t.icc', icc);
    mod.callMain(['t.icc', '1', '0']);
  })) pass++; else fail++;

  // HdrFallback - bake an HDR profile's SDR rendering into an A2B0/B2A0 pair.
  // Driven from XML rather than a stock .icc because no HDR profile is tracked
  // under Testing/; they are all generated. Run at the default grid, which is
  // the size a consumer gets and the one that puts a 33^3 CLUT through the
  // WASM heap, and assert on the tags rather than just on a clean exit.
  if (await test('IccHdrFallback', require('./IccHdrFallback/iccHdrFallback.js'), async mod => {
    const fromXml = await require('./IccFromXml/iccFromXml.js')({ noExitRuntime: true, noInitialRun: true, print: () => {}, printErr: () => {} });
    const hdrXml = fs.readFileSync(path.join(__dirname, 'Testing', 'HDR', 'HagcDisplay.xml'));
    fromXml.FS.writeFile('hdr.xml', hdrXml);
    fromXml.callMain(['hdr.xml', 'hdr.icc']);
    const hdrIcc = fromXml.FS.readFile('hdr.icc');
    mod.FS.writeFile('hdr.icc', hdrIcc);
    // Check the status before reading: the tool refuses a profile it cannot
    // bake (an MPE-based one, say) with a non-zero exit, and the readFile that
    // would follow throws an error carrying no message at all.
    const rc = mod.callMain(['hdr.icc', 'fallback.icc']);
    if (rc) throw new Error('bake exited ' + rc);
    const baked = mod.FS.readFile('fallback.icc');
    if (baked.length <= hdrIcc.length) throw new Error('no tags added: ' + baked.length);
    // Scan for the signatures: the size check above only proves the profile
    // grew, and hagc has to still be there afterwards - the whole point of the
    // bake is that an HDR-aware CMM keeps evaluating the curve.
    const sigs = Buffer.from(baked).toString('latin1');
    for (const sig of ['A2B0', 'B2A0', 'hagc']) {
      if (!sigs.includes(sig)) throw new Error('missing ' + sig + ' tag');
    }
  })) pass++; else fail++;

  // Usage tests - tools that need multiple files, verify they load and print usage
  const usageTools = [
    ['IccApplyNamedCmm', 'IccApplyNamedCmm', 'iccApplyNamedCmm.js'],
    ['IccApplyProfiles', 'IccApplyProfiles', 'iccApplyProfiles.js'],
    ['IccApplySearch', 'IccApplySearch', 'iccApplySearch.js'],
    ['IccApplyToLink', 'IccApplyToLink', 'iccApplyToLink.js'],
    ['IccSpecSepToTiff', 'IccSpecSepToTiff', 'iccSpecSepToTiff.js'],
    ['IccFromCube', 'IccFromCube', 'iccFromCube.js'],
    ['IccPawgReport', 'IccPawgReport', 'iccPawgReport.js'],
  ];
  for (const [name, dir, jsFile] of usageTools) {
    if (await test(name + ' (usage)', require('./' + dir + '/' + jsFile), mod => {
      try { mod.callMain([]); } catch (e) { if (e.name !== 'ExitStatus') throw e; }
    })) pass++; else fail++;
  }

  // V5DspObsToV4Dsp
  if (await test('IccV5DspObsToV4Dsp (usage)', require('./IccV5DspObsToV4Dsp/iccV5DspObsToV4Dsp.js'), mod => {
    try { mod.callMain([]); } catch (e) { if (e.name !== 'ExitStatus') throw e; }
  })) pass++; else fail++;

  // Image dump tools - usage tests
  for (const [name, dir, jsFile] of [
    ['IccTiffDump', 'IccTiffDump', 'iccTiffDump.js'],
    ['IccJpegDump', 'IccJpegDump', 'iccJpegDump.js'],
    ['IccPngDump', 'IccPngDump', 'iccPngDump.js'],
  ]) {
    if (await test(name + ' (usage)', require('./' + dir + '/' + jsFile), mod => {
      try { mod.callMain([]); } catch (e) { if (e.name !== 'ExitStatus') throw e; }
    })) pass++; else fail++;
  }

  console.log('\nResults: ' + pass + ' passed, ' + fail + ' failed out of ' + (pass + fail));
  process.exit(fail > 0 ? 1 : 0);
})();
