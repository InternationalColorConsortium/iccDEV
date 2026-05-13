# iccdev

ICC color profile CLI tools as WebAssembly. 17 modules, Node.js. Consumer
package only - no build, no browser UI.

## Install

```bash
npm install iccdev
```

## Use

```js
const { IccDumpProfile } = require('iccdev');

(async () => {
  const mod = await IccDumpProfile({ noExitRuntime: true, noInitialRun: true });
  mod.FS.writeFile('p.icc', new Uint8Array(require('fs').readFileSync('input.icc')));
  mod.callMain(['p.icc', 'ALL']);
})();
```

## Modules

`IccDumpProfile`, `IccPawgReport`, `IccToXml`, `IccFromXml`, `IccToJson`, `IccFromJson`,
`IccRoundTrip`, `IccFromCube`, `IccApplyNamedCmm`, `IccApplyProfiles`,
`IccApplySearch`, `IccApplyToLink`, `IccTiffDump`, `IccJpegDump`,
`IccPngDump`, `IccSpecSepToTiff`, `IccV5DspObsToV4Dsp`.

Tool reference: ../../../docs/tools-cli-reference.md

## Test

```bash
node test_all.js
```

## Data

`Testing/` ships canonical ICC test profiles (`*.icc`, `*.json`, `*.xml`).

## License

BSD-3-Clause.
