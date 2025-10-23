/** @file
    File:       IccDEVCmm.cpp

    Contains:   Defines the entry point for the DLL application.

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2015 The International Color Consortium. All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium. 
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes. 
 *
 *  
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *  
 * 
 */

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 1-15-2007
//
//////////////////////////////////////////////////////////////////////
//

#include "stdafx.h"
#include "IccCmm.h"
#include "IccDefs.h"
#include "Icm.h"
#include "IccDEVCmm.h"
#include "resource.h"
#include <string>
#include "Winerror.h"

static CIccProfile* OpenProfile(HPROFILE hProfile)
{
  DWORD dwSize=0;

  GetColorProfileFromHandle(hProfile, NULL, &dwSize);
  
  if (!dwSize)
    return NULL;

  CIccMemIO *pIO = new CIccMemIO();

  if (!pIO->Alloc(dwSize)) {
    delete pIO;

    return NULL;
  }

  if (!GetColorProfileFromHandle(hProfile, pIO->GetData(), &dwSize)) {
    delete pIO;

    return NULL;
  }

  CIccProfile *pProfile = new CIccProfile();

  if (!pProfile)
    return NULL;

  if (!pProfile->Attach(pIO)) {
    delete pProfile;
    delete pIO;

    return NULL;
  }

  return pProfile;
}

#define AVAILABLE_TRANSFORMS 1024

CIccCmm *g_Transforms[AVAILABLE_TRANSFORMS];

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
    memset(g_Transforms, 0, sizeof(g_Transforms));
    break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}

typedef HPROFILE* LPHPROFILE;

static BOOL IsValidColorType(COLORTYPE ctSpace, icColorSpaceSignature sigSpace)
{
  switch(ctSpace) {
    case COLOR_GRAY:
      if (sigSpace!=icSigGrayData) {
        return FALSE;
      }
      break;

    case COLOR_RGB:
      if (sigSpace!=icSigRgbData) {
        return FALSE;
      }
      break;

    case COLOR_XYZ:
      if (sigSpace!=icSigXYZData) {
        return FALSE;
      }
      break;

    case COLOR_Yxy:
      if (sigSpace!=icSigYxyData) {
        return FALSE;
      }
      break;

    case COLOR_Lab:
      if (sigSpace!=icSigLabData) {
        return FALSE;
      }
      break;

    case COLOR_3_CHANNEL:        // WORD per channel
      if (icGetSpaceSamples(sigSpace)!=3) {
        return FALSE;
      }
      break;

    case COLOR_CMYK:
      if (sigSpace!=icSigCmykData) {
        return FALSE;
      }
      break;

    case COLOR_5_CHANNEL:        // BYTE per channel
      if (sigSpace!=icSig5colorData) {
        return FALSE;
      }
      break;

    case COLOR_6_CHANNEL:        //      - do -
      if (sigSpace!=icSig6colorData) {
        return FALSE;
      }
      break;

    case COLOR_7_CHANNEL:        //      - do -
      if (sigSpace!=icSig7colorData) {
        return FALSE;
      }
      break;

    case COLOR_8_CHANNEL:        //      - do -
      if (sigSpace!=icSig8colorData) {
        return FALSE;
      }
      break;

    case COLOR_NAMED:  //IccDEVCmm Unsupported color spaace in transform
    default:
      return FALSE;
  }

  return TRUE;
}

static BOOL IsValidBitmapType(BMFORMAT bmType, icColorSpaceSignature sigSpace)
{
  switch(bmType) {
    case BM_GRAY:
      if (sigSpace!=icSigGrayData) {
        return FALSE;
      }
      break;

    case BM_x555RGB:
    case BM_RGBTRIPLETS:
    case BM_BGRTRIPLETS:
    case BM_xRGBQUADS:
    case BM_xBGRQUADS:
    case BM_10b_RGB:
    case BM_16b_RGB:
    case BM_565RGB:
      if (sigSpace!=icSigRgbData) {
        return FALSE;
      }
      break;

    case BM_x555XYZ:
    case BM_XYZTRIPLETS:
    case BM_10b_XYZ:
    case BM_16b_XYZ:
      if (sigSpace!=icSigXYZData) {
        return FALSE;
      }
      break;

    case BM_x555Yxy:
    case BM_YxyTRIPLETS:
    case BM_10b_Yxy:
    case BM_16b_Yxy:
      if (sigSpace!=icSigYxyData) {
        return FALSE;
      }
      break;

    case BM_x555Lab:
    case BM_LabTRIPLETS:
    case BM_10b_Lab:
    case BM_16b_Lab:
      if (sigSpace!=icSigLabData) {
        return FALSE;
      }
      break;

    case BM_x555G3CH:
    case BM_G3CHTRIPLETS:
    case BM_xG3CHQUADS:
    case BM_10b_G3CH:
    case BM_16b_G3CH:
      if (icGetSpaceSamples(sigSpace)!=3) {
        return FALSE;
      }
      break;

    case BM_KYMCQUADS:
    case BM_CMYKQUADS:
      if (sigSpace!=icSigCmykData) {
        return FALSE;
      }
      break;

    case BM_5CHANNEL:
      if (sigSpace!=icSig5colorData) {
        return FALSE;
      }
      break;

    case BM_6CHANNEL:
      if (sigSpace!=icSig6colorData) {
        return FALSE;
      }
      break;

    case BM_7CHANNEL:
      if (sigSpace!=icSig7colorData) {
        return FALSE;
      }
      break;

    case BM_8CHANNEL:
      if (sigSpace!=icSig8colorData) {
        return FALSE;
      }
      break;

    case COLOR_NAMED:  //IccDEVCmm Unsupported color spaace in transform
    default:
      return FALSE;
  }

  return TRUE;
}

__inline icFloatNumber UnitClip(icFloatNumber v)
{
  if (v<0.0)
    return 0.0;
  if (v>1.0)
    return 1.0;
  return v;
}


