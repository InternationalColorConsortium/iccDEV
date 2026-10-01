# IccApplySearch

`iccApplySearch` applies a profile sequence using search against the forward
transform of the last profile. It is useful when a reverse transform is not
available, including spectral PCS workflows.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccApplySearch
```

## Export and Replay Example

`iccApplySearch` supports both configuration export forms:

- `-exportcfg FILE` writes the resolved operation settings.
- `-exportcfganddata FILE` also embeds the input color data, so `-cfg FILE`
  replays without a separate data file.

Run the following command from `Testing/hybrid` after its profiles are built.
It exports a portable configuration and writes transformed text data to
standard output:

```sh
iccApplySearch -exportcfganddata config/cmykGraysEst.json \
  Results/cmykGraysRef.txt 0 1 \
  ICC/Spec380_10_730-D50_2deg.icc 3 \
  ICC/Lab_float-D50_2deg.icc 3 \
  ICC/CMYK_Hybrid_Profile.icc 10003 -INIT 3 \
  ICC/Lab_float-D50_2deg.icc 1 \
  ICC/Lab_float-D93_2deg-MAT.icc 1 \
  ICC/Lab_float-F11_2deg-MAT.icc 1 \
  ICC/Lab_float-IllumA_2deg-MAT.icc 1 \
  > Results/cmykGraysEst.txt

iccApplySearch -cfg config/cmykGraysEst.json > Results/cmykGraysEst-replay.txt
```

`iccApplySearch` accepts `--telemetry=off|human|jsonl`,
`--telemetry-file FILE`, and `--evidence-file FILE` anywhere in the command.
Legacy single-dash spellings remain accepted. Human telemetry uses UTC
timestamps and is written to stderr, so transform data on stdout remains
unchanged. Threaded lifecycle lines report requested and effective threads.
JSONL records
`run_started`, `transform_ready`, and a terminal event; the evidence file
records input/output record counts, requested/effective threads, elapsed time,
and throughput:

```sh
iccApplySearch --telemetry=jsonl --telemetry-file search.jsonl \
  --evidence-file search-evidence.json -cfg config/cmykGraysEst.json \
  > Results/cmykGraysEst-replay.txt \
  2> Results/cmykGraysEst-replay.log
```

## Search Cost and Metamerism Index

The underlying [`CIccCmmSearch`](../../../IccProfLib/IccCmmSearch.h) class
exposes a `GetApplyCost(icFloatNumber& dCost, const icFloatNumber* SrcPixel)`
method that returns the residual cost of an inverse-search apply. The cost
is the weighted average of color differences between the target appearance
(`SrcPixel`) and the appearance the matched device values produce under
each attached Profile Connection Condition (PCC):

```text
cost = (sum over PCC_i of weight_i * || dst_to_pcc_i(dev) - target_i ||) / sum(weight_i)
```

For a single-PCC chain the cost reflects how close the optimizer got to the
exact match (small = near-perfect; large = the target is outside the
device's reachable set under that observation condition).

For a multi-PCC chain - for example, an `iccApplySearch` invocation that
attaches the same chain under D50, D93, F11, and Illuminant A - the cost
is the unavoidable residual after the optimizer trades a perfect match
under one PCC for a better compromise across all. In this role it
functions as an **index of metamerism** for the input reflectance / PCS
value: a low cost means a single device value exists that reproduces the
target across every attached observation condition; a high cost means the
target is metameric - the device must compromise between conditions, and
the cost quantifies that compromise.

`GetApplyCost` is a library-level entry point; the CLI does not currently
print costs alongside the apply output. To obtain costs programmatically,
construct the search CMM via [`CIccConnectCmm::CreateSearch`](../../../docs/icc-connect.md)
(or directly with `CIccCmmSearch::AddXform` + `AttachPCC`), call `Begin()`,
and then call `GetApplyCost` per sample. The method is not thread-safe; it
shares apply state with `Apply()`.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccJSON guide](../../../docs/iccjson.md)
- [IccConnect library](../../../docs/icc-connect.md) - `CreateSearch` factory
  and JSON-driven setup of multi-PCC search chains.
