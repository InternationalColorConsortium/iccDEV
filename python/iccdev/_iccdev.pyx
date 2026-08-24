# _iccdev.pyx - Cython bindings for IccProfLib C Wrapper API
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.
#
# cython: language_level=3
# cython: cdivision=False

"""
Cython extension wrapping the IccProfLib C Wrapper API.

Provides:
- IccProfile: Read and inspect ICC profile headers
- IccCmm: Build multi-profile color transformation pipelines
- IccApply: Thread-safe per-thread apply handles
- NumPy-compatible array transforms via buffer protocol
"""

from libc.stdlib cimport malloc, free
from libc.string cimport memset, memcpy
from cpython.bytes cimport PyBytes_AsString

cimport iccdev.cicc_wrapper as cicc

import enum
import os
from collections import namedtuple

cimport cython

# Optional NumPy for zero-copy array transforms
try:
    import numpy as _np
    HAS_NUMPY = True
except ImportError:
    _np = None
    HAS_NUMPY = False


# ---------------------------------------------------------------------------
# Python Enum wrappers
# ---------------------------------------------------------------------------

class ColorSpace(enum.IntEnum):
    """ICC color space signatures (icColorSpaceSignature).

    >>> ColorSpace.RGB
    <ColorSpace.RGB: 1380401696>
    >>> ColorSpace.from_sig(0x52474220)
    <ColorSpace.RGB: 1380401696>
    """
    XYZ       = cicc.icSigXYZData
    Lab       = cicc.icSigLabData
    Luv       = cicc.icSigLuvData
    YCbCr     = cicc.icSigYCbCrData
    Yxy       = cicc.icSigYxyData
    RGB       = cicc.icSigRgbData
    Gray      = cicc.icSigGrayData
    HSV       = cicc.icSigHsvData
    HLS       = cicc.icSigHlsData
    CMYK      = cicc.icSigCmykData
    CMY       = cicc.icSigCmyData
    CLR1      = cicc.icSig1colorData
    CLR2      = cicc.icSig2colorData
    CLR3      = cicc.icSig3colorData
    CLR4      = cicc.icSig4colorData
    CLR5      = cicc.icSig5colorData
    CLR6      = cicc.icSig6colorData
    CLR7      = cicc.icSig7colorData
    CLR8      = cicc.icSig8colorData
    CLR9      = cicc.icSig9colorData
    CLR10     = cicc.icSig10colorData
    CLR11     = cicc.icSig11colorData
    CLR12     = cicc.icSig12colorData
    CLR13     = cicc.icSig13colorData
    CLR14     = cicc.icSig14colorData
    CLR15     = cicc.icSig15colorData
    Named     = cicc.icSigNamedData
    NChannel  = cicc.icSigNChannelData
    DevLab    = cicc.icSigDevLabData
    DevXYZ    = cicc.icSigDevXYZData
    Gamut     = cicc.icSigGamutData
    Unknown   = cicc.icSigUnknownData

    @classmethod
    def from_sig(cls, int sig):
        """Create from raw 4-byte signature, returning Unknown for unrecognized."""
        try:
            return cls(sig)
        except ValueError:
            return cls.Unknown


class ProfileClass(enum.IntEnum):
    """ICC profile/device class signatures (icProfileClassSignature)."""
    Input        = cicc.icSigInputClass
    Display      = cicc.icSigDisplayClass
    Output       = cicc.icSigOutputClass
    Link         = cicc.icSigLinkClass
    Abstract     = cicc.icSigAbstractClass
    ColorSpace   = cicc.icSigColorSpaceClass
    NamedColor   = cicc.icSigNamedColorClass
    ColorEncoding = cicc.icSigColorEncodingClass
    MaterialID   = cicc.icSigMultiplexIdentificationClass
    MaterialLink = cicc.icSigMultiplexLinkClass
    MaterialVis  = cicc.icSigMultiplexVisualizationClass


class RenderingIntent(enum.IntEnum):
    """ICC rendering intents (icRenderingIntent)."""
    Perceptual          = cicc.icPerceptual
    RelativeColorimetric = cicc.icRelativeColorimetric
    Saturation          = cicc.icSaturation
    AbsoluteColorimetric = cicc.icAbsoluteColorimetric

# Short alias
Intent = RenderingIntent


class Interpolation(enum.IntEnum):
    """CMM interpolation methods (icXformInterp)."""
    Linear      = cicc.icInterpLinear
    Tetrahedral = cicc.icInterpTetrahedral


class LutType(enum.IntEnum):
    """CMM LUT transform types (icXformLutType)."""
    Color        = cicc.icXformLutColor
    NamedColor   = cicc.icXformLutNamedColor
    Preview      = cicc.icXformLutPreview
    Gamut        = cicc.icXformLutGamut
    BPC          = cicc.icXformLutBPC
    BRDFParam    = cicc.icXformLutBRDFParam
    BRDFDirect   = cicc.icXformLutBRDFDirect
    BRDFMcsParam = cicc.icXformLutBRDFMcsParam
    MCS          = cicc.icXformLutMCS
    Colorimetric = cicc.icXformLutColorimetric
    Spectral     = cicc.icXformLutSpectral


