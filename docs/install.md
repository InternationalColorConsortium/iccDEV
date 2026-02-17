## Quickstart installation: 

| Method | Command |
|--------|---------|
| **NPM** | `npm install iccdev` |
| **Homebrew** | `brew install iccdev` |
| **Docker Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **Docker Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **NixOS Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev-nixos:latest` |
| **NixOS Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev-nixos:latest` |

## Examples for Docker & NixOS

### Docker Examples

`iccDumpProfile`

```
Usage: iccDumpProfile {-v} {int} profile {tagId to dump/"ALL"}
Built with IccProfLib version 2.3.1.1

The -v option causes profile validation to be performed.
The optional integer parameter specifies verboseness of output (1-100, default=100).
```

`iccFromXml`

```
IccFromXml built with IccProfLib Version 2.3.1.1, IccLibXML Version 2.3.1.1

Usage: IccFromXml xml_file saved_profile_file {-noid -v{=[relax_ng_schema_file - optional]}}
```

`iccToXml`

```
IccToXml built with IccProfLib Version 2.3.1.1, IccLibXML Version 2.3.1.1

Usage: IccToXml src_icc_profile dest_xml_file
```

`iccRoundTrip`

```
Usage: iccRoundTrip profile {rendering_intent=1 {use_mpe=0}}
Built with IccProfLib version 2.3.1.1
  where rendering_intent is (0=perceptual, 1=relative, 2=saturation, 3=absolute)
```

### NixOS Examples

2026-02-10 02:17:06 UTC

#### List the Libraries & Tools

##### Show Tools

`docker run --rm ghcr.io/internationalcolorconsortium/iccdev-nixos:latest sh -c "ls /workspace/iccDEV/Build/Tools/*/icc*"`

##### Expected Output

```
/workspace/iccDEV/Build/Tools/IccApplyNamedCmm/iccApplyNamedCmm
/workspace/iccDEV/Build/Tools/IccApplyProfiles/iccApplyProfiles
/workspace/iccDEV/Build/Tools/IccApplySearch/iccApplySearch
/workspace/iccDEV/Build/Tools/IccApplyToLink/iccApplyToLink
/workspace/iccDEV/Build/Tools/IccDumpProfile/iccDumpProfile
/workspace/iccDEV/Build/Tools/IccFromCube/iccFromCube
/workspace/iccDEV/Build/Tools/IccFromXml/iccFromXml
/workspace/iccDEV/Build/Tools/IccJpegDump/iccJpegDump
/workspace/iccDEV/Build/Tools/IccPngDump/iccPngDump
/workspace/iccDEV/Build/Tools/IccRoundTrip/iccRoundTrip
/workspace/iccDEV/Build/Tools/IccSpecSepToTiff/iccSpecSepToTiff
/workspace/iccDEV/Build/Tools/IccTiffDump/iccTiffDump
/workspace/iccDEV/Build/Tools/IccToXml/iccToXml
/workspace/iccDEV/Build/Tools/IccV5DspObsToV4Dsp/iccV5DspObsToV4Dsp
/workspace/iccDEV/Build/Tools/wxProfileDump/iccDumpProfileGui
```

#### Show Libraries 

`docker run --rm ghcr.io/internationalcolorconsortium/iccdev-nixos:latest sh -c "find /workspace/iccDEV/Build -name 'libIcc*.so*' -o -name 'libIcc*.a' | sort"`

#### Expected Output

```
/workspace/iccDEV/Build/IccProfLib/libIccProfLib2-static.a
/workspace/iccDEV/Build/IccProfLib/libIccProfLib2.so
/workspace/iccDEV/Build/IccProfLib/libIccProfLib2.so.2
/workspace/iccDEV/Build/IccProfLib/libIccProfLib2.so.2.3.1.4
/workspace/iccDEV/Build/IccXML/libIccXML2-static.a
/workspace/iccDEV/Build/IccXML/libIccXML2.so
/workspace/iccDEV/Build/IccXML/libIccXML2.so.2
/workspace/iccDEV/Build/IccXML/libIccXML2.so.2.3.1.4
```

#### Run iccRoundTrip

Command: `docker run --rm ghcr.io/internationalcolorconsortium/iccdev-nixos:latest iccRoundTrip Testing/sRGB_v4_ICC_preference.icc`

#### Expected Output

```
Profile:          'Testing/sRGB_v4_ICC_preference.icc'
Rendering Intent: Relative Colorimetric
Specified Gamut:  Not Specified

Round Trip 1
------------
Min DeltaE:        0.00
Mean DeltaE:       0.01
Max DeltaE:        0.05

Max L, a, b:   10.919208, 0.001984, 0.002167

Round Trip 2
------------
Min DeltaE:        0.00
Mean DeltaE:       0.01
Max DeltaE:        0.05

Max L, a, b:   10.919208, 0.001984, 0.002167

PRMG Interoperability - Round Trip Results
------------------------------------------------------
DE <= 1.0 (  122111):  60.6%
DE <= 2.0 (  126939):  63.0%
DE <= 3.0 (  131673):  65.3%
DE <= 5.0 (  140602):  69.7%
DE <=10.0 (  159684):  79.2%
Total     (  201613)
```

#### Run iccDumpProfile

Command: `docker run --rm ghcr.io/internationalcolorconsortium/iccdev-nixos:latest iccDumpProfile -v Testing/sRGB_v4_ICC_preference.icc`

#### Expected Output

```
Built with IccProfLib version 2.3.1.4+260b499

Profile:            'Testing/sRGB_v4_ICC_preference.icc'
Profile ID:         34562abf994ccd066d2c5721d0d68c5d
Size:               60960 (0xee20) bytes

Header
------
Attributes:         Reflective | Glossy
Cmm:                Unknown NULL
Creation Date:      7/25/2007 (M/D/Y)  00:05:37
Creator:            NULL
Device Manufacturer:NULL
Data Color Space:   RgbData
Flags:              EmbeddedProfileFalse | UseAnywhere
PCS Color Space:    LabData
Platform:           Unknown
Rendering Intent:   Perceptual
Profile Class:      ColorSpaceClass
Profile SubClass:   Not Defined
Version:            4.20
Illuminant:         X=0.9642, Y=1.0000, Z=0.8249
Spectral PCS:       NoSpectralData
Spectral PCS Range: Not Defined
BiSpectral Range:   Not Defined
MCS Color Space:    Not Defined

Profile Tags (9)
------------
                         Tag    ID      Offset      Size             Pad
                        ----  ------    ------      ----             ---
       profileDescriptionTag  'desc'       240       118               2
                    AToB0Tag  'A2B0'       360     29712               0
                    AToB1Tag  'A2B1'     30072       436               0
                    BToA0Tag  'B2A0'     30508     29748               0
                    BToA1Tag  'B2A1'     60256       508               0
perceptualRenderingIntentGamutTag  'rig0'     60764           12               0
          mediaWhitePointTag  'wtpt'     60776        20               0
                copyrightTag  'cprt'     60796       118               2
      chromaticAdaptationTag  'chad'     60916        44               0


Validation Report
-----------------
Profile is valid for version 4.20
```

## Build Instructions
Build instructions are available in [docs/build.md](docs/build.md).