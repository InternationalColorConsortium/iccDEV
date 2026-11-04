# ApplyTools JSON and Positional CLI Parity

This document maps the JSON configuration objects used by
`iccApplyProfiles`, `iccApplyNamedCmm`, and `iccApplySearch` to their direct
`argc`/`argv` forms. It also identifies configuration shapes that cannot be
represented losslessly as positional arguments.

The source of truth is the `CIccCfg*::fromArgs()` and `fromJson()` pair in
`IccConnect/IccLibConnect/IccCmmConfig.cpp`, together with each tool's option
parser and CMM factory. A JSON field is not proven supported merely because it
parses or round-trips: it must reach the runtime consumer.

## Parity terms

- Exact: the positional command selects the same input, output, transform, and
  runtime options as the JSON command.
- Replay-equivalent: the transform is the same, but JSON embeds color records
  while the positional form reads the original external data file.
- Config-only: no positional slot exists for one or more required fields. Use
  `--cfg` rather than dropping those fields.
- Parsed-only: the field is accepted or exported but is not consumed by the
  current runtime path. This is a defect or an explicitly documented
  limitation, not positional parity.

## `iccApplyProfiles`

The positional shape is:

```text
iccApplyProfiles {global options} {--exportcfg FILE} \
  SRC_TIFF DST_TIFF ENCODING COMPRESSION PLANAR EMBED INTERPOLATION \
  {{-ENV:SIG VALUE} PROFILE INTENT {-PCC PCC_PROFILE}}
```

### Image fields

| JSON field | Positional representation | Status |
| --- | --- | --- |
| `srcImageFile` | `SRC_TIFF` | Exact |
| `dstImageFile` | `DST_TIFF` | Exact |
| `srcIfd` | `--ifd N` | Exact; CLI overrides JSON |
| `dstEncoding: sameAsSource/8Bit/16Bit/float` | `0/1/2/3` | Exact |
| `dstCompression: false/true` | `0/1` | Exact |
| `dstPlanar: false/true` | `0/1` | Exact |
| `dstEmbedIcc: false/true` | `0/1` | Exact |
| `dstCompression`, `dstPlanar`, or `dstEmbedIcc: sameAsSource` | None | Config-only |
| `dstTileWidth` plus `dstTileHeight` | None | Config-only |
| `dstBigTiff` | None | Config-only |
| `connect.threads` | `--threads N` | Exact; CLI overrides JSON |
| `connect.useSearch` | None | Config-only |

### Profile fields

The positional interpolation selector is global to the profile sequence.
JSON can select `interpolation` independently on every stage, so a mixed
linear/tetrahedral sequence is config-only.

The packed `INTENT` uses the units digit for rendering intent and other decimal
columns for transform and flags. The current usage screen is the concise
reference. Common values are:

| JSON setting | Packed CLI form |
| --- | --- |
| perceptual, relative, saturation, absolute | `0`, `1`, `2`, `3` |
| no D2Bx/B2Dx | add `10` |
| BPC | add `40` instead of the default transform column |
| PCS-luminance adjustment | add `1000` |
| V5 sub-profile | add `10000` |
| HToS selector | add `100000` |
| named-color over black or gray | add `1000000` or `2000000` |

Only the first positional stage recognizes `-embedded`. JSON can spell an
empty first `iccFile` for the same behavior. `-ENV:SIG VALUE` maps to
`iccEnvVars`, and `-PCC FILE` maps to `pccFile` for the preceding profile.
There is no positional spelling for `pccEnvVars`.

`useHToS` currently parses and exports but has no downstream `AddXform()`
parameter. Do not treat an HToS round-trip as proof that the transform used the
selector.

### Inverse image search

An image search configuration requires:

```json
{
  "imageFiles": {},
  "connect": {"useSearch": true},
  "searchApply": {
    "profileSequence": [],
    "initial": {},
    "pccWeights": []
  }
}
```

The direct `iccApplyProfiles` grammar has no initializer or weighted-PCC
positions. This shape is intentionally config-only:

```sh
iccApplyProfiles --threads 0 --cfg config/image-search.json
```

Do not translate it into a forward profile sequence; that is a different CMM.

## `iccApplyNamedCmm`

The positional shape is:

```text
iccApplyNamedCmm {--exportcfg|--exportcfganddata FILE} {--debugcalc} \
  DATA_FILE ENCODING[:PRECISION[:DIGITS]] INTERPOLATION \
  {{-ENV:SIG VALUE} PROFILE INTENT {-PCC PCC_PROFILE}}
```

The direct form always reads legacy text data and writes legacy text to stdout.
The following are config-only as argv shapes:

- inline or external `colorData` input;
- IT8 input or output;
- `srcSpace` override;
- `dstType` and `dstFile`;
- per-stage interpolation; and
- per-stage `pccEnvVars`.

`--exportcfganddata` bridges the first limitation by embedding records read
from a legacy file into a replayable JSON document. The original positional
command and the JSON replay are transform-equivalent, but their data transport
is not the same argv shape.

## `iccApplySearch`

The positional shape is:

```text
iccApplySearch {--threads N} \
  {--exportcfg|--exportcfganddata FILE} {--debugcalc} \
  DATA_FILE ENCODING[:PRECISION[:DIGITS]] INTERPOLATION \
  PROFILE1 INTENT1 {MIDDLE_PROFILE MIDDLE_INTENT} PROFILE2 INTENT2 \
  -INIT INIT_INTENT {PCC_PROFILE WEIGHT ...}
```

The data transport limitations are the same as `iccApplyNamedCmm`. Positional
search can represent two or three profile stages, one global interpolation,
an initializer, and a weighted PCC list.

The initializer API supports intent, transform type, interpolation,
D2Bx/B2Dx selection, a PCC, and V5 sub-profile selection. It has no BPC member
or xform-hint channel. The packed BPC column is therefore rejected in the
`-INIT` position; BPC remains a valid parser field on ordinary profile stages.

Current runtime caveats requiring separate behavioral QA:

- `searchApply.initial.adjustPcsLuminance` is present in the schema and config
  object, but the initial-profile API has no consumer for the hint.
- JSON can select `searchApply.initial.interpolation` independently of the final
  profile, but `CreateSearch()` currently passes the final profile's
  interpolation to `SetDstInitProfile()` instead of the initial value.
- File-backed search stages accept `useBPC` and `adjustPcsLuminance`, while
  `CreateSearch()` currently attaches those hints only to an embedded first
  stage.
- Exit status or JSON round-trip alone does not prove either setting was used.

## Hybrid reconstruction

Run the examples from `Testing/hybrid`. Use `--threads 0` for local hardware
concurrency.

| JSON file | Parity | Direct command source |
| --- | --- | --- |
| `makeMS_smCows.json` | Exact | Phase 2 of `BuildAndTest.sh` |
| `makeCowsA_fromRef.json` | Exact | Phase 3 |
| `makeCowsA_fromMS.json` | Exact | Phase 3 |
| `makeCowsF11_fromRef.json` | Exact | Phase 3 |
| `makeCowsF11_fromMS.json` | Exact | Phase 3 |
| `cmykGraysRef.json` | Replay-equivalent | Phase 5 |
| `cmykGraysEst.json` | Replay-equivalent | Phase 5 |
| `TShirtDesignPrevUW-W.json` | Exact | Phase 6 |
| `msCowsIconToCmyk.json` | Config-only | Phase 5 image search |
| `msCowsToCmyk.json` | Config-only | `SpectralImageReproduction.sh` |

The eight direct reconstructions are:

```sh
iccApplyProfiles --threads 0 Data/smCows380_5_780.tif Results/MS_smCows.tif 2 1 0 1 1 -embedded 3 ICC/MultSpectralRGB.icc 10003
iccApplyProfiles --threads 0 Data/smCows380_5_780.tif Results/cowsA_fromRef.tif 1 1 0 1 1 -embedded 3 -PCC ICC/Spec400_10_700-IllumA_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles --threads 0 Results/MS_smCows.tif Results/cowsA_fromMS.tif 1 1 0 1 1 -embedded 10003 -PCC ICC/Spec400_10_700-IllumA_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles --threads 0 Data/smCows380_5_780.tif Results/cowsF11_fromRef.tif 1 1 0 1 1 -embedded 3 -PCC ICC/Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles --threads 0 Results/MS_smCows.tif Results/cowsF11_fromMS.tif 1 1 0 1 1 -embedded 10003 -PCC ICC/Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyNamedCmm Data/cmykGrays.txt 3 1 ICC/CMYK_Hybrid_Profile.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3 > Results/cmykGraysRef.txt
iccApplySearch --threads 0 Results/cmykGraysRef.txt 0 1 ICC/Spec380_10_730-D50_2deg.icc 3 ICC/Lab_float-D50_2deg.icc 3 ICC/CMYK_Hybrid_Profile.icc 10003 -INIT 3 ICC/Lab_float-D50_2deg.icc 1 ICC/Lab_float-D93_2deg-MAT.icc 1 ICC/Lab_float-F11_2deg-MAT.icc 1 ICC/Lab_float-IllumA_2deg-MAT.icc 1 > Results/cmykGraysEst.txt
iccApplyProfiles --threads 0 Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUW-W.tif 1 1 0 0 0 -embedded 10001 ../sRGB_v4_ICC_preference.icc 1
```

The two config-only invocations are:

```sh
iccApplyProfiles --threads 0 --cfg config/msCowsIconToCmyk.json
iccApplyProfiles --threads 0 --cfg config/msCowsToCmyk.json
```

## Validation

Build with tests enabled, then run the focused parser and tool contracts:

```sh
cmake -S Build/Cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_TESTS=ON -DENABLE_WXWIDGETS=OFF
cmake --build build --parallel
ctest --test-dir build -R 'iccdev\.(iccconnect-config-parser|applynamedcmm-cli-args|applysearch-cli-args|applyprofiles-cli-args|json-cfg|hybrid-pipeline)$' --output-on-failure
```

For direct script reproduction:

```sh
ICCDEV_TOOLS_DIR=$PWD/build/Tools ICCDEV_TESTING_DIR=$PWD/Testing .github/scripts/iccdev-applysearch-cli-args-regression.sh
```

When a parity claim changes, verify all four edges:

1. JSON parse into the `CIccCfg*` member.
2. Positional decode into the same member, or explicit rejection.
3. Export/serialization back to the documented key.
4. Runtime consumption by the CMM factory or output writer.