class CmmStatus(enum.IntEnum):
    """CMM operation status codes (icStatusCMM)."""
    Bad                = cicc.icCmmStatBad
    Ok                 = cicc.icCmmStatOk
    CantOpenProfile    = cicc.icCmmStatCantOpenProfile
    BadSpaceLink       = cicc.icCmmStatBadSpaceLink
    InvalidProfile     = cicc.icCmmStatInvalidProfile
    BadXform           = cicc.icCmmStatBadXform
    InvalidLut         = cicc.icCmmStatInvalidLut
    ProfileMissingTag  = cicc.icCmmStatProfileMissingTag
    ColorNotFound      = cicc.icCmmStatColorNotFound
    IncorrectApply     = cicc.icCmmStatIncorrectApply
    BadColorEncoding   = cicc.icCmmStatBadColorEncoding
    AllocErr           = cicc.icCmmStatAllocErr
    BadLutType         = cicc.icCmmStatBadLutType
    IdentityXform      = cicc.icCmmStatIdentityXform
    UnsupportedPcsLink = cicc.icCmmStatUnsupportedPcsLink
    BadConnection      = cicc.icCmmStatBadConnection
    BadTintXform       = cicc.icCmmStatBadTintXform
    TooManySamples     = cicc.icCmmStatTooManySamples
    BadMCSLink         = cicc.icCmmStatBadMCSLink
    Unsupported        = cicc.icCmmStatUnsupported


class ValidationStatus(enum.IntEnum):
    """ICC profile validation status returned by :func:`validate_profile`."""
    OK               = cicc.ICC_VALIDATION_OK
    WARNING          = cicc.ICC_VALIDATION_WARNING
    NON_COMPLIANT    = cicc.ICC_VALIDATION_NON_COMPLIANT
    CRITICAL_ERROR   = cicc.ICC_VALIDATION_CRITICAL_ERROR
    INVALID_ARGUMENT = cicc.ICC_VALIDATION_INVALID_ARGUMENT
    INTERNAL_ERROR   = cicc.ICC_VALIDATION_INTERNAL_ERROR


ValidationResult = namedtuple('ValidationResult', ['status', 'report'])


def _validation_status_from_code(int status):
    """Map a native validation status, handling ABI-version mismatches."""
    try:
        return ValidationStatus(status)
    except ValueError:
        return ValidationStatus.INTERNAL_ERROR


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class IccError(Exception):
    """Base exception for ICC library errors.

    Attributes
    ----------
    status : int
        CMM status code (-1 if not a CMM error).
    status_name : str
        Human-readable status name.
    """

    def __init__(self, str message, int status=-1):
        self.status = status
        try:
            self.status_name = CmmStatus(status).name
        except ValueError:
            self.status_name = f"unknown({status})"
        # Keep backward compat alias
        self.status_code = status
        super().__init__(message)


class IccProfileError(IccError):
    """Raised for profile open/read/header errors."""
    pass


class IccCmmError(IccError):
    """Raised for CMM pipeline errors (attach, begin, apply)."""
    pass


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

cdef inline void _check_status(cicc.icStatusCMM stat, str context) except *:
    """Raise IccCmmError if CMM status is not OK."""
    if stat != cicc.icCmmStatOk:
        try:
            name = CmmStatus(stat).name
        except ValueError:
            name = f"code({stat})"
        raise IccCmmError(f"{context}: {name}", <int>stat)


cdef inline bytes _encode_path(path):
    """Convert str, bytes, or os.PathLike to bytes for C API."""
    if isinstance(path, bytes):
        if b'\x00' in <bytes>path:
            raise ValueError("Path must not contain null bytes")
        return <bytes>path
    if hasattr(path, '__fspath__'):
        path = os.fspath(path)
    if isinstance(path, str):
        if '\x00' in path:
            raise ValueError("Path must not contain null bytes")
        return path.encode('utf-8')
    raise TypeError(f"expected str, bytes, or os.PathLike, got {type(path).__name__}")


def sig_to_str(unsigned int sig):
    """Convert a 4-byte ICC signature to a readable 4-char ASCII string.

    >>> sig_to_str(0x52474220)
    'RGB'
    """
    cdef bytes b = bytes([
        (sig >> 24) & 0xFF,
        (sig >> 16) & 0xFF,
        (sig >> 8) & 0xFF,
        sig & 0xFF,
    ])
    return b.decode('ascii', errors='replace').strip()


def validate_profile(profile):
    """Validate ICC profile bytes without invoking a command-line tool.

    Returns a :class:`ValidationResult` containing the native validation
    status and report. Malformed profiles return a non-OK status rather than
    raising an exception.
    """
    cdef bytes profile_bytes
    cdef const unsigned char *profile_data
    cdef char report[8192]
    cdef cicc.icc_validation_status status

    if isinstance(profile, bytes):
        profile_bytes = profile
    elif isinstance(profile, (bytearray, memoryview)):
        profile_bytes = bytes(profile)
    else:
        raise TypeError(
            f"expected bytes-like ICC profile data, got {type(profile).__name__}"
        )

    memset(report, 0, sizeof(report))
    profile_data = <const unsigned char *>PyBytes_AsString(profile_bytes)
    status = cicc.icc_validate_profile(
        profile_data, len(profile_bytes), report, sizeof(report))
    return ValidationResult(
        _validation_status_from_code(<int>status),
        (<bytes>report).decode('utf-8', errors='replace'),
    )


