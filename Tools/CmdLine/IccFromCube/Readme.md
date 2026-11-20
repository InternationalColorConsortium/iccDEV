# IccFromCube

`iccFromCube` converts a `.cube` 3D LUT into an ICC.2 DeviceLink profile for
ICC-managed workflows.

The reader accepts the Adobe Cube LUT Specification 1.0 `DOMAIN_MIN` and
`DOMAIN_MAX` keywords and the legacy `LUT_3D_INPUT_RANGE` extension. Standard
Cube rows are interpreted with red varying fastest, then green, then blue.
Every finite non-default input-domain endpoint is preserved in the generated
ICC.2 curve set, including values close to the default 0-to-1 domain.
The DaVinci Resolve `LUT_IN_VIDEO_RANGE` and `LUT_OUT_VIDEO_RANGE` extensions
require video-range scaling that this tool does not implement. They are
rejected explicitly instead of being accepted and silently ignored.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccFromCube
```

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [IccApplyToLink](../IccApplyToLink/Readme.md)
