# iccdev-mcp -- Python-native ICC helpers
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""Small Python-native ICC profile interface used by iccdev-mcp.

This module provides the import surface expected by the MCP server without
requiring an unpublished external ``iccdev`` wheel. It parses ICC headers
directly and provides deterministic identity transforms for REST/MCP smoke
testing of Python-native endpoints.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
from enum import IntEnum
from pathlib import Path
import struct

_HEADER_SIZE = 128


def _sig(value: str) -> int:
    return int.from_bytes(value.encode("ascii"), "big")


def sig_to_str(signature: int) -> str:
    """Convert a 32-bit ICC signature integer to a 4-character string."""
    if not isinstance(signature, int):
        raise TypeError("signature must be an integer")
    if signature < 0 or signature > 0xFFFFFFFF:
        raise ValueError("signature must fit in 32 bits")
    return signature.to_bytes(4, "big").decode("latin-1")


class RenderingIntent(IntEnum):
    Perceptual = 0
    RelativeColorimetric = 1
    Saturation = 2
    AbsoluteColorimetric = 3


class Interpolation(IntEnum):
    Linear = 0
    Tetrahedral = 1


class ColorSpace(IntEnum):
    XYZ = _sig("XYZ ")
    Lab = _sig("Lab ")
    Luv = _sig("Luv ")
    YCbCr = _sig("YCbr")
    Yxy = _sig("Yxy ")
    RGB = _sig("RGB ")
    GRAY = _sig("GRAY")
    HSV = _sig("HSV ")
    HLS = _sig("HLS ")
    CMYK = _sig("CMYK")
    CMY = _sig("CMY ")
    MCH1 = _sig("MCH1")
    MCH2 = _sig("MCH2")
    MCH3 = _sig("MCH3")
    MCH4 = _sig("MCH4")
    MCH5 = _sig("MCH5")
    MCH6 = _sig("MCH6")
    MCH7 = _sig("MCH7")
    MCH8 = _sig("MCH8")
    MCH9 = _sig("MCH9")
    MCHA = _sig("MCHA")
    MCHB = _sig("MCHB")
    MCHC = _sig("MCHC")
    MCHD = _sig("MCHD")
    MCHE = _sig("MCHE")
    MCHF = _sig("MCHF")
    Named = _sig("nmcl")
    One = _sig("1CLR")
    Two = _sig("2CLR")
    Three = _sig("3CLR")
    Four = _sig("4CLR")
    Five = _sig("5CLR")


_COLOR_SPACE_NAMES = {
    "XYZ ": "XYZ",
    "Lab ": "Lab",
    "Luv ": "Luv",
    "YCbr": "YCbCr",
    "Yxy ": "Yxy",
    "RGB ": "RGB",
    "GRAY": "Gray",
    "HSV ": "HSV",
    "HLS ": "HLS",
    "CMYK": "CMYK",
    "CMY ": "CMY",
    "nmcl": "Named",
}

_DEVICE_CLASS_NAMES = {
    "scnr": "Input",
    "mntr": "Display",
    "prtr": "Output",
    "link": "DeviceLink",
    "spac": "ColorSpace",
    "abst": "Abstract",
    "nmcl": "NamedColor",
}

_PLATFORM_NAMES = {
    "APPL": "Apple",
    "MSFT": "Microsoft",
    "SGI ": "Silicon Graphics",
    "SUNW": "Sun",
}

_RENDERING_INTENT_NAMES = {
    0: "Perceptual",
    1: "Media-Relative Colorimetric",
    2: "Saturation",
    3: "ICC-Absolute Colorimetric",
}

_CHANNEL_COUNTS = {
    "XYZ ": 3,
    "Lab ": 3,
    "Luv ": 3,
    "YCbr": 3,
    "Yxy ": 3,
    "RGB ": 3,
    "GRAY": 1,
    "HSV ": 3,
    "HLS ": 3,
    "CMYK": 4,
    "CMY ": 3,
    "nmcl": 1,
}


@dataclass(frozen=True)
class IccHeader:
    size: int
    cmm_type: str
    version: int
    device_class: str
    color_space: str
    pcs: str
    date_time: tuple[int, int, int, int, int, int]
    magic: str
    platform: str
    flags: int
    manufacturer: str
    model: str
    attributes: int
    rendering_intent: int
    illuminant: tuple[float, float, float]
    creator: str
    profile_id: bytes
    color_space_name: str
    pcs_name: str
    device_class_name: str
    version_string: str
    rendering_intent_name: str
    platform_name: str

    def _asdict(self) -> dict:
        return asdict(self)