def validate_profile_file(path):
    """Read and validate an ICC profile file using :func:`validate_profile`."""
    with open(path, 'rb') as profile_file:
        return validate_profile(profile_file.read())


# ---------------------------------------------------------------------------
# Profile Header (immutable named tuple with properties)
# ---------------------------------------------------------------------------

_HeaderFields = [
    'size', 'cmm_id', 'version', 'device_class', 'color_space',
    'pcs', 'date', 'magic', 'platform', 'flags', 'manufacturer',
    'model', 'attributes', 'rendering_intent', 'illuminant',
    'creator', 'profile_id',
    'spectral_pcs', 'spectral_range', 'bi_spectral_range',
    'mcs', 'device_sub_class',
]

_IccProfileHeaderBase = namedtuple('IccProfileHeader', _HeaderFields)


class IccProfileHeader(_IccProfileHeaderBase):
    """Immutable ICC profile header snapshot (128-byte structure).

    All signature fields are raw integers. Use :class:`ColorSpace`,
    :class:`ProfileClass`, etc. to decode them. The ``illuminant`` and
    ``date`` fields are dicts.

    This is an immutable named tuple - it can be used as a dict key,
    compared with ``==``, and unpacked.

    >>> hdr.version_string
    '4.3.0'
    >>> hdr.color_space_name
    'RGB'
    """
    __slots__ = ()

    @property
    def version_string(self):
        """Human-readable version (e.g., '4.3.0' or '5.0.0')."""
        major = (self.version >> 24) & 0xFF
        minor = (self.version >> 20) & 0xF
        bugfix = (self.version >> 16) & 0xF
        return f"{major}.{minor}.{bugfix}"

    @property
    def color_space_name(self):
        """Color space as a human-readable string."""
        try:
            return ColorSpace(self.color_space).name
        except ValueError:
            return sig_to_str(self.color_space)

    @property
    def pcs_name(self):
        """Profile Connection Space as a human-readable string."""
        try:
            return ColorSpace(self.pcs).name
        except ValueError:
            return sig_to_str(self.pcs)

    @property
    def device_class_name(self):
        """Device class as a human-readable string."""
        try:
            return ProfileClass(self.device_class).name
        except ValueError:
            return sig_to_str(self.device_class)

    @property
    def rendering_intent_name(self):
        """Rendering intent as a human-readable string."""
        try:
            return RenderingIntent(self.rendering_intent).name
        except ValueError:
            return f"intent({self.rendering_intent})"

    @property
    def platform_name(self):
        """Platform signature as a string."""
        return sig_to_str(self.platform)

    def __repr__(self):
        return (
            f"IccProfileHeader(version={self.version_string!r}, "
            f"class={self.device_class_name!r}, "
            f"color_space={self.color_space_name!r}, "
            f"pcs={self.pcs_name!r}, size={self.size})"
        )


# ---------------------------------------------------------------------------
# IccProfile - read and inspect ICC profiles
# ---------------------------------------------------------------------------