BOOL WINAPI CMCheckColors(
                          HCMTRANSFORM hcmTransform,  // transform handle
                          LPCOLOR lpaInputColors,     // array of COLORs
                          DWORD nColors,              // COLOR array size
                          COLORTYPE ctInput,          // input color type
                          LPBYTE lpaResult            // buffer for results
                          )
{
  size_t i = (size_t)hcmTransform;
  icFloatNumber srcPixel[MAX_COLOR_CHANNELS], destPixel[MAX_COLOR_CHANNELS];

  if (i<256 || i>=256+AVAILABLE_TRANSFORMS || !g_Transforms[i-256]) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccCmm *pCmm = g_Transforms[i-256];

  //Check if color types matches transform color spaces
  if (!IsValidColorType(ctInput, pCmm->GetSourceSpace()) ||
    pCmm->GetDestSpace() != icSigGamutData) {
    SetLastError(ERROR_INVALID_DATATYPE);
    return FALSE;
  }

  for (i=0; i<nColors; i++) {
    switch (ctInput) {
    case COLOR_GRAY:
      srcPixel[0] = (icFloatNumber)(lpaInputColors->gray.gray) / 65535.0f;
      break;

    case COLOR_RGB:
      srcPixel[0] = (icFloatNumber)(lpaInputColors->rgb.red) / 65535.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->rgb.green) / 65535.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->rgb.blue) / 65535.0f;
      break;

    case COLOR_XYZ:
      srcPixel[0] = (icFloatNumber)(lpaInputColors->XYZ.X) / 65535.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->XYZ.Y) / 65535.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->XYZ.Z) / 65535.0f;
      break;

    case COLOR_Yxy:
      srcPixel[0] = (icFloatNumber)(lpaInputColors->XYZ.X) / 65535.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->XYZ.Y) / 65535.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->XYZ.Z) / 65535.0f;
      break;

    case COLOR_Lab:
      srcPixel[0] = (icFloatNumber)(lpaInputColors->Lab.L) / 65535.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->Lab.a) / 65535.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->Lab.b) / 65535.0f;
      break;

    case COLOR_3_CHANNEL:        // WORD per channel
      srcPixel[0] = (icFloatNumber)(lpaInputColors->gen3ch.ch1) / 65535.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->gen3ch.ch2) / 65535.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->gen3ch.ch3) / 65535.0f;
      break;

    case COLOR_CMYK:
      srcPixel[0] = (icFloatNumber)(lpaInputColors->cmyk.cyan) / 65535.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->cmyk.magenta) / 65535.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->cmyk.yellow) / 65535.0f;
      srcPixel[3] = (icFloatNumber)(lpaInputColors->cmyk.black) / 65535.0f;
      break;

      // Hifi colors use BYTE per channel
    case COLOR_8_CHANNEL:
      srcPixel[7] = (icFloatNumber)(lpaInputColors->hifi.channel[7]) / 255.0f;
      //No break here!

    case COLOR_7_CHANNEL:
      srcPixel[6] = (icFloatNumber)(lpaInputColors->hifi.channel[6]) / 255.0f;
      //No break here!

    case COLOR_6_CHANNEL:
      srcPixel[5] = (icFloatNumber)(lpaInputColors->hifi.channel[5]) / 255.0f;
      //No break here!

    case COLOR_5_CHANNEL:        
      srcPixel[0] = (icFloatNumber)(lpaInputColors->hifi.channel[0]) / 255.0f;
      srcPixel[1] = (icFloatNumber)(lpaInputColors->hifi.channel[1]) / 255.0f;
      srcPixel[2] = (icFloatNumber)(lpaInputColors->hifi.channel[2]) / 255.0f;
      srcPixel[3] = (icFloatNumber)(lpaInputColors->hifi.channel[3]) / 255.0f;
      srcPixel[4] = (icFloatNumber)(lpaInputColors->hifi.channel[4]) / 255.0f;
      break;

    default:
      SetLastError(ERROR_UNSUPPORTED_TYPE);
      return FALSE;
    }
    lpaInputColors++;

    if (pCmm->Apply(destPixel, srcPixel)!=icCmmStatOk) {
      SetLastError(ERROR_FUNCTION_FAILED);
      return FALSE;
    }

    *lpaResult = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);

    lpaResult++;
  }

  return TRUE;
}


// Avoid " warning C4995: 'CMxxxx': name was marked as #pragma deprecated
#pragma warning(disable : 4995)
BOOL WINAPI CMCheckColorsInGamut(
                                 HCMTRANSFORM hcmTransform,  // transform handle
                                 RGBTRIPLE *lpaRGBTriple,    // RGB triple array
                                 LPBYTE lpaResult,           // buffer for results
                                 UINT nCount                 // result buffer size
                                )
{
  size_t i = (size_t)hcmTransform;
  icFloatNumber srcPixel[MAX_COLOR_CHANNELS], destPixel[MAX_COLOR_CHANNELS];

  if (i<256 || i>=256+AVAILABLE_TRANSFORMS || !g_Transforms[i-256]) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccCmm *pCmm = g_Transforms[i-256];

  //Check if color types matches transform color spaces
  if (pCmm->GetSourceSpace()!=icSigRgbData ||
      pCmm->GetDestSpace() != icSigGamutData) {
     SetLastError(ERROR_INVALID_DATATYPE);
     return FALSE;
  }

  for (i=0; i<nCount; i++) {
    srcPixel[0] = (icFloatNumber)(lpaRGBTriple->rgbtRed) / 65535.0f;
    srcPixel[1] = (icFloatNumber)(lpaRGBTriple->rgbtGreen) / 65535.0f;
    srcPixel[2] = (icFloatNumber)(lpaRGBTriple->rgbtBlue) / 65535.0f;

    lpaRGBTriple++;

    if (pCmm->Apply(destPixel, srcPixel)!=icCmmStatOk) {
      SetLastError(ERROR_FUNCTION_FAILED);
      return FALSE;
    }

    *lpaResult = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);

    lpaResult++;
  }
  return TRUE;
}


