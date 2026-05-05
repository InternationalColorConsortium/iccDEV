---
name: Bug report
about: Report a bug or crash
title: ''
labels: 'bug'
assignees: ''
---

## Describe the Bug

## Build Instructions

Please include your Build Instructions.

Example:

```
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV
vcpkg integrate install
vcpkg install
cmake --preset vs2022-x64 -B msvc -S Build/Cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build msvc -- /m /maxcpucount
```

## Reproduce Bug or Crash

1. Run `<command>`
2. Observe `<output or behavior>`

## Expected Behavior

## Environment

- OS:
- iccDEV version or commit:
- Compiler/toolchain:
- Shell:

## Additional Context