cdef class IccProfile:
    """An ICC color profile handle.

    Wraps ``CIccProfileHandle`` from the C wrapper API. Supports both
    lazy open (tag data on demand) and full read.

    Parameters
    ----------
    path : str, bytes, or os.PathLike
        Path to an ICC profile file.
    lazy : bool
        If True (default), open lazily. If False, read the full profile.

    Examples
    --------
    >>> with IccProfile("sRGB.icc") as p:
    ...     print(p.header.version_string)
    '4.3.0'
    """

    cdef cicc.CIccProfileHandle *_handle
    cdef bint _owns
    cdef object _cached_header

    def __cinit__(self):
        self._handle = NULL
        self._owns = False
        self._cached_header = None

    def __init__(self, path=None, *, bint lazy=True):
        cdef const char *c_path
        if path is not None:
            b_path = _encode_path(path)
            c_path = b_path
            if lazy:
                self._handle = cicc.IccProfileOpenHandle(c_path)
            else:
                self._handle = cicc.IccProfileReadHandle(c_path)
            if self._handle == NULL:
                raise IccProfileError(f"Failed to open profile: {path}")
            self._owns = True

    def __dealloc__(self):
        if self._handle != NULL and self._owns:
            cicc.IccProfileFree(self._handle)
            self._handle = NULL

    def close(self):
        """Release the underlying profile handle."""
        if self._handle != NULL and self._owns:
            cicc.IccProfileFree(self._handle)
            self._handle = NULL
            self._owns = False
            self._cached_header = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    cdef cicc.CIccProfileHandle* _get_handle(self) except NULL:
        if self._handle == NULL:
            raise IccProfileError("Profile handle is closed or invalid")
        return self._handle

    @staticmethod
    cdef IccProfile _from_handle(cicc.CIccProfileHandle *handle, bint owns):
        cdef IccProfile p = IccProfile.__new__(IccProfile)
        p._handle = handle
        p._owns = owns
        return p

    @property
    def header(self):
        """The profile header as an immutable :class:`IccProfileHeader`.

        Result is cached after first access for performance.
        """
        if self._cached_header is not None:
            return self._cached_header

        cdef cicc.icHeader hdr
        cdef cicc.CIccProfileHandle *h
        memset(&hdr, 0, sizeof(cicc.icHeader))
        h = self._get_handle()
        if not cicc.IccProfileGetHeader(h, &hdr):
            raise IccProfileError("Failed to read profile header")

        result = IccProfileHeader(
            size=hdr.size,
            cmm_id=hdr.cmmId,
            version=hdr.version,
            device_class=hdr.deviceClass,
            color_space=hdr.colorSpace,
            pcs=hdr.pcs,
            date={
                'year': hdr.date.year,
                'month': hdr.date.month,
                'day': hdr.date.day,
                'hours': hdr.date.hours,
                'minutes': hdr.date.minutes,
                'seconds': hdr.date.seconds,
            },
            magic=hdr.magic,
            platform=hdr.platform,
            flags=hdr.flags,
            manufacturer=hdr.manufacturer,
            model=hdr.model,
            attributes=int(hdr.attributes),
            rendering_intent=hdr.renderingIntent,
            illuminant={
                'X': hdr.illuminant.X / 65536.0,
                'Y': hdr.illuminant.Y / 65536.0,
                'Z': hdr.illuminant.Z / 65536.0,
            },
            creator=hdr.creator,
            profile_id=bytes(hdr.profileID.ID8[:16]),
            spectral_pcs=hdr.spectralPCS,
            spectral_range={
                'start': hdr.spectralRange.start,
                'end': hdr.spectralRange.end,
                'steps': hdr.spectralRange.steps,
            },
            bi_spectral_range={
                'start': hdr.biSpectralRange.start,
                'end': hdr.biSpectralRange.end,
                'steps': hdr.biSpectralRange.steps,
            },
            mcs=hdr.mcs,
            device_sub_class=hdr.deviceSubClass,
        )
        self._cached_header = result
        return result

    @property
    def color_space(self):
        """Data color space as a :class:`ColorSpace` enum."""
        return ColorSpace.from_sig(self.header.color_space)

    @property
    def pcs(self):
        """Profile Connection Space as a :class:`ColorSpace` enum."""
        return ColorSpace.from_sig(self.header.pcs)

    @property
    def device_class(self):
        """Profile/device class as a :class:`ProfileClass` enum."""
        try:
            return ProfileClass(self.header.device_class)
        except ValueError:
            return self.header.device_class

    @property
    def version_string(self):
        """Profile version as ``'major.minor.bugfix'``."""
        return self.header.version_string

    @property
    def is_valid(self):
        """True if the profile handle is open."""
        return self._handle != NULL

    @property
    def is_v5(self):
        """True if this is an iccMAX (v5+) profile."""
        return (self.header.version >> 24) >= 5

    def __repr__(self):
        if self._handle == NULL:
            return "IccProfile(<closed>)"
        try:
            h = self.header
            return (
                f"IccProfile(color_space={h.color_space_name!r}, "
                f"version={h.version_string!r})"
            )
        except IccError:
            return "IccProfile(<error reading header>)"

    def __bool__(self):
        return self._handle != NULL


# ---------------------------------------------------------------------------
# IccApply - thread-safe per-thread apply handle
# ---------------------------------------------------------------------------