BOOL WINAPI CMCheckRGBs(
                        HCMTRANSFORM hcmTransform,
                        LPBYTE lpSrcBits,
                        BMFORMAT bmInput, 
                        DWORD dwWidth,
                        DWORD dwHeight,
                        DWORD dwStride, 
                        LPBYTE lpaResult, 
                        PBMCALLBACKFN pfnCallback,
                        ULONG ulCallbackData 
                        )
{
  size_t i = (size_t)hcmTransform;
  icFloatNumber srcPixel[MAX_COLOR_CHANNELS], destPixel[MAX_COLOR_CHANNELS];

  if (i<256 || i>=256+AVAILABLE_TRANSFORMS || !g_Transforms[i-256]) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccCmm *pCmm = g_Transforms[i-256];
  LPBYTE lpSrcLine;
  DWORD j;

  //Check if color types matches transform color spaces
  if (!IsValidBitmapType(bmInput, pCmm->GetSourceSpace()) ||
      pCmm->GetDestSpace() != icSigGamutData) {
    SetLastError(ERROR_INVALID_DATATYPE);
    return FALSE;
  }

  for (j=0; j<dwHeight; j++) {
    lpSrcLine = lpSrcBits;

    for (i=0; i<dwWidth; i++) {
      switch(bmInput) {
        case BM_x555Lab:
        case BM_x555RGB:
        case BM_x555XYZ:
        case BM_x555Yxy:
        case BM_x555G3CH:
        {
          icUInt16Number bits = *((icUInt16Number*)lpSrcBits);

          srcPixel[0] = (icFloatNumber)((bits&0x7C00)>>10) / 31.0f;
          srcPixel[1] = (icFloatNumber)((bits&0x03E0)>>5) / 31.0f;
          srcPixel[2] = (icFloatNumber)(bits&0x001F) / 31.0f;

          lpSrcBits += 2;
        }
        break;

        //
        // Packed 8 bits per channel => 8bpp for GRAY and
        // 24bpp for the 3 channel colors, more for hifi channels
        //

        case BM_LabTRIPLETS:
        case BM_RGBTRIPLETS:
        case BM_BGRTRIPLETS:
        case BM_XYZTRIPLETS:
        case BM_YxyTRIPLETS:
        case BM_G3CHTRIPLETS:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;

          lpSrcBits += 3;

        }
        break;

        case BM_5CHANNEL:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
          srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
          srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;

          lpSrcBits += 5;

        }
        break;

        case BM_6CHANNEL:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
          srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
          srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;
          srcPixel[5] = (icFloatNumber)bits[5] / 255.0f;

          lpSrcBits += 6;

        }
        break;

        case BM_7CHANNEL:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
          srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
          srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;
          srcPixel[5] = (icFloatNumber)bits[5] / 255.0f;
          srcPixel[6] = (icFloatNumber)bits[6] / 255.0f;

          lpSrcBits += 7;

        }
        break;

        case BM_8CHANNEL:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
          srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
          srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;
          srcPixel[5] = (icFloatNumber)bits[5] / 255.0f;
          srcPixel[6] = (icFloatNumber)bits[6] / 255.0f;
          srcPixel[7] = (icFloatNumber)bits[7] / 255.0f;

          lpSrcBits += 8;

        }
        break;

        case BM_GRAY:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;

          lpSrcBits++;

        }
        break;

        //
        // 32bpp - 8 bits per channel. The most significant byte is ignored
        // for the 3 channel colors.
        //

        case BM_xRGBQUADS:
        case BM_xG3CHQUADS:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[2] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[0] = (icFloatNumber)bits[2] / 255.0f;

          lpSrcBits += 4;

        }
        break;

        case BM_xBGRQUADS:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;

          lpSrcBits += 4;

        }
        break;

        case BM_KYMCQUADS:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
          srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;

          lpSrcBits += 4;

        }
        break;

        case BM_CMYKQUADS:
        {
          icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

          srcPixel[3] = (icFloatNumber)bits[0] / 255.0f;
          srcPixel[2] = (icFloatNumber)bits[1] / 255.0f;
          srcPixel[1] = (icFloatNumber)bits[2] / 255.0f;
          srcPixel[0] = (icFloatNumber)bits[3] / 255.0f;

          lpSrcBits += 4;

        }
        break;

        //
        // 32bpp - 10 bits per channel. The 2 most significant bits are ignored.
        //

        case BM_10b_Lab:
        case BM_10b_RGB:
        case BM_10b_XYZ:
        case BM_10b_Yxy:
        case BM_10b_G3CH:
        {
          icUInt32Number bits = *((icUInt32Number*)lpSrcBits);

          srcPixel[0] = (icFloatNumber)((bits&0x3FF00000)>>20) / 1023.0f;
          srcPixel[1] = (icFloatNumber)((bits&0x000FFC00)>>10) / 1023.0f;
          srcPixel[2] = (icFloatNumber)(bits&0x000003FF) / 1023.0f;

          lpSrcBits += 4;
        }
        break;

        //
        // Packed 16 bits per channel => 16bpp for GRAY and
        // 48bpp for the 3 channel colors.
        //

        case BM_16b_Lab:
        case BM_16b_RGB:
        case BM_16b_XYZ:
        case BM_16b_Yxy:
        case BM_16b_G3CH:
        {
          icUInt16Number *bits = (icUInt16Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 65535.0f;
          srcPixel[1] = (icFloatNumber)bits[1] / 65535.0f;
          srcPixel[2] = (icFloatNumber)bits[2] / 65535.0f;

          lpSrcBits += 6;
        }
        break;

        case BM_16b_GRAY:
        {
          icUInt16Number *bits = (icUInt16Number*)lpSrcBits;

          srcPixel[0] = (icFloatNumber)bits[0] / 65535.0f;

          lpSrcBits += 2;
        }
        break;

        //
        // 16 bpp - 5 bits for Red & Blue, 6 bits for Green
        //

        case BM_565RGB:
        {
          icUInt16Number bits = *((icUInt16Number*)lpSrcBits);

          srcPixel[0] = (icFloatNumber)((bits&0xF800)>>11) / 31.0f;
          srcPixel[1] = (icFloatNumber)((bits&0x07E0)>>5) / 63.0f;
          srcPixel[2] = (icFloatNumber)(bits&0x001F) / 31.0f;

          lpSrcBits += 2;
        }
        break;

        //
        // 32bpp - named color indices (1-based)
        //

        case BM_NAMED_INDEX:
        default:
          SetLastError(ERROR_UNSUPPORTED_TYPE);
          return FALSE;

      }
      

      if (pCmm->Apply(destPixel, srcPixel)!=icCmmStatOk) {
        SetLastError(ERROR_FUNCTION_FAILED);
        return FALSE;
      }

      *lpaResult = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);

      lpaResult++;
    }

    if (pfnCallback && !pfnCallback(j, dwHeight, ulCallbackData)) {
      SetLastError(ERROR_CANCELLED);
      return FALSE;
    }

    if (!dwStride) {
      size_t dwAlignedSize = (((lpSrcBits - lpSrcLine) + 3)>>2)<<2;
      lpSrcBits = lpSrcLine + dwAlignedSize;
    }
    else {
      lpSrcBits = lpSrcLine + dwStride;
    }
  }
  return TRUE;
}


/* Optionally defined
BOOL WINAPI CMCreateDeviceLinkProfile(
                                      LPHPROFILE lpahProfiles,  // array of profiles
                                      DWORD nProfiles,          // profile array size
                                      PDWORD padwIntents,       // array of rendering intents
                                      DWORD nIntents,           // intent array size
                                      DWORD dwFlags,            // transform creation flags
                                      LPBYTE *lpProfileData     // pointer to pointer to buffer
                                     )
{
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}
*/

static icRenderingIntent GetRenderingIntent(DWORD nIntent)
{
  switch(nIntent) {
    case INTENT_PERCEPTUAL:
      return icPerceptual;

    case INTENT_SATURATION:
      return icSaturation;

    case INTENT_RELATIVE_COLORIMETRIC:
      return icRelativeColorimetric;

    case INTENT_ABSOLUTE_COLORIMETRIC:
      return icAbsoluteColorimetric;

    default:
      return icUnknownIntent;
  }

  return icUnknownIntent;
}

HCMTRANSFORM WINAPI CMCreateMultiProfileTransform(
                                                  LPHPROFILE lpahProfiles,  // array of profile handles
                                                  DWORD nProfiles,          // profile handle array size
                                                  PDWORD padwIntents,       // array of rendering intents
                                                  DWORD nIntents,           // intent array size
                                                  DWORD dwFlags             // transform creation flags
                                                 )
{
  size_t rv;
  DWORD i, n=0;
  DWORD dwIntent;

  if (!nIntents) {
    SetLastError(ERROR_BAD_ARGUMENTS);
    return 0;
  }

  dwIntent = padwIntents[n];

  for (rv=0; rv<AVAILABLE_TRANSFORMS; rv++) {
    if (!g_Transforms[rv])
      break;
  }

  if (rv==AVAILABLE_TRANSFORMS) {
    SetLastError(ERROR_OUT_OF_STRUCTURES);
    return 0;
  }

  CIccCmm *pCmm = new CIccCmm();

  for (i=0; i<nProfiles; i++) {
    CIccProfile *pProfile = OpenProfile(lpahProfiles[i]);

    if (!pProfile) {
      delete pCmm;
      SetLastError(ERROR_BAD_ARGUMENTS);
      return 0;
    }
    
    if (pCmm->AddXform(pProfile, 
                       GetRenderingIntent(dwIntent),
                       icInterpLinear,
                       NULL,
                       (i+1==nProfiles && (dwFlags & ENABLE_GAMUT_CHECKING)) ? icXformLutGamut : icXformLutColor
                      ) != icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      SetLastError(ERROR_BAD_ARGUMENTS);
      return 0;
    }

    if (i) {
      if (n+1<nIntents) {
        n++;
      }
      dwIntent = padwIntents[n];
    }
  }

  if (pCmm->Begin()!=icCmmStatOk) {
    delete pCmm;
    SetLastError(ERROR_FUNCTION_FAILED);
    return 0;
  }

  pCmm->RemoveAllIO();

  pCmm = CIccMruCmm::Attach(pCmm);

  g_Transforms[rv] = pCmm;

  return (HCMTRANSFORM)(rv+256);
}


/*
BOOL WINAPI CMCreateProfile(
                            LPLOGCOLORSPACEA lpColorSpace,  // pointer to a color space
                            LPBYTE *lpProfileData           // pointer to pointer to buffer
                            )
{
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}


BOOL WINAPI CMCreateProfileW(
                             LPLOGCOLORSPACEW lpColorSpace,  // pointer to a color space
                             LPBYTE *lpProfileData           // pointer to pointer to buffer
                            )
{
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}
*/