class IccProfile:
    """Parsed ICC profile with header metadata."""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        data = self.path.read_bytes()
        if len(data) < _HEADER_SIZE:
            raise ValueError("ICC profile is shorter than the 128-byte header")
        self.header = _parse_header(data[:_HEADER_SIZE])
        self.is_v5 = ((self.header.version >> 24) & 0xFF) >= 5

    def close(self) -> None:
        return None


def open_profile(path: str | Path) -> IccProfile:
    """Open and parse an ICC profile header."""
    return IccProfile(path)


class IccCmm:
    """Deterministic identity CMM used for Python-native MCP endpoints."""

    def __init__(self):
        self._profiles: list[IccProfile] = []
        self.src_channels = 0
        self.dst_channels = 0

    def attach(
        self,
        path: str | Path,
        intent: RenderingIntent = RenderingIntent.Perceptual,
        interp: Interpolation = Interpolation.Tetrahedral,
    ) -> None:
        del intent, interp
        self._profiles.append(open_profile(path))

    def begin(self) -> None:
        if not self._profiles:
            raise ValueError("at least one profile must be attached")
        first = self._profiles[0].header.color_space
        last = self._profiles[-1].header.color_space
        self.src_channels = _channel_count(first)
        self.dst_channels = _channel_count(last)

    def apply_multi(self, pixels: list[list[float]]) -> list[list[float]]:
        if self.dst_channels <= 0:
            self.begin()
        transformed: list[list[float]] = []
        for pixel in pixels:
            values = [float(value) for value in pixel]
            if len(values) >= self.dst_channels:
                transformed.append(values[:self.dst_channels])
            else:
                transformed.append(values + [0.0] * (self.dst_channels - len(values)))
        return transformed

    def close(self) -> None:
        for profile in self._profiles:
            profile.close()
        self._profiles = []


def _parse_header(data: bytes) -> IccHeader:
    size = _u32(data, 0)
    cmm_type = _ascii_sig(data, 4)
    version = _u32(data, 8)
    device_class = _ascii_sig(data, 12)
    color_space = _ascii_sig(data, 16)
    pcs = _ascii_sig(data, 20)
    date_time = struct.unpack(">6H", data[24:36])
    magic = _ascii_sig(data, 36)
    if magic != "acsp":
        raise ValueError("ICC header magic is not acsp")
    platform = _ascii_sig(data, 40)
    flags = _u32(data, 44)
    manufacturer = _ascii_sig(data, 48)
    model = _ascii_sig(data, 52)
    attributes = struct.unpack(">Q", data[56:64])[0]
    rendering_intent = _u32(data, 64)
    illuminant = (
        _s15_fixed16(data, 68),
        _s15_fixed16(data, 72),
        _s15_fixed16(data, 76),
    )
    creator = _ascii_sig(data, 80)
    profile_id = data[84:100]
    return IccHeader(
        size=size,
        cmm_type=cmm_type,
        version=version,
        device_class=device_class,
        color_space=color_space,
        pcs=pcs,
        date_time=date_time,
        magic=magic,
        platform=platform,
        flags=flags,
        manufacturer=manufacturer,
        model=model,
        attributes=attributes,
        rendering_intent=rendering_intent,
        illuminant=illuminant,
        creator=creator,
        profile_id=profile_id,
        color_space_name=_COLOR_SPACE_NAMES.get(color_space, color_space.strip()),
        pcs_name=_COLOR_SPACE_NAMES.get(pcs, pcs.strip()),
        device_class_name=_DEVICE_CLASS_NAMES.get(device_class, device_class.strip()),
        version_string=_version_string(version),
        rendering_intent_name=_RENDERING_INTENT_NAMES.get(
            rendering_intent, str(rendering_intent)
        ),
        platform_name=_PLATFORM_NAMES.get(platform, platform.strip()),
    )


def _ascii_sig(data: bytes, offset: int) -> str:
    return data[offset:offset + 4].decode("latin-1")


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack(">I", data[offset:offset + 4])[0]


def _s15_fixed16(data: bytes, offset: int) -> float:
    return struct.unpack(">i", data[offset:offset + 4])[0] / 65536.0


def _version_string(version: int) -> str:
    major = (version >> 24) & 0xFF
    minor = (version >> 20) & 0x0F
    bugfix = (version >> 16) & 0x0F
    return f"{major}.{minor}.{bugfix}"


def _channel_count(color_space: str) -> int:
    if color_space in _CHANNEL_COUNTS:
        return _CHANNEL_COUNTS[color_space]
    if len(color_space) == 4 and color_space[1:] == "CLR" and color_space[0].isdigit():
        return int(color_space[0])
    if len(color_space) == 4 and color_space.startswith("MCH"):
        suffix = color_space[3]
        if suffix.isdigit():
            return int(suffix)
        return 10 + ord(suffix.upper()) - ord("A")
    return 3
