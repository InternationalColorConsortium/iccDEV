# cicc_wrapper.pxd - Cython declarations for IccProfLib C Wrapper API
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

from libc.stddef cimport size_t
from libc.stdint cimport uint8_t, uint16_t, uint32_t, int8_t, int32_t

cdef extern from "IccProfLibConf.h":
    pass

# Ensure icUInt64Number is always a scalar unsigned long long.
# On non-MSVC, IccProfLibConf.h defines ICUINT64TYPE.
# On MSVC, it does not - this fallback makes icUInt64Number a scalar
# instead of icUInt32Number[2]. The struct layout is ABI-compatible
# (both are 8 bytes, same alignment).
cdef extern from *:
    """
    #ifndef ICUINT64TYPE
    #define ICUINT64TYPE unsigned long long
    #endif
    """
    pass

cdef extern from "icProfileHeader.h":
    # Basic ICC integer types
    ctypedef uint8_t   icUInt8Number
    ctypedef uint16_t  icUInt16Number
    ctypedef uint32_t  icUInt32Number
    ctypedef int8_t    icInt8Number
    ctypedef int32_t   icInt32Number
    ctypedef uint32_t  icSignature

    ctypedef float icFloatNumber

    # Color Space Signatures
    ctypedef enum icColorSpaceSignature:
        icSigXYZData        = 0x58595A20
        icSigLabData        = 0x4C616220
        icSigLuvData        = 0x4C757620
        icSigYCbCrData      = 0x59436272
        icSigYxyData        = 0x59787920
        icSigRgbData        = 0x52474220
        icSigGrayData       = 0x47524159
        icSigHsvData        = 0x48535620
        icSigHlsData        = 0x484C5320
        icSigCmykData       = 0x434D594B
        icSigCmyData        = 0x434D5920
        icSig1colorData     = 0x31434C52
        icSig2colorData     = 0x32434C52
        icSig3colorData     = 0x33434C52
        icSig4colorData     = 0x34434C52
        icSig5colorData     = 0x35434C52
        icSig6colorData     = 0x36434C52
        icSig7colorData     = 0x37434C52
        icSig8colorData     = 0x38434C52
        icSig9colorData     = 0x39434C52
        icSig10colorData    = 0x41434C52
        icSig11colorData    = 0x42434C52
        icSig12colorData    = 0x43434C52
        icSig13colorData    = 0x44434C52
        icSig14colorData    = 0x45434C52
        icSig15colorData    = 0x46434C52
        icSigNamedData      = 0x6e6d636c
        icSigNChannelData   = 0x6e630000
        icSigDevLabData     = 0x644C6162
        icSigDevXYZData     = 0x6458595A
        icSigGamutData      = 0x67616D74
        icSigNoColorData    = 0x00000000
        icSigUnknownData    = 0x3f3f3f3f

    # Profile Class Signatures
    ctypedef enum icProfileClassSignature:
        icSigInputClass                 = 0x73636E72
        icSigDisplayClass               = 0x6D6E7472
        icSigOutputClass                = 0x70727472
        icSigLinkClass                  = 0x6C696E6B
        icSigAbstractClass              = 0x61627374
        icSigColorSpaceClass            = 0x73706163
        icSigNamedColorClass            = 0x6e6d636c
        icSigColorEncodingClass         = 0x63656e63
        icSigMultiplexIdentificationClass = 0x6D696420
        icSigMultiplexLinkClass           = 0x6d6c6e6b
        icSigMultiplexVisualizationClass  = 0x6d766973

    # Platform Signatures
    ctypedef enum icPlatformSignature:
        icSigMacintosh      = 0x4150504C
        icSigMicrosoft      = 0x4D534654
        icSigSolaris        = 0x53554E57
        icSigSGI            = 0x53474920
        icSigUnknownPlatform = 0x00000000

    # Rendering Intents
    ctypedef enum icRenderingIntent:
        icPerceptual            = 0
        icRelativeColorimetric  = 1
        icRelative              = 1
        icSaturation            = 2
        icAbsoluteColorimetric  = 3
        icAbsolute              = 3
        icUnknownIntent         = 0x3f3f3f3f

    # Date-Time structure
    ctypedef struct icDateTimeNumber:
        icUInt16Number year
        icUInt16Number month
        icUInt16Number day
        icUInt16Number hours
        icUInt16Number minutes
        icUInt16Number seconds

    # XYZ Number
    ctypedef struct icXYZNumber:
        icInt32Number X
        icInt32Number Y
        icInt32Number Z

    # Spectral Range
    ctypedef struct icSpectralRange:
        icUInt16Number start
        icUInt16Number end
        icUInt16Number steps

    # Multiplex Color Signature
    ctypedef enum icMultiplexColorSignature:
        icSigNoMCSData  = 0x00000000
        icSigMCSData    = 0x6d630000

    # Profile ID
    ctypedef union icProfileID:
        icUInt8Number  ID8[16]
        icUInt16Number ID16[8]
        icUInt32Number ID32[4]

    # Profile Header
    ctypedef struct icHeader:
        icUInt32Number          size
        icSignature             cmmId
        icUInt32Number          version
        icProfileClassSignature deviceClass
        icColorSpaceSignature   colorSpace
        icColorSpaceSignature   pcs
        icDateTimeNumber        date
        icSignature             magic
        icPlatformSignature     platform
        icUInt32Number          flags
        icSignature             manufacturer
        icUInt32Number          model
        unsigned long long      attributes
        icUInt32Number          renderingIntent
        icXYZNumber             illuminant
        icSignature             creator
        icProfileID             profileID
        icColorSpaceSignature   spectralPCS
        icSpectralRange         spectralRange
        icSpectralRange         biSpectralRange
        icMultiplexColorSignature mcs
        icSignature             deviceSubClass
        icInt8Number            reserved[4]


