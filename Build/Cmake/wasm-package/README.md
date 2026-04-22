# iccdev

ICC color profile CLI tools as WebAssembly. 16 modules, Node.js. Consumer
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

`IccDumpProfile`, `IccToXml`, `IccFromXml`, `IccToJson`, `IccFromJson`,
`IccRoundTrip`, `IccFromCube`, `IccApplyNamedCmm`, `IccApplyProfiles`,
`IccApplySearch`, `IccApplyToLink`, `IccTiffDump`, `IccJpegDump`,
`IccPngDump`, `IccSpecSepToTiff`, `IccV5DspObsToV4Dsp`.

Tool reference: https://github.com/xsscx/research/tree/main/docs/iccDEV/Tools

## Test

```bash
node test_all.js
```

## Data

`Testing/` ships canonical ICC test profiles (`*.icc`, `*.json`, `*.xml`).

## License

BSD-3-Clause.