static CIccProfile *GetProfileFromBuf(LPBYTE lpBuf)
{
  BYTE size[4];
  size[3] = lpBuf[0];
  size[2] = lpBuf[1];
  size[1] = lpBuf[2];
  size[0] = lpBuf[3];

  DWORD dwSize = *((DWORD*)&size[0]);

  if (!(dwSize > 128 && lpBuf[36]=='a' && lpBuf[37]=='c' && lpBuf[38]=='s' && lpBuf[39]=='p')) {
    return NULL;
  }

  return OpenIccProfile(lpBuf, dwSize);
}


HCMTRANSFORM WINAPI CMCreateTransformExt(
                                         LPLOGCOLORSPACEA lpColorSpace,        // color space pointer
                                         LPBYTE lpDevCharacter,        // device profile
                                         LPBYTE lpTargetDevCharacter,  // target profile
                                         DWORD dwFlags                         // creation flags
                                        )
{
  size_t rv;

  for (rv=0; rv<AVAILABLE_TRANSFORMS; rv++) {
    if (!g_Transforms[rv])
      break;
  }

  if (rv==AVAILABLE_TRANSFORMS) {
    SetLastError(ERROR_BAD_ARGUMENTS);
    return 0;
  }

  CIccCmm *pCmm = new CIccCmm();
  CIccProfile *pProfile;

  LPBYTE lpSrcProfileBuf=NULL;

  if (lpColorSpace) {
    if (!CreateProfileFromLogColorSpaceA(lpColorSpace, &lpSrcProfileBuf) || !lpSrcProfileBuf) {
      SetLastError(ERROR_INVALID_HANDLE);
      return 0;
    }

    pProfile = GetProfileFromBuf(lpSrcProfileBuf);

    if (!pProfile) {
      GlobalFree(lpSrcProfileBuf);
      SetLastError(ERROR_INVALID_HANDLE);
      return 0;
    }

    if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      GlobalFree(lpSrcProfileBuf);
      return 0;
    }
  }

  if (lpTargetDevCharacter) {
    pProfile = GetProfileFromBuf(lpTargetDevCharacter);
    if (!pProfile) {
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }
    if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }

    //If we are connecting a target between and input and output device profile we need to insert the target profile again
    if (lpSrcProfileBuf && lpDevCharacter) {
      pProfile = GetProfileFromBuf(lpTargetDevCharacter);
      if (!pProfile) {
        delete pCmm;
        GlobalFree(lpSrcProfileBuf);
        return 0;
      }
      if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
        delete pProfile;
        delete pCmm;
        GlobalFree(lpSrcProfileBuf);
        return 0;
      }
    }
  }

  if (lpDevCharacter) {
    pProfile = GetProfileFromBuf(lpDevCharacter);
    if (!pProfile) {
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }
    if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }

  }

  if (pCmm->Begin()!=icCmmStatOk) {
    delete pCmm;
    if (lpSrcProfileBuf)
      GlobalFree(lpSrcProfileBuf);
    return 0;
  }

  pCmm->RemoveAllIO();

  if (lpSrcProfileBuf)
    GlobalFree(lpSrcProfileBuf);

  g_Transforms[rv] = pCmm;

  return (HCMTRANSFORM)(rv+256);
}


HCMTRANSFORM WINAPI CMCreateTransformExtW(
                                          LPLOGCOLORSPACEW lpColorSpace,        // color space pointer
                                          LPBYTE lpDevCharacter,        // device profile
                                          LPBYTE lpTargetDevCharacter,  // target profile
                                          DWORD dwFlags                         // creation flags
                                         )
{
  size_t rv;

  for (rv=0; rv<AVAILABLE_TRANSFORMS; rv++) {
    if (!g_Transforms[rv])
      break;
  }

  if (rv==AVAILABLE_TRANSFORMS) {
    return 0;
  }

  CIccCmm *pCmm = new CIccCmm();
  CIccProfile *pProfile;

  LPBYTE lpSrcProfileBuf=NULL;

  if (lpColorSpace) {
    if (!CreateProfileFromLogColorSpaceW(lpColorSpace, &lpSrcProfileBuf) || !lpSrcProfileBuf) {
      return 0;
    }

    pProfile = GetProfileFromBuf(lpSrcProfileBuf);

    if (!pProfile) {
      GlobalFree(lpSrcProfileBuf);
      return 0;
    }

    if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      GlobalFree(lpSrcProfileBuf);
      return 0;
    }
  }

  if (lpTargetDevCharacter) {
    pProfile = GetProfileFromBuf(lpTargetDevCharacter);
    if (!pProfile) {
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }
    if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }

    //If we are connecting a target between and input and output device profile we need to insert the target profile again
    if (lpSrcProfileBuf && lpDevCharacter) {
      pProfile = GetProfileFromBuf(lpTargetDevCharacter);
      if (!pProfile) {
        delete pCmm;
        GlobalFree(lpSrcProfileBuf);
        return 0;
      }
      if (pCmm->AddXform(pProfile)!=icCmmStatOk) {
        delete pProfile;
        delete pCmm;
        GlobalFree(lpSrcProfileBuf);
        return 0;
      }
    }
  }

  if (lpDevCharacter) {
    pProfile = GetProfileFromBuf(lpDevCharacter);
    if (!pProfile) {
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }
    if (pCmm->AddXform(*pProfile)!=icCmmStatOk) {
      delete pProfile;
      delete pCmm;
      if (lpSrcProfileBuf)
        GlobalFree(lpSrcProfileBuf);
      return 0;
    }

  }

  if (pCmm->Begin()!=icCmmStatOk) {
    delete pCmm;
    if (lpSrcProfileBuf)
      GlobalFree(lpSrcProfileBuf);
      return 0;
  }

  if (lpSrcProfileBuf)
    GlobalFree(lpSrcProfileBuf);

    g_Transforms[rv] = pCmm;

  return (HCMTRANSFORM)(rv+256);
}


HCMTRANSFORM WINAPI CMCreateTransform(
                                      LPLOGCOLORSPACEA lpColorSpace,       // color space pointer
                                      LPBYTE lpDevCharacter,       // device profile
                                      LPBYTE lpTargetDevCharacter  // target profile
                                      )
{
  return CMCreateTransformExt(lpColorSpace, lpDevCharacter, lpTargetDevCharacter, NORMAL_MODE);
}


HCMTRANSFORM WINAPI CMCreateTransformW(
                                       LPLOGCOLORSPACEW lpColorSpace,       // color space pointer
                                       LPBYTE lpDevCharacter,       // device profile
                                       LPBYTE lpTargetDevCharacter  // target profile
                                       )
{
  return CMCreateTransformExtW(lpColorSpace, lpDevCharacter, lpTargetDevCharacter, NORMAL_MODE);
}




BOOL WINAPI CMDeleteTransform(
                              HCMTRANSFORM hcmTransform
                             )
{
  size_t i = (size_t)hcmTransform;

  if (i>256 && i<256+AVAILABLE_TRANSFORMS && g_Transforms[i-256]) {
    delete g_Transforms[i-256];
    g_Transforms[i-256] = NULL;

    return TRUE;
  }

  SetLastError(ERROR_INVALID_HANDLE);
  return FALSE;
}