cdef class IccApply:
    """Thread-safe apply handle for pixel transforms.

    Obtained from :meth:`IccCmm.get_apply`. Each thread should have
    its own ``IccApply`` for concurrent transforms.

    Supports both list-based and NumPy array-based transforms.
    """

    cdef cicc.CIccApplyHandle *_handle
    cdef int _src_samples
    cdef int _dst_samples
    cdef object _parent_cmm  # prevent GC of parent CMM
    cdef object _alive       # shared [True] list - parent CMM sets [False] on close

    def __cinit__(self):
        self._handle = NULL
        self._src_samples = 0
        self._dst_samples = 0
        self._parent_cmm = None
        self._alive = None

    def __dealloc__(self):
        if self._handle != NULL:
            cicc.IccApplyFree(self._handle)
            self._handle = NULL

    def close(self):
        """Release the apply handle."""
        if self._handle != NULL:
            cicc.IccApplyFree(self._handle)
            self._handle = NULL
        self._parent_cmm = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    @property
    def src_channels(self):
        """Number of source channels."""
        return self._src_samples

    @property
    def dst_channels(self):
        """Number of destination channels."""
        return self._dst_samples

    def apply(self, pixel):
        """Transform a single pixel.

        Parameters
        ----------
        pixel : sequence of float
            Input pixel (length must match source channels).

        Returns
        -------
        list[float]
            Transformed pixel values.

        Note
        ----
        This method is NOT thread-safe. Each thread must have its own
        ``IccApply`` instance obtained from :meth:`IccCmm.get_apply`.
        """
        if self._handle == NULL:
            raise IccCmmError("Apply handle is closed")
        if self._alive is not None and not self._alive[0]:
            raise IccCmmError("Parent CMM has been closed")

        cdef int n_src = self._src_samples
        cdef int n_dst = self._dst_samples
        cdef cicc.icFloatNumber *src
        cdef cicc.icFloatNumber *dst
        cdef cicc.icStatusCMM stat
        cdef int i

        if len(pixel) != n_src:
            raise ValueError(
                f"Expected {n_src} source channels, got {len(pixel)}"
            )

        src = <cicc.icFloatNumber*>malloc(<size_t>n_src * sizeof(cicc.icFloatNumber))
        dst = <cicc.icFloatNumber*>malloc(<size_t>n_dst * sizeof(cicc.icFloatNumber))
        if src == NULL or dst == NULL:
            free(src)
            free(dst)
            raise MemoryError("Failed to allocate pixel buffers")

        try:
            for i in range(n_src):
                src[i] = <cicc.icFloatNumber>pixel[i]
            stat = cicc.IccApplyApplyFloat(self._handle, dst, src)
            _check_status(stat, "IccApply.apply")
            return [float(dst[i]) for i in range(n_dst)]
        finally:
            free(src)
            free(dst)

    def apply_multi(self, pixels):
        """Transform multiple pixels.

        Parameters
        ----------
        pixels : sequence of sequence of float
            Each element is one pixel.

        Returns
        -------
        list[list[float]]
            Transformed pixels.
        """
        if self._handle == NULL:
            raise IccCmmError("Apply handle is closed")
        if self._alive is not None and not self._alive[0]:
            raise IccCmmError("Parent CMM has been closed")

        cdef Py_ssize_t n_pixels_s = len(pixels)
        if n_pixels_s > <Py_ssize_t>0xFFFFFFFF:
            raise OverflowError("Too many pixels (max 4 billion)")
        cdef int n_pixels = <int>n_pixels_s
        cdef int n_src = self._src_samples
        cdef int n_dst = self._dst_samples
        cdef cicc.icFloatNumber *src
        cdef cicc.icFloatNumber *dst
        cdef cicc.icStatusCMM stat
        cdef int p_idx, c
        cdef size_t src_size, dst_size

        if n_pixels == 0:
            return []

        src_size = <size_t>n_pixels * <size_t>n_src * sizeof(cicc.icFloatNumber)
        dst_size = <size_t>n_pixels * <size_t>n_dst * sizeof(cicc.icFloatNumber)
        if n_pixels > 0 and (src_size // sizeof(cicc.icFloatNumber)) // n_pixels != <size_t>n_src:
            raise OverflowError("Source buffer size overflow")
        if n_pixels > 0 and (dst_size // sizeof(cicc.icFloatNumber)) // n_pixels != <size_t>n_dst:
            raise OverflowError("Destination buffer size overflow")

        src = <cicc.icFloatNumber*>malloc(src_size)
        dst = <cicc.icFloatNumber*>malloc(dst_size)
        if src == NULL or dst == NULL:
            free(src)
            free(dst)
            raise MemoryError("Failed to allocate pixel buffers")

        try:
            for p_idx in range(n_pixels):
                pix = pixels[p_idx]
                if len(pix) != n_src:
                    raise ValueError(
                        f"Pixel {p_idx}: expected {n_src} channels, got {len(pix)}"
                    )
                for c in range(n_src):
                    src[p_idx * n_src + c] = <cicc.icFloatNumber>pix[c]

            stat = cicc.IccApplyApplyFloatMulti(
                self._handle, dst, src, <cicc.icUInt32Number>n_pixels)
            _check_status(stat, "IccApply.apply_multi")

            return [
                [float(dst[p_idx * n_dst + c]) for c in range(n_dst)]
                for p_idx in range(n_pixels)
            ]
        finally:
            free(src)
            free(dst)

    @cython.boundscheck(False)
    @cython.wraparound(False)
    def apply_ndarray(self, const float[:, ::1] pixels not None):
        """Transform pixels from a C-contiguous float32 buffer (zero-copy input).

        This is the fastest path for bulk transforms. Accepts any object
        implementing the buffer protocol with float32 C-contiguous data
        (``numpy.ndarray``, ``array.array``, etc.).

        Parameters
        ----------
        pixels : array-like, shape (N, src_channels), dtype float32
            Input pixel data. Must be C-contiguous.

        Returns
        -------
        numpy.ndarray, shape (N, dst_channels), dtype float32
            Transformed pixels. Requires NumPy at runtime.

        Raises
        ------
        ImportError
            If NumPy is not installed (needed for output array).
        ValueError
            If channel count doesn't match.
        """
        if self._handle == NULL:
            raise IccCmmError("Apply handle is closed")
        if self._alive is not None and not self._alive[0]:
            raise IccCmmError("Parent CMM has been closed")
        if _np is None:
            raise ImportError("NumPy is required for apply_ndarray()")

        cdef int n_pixels = pixels.shape[0]
        cdef int n_src = pixels.shape[1]
        cdef int n_dst = self._dst_samples
        cdef cicc.icStatusCMM stat

        if n_src != self._src_samples:
            raise ValueError(
                f"Expected {self._src_samples} source channels, got {n_src}"
            )
        if n_pixels < 0 or <unsigned int>n_pixels > 0xFFFFFFFF:
            raise OverflowError("Too many pixels (max 4 billion)")
        if n_pixels == 0:
            return _np.empty((0, n_dst), dtype=_np.float32)

        result = _np.empty((n_pixels, n_dst), dtype=_np.float32)
        cdef float[:, ::1] out_view = result

        stat = cicc.IccApplyApplyFloatMulti(
            self._handle,
            &out_view[0, 0],
            <cicc.icFloatNumber*>&pixels[0, 0],
            <cicc.icUInt32Number>n_pixels,
        )
        _check_status(stat, "IccApply.apply_ndarray")
        return result

    def __repr__(self):
        if self._handle == NULL:
            return "IccApply(<closed>)"
        return (
            f"IccApply(src_channels={self._src_samples}, "
            f"dst_channels={self._dst_samples})"
        )

    def __bool__(self):
        return self._handle != NULL


# ---------------------------------------------------------------------------
# IccCmm - Color Management Module
# ---------------------------------------------------------------------------

cdef class IccCmm:
    """ICC Color Management Module - multi-profile transform pipeline.

    Chains one or more ICC profiles and applies color transforms.
    After attaching profiles, call :meth:`begin` to initialize, then
    :meth:`apply` / :meth:`apply_ndarray` to transform pixels.

    Parameters
    ----------
    src_space : ColorSpace or int
        Source color space (default: auto-detect).
    dst_space : ColorSpace or int
        Destination color space (default: auto-detect).
    first_is_input : bool
        Whether the first profile is an input profile (default True).

    Examples
    --------
    >>> import numpy as np
    >>> with IccCmm() as cmm:
    ...     cmm.attach("input.icc", intent=Intent.Perceptual)
    ...     cmm.attach("output.icc")
    ...     cmm.begin()
    ...     result = cmm.apply([0.5, 0.3, 0.1])
    ...     # Bulk NumPy (zero-copy, fast)
    ...     pixels = np.array([[0.5, 0.3, 0.1]], dtype=np.float32)
    ...     out = cmm.apply_ndarray(pixels)
    """

    cdef cicc.CIccCmmHandle *_handle
    cdef int _src_samples
    cdef int _dst_samples
    cdef bint _begun
    cdef object _alive  # shared liveness flag [True]; set to [False] on close

    def __cinit__(self):
        self._handle = NULL
        self._src_samples = 0
        self._dst_samples = 0
        self._begun = False
        self._alive = [True]

    def __init__(self, src_space=ColorSpace.Unknown, dst_space=ColorSpace.Unknown,
                 bint first_is_input=True):
        self._handle = cicc.IccCmmCreate(
            <cicc.icColorSpaceSignature><int>src_space,
            <cicc.icColorSpaceSignature><int>dst_space,
            <cicc.icBoolean>(1 if first_is_input else 0),
        )
        if self._handle == NULL:
            raise IccCmmError("Failed to create CMM")

    def __dealloc__(self):
        if self._handle != NULL:
            cicc.IccCmmFree(self._handle)
            self._handle = NULL
        if self._alive is not None:
            self._alive[0] = False

    def close(self):
        """Release the CMM and all attached profiles."""
        if self._handle != NULL:
            cicc.IccCmmFree(self._handle)
            self._handle = NULL
            self._begun = False
        if self._alive is not None:
            self._alive[0] = False

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    cdef cicc.CIccCmmHandle* _get_handle(self) except NULL:
        if self._handle == NULL:
            raise IccCmmError("CMM handle is closed")
        return self._handle

    def attach(self, path, *,
               intent=RenderingIntent.Perceptual,
               interp=Interpolation.Linear,
               lut_type=LutType.Color,
               bint use_d2b=True,
               bint use_bpc=False):
        """Attach an ICC profile file to the pipeline.

        Parameters
        ----------
        path : str, bytes, or os.PathLike
            Path to the ICC profile file.
        intent : RenderingIntent
            Rendering intent (default: Perceptual).
        interp : Interpolation
            Interpolation method (default: Linear).
        lut_type : LutType
            LUT type (default: Color).
        use_d2b : bool
            Use DToB/BToD tags (default: True).
        use_bpc : bool
            Apply Black Point Compensation (default: False).
        """
        if self._begun:
            raise IccCmmError("Cannot attach profiles after begin()")

        cdef const char *c_path
        cdef cicc.CIccCmmHandle *h
        cdef cicc.icStatusCMM stat

        if not (0 <= int(intent) <= 3):
            raise ValueError(f"Invalid rendering intent: {intent}")
        if not (0 <= int(interp) <= 1):
            raise ValueError(f"Invalid interpolation: {interp}")
        if not (0 <= int(lut_type) <= 0xA):
            raise ValueError(f"Invalid LUT type: {lut_type}")

        b_path = _encode_path(path)
        c_path = b_path
        h = self._get_handle()

        stat = cicc.IccCmmAttachProfileFile(
            h, c_path,
            <cicc.icRenderingIntent><int>intent,
            <cicc.icXformInterp><int>interp,
            NULL,
            <cicc.icXformLutType><int>lut_type,
            <cicc.icBoolean>(1 if use_d2b else 0),
            <cicc.icBoolean>(1 if use_bpc else 0),
            NULL,
        )
        _check_status(stat, f"attach({path})")

    # Backward-compatible alias
    def attach_profile(self, path, **kwargs):
        """Alias for :meth:`attach` (backward compatibility)."""
        return self.attach(path, **kwargs)

    def begin(self):
        """Initialize the pipeline. Call after attaching all profiles."""
        cdef cicc.CIccCmmHandle *h = self._get_handle()
        cdef cicc.icStatusCMM stat = cicc.IccCmmBegin(h)
        _check_status(stat, "begin")

        cdef cicc.SIccCmmStruct info
        stat = cicc.IccCmmGetInfo(h, &info)
        _check_status(stat, "begin (get_info)")

        self._src_samples = info.srcSamples
        self._dst_samples = info.dstSamples
        self._begun = True

    @property
    def src_space(self):
        """Source color space (after begin)."""
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")
        cdef cicc.SIccCmmStruct info
        cdef cicc.icStatusCMM stat = cicc.IccCmmGetInfo(self._get_handle(), &info)
        if stat != cicc.icCmmStatOk:
            raise IccCmmError("Failed to get CMM info")
        return ColorSpace.from_sig(info.srcSpace)

    @property
    def dst_space(self):
        """Destination color space (after begin)."""
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")
        cdef cicc.SIccCmmStruct info
        cdef cicc.icStatusCMM stat = cicc.IccCmmGetInfo(self._get_handle(), &info)
        if stat != cicc.icCmmStatOk:
            raise IccCmmError("Failed to get CMM info")
        return ColorSpace.from_sig(info.dstSpace)

    @property
    def src_channels(self):
        """Number of source channels (after begin)."""
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")
        return self._src_samples

    @property
    def dst_channels(self):
        """Number of destination channels (after begin)."""
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")
        return self._dst_samples

    def get_apply(self):
        """Create a thread-safe apply handle.

        Returns
        -------
        IccApply
            Each thread should have its own handle.
        """
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")

        cdef cicc.CIccCmmHandle *h = self._get_handle()
        cdef cicc.CIccApplyHandle *ah = cicc.IccCmmGetApply(h)
        if ah == NULL:
            raise IccCmmError("Failed to create apply handle")

        cdef IccApply apply_obj = IccApply.__new__(IccApply)
        apply_obj._handle = ah
        apply_obj._src_samples = self._src_samples
        apply_obj._dst_samples = self._dst_samples
        apply_obj._parent_cmm = self  # prevent GC of parent CMM
        apply_obj._alive = self._alive  # shared liveness flag
        return apply_obj

    def apply(self, pixel):
        """Transform a single pixel.

        Parameters
        ----------
        pixel : sequence of float
            Input pixel values.

        Returns
        -------
        list[float]
            Transformed pixel values.

        Note
        ----
        This method is NOT thread-safe. For concurrent transforms,
        use :meth:`get_apply` to obtain per-thread ``IccApply`` handles.
        """
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")

        cdef int n_src = self._src_samples
        cdef int n_dst = self._dst_samples
        cdef cicc.icFloatNumber *src
        cdef cicc.icFloatNumber *dst
        cdef cicc.CIccCmmHandle *h
        cdef cicc.icStatusCMM stat
        cdef int i

        if len(pixel) != n_src:
            raise ValueError(
                f"Expected {n_src} source channels, got {len(pixel)}"
            )

        src = <cicc.icFloatNumber*>malloc(<size_t>n_src * sizeof(cicc.icFloatNumber))
        dst = <cicc.icFloatNumber*>malloc(<size_t>n_dst * sizeof(cicc.icFloatNumber))
        if src == NULL or dst == NULL:
            free(src)
            free(dst)
            raise MemoryError("Failed to allocate pixel buffers")

        try:
            for i in range(n_src):
                src[i] = <cicc.icFloatNumber>pixel[i]
            h = self._get_handle()
            stat = cicc.IccCmmApplyFloat(h, dst, src)
            _check_status(stat, "apply")
            return [float(dst[i]) for i in range(n_dst)]
        finally:
            free(src)
            free(dst)

    def apply_multi(self, pixels):
        """Transform multiple pixels.

        Parameters
        ----------
        pixels : sequence of sequence of float
            Each element is one pixel.

        Returns
        -------
        list[list[float]]
            Transformed pixels.
        """
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")

        cdef Py_ssize_t n_pixels_s = len(pixels)
        if n_pixels_s > <Py_ssize_t>0xFFFFFFFF:
            raise OverflowError("Too many pixels (max 4 billion)")
        cdef int n_pixels = <int>n_pixels_s
        cdef int n_src = self._src_samples
        cdef int n_dst = self._dst_samples
        cdef cicc.icFloatNumber *src
        cdef cicc.icFloatNumber *dst
        cdef cicc.CIccCmmHandle *h
        cdef cicc.icStatusCMM stat
        cdef int p_idx, c
        cdef size_t src_size, dst_size

        if n_pixels == 0:
            return []

        src_size = <size_t>n_pixels * <size_t>n_src * sizeof(cicc.icFloatNumber)
        dst_size = <size_t>n_pixels * <size_t>n_dst * sizeof(cicc.icFloatNumber)
        if n_pixels > 0 and (src_size // sizeof(cicc.icFloatNumber)) // n_pixels != <size_t>n_src:
            raise OverflowError("Source buffer size overflow")
        if n_pixels > 0 and (dst_size // sizeof(cicc.icFloatNumber)) // n_pixels != <size_t>n_dst:
            raise OverflowError("Destination buffer size overflow")

        src = <cicc.icFloatNumber*>malloc(src_size)
        dst = <cicc.icFloatNumber*>malloc(dst_size)
        if src == NULL or dst == NULL:
            free(src)
            free(dst)
            raise MemoryError("Failed to allocate pixel buffers")

        try:
            for p_idx in range(n_pixels):
                pix = pixels[p_idx]
                if len(pix) != n_src:
                    raise ValueError(
                        f"Pixel {p_idx}: expected {n_src} channels, got {len(pix)}"
                    )
                for c in range(n_src):
                    src[p_idx * n_src + c] = <cicc.icFloatNumber>pix[c]

            h = self._get_handle()
            stat = cicc.IccCmmApplyFloatMulti(
                h, dst, src, <cicc.icUInt32Number>n_pixels)
            _check_status(stat, "apply_multi")

            return [
                [float(dst[p_idx * n_dst + c]) for c in range(n_dst)]
                for p_idx in range(n_pixels)
            ]
        finally:
            free(src)
            free(dst)

    @cython.boundscheck(False)
    @cython.wraparound(False)
    def apply_ndarray(self, const float[:, ::1] pixels not None):
        """Transform pixels from a C-contiguous float32 buffer (zero-copy).

        The fastest path for bulk transforms. Accepts any buffer-protocol
        object with C-contiguous float32 layout (e.g., ``numpy.ndarray``).

        Parameters
        ----------
        pixels : array-like, shape (N, src_channels), dtype float32
            Input pixels. Must be C-contiguous float32.

        Returns
        -------
        numpy.ndarray, shape (N, dst_channels), dtype float32
            Transformed pixels.

        Raises
        ------
        ImportError
            If NumPy is not installed.
        """
        if not self._begun:
            raise IccCmmError("CMM not yet initialized (call begin())")
        if _np is None:
            raise ImportError("NumPy is required for apply_ndarray()")

        cdef int n_pixels = pixels.shape[0]
        cdef int n_src = pixels.shape[1]
        cdef int n_dst = self._dst_samples
        cdef cicc.CIccCmmHandle *h
        cdef cicc.icStatusCMM stat

        if n_src != self._src_samples:
            raise ValueError(
                f"Expected {self._src_samples} source channels, got {n_src}"
            )
        if n_pixels == 0:
            return _np.empty((0, n_dst), dtype=_np.float32)

        result = _np.empty((n_pixels, n_dst), dtype=_np.float32)
        cdef float[:, ::1] out_view = result

        h = self._get_handle()
        stat = cicc.IccCmmApplyFloatMulti(
            h,
            &out_view[0, 0],
            <cicc.icFloatNumber*>&pixels[0, 0],
            <cicc.icUInt32Number>n_pixels,
        )
        _check_status(stat, "apply_ndarray")
        return result

    @property
    def is_ready(self):
        """True if initialized and ready for transforms."""
        return self._handle != NULL and self._begun

    def __repr__(self):
        if self._handle == NULL:
            return "IccCmm(<closed>)"
        if not self._begun:
            return "IccCmm(<not initialized>)"
        return (
            f"IccCmm(src_channels={self._src_samples}, "
            f"dst_channels={self._dst_samples})"
        )

    def __bool__(self):
        return self._handle != NULL


# ---------------------------------------------------------------------------
# Module-level convenience functions
# ---------------------------------------------------------------------------

def open_profile(path, *, bint lazy=True):
    """Open an ICC profile file. Convenience wrapper for ``IccProfile(path)``.

    Parameters
    ----------
    path : str, bytes, or os.PathLike
        Path to profile.
    lazy : bool
        Lazy open (default True).

    Returns
    -------
    IccProfile
    """
    return IccProfile(path, lazy=lazy)


def read_profile(path):
    """Read an ICC profile fully into memory.

    Equivalent to ``IccProfile(path, lazy=False)``.
    """
    return IccProfile(path, lazy=False)
