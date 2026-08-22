/*
    File:       TiffImg.cpp

    Contains:   Implementation of the CTiffImg class.

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * Copyright (c) International Color Consortium.
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
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#if !defined(_TIFFIMG_H)
#define _TIFFIMG_H

#include "tiffio.h"                  // need tiff library
#include "IccProfLibConf.h"
#include "icProfileHeader.h"

#define PHOTO_MINISBLACK  0
#define PHOTO_MINISWHITE  1
#define PHOTO_CIELAB      2
#define PHOTO_ICCLAB      3
#define PHOTO_RGB         4
// PHOTO_PALETTE / PHOTO_UNKNOWN are CTiffImg's INTERNAL photometric values
// (returned by GetPhoto()), and are deliberately NOT libtiff's PHOTOMETRIC_*
// constants.  GetPhoto() must map PHOTOMETRIC_PALETTE to PHOTO_PALETTE so callers
// can detect/reject palette input (#1381), and report anything it does not
// recognise as PHOTO_UNKNOWN instead of silently as PHOTO_MINISWHITE (#1380).
#define PHOTO_PALETTE     5
#define PHOTO_UNKNOWN     0xffffffff

// Declare CTiffImg inside the iccDEV namespace under the library-wide
// USEICCDEVNAMESPACE convention so its definitions in TiffImg.cpp (which wraps
// the whole translation unit in iccDEV for #1428) can enclose this class.  The
// macro is off in the default build, so CTiffImg stays in the global namespace
// there and the three tools that include this header (iccApplyProfiles,
// iccTiffDump, iccSpecSepToTiff) are unaffected; only the Doxygen pass /
// namespace-enabled builds see the wrapper.  The PHOTO_* photometric codes above
// stay as preprocessor macros (no namespace) so callers keep using them as-is.
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CTiffImg
{
public:
  CTiffImg();
  virtual ~CTiffImg();

  void Close();

  // nResolutionUnit names the unit fXRes/fYRes are expressed in (one of libtiff's
  // RESUNIT_* codes).  It is a defaulted trailing parameter so the existing callers
  // that do not care keep compiling unchanged and keep writing inch-based output,
  // while the tools that copy a source image's resolution can now carry its unit
  // across as well (#2220).
  bool Create(const char *szFname, unsigned int nWidth, unsigned int nHeight,
              unsigned int nBPS, unsigned int nPhoto, unsigned int nSamples, unsigned int nExtraSamples,
              float fXRes, float fYRes, bool bCompress=true, bool bSep=false,
              unsigned int nResolutionUnit=RESUNIT_INCH);
  bool Open(const char *szFname);
  // True once this object's Create() got past TIFFOpen(..., "w") -- i.e. the
  // destination was actually created or truncated by this run.  A caller that
  // discards a failed output needs this to tell "Create() refused the path and
  // touched nothing" from "Create() opened the path and then failed", because
  // only the second case leaves something of ours to remove (#2242).  Reset by
  // Create() and Open(), and deliberately NOT cleared by Close(), so it stays
  // readable on the failure path after Create() has closed the handle.
  bool WasOutputOpened() const { return m_bOutputOpened; }

  bool ReadLine(unsigned char *pBuf);
  bool WriteLine(unsigned char *pBuf);
  
  unsigned int GetWidth() { return m_nWidth;}
  unsigned int GetHeight() { return m_nHeight;}
  // Physical size in inches, as the names promise.  m_fXRes/m_fYRes are expressed in
  // whatever unit TIFFTAG_RESOLUTIONUNIT names, so the bare division these used to do
  // only yields inches when that unit is RESUNIT_INCH: a centimetre-based image was
  // reported 2.54x too large.  RESUNIT_NONE carries no absolute size at all -- its
  // resolution values fix only an aspect ratio (TIFF 6.0, TIFFTAG_RESOLUTIONUNIT) --
  // so return 0 there and let the caller omit the figure rather than print a
  // meaningless one (#2220).
  double GetWidthIn() { return InchesFromPixels(m_nWidth, m_fXRes); }
  double GetHeightIn() { return InchesFromPixels(m_nHeight, m_fYRes); }
  unsigned int GetBitsPerSample() { return m_nBitsPerSample;}
  unsigned int GetPhoto();
  unsigned int GetSamples() { return m_nSamples;}
  unsigned int GetExtraSamples() { return m_nExtraSamples; }
  unsigned int GetCompress() { return m_nCompress; }
  unsigned int GetPlanar() { return m_nPlanar; }
  unsigned int GetSampleFormat() { return m_nSampleFormat; }
  unsigned int GetOrientation() { return m_nOrientation; }
  // The unit GetXRes()/GetYRes() are counted in; RESUNIT_INCH when the source image
  // said nothing, which is what libtiff defaults an absent tag to.
  unsigned int GetResolutionUnit() { return m_nResolutionUnit; }
  float GetXRes() {return m_fXRes;}
  float GetYRes() {return m_fYRes;}

  unsigned int GetBytesPerLine() { return m_nBytesPerLine; }

  bool GetIccProfile(unsigned char *&pProfile, unsigned int  &nLen);
  bool SetIccProfile(unsigned char *pProfile, unsigned int  nLen);

protected:
  // Shared by GetWidthIn()/GetHeightIn(); see the comment on those.  Guards fRes as
  // well as the unit because it is genuinely observable as zero: a freshly constructed
  // object has it, and so does one whose Open() failed, since the failure path runs
  // Close() and Close() resets it before the non-positive-resolution fix-up is reached.
  // Both members are zero together in those states, so the old bare division computed
  // 0.0/0.0 and returned a NaN.
  double InchesFromPixels(unsigned int nPixels, float fRes)
  {
    if (fRes <= 0.0f || m_nResolutionUnit == RESUNIT_NONE)
      return 0.0;

    double dInches = (double)nPixels / (double)fRes;
    if (m_nResolutionUnit == RESUNIT_CENTIMETER)
      dInches /= 2.54;

    return dInches;
  }

  TIFF *m_hTif;
  bool m_bRead;
  bool m_bOutputOpened;

  unsigned int m_nWidth;
  unsigned int m_nHeight;
  icUInt16Number m_nBitsPerSample;
  icUInt16Number m_nBytesPerSample;
  icUInt16Number m_nPhoto;
  icUInt16Number m_nSamples;
  icUInt16Number m_nExtraSamples;
  icUInt16Number m_nPlanar;
  icUInt16Number m_nCompress;
  icUInt16Number m_nSampleFormat;
  icUInt16Number m_nOrientation;
  icUInt16Number m_nResolutionUnit;

  float m_fXRes;
  float m_fYRes;

  unsigned int m_nBytesPerLine;
  unsigned int m_nRowsPerStrip;
  unsigned int m_nStripSize;
  unsigned int m_nStripSamples;
  unsigned int m_nStripsPerSample;
  unsigned int m_nBytesPerStripLine;

  unsigned char *m_pStripBuf;

  unsigned int m_nCurLine;
  unsigned int m_nCurStrip;

  unsigned char *m_pProfile;
  unsigned int m_nProfileLength;
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_TIFFIMG_H)
