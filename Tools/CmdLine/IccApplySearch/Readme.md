# IccApplySearch

`iccApplySearch` applies a profile sequence using search against the forward
transform of the last profile. It is useful when a reverse transform is not
available, including spectral PCS workflows.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccApplySearch
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