cdef extern from "IccCmm.h":
    # CMM Status codes
    ctypedef enum icStatusCMM:
        icCmmStatBad                = -1
        icCmmStatOk                 = 0
        icCmmStatCantOpenProfile    = 1
        icCmmStatBadSpaceLink       = 2
        icCmmStatInvalidProfile     = 3
        icCmmStatBadXform           = 4
        icCmmStatInvalidLut         = 5
        icCmmStatProfileMissingTag  = 6
        icCmmStatColorNotFound      = 7
        icCmmStatIncorrectApply     = 8
        icCmmStatBadColorEncoding   = 9
        icCmmStatAllocErr           = 10
        icCmmStatBadLutType         = 11
        icCmmStatIdentityXform      = 12
        icCmmStatUnsupportedPcsLink = 13
        icCmmStatBadConnection      = 14
        icCmmStatBadTintXform       = 15
        icCmmStatTooManySamples     = 16
        icCmmStatBadMCSLink         = 17
        icCmmStatUnsupported        = 18

    # Interpolation types
    ctypedef enum icXformInterp:
        icInterpLinear       = 0
        icInterpTetrahedral  = 1

    # LUT types
    ctypedef enum icXformLutType:
        icXformLutColor       = 0x0
        icXformLutNamedColor  = 0x1
        icXformLutPreview     = 0x2
        icXformLutGamut       = 0x3
        icXformLutBPC         = 0x4
        icXformLutBRDFParam   = 0x5
        icXformLutBRDFDirect  = 0x6
        icXformLutBRDFMcsParam = 0x7
        icXformLutMCS         = 0x8
        icXformLutColorimetric = 0x9
        icXformLutSpectral    = 0xA

    # CMM env var signature
    ctypedef icUInt32Number icSigCmmEnvVar


cdef extern from "IccWrapper.h":
    ctypedef void CIccCmmHandle
    ctypedef void CIccApplyHandle
    ctypedef void CIccProfileHandle

    ctypedef struct SIccCmmStruct:
        icColorSpaceSignature srcSpace
        icColorSpaceSignature dstSpace
        icUInt16Number srcSamples
        icUInt16Number dstSamples

    ctypedef struct SIccCmmEnvVars:
        icUInt32Number nVars
        icSigCmmEnvVar *sigs
        icFloatNumber *vals

    ctypedef unsigned char icBoolean

    # CMM functions
    CIccCmmHandle* IccCmmCreate(icColorSpaceSignature srcSpace,
                                icColorSpaceSignature dstSpace,
                                icBoolean bFirstIsInput)

    icStatusCMM IccCmmAttachProfileFile(CIccCmmHandle *pCmm,
                                        const char *szFname,
                                        icRenderingIntent nIntent,
                                        icXformInterp nInterp,
                                        CIccProfileHandle *pPcc,
                                        icXformLutType nLutType,
                                        icBoolean bUseDtoBTags,
                                        icBoolean bUseBPC,
                                        SIccCmmEnvVars *pVars)

    icStatusCMM IccCmmAttachProfileHandle(CIccCmmHandle *pCmm,
                                           CIccProfileHandle *pProfile,
                                           icRenderingIntent nIntent,
                                           icXformInterp nInterp,
                                           CIccProfileHandle *pPcc,
                                           icXformLutType nLutType,
                                           icBoolean bUseDtoBTags,
                                           icBoolean bUseBPC,
                                           SIccCmmEnvVars *pVars)

    icStatusCMM IccCmmBegin(CIccCmmHandle *pCmm)

    CIccApplyHandle* IccCmmGetApply(CIccCmmHandle *pCmm)

    icStatusCMM IccCmmGetInfo(CIccCmmHandle *pCmm, SIccCmmStruct *pCmmInfo)

    icStatusCMM IccCmmApplyFloat(CIccCmmHandle *pCmm,
                                 icFloatNumber *pTo,
                                 icFloatNumber *pFrom)

    icStatusCMM IccCmmApplyFloatMulti(CIccCmmHandle *pCmm,
                                      icFloatNumber *pTo,
                                      icFloatNumber *pFrom,
                                      icUInt32Number nPixels)

    void IccCmmFree(CIccCmmHandle *pCmm)

    # Apply functions
    icStatusCMM IccApplyApplyFloat(CIccApplyHandle *pApply,
                                   icFloatNumber *pTo,
                                   icFloatNumber *pFrom)

    icStatusCMM IccApplyApplyFloatMulti(CIccApplyHandle *pApply,
                                        icFloatNumber *pTo,
                                        icFloatNumber *pFrom,
                                        icUInt32Number nPixels)

    void IccApplyFree(CIccApplyHandle *pApply)

    # Profile functions
    CIccProfileHandle* IccProfileReadHandle(const char *szFname)
    CIccProfileHandle* IccProfileOpenHandle(const char *szFname)
    icBoolean IccProfileGetHeader(CIccProfileHandle *pProfile, icHeader *pHeader)
    void IccProfileFree(CIccProfileHandle *pProfile)


cdef extern from "IccCValidation.h":
    ctypedef enum icc_validation_status:
        ICC_VALIDATION_OK
        ICC_VALIDATION_WARNING
        ICC_VALIDATION_NON_COMPLIANT
        ICC_VALIDATION_CRITICAL_ERROR
        ICC_VALIDATION_INVALID_ARGUMENT
        ICC_VALIDATION_INTERNAL_ERROR

    icc_validation_status icc_validate_profile(
        const unsigned char *icc_data,
        size_t icc_size,
        char *report,
        size_t report_size,
    )