DWORD WINAPI CMGetInfo(
                       DWORD dwInfo  
                      )
{
  switch (dwInfo) {
    case CMM_DESCRIPTION:
      return IDS_DESCRIPTION;

    case CMM_DLL_VERSION:
      return 0x00010000;

    case CMM_DRIVER_VERSION:
      return 0x01;

    case CMM_IDENT:
      return icSigIccDEV;

    case CMM_LOGOICON:
      return IDI_LOGO;

    case CMM_VERSION:
      return 0x00500008;

    case CMM_WIN_VERSION:
      return 0x00040000;

  }
  return 0;
}


BOOL WINAPI CMIsProfileValid(
                             HPROFILE hProfile, 
                             LPBOOL lpbValid
                            )
{
  if (!lpbValid) {
    SetLastError(ERROR_BAD_ARGUMENTS);
    return FALSE;
  }

  CIccProfile *pProfile = OpenProfile(hProfile);

  if (pProfile) {
    std::string report;

    if (pProfile->Validate(report)<=icValidateNonCompliant) {
      *lpbValid = TRUE;
    }
    else {
      *lpbValid = FALSE;
    }
    delete pProfile;

    return TRUE;
  }

  *lpbValid = FALSE;

  SetLastError(ERROR_INVALID_HANDLE);
  return FALSE;
}

BOOL WINAPI CMTranslateColors(
                              HCMTRANSFORM hcmTransform, 
                              LPCOLOR lpaInputColors, 
                              DWORD nColors, 
                              COLORTYPE ctInput, 
                              LPCOLOR lpaOutputColors, 
                              COLORTYPE ctOutput 
                             )
{
  size_t i = (size_t)hcmTransform;
  icFloatNumber srcPixel[MAX_COLOR_CHANNELS], destPixel[MAX_COLOR_CHANNELS];

  if (i<256 || i>=256+AVAILABLE_TRANSFORMS || !g_Transforms[i-256]) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccCmm *pCmm = g_Transforms[i-256];

  //Check if color types matches transform color spaces
  if (!IsValidColorType(ctInput, pCmm->GetSourceSpace()) ||
      !IsValidColorType(ctOutput, pCmm->GetDestSpace())) {
    SetLastError(ERROR_INVALID_DATATYPE);
    return FALSE;
  }


  for (i=0; i<nColors; i++) {
    switch (ctInput) {
      case COLOR_GRAY:
        srcPixel[0] = (icFloatNumber)(lpaInputColors->gray.gray) / 65535.0f;
        break;

      case COLOR_RGB:
        srcPixel[0] = (icFloatNumber)(lpaInputColors->rgb.red) / 65535.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->rgb.green) / 65535.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->rgb.blue) / 65535.0f;
        break;

      case COLOR_XYZ:
        srcPixel[0] = (icFloatNumber)(lpaInputColors->XYZ.X) / 65535.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->XYZ.Y) / 65535.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->XYZ.Z) / 65535.0f;

        break;

      case COLOR_Yxy:
        srcPixel[0] = (icFloatNumber)(lpaInputColors->Yxy.Y) / 65535.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->Yxy.x) / 65535.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->Yxy.y) / 65535.0f;
        break;

      case COLOR_Lab:
        srcPixel[0] = (icFloatNumber)(lpaInputColors->Lab.L) / 65535.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->Lab.a) / 65535.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->Lab.b) / 65535.0f;
        if (pCmm->GetFirstXformSource()!=icSigDevLabData) { //
          CIccPCSUtil::Lab2ToLab4(srcPixel, srcPixel);
        }
        break;

      case COLOR_3_CHANNEL:        // WORD per channel
        srcPixel[0] = (icFloatNumber)(lpaInputColors->gen3ch.ch1) / 65535.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->gen3ch.ch2) / 65535.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->gen3ch.ch3) / 65535.0f;
        break;

      case COLOR_CMYK:
        srcPixel[0] = (icFloatNumber)(lpaInputColors->cmyk.cyan) / 65535.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->cmyk.magenta) / 65535.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->cmyk.yellow) / 65535.0f;
        srcPixel[3] = (icFloatNumber)(lpaInputColors->cmyk.black) / 65535.0f;
        break;

      // Hifi colors use BYTE per channel
      case COLOR_8_CHANNEL:
        srcPixel[7] = (icFloatNumber)(lpaInputColors->hifi.channel[7]) / 255.0f;
        //No break here!

      case COLOR_7_CHANNEL:
        srcPixel[6] = (icFloatNumber)(lpaInputColors->hifi.channel[6]) / 255.0f;
        //No break here!

      case COLOR_6_CHANNEL:
        srcPixel[5] = (icFloatNumber)(lpaInputColors->hifi.channel[5]) / 255.0f;
        //No break here!

      case COLOR_5_CHANNEL:        
        srcPixel[0] = (icFloatNumber)(lpaInputColors->hifi.channel[0]) / 255.0f;
        srcPixel[1] = (icFloatNumber)(lpaInputColors->hifi.channel[1]) / 255.0f;
        srcPixel[2] = (icFloatNumber)(lpaInputColors->hifi.channel[2]) / 255.0f;
        srcPixel[3] = (icFloatNumber)(lpaInputColors->hifi.channel[3]) / 255.0f;
        srcPixel[4] = (icFloatNumber)(lpaInputColors->hifi.channel[4]) / 255.0f;
        break;

      default:
        SetLastError(ERROR_UNSUPPORTED_TYPE);
        return FALSE;
    }
    lpaInputColors++;

    if (pCmm->Apply(destPixel, srcPixel) != icCmmStatOk) {
      SetLastError(ERROR_FUNCTION_FAILED);
      return FALSE;
    }

    switch (ctOutput) {
      case COLOR_GRAY:
        lpaOutputColors->gray.gray = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        break;

      case COLOR_RGB:
        lpaOutputColors->rgb.red = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        lpaOutputColors->rgb.green = (icUInt16Number)(UnitClip(destPixel[1]) * 65535.0 + 0.5);
        lpaOutputColors->rgb.blue = (icUInt16Number)(UnitClip(destPixel[2]) * 65535.0 + 0.5);
        break;

      case COLOR_XYZ:
        lpaOutputColors->XYZ.X = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        lpaOutputColors->XYZ.Y = (icUInt16Number)(UnitClip(destPixel[1]) * 65535.0 + 0.5);
        lpaOutputColors->XYZ.Z = (icUInt16Number)(UnitClip(destPixel[2]) * 65535.0 + 0.5);
        break;

      case COLOR_Yxy:
        lpaOutputColors->Yxy.Y = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        lpaOutputColors->Yxy.x = (icUInt16Number)(UnitClip(destPixel[1]) * 65535.0 + 0.5);
        lpaOutputColors->Yxy.y = (icUInt16Number)(UnitClip(destPixel[2]) * 65535.0 + 0.5);
        break;

      case COLOR_Lab:
        if (pCmm->GetLastXformDest()!=icSigDevLabData) {
          CIccPCSUtil::Lab4ToLab2(destPixel, destPixel);
        }
        lpaOutputColors->Lab.L = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        lpaOutputColors->Lab.a = (icUInt16Number)(UnitClip(destPixel[1]) * 65535.0 + 0.5);
        lpaOutputColors->Lab.b = (icUInt16Number)(UnitClip(destPixel[2]) * 65535.0 + 0.5);
        break;

      case COLOR_3_CHANNEL:        // WORD per channel
        lpaOutputColors->gen3ch.ch1 = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        lpaOutputColors->gen3ch.ch2 = (icUInt16Number)(UnitClip(destPixel[1]) * 65535.0 + 0.5);
        lpaOutputColors->gen3ch.ch3 = (icUInt16Number)(UnitClip(destPixel[2]) * 65535.0 + 0.5);
        break;

      case COLOR_CMYK:
        lpaOutputColors->cmyk.cyan = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);
        lpaOutputColors->cmyk.magenta = (icUInt16Number)(UnitClip(destPixel[1]) * 65535.0 + 0.5);
        lpaOutputColors->cmyk.yellow = (icUInt16Number)(UnitClip(destPixel[2]) * 65535.0 + 0.5);
        lpaOutputColors->cmyk.black = (icUInt16Number)(UnitClip(destPixel[3]) * 65535.0 + 0.5);
        break;

        // Hifi colors use BYTE per channel
      case COLOR_8_CHANNEL:
        lpaOutputColors->hifi.channel[7] = (icUInt8Number)(UnitClip(destPixel[7]) * 255.0 + 0.5);
        //No break here!

      case COLOR_7_CHANNEL:
        lpaOutputColors->hifi.channel[6] = (icUInt8Number)(UnitClip(destPixel[6]) * 255.0 + 0.5);
        //No break here!

      case COLOR_6_CHANNEL:
        lpaOutputColors->hifi.channel[5] = (icUInt8Number)(UnitClip(destPixel[5]) * 255.0 + 0.5);
        //No break here!

      case COLOR_5_CHANNEL:        
        lpaOutputColors->hifi.channel[0] = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
        lpaOutputColors->hifi.channel[1] = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
        lpaOutputColors->hifi.channel[2] = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
        lpaOutputColors->hifi.channel[3] = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
        lpaOutputColors->hifi.channel[4] = (icUInt8Number)(UnitClip(destPixel[4]) * 255.0 + 0.5);
        break;

      default:
        SetLastError(ERROR_UNSUPPORTED_TYPE);
        return FALSE;
    }
    lpaOutputColors++;
  }
  return TRUE;
}


