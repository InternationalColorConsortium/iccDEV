# Issue 2688 Colorant Table PCS Reproduction

`colorantTableType` does not carry its own PCS encoding. ICC.1 defines its
values in the profile header PCS; `colorantTableOutTag` is the DeviceLink Lab
exception. `CIccTagColorantTable::m_PCS` was left uninitialized and neither
profile loading nor attachment supplied this context. `Describe()` therefore
branched on uninitialized memory.

## Unfixed MSan reproduction

These commands retain the branch fixture before checking out the unfixed base:

```bash
git clone -b ci-qa-issue-2686 https://github.com/InternationalColorConsortium/iccDEV.git "$HOME/iccDEV-msan"
cd "$HOME/iccDEV-msan/Build"
cp ../.github/ci/regression/issue-2688-colorant-table-pcs.xml issue-2688.xml
git checkout 805b316c1bf250232fbfea19f17a35aa7f0de43c
git branch&&echo ---&&git log --oneline -1
../.github/scripts/iccdev-build-msan-libcxx.sh --prefix "$PWD/msan-runtime" --jobs 32
export ICCDEV_MSAN_LIBCXX_DIR="$PWD/msan-runtime"
cmake --preset linux-clang-msan -S Cmake -B .
cmake --build . -j32 --target iccFromXml iccDumpProfile
./Tools/IccFromXml/iccFromXml issue-2688.xml issue-2688.icc -noid
export MSAN_OPTIONS='halt_on_error=1:exit_code=86:origin_history_size=7'
./Tools/IccDumpProfile/iccDumpProfile issue-2688.icc ALL
```

Expected: the final command exits 86 with
`MemorySanitizer: use-of-uninitialized-value` in
`CIccTagColorantTable::Describe()`.

The XML SHA-256 is
`9753dd8de0eae4724891fff4df7488b143529e62666c34d22a98bab0c4af10d5`.
It generates a 684-byte ICC profile with SHA-256
`184186aff308698774282accdcb41233a83824499c6ca30fe67f223cdc9aabf1`.

## Fix contract

The constructor gives standalone tables a deterministic Lab fallback.
`CIccProfile` then supplies the profile header PCS for `colorantTableTag` and
Lab for `colorantTableOutTag` when loading or attaching the tag. The patched
MSan reproduction exits 0 and contains:

```text
BEGIN_COLORANTS 3
# NAME XYZ_X XYZ_Y XYZ_Z
```

A `503` from `gitlab.gnome.org` while building the instrumented runtime is an
external transient. Rerun the unchanged runtime-build command.
