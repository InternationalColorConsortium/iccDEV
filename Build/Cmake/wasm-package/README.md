# iccdev

ICC color profile CLI tools as WebAssembly. 17 modules, Node.js. Consumer
package only - no build, no browser UI, and no legacy `wasm/*.html`,
`wasm/*.css`, or `wasm/*.js` assets.

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

## Maintainer release publishing

The manual `ci-latest-release.yml` workflow can publish the staged package
with npm OIDC trusted publishing. Select `iccdev` for `npm_packages` and enter
an unpublished SemVer value without build metadata for `npm_version`. Use
`npm_dist_tag=latest` for a stable release. Prerelease versions require a
non-`latest` npm dist-tag such as `next`. Dispatches outside `master` do not
stage or publish an npm release.

Before the first publication, an npm package administrator must configure
Trusted Publisher with GitHub Actions, organization
`InternationalColorConsortium`, repository `iccDEV`, workflow filename
`ci-latest-release.yml`, and environment `release`. Allow direct `npm publish`.
The workflow uses Node 24, npm's OIDC authentication, provenance, no npm token,
and then retrieves and verifies the exact public package tarball after all
selected package publications. This diagnostic smoke test records its outcome
without blocking other publication lanes during registry propagation.

## Data

`Testing/` ships canonical ICC test profiles (`*.icc`, `*.json`, `*.xml`).

## License

BSD-3-Clause.