BOOL WINAPI CMTranslateRGB(
                           HCMTRANSFORM hcmTransform,
                           COLORREF ColorRef, 
                           LPCOLORREF lpColorRef, 
                           DWORD dwFlags
                          )
{
  size_t i = (size_t)hcmTransform;
  icFloatNumber srcPixel[MAX_COLOR_CHANNELS], destPixel[MAX_COLOR_CHANNELS];

  if (i<256 || i>=256+AVAILABLE_TRANSFORMS || !g_Transforms[i-256]) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  if (dwFlags != CMS_FORWARD) {
    SetLastError(ERROR_BAD_ARGUMENTS);
    return FALSE;
  }

  CIccCmm *pCmm = g_Transforms[i-256];

  //Check if color types matches transform color spaces
  if (pCmm->GetSourceSpace()!=icSigRgbData ||
      pCmm->GetDestSpace() != icSigRgbData) {
    SetLastError(ERROR_INVALID_DATATYPE);
    return FALSE;
  }

  LPBYTE pSrc = (LPBYTE)(&ColorRef);

  srcPixel[2] = (icFloatNumber)(*pSrc++) / 255.0f;
  srcPixel[1] = (icFloatNumber)(*pSrc++) / 255.0f;
  srcPixel[0] = (icFloatNumber)(*pSrc++) / 255.0f;

  if (pCmm->Apply(destPixel, srcPixel)!=icCmmStatOk) {
    SetLastError(ERROR_FUNCTION_FAILED);
    return FALSE;
  }

  LPBYTE pDest = (LPBYTE)lpColorRef;
  *pDest++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
  *pDest++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
  *pDest++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
  *pDest = *pSrc;

  return TRUE;
}


BOOL WINAPI CMTranslateRGBsExt(
                               HCMTRANSFORM hcmTransform, 
                               LPBYTE lpSrcBits,
                               BMFORMAT bmInput,
                               DWORD dwWidth, 
                               DWORD dwHeight,
                               DWORD dwInputStride, 
                               LPBYTE lpDestBits,
                               BMFORMAT bmOutput, 
                               DWORD dwOutputStride,
                               LPBMCALLBACKFN lpfnCallback,
                               ULONG ulCallbackData
                              )
{
  size_t i = (size_t)hcmTransform;
  icFloatNumber srcPixel[MAX_COLOR_CHANNELS], destPixel[MAX_COLOR_CHANNELS];

  if (i<256 || i>=256+AVAILABLE_TRANSFORMS || !g_Transforms[i-256]) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccCmm *pCmm = g_Transforms[i-256];
  LPBYTE lpSrcLine;
  LPBYTE lpDestLine;
  DWORD j;

  //Check if color types matches transform color spaces
  if (!IsValidBitmapType(bmInput, pCmm->GetSourceSpace()) ||
      !IsValidBitmapType(bmOutput, pCmm->GetDestSpace())) {
    SetLastError(ERROR_INVALID_DATATYPE);
    return FALSE;
  }

  for (j=0; j<dwHeight; j++) {
    lpSrcLine = lpSrcBits;
    lpDestLine = lpDestBits;

    for (i=0; i<dwWidth; i++) {
      switch(bmInput) {
        case BM_x555Lab:
          {
            icUInt16Number bits = *((icUInt16Number*)lpSrcBits);

            srcPixel[0] = (icFloatNumber)((bits&0x7C00)>>10) / 31.0f;
            srcPixel[1] = (icFloatNumber)((bits&0x03E0)>>5) / 31.0f;
            srcPixel[2] = (icFloatNumber)(bits&0x001F) / 31.0f;

            if (pCmm->GetSourceSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab2ToLab4(srcPixel, srcPixel);
            }

            lpSrcBits += 2;
          }
          break;

        case BM_x555RGB:
        case BM_x555XYZ:
        case BM_x555Yxy:
        case BM_x555G3CH:
          {
            icUInt16Number bits = *((icUInt16Number*)lpSrcBits);

            srcPixel[0] = (icFloatNumber)((bits&0x7C00)>>10) / 31.0f;
            srcPixel[1] = (icFloatNumber)((bits&0x03E0)>>5) / 31.0f;
            srcPixel[2] = (icFloatNumber)(bits&0x001F) / 31.0f;

            lpSrcBits += 2;
          }
          break;

          //
          // Packed 8 bits per channel => 8bpp for GRAY and
          // 24bpp for the 3 channel colors, more for hifi channels
          //

        case BM_LabTRIPLETS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;

            if (pCmm->GetSourceSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab2ToLab4(srcPixel, srcPixel);
            }

            lpSrcBits += 3;

          }
          break;

        case BM_RGBTRIPLETS:
        case BM_XYZTRIPLETS:
        case BM_YxyTRIPLETS:
        case BM_G3CHTRIPLETS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;

            lpSrcBits += 3;

          }
          break;

        case BM_BGRTRIPLETS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[0] / 255.0f;

            lpSrcBits += 3;

          }
          break;


        case BM_5CHANNEL:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
            srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;

            lpSrcBits += 5;

          }
          break;

        case BM_6CHANNEL:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
            srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;
            srcPixel[5] = (icFloatNumber)bits[5] / 255.0f;

            lpSrcBits += 6;

          }
          break;

        case BM_7CHANNEL:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
            srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;
            srcPixel[5] = (icFloatNumber)bits[5] / 255.0f;
            srcPixel[6] = (icFloatNumber)bits[6] / 255.0f;

            lpSrcBits += 7;

          }
          break;

        case BM_8CHANNEL:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;
            srcPixel[4] = (icFloatNumber)bits[4] / 255.0f;
            srcPixel[5] = (icFloatNumber)bits[5] / 255.0f;
            srcPixel[6] = (icFloatNumber)bits[6] / 255.0f;
            srcPixel[7] = (icFloatNumber)bits[7] / 255.0f;

            lpSrcBits += 8;

          }
          break;

        case BM_GRAY:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;

            lpSrcBits++;

          }
          break;

          //
          // 32bpp - 8 bits per channel. The most significant byte is ignored
          // for the 3 channel colors.
          //

        case BM_xRGBQUADS:
        case BM_xG3CHQUADS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[2] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[0] = (icFloatNumber)bits[2] / 255.0f;

            lpSrcBits += 4;

          }
          break;

        case BM_xBGRQUADS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;

            lpSrcBits += 4;

          }
          break;

        case BM_KYMCQUADS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[3] = (icFloatNumber)bits[3] / 255.0f;

            lpSrcBits += 4;

          }
          break;

        case BM_CMYKQUADS:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            srcPixel[3] = (icFloatNumber)bits[0] / 255.0f;
            srcPixel[2] = (icFloatNumber)bits[1] / 255.0f;
            srcPixel[1] = (icFloatNumber)bits[2] / 255.0f;
            srcPixel[0] = (icFloatNumber)bits[3] / 255.0f;

            lpSrcBits += 4;

          }
          break;

          //
          // 32bpp - 10 bits per channel. The 2 most significant bits are ignored.
          //

        case BM_10b_Lab:
          {
            icUInt32Number bits = *((icUInt32Number*)lpSrcBits);

            srcPixel[0] = (icFloatNumber)((bits&0x3FF00000)>>20) / 1023.0f;
            srcPixel[1] = (icFloatNumber)((bits&0x000FFC00)>>10) / 1023.0f;
            srcPixel[2] = (icFloatNumber)(bits&0x000003FF) / 1023.0f;

            if (pCmm->GetSourceSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab2ToLab4(srcPixel, srcPixel);
            }

            lpSrcBits += 4;
          }
          break;

        case BM_10b_RGB:
        case BM_10b_XYZ:
        case BM_10b_Yxy:
        case BM_10b_G3CH:
          {
            icUInt32Number bits = *((icUInt32Number*)lpSrcBits);

            srcPixel[0] = (icFloatNumber)((bits&0x3FF00000)>>20) / 1023.0f;
            srcPixel[1] = (icFloatNumber)((bits&0x000FFC00)>>10) / 1023.0f;
            srcPixel[2] = (icFloatNumber)(bits&0x000003FF) / 1023.0f;

            lpSrcBits += 4;
          }
          break;

          //
          // Packed 16 bits per channel => 16bpp for GRAY and
          // 48bpp for the 3 channel colors.
          //

        case BM_16b_Lab:
          {
            icUInt16Number *bits = (icUInt16Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 65535.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 65535.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 65535.0f;

            if (pCmm->GetSourceSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab2ToLab4(srcPixel, srcPixel);
            }

            lpSrcBits += 6;
          }
          break;

        case BM_16b_RGB:
        case BM_16b_XYZ:
        case BM_16b_Yxy:
        case BM_16b_G3CH:
          {
            icUInt16Number *bits = (icUInt16Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 65535.0f;
            srcPixel[1] = (icFloatNumber)bits[1] / 65535.0f;
            srcPixel[2] = (icFloatNumber)bits[2] / 65535.0f;

            lpSrcBits += 6;
          }
          break;

        case BM_16b_GRAY:
          {
            icUInt16Number *bits = (icUInt16Number*)lpSrcBits;

            srcPixel[0] = (icFloatNumber)bits[0] / 65535.0f;

            lpSrcBits += 2;
          }
          break;

          //
          // 16 bpp - 5 bits for Red & Blue, 6 bits for Green
          //

        case BM_565RGB:
          {
            icUInt16Number bits = *((icUInt16Number*)lpSrcBits);

            srcPixel[0] = (icFloatNumber)((bits&0xF800)>>11) / 31.0f;
            srcPixel[1] = (icFloatNumber)((bits&0x07E0)>>5) / 63.0f;
            srcPixel[2] = (icFloatNumber)(bits&0x001F) / 31.0f;

            lpSrcBits += 2;
          }
          break;

          //
          // 32bpp - named color indices (1-based)
          //

        case BM_NAMED_INDEX:
        default:
          SetLastError(ERROR_UNSUPPORTED_TYPE);
          return FALSE;

      }

      if (pCmm->Apply(destPixel, srcPixel)!=icCmmStatOk) {
        SetLastError(ERROR_FUNCTION_FAILED);
        return FALSE;
      }

      switch(bmOutput) {
        case BM_x555Lab:
          {
            if (pCmm->GetDestSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab4ToLab2(destPixel, destPixel);
            }
            *((icUInt16Number*)lpDestBits) = ((icUInt16Number)(UnitClip(destPixel[0]) * 31.0 + 0.5) << 10) + 
              ((icUInt16Number)(UnitClip(destPixel[1]) * 31.0 + 0.5) << 5) +
              (icUInt16Number)(UnitClip(destPixel[2]) * 31.0 + 0.5);

            lpDestBits += 2;
          }
          break;

        case BM_x555RGB:
        case BM_x555XYZ:
        case BM_x555Yxy:
        case BM_x555G3CH:
          {
            *((icUInt16Number*)lpDestBits) = ((icUInt16Number)(UnitClip(destPixel[0]) * 31.0 + 0.5) << 10) + 
              ((icUInt16Number)(UnitClip(destPixel[1]) * 31.0 + 0.5) << 5) +
              (icUInt16Number)(UnitClip(destPixel[2]) * 31.0 + 0.5);

            lpDestBits += 2;
          }
          break;

          //
          // Packed 8 bits per channel => 8bpp for GRAY and
          // 24bpp for the 3 channel colors, more for hifi channels
          //

        case BM_LabTRIPLETS:
          {
            if (pCmm->GetDestSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab4ToLab2(destPixel, destPixel);
            }
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
          }
          break;

        case BM_RGBTRIPLETS:
        case BM_XYZTRIPLETS:
        case BM_YxyTRIPLETS:
        case BM_G3CHTRIPLETS:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
          }
          break;

        case BM_BGRTRIPLETS:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
          }
          break;

        case BM_5CHANNEL:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[4]) * 255.0 + 0.5);
          }
          break;

        case BM_6CHANNEL:
          {
            icUInt8Number *bits = (icUInt8Number*)lpSrcBits;

            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[4]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[5]) * 255.0 + 0.5);
          }
          break;

        case BM_7CHANNEL:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[4]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[5]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[6]) * 255.0 + 0.5);
          }
          break;

        case BM_8CHANNEL:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[4]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[5]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[6]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[7]) * 255.0 + 0.5);
          }
          break;

        case BM_GRAY:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);;
          }
          break;

          //
          // 32bpp - 8 bits per channel. The most significant byte is ignored
          // for the 3 channel colors.
          //

        case BM_xRGBQUADS:
        case BM_xG3CHQUADS:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = 0x00;
          }
          break;

        case BM_xBGRQUADS:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = 0x00;
          }
          break;

        case BM_KYMCQUADS:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
          }
          break;

        case BM_CMYKQUADS:
          {
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[3]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *lpDestBits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
          }
          break;

          //
          // 32bpp - 10 bits per channel. The 2 most significant bits are ignored.
          //

        case BM_10b_Lab:
          {
            if (pCmm->GetDestSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab4ToLab2(destPixel, destPixel);
            }
            *((icUInt32Number*)lpDestBits) = ((icUInt32Number)(UnitClip(destPixel[0]) * 1023.0 + 0.5) << 20) + 
              ((icUInt32Number)(UnitClip(destPixel[1]) * 1023.0 + 0.5) << 10) +
              (icUInt32Number)(UnitClip(destPixel[2]) *  1023.0 + 0.5);

            lpDestBits += 4;
          }
          break;

        case BM_10b_RGB:
        case BM_10b_XYZ:
        case BM_10b_Yxy:
        case BM_10b_G3CH:
          {
            *((icUInt32Number*)lpDestBits) = ((icUInt32Number)(UnitClip(destPixel[0]) * 1023.0 + 0.5) << 20) + 
              ((icUInt32Number)(UnitClip(destPixel[1]) * 1023.0 + 0.5) << 10) +
              (icUInt32Number)(UnitClip(destPixel[2]) *  1023.0 + 0.5);

            lpDestBits += 4;
          }
          break;

          //
          // Packed 16 bits per channel => 16bpp for GRAY and
          // 48bpp for the 3 channel colors.
          //

        case BM_16b_Lab:
          {
            if (pCmm->GetDestSpace()!=icSigDevLabData) {
              CIccPCSUtil::Lab4ToLab2(destPixel, destPixel);
            }
            icUInt16Number *bits = (icUInt16Number*)lpDestBits;

            *bits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *bits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *bits   = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);

            lpDestBits += 6;
          }
          break;

        case BM_16b_RGB:
        case BM_16b_XYZ:
        case BM_16b_Yxy:
        case BM_16b_G3CH:
          {
            icUInt16Number *bits = (icUInt16Number*)lpDestBits;

            *bits++ = (icUInt8Number)(UnitClip(destPixel[0]) * 255.0 + 0.5);
            *bits++ = (icUInt8Number)(UnitClip(destPixel[1]) * 255.0 + 0.5);
            *bits   = (icUInt8Number)(UnitClip(destPixel[2]) * 255.0 + 0.5);

            lpDestBits += 6;
          }
          break;

        case BM_16b_GRAY:
          {
            *((icUInt16Number*)lpDestBits) = (icUInt16Number)(UnitClip(destPixel[0]) * 65535.0 + 0.5);

            lpDestBits += 2;
          }
          break;

          //
          // 16 bpp - 5 bits for Red & Blue, 6 bits for Green
          //

        case BM_565RGB:
          {
            *((icUInt16Number*)lpDestBits) = ((icUInt16Number)(UnitClip(destPixel[0]) * 31.0 + 0.5) << 11) + 
                                              ((icUInt16Number)(UnitClip(destPixel[1]) * 63.0 + 0.5) << 5) +
                                              (icUInt16Number)(UnitClip(destPixel[2]) * 31.0 + 0.5);

            lpDestBits += 2;
          }
          break;

          //
          // 32bpp - named color indices (1-based)
          //

        case BM_NAMED_INDEX:
        default:
          SetLastError(ERROR_UNSUPPORTED_TYPE);
          return FALSE;

      }
    }
    
    if (lpfnCallback && !lpfnCallback(j, dwHeight, ulCallbackData)) {
      SetLastError(ERROR_CANCELLED);
      return FALSE;
    }

    if (!dwInputStride) {
      size_t dwAlignedSize = (((lpSrcBits - lpSrcLine) + 3)>>2)<<2;
      lpSrcBits = lpSrcLine + dwAlignedSize;
    }
    else {
      lpSrcBits = lpSrcLine + dwInputStride;
    }

    if (!dwOutputStride) {
      size_t dwAlignedSize = (((lpDestBits - lpDestLine) + 3)>>2)<<2;
      lpDestBits = lpDestLine + dwAlignedSize;
    }
    else {
      lpSrcBits = lpDestLine + dwOutputStride;
    }
  }
  return TRUE;
}


BOOL WINAPI CMTranslateRGBs(
                            HCMTRANSFORM hcmTransform,
                            LPBYTE lpSrcBits, 
                            BMFORMAT bmInput, 
                            DWORD dwWidth, 
                            DWORD dwHeight, 
                            DWORD dwInputStride,
                            LPBYTE lpDestBits, 
                            BMFORMAT bmOutput, 
                            DWORD dwTranslateDirection
                           )
{
  if (dwTranslateDirection!=CMS_FORWARD) {
    SetLastError(ERROR_BAD_ARGUMENTS);
    return FALSE;
  }

  return CMTranslateRGBsExt(
    hcmTransform,
    lpSrcBits,
    bmInput,
    dwWidth,
    dwHeight,
    dwInputStride,
    lpDestBits,
    bmOutput,
    0,
    NULL,
    NULL);
}


BOOL WINAPI CMConvertColorNameToIndex(
                                      HPROFILE hProfile, 
                                      PCOLOR_NAME paColorName,
                                      PDWORD paIndex,
                                      DWORD dwCount
                                     )
{
  CIccProfile *pProfile = OpenProfile(hProfile);

  if (!pProfile) {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }

  CIccTagNamedColor2 *pTag = (CIccTagNamedColor2*)pProfile->FindTag(icSigNamedColor2Tag);
  if (!pTag) {
    delete pProfile;
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  DWORD i;

  for (i=0; i<dwCount; i++) {
    icInt32Number rv = pTag->FindRootColor(*paColorName);
    if (rv>=0)
      paIndex[i] = rv;
    else {
      delete pProfile;

      SetLastError(ERROR_NOT_FOUND);
      return FALSE;
    }

    paColorName++;
    paIndex++;
  }

  delete pProfile;

  return TRUE;
}


BOOL WINAPI CMConvertIndexToColorName(
                                      HPROFILE hProfile,
                                      PDWORD paIndex,
                                      PCOLOR_NAME paColorName,
                                      DWORD dwCount
                                     )
{
  CIccProfile *pProfile = OpenProfile(hProfile);

  if (!pProfile) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccTagNamedColor2 *pTag = (CIccTagNamedColor2*)pProfile->FindTag(icSigNamedColor2Tag);
  if (!pTag) {
    delete pProfile;

    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  DWORD i;

  for (i=0; i<dwCount; i++) {

    if (paIndex[i] > pTag->GetSize()) {
      SetLastError(ERROR_BAD_ARGUMENTS);
      return FALSE;
    }

    SIccNamedColorEntry *pEntry = pTag->GetEntry(paIndex[i]);

    memcpy(*paColorName, pEntry->rootName, sizeof(pEntry->rootName));

    paColorName++;
    paIndex++;
  }

  delete pProfile;

  return TRUE;
}


BOOL WINAPI CMGetNamedProfileInfo(
                                  HPROFILE hProfile,
                                  PNAMED_PROFILE_INFO pNamedProfileInfo
                                 )
{
  CIccProfile *pProfile = OpenProfile(hProfile);

  if (!pProfile) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  CIccTagNamedColor2 *pTag = (CIccTagNamedColor2*)pProfile->FindTag(icSigNamedColor2Tag);
  if (!pTag) {
    delete pProfile;
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  pNamedProfileInfo->dwFlags = pTag->GetVendorFlags();
  pNamedProfileInfo->dwCount = pTag->GetSize();
  pNamedProfileInfo->dwCountDevCoordinates = pTag->GetDeviceCoords();
  memcpy(pNamedProfileInfo->szPrefix, pTag->GetPrefix(), sizeof(pNamedProfileInfo->szPrefix));
  memcpy(pNamedProfileInfo->szSuffix, pTag->GetSufix(), sizeof(pNamedProfileInfo->szSuffix));

  delete pProfile;

  return TRUE;
}

