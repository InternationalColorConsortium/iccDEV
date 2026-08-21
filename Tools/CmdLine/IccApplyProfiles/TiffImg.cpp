/*
    File:       TiffImg.cpp

    Contains:   Implementation of the CTiffImg class.

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2010 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include "TiffImg.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif


#if false && defined(_DEBUG)
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// Wrap this translation unit in the iccDEV namespace under the library-wide
// USEICCDEVNAMESPACE convention (see IccArrayFactory.cpp / IccTagFactory.cpp,
// and the sibling guard in IccTagEmbedIcc.cpp / IccTagJson.cpp). This localizes
// the file-local anonymous namespace below (the #1204 bounds-checking helpers
// kMaxTiffSamples / checkedUInt32 / calcBytesPerLine / canCreateRegularOutput)
// so it documents as iccDEV::anonymous_namespace{TiffImg.cpp} instead of a
// top-level anonymous_namespace{} (issue #1428). The macro is off in the default
// build, so this is compiled away there and only the Doxygen pass / namespace-
// enabled builds see the wrapper; behaviour and linkage are unchanged either way
// (the helpers keep internal linkage from the anonymous namespace regardless).
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

namespace {

const icUInt16Number kMaxTiffSamples = std::numeric_limits<icUInt16Number>::max();

bool checkedUInt32(icUInt64Number value, unsigned int &result)
{
  if (value > static_cast<icUInt64Number>(std::numeric_limits<unsigned int>::max()))
    return false;

  result = static_cast<unsigned int>(value);
  return true;
}

bool checkedUInt32Product(unsigned int a, unsigned int b, unsigned int &result)
{
  return checkedUInt32(static_cast<icUInt64Number>(a) * b, result);
}

bool canCreateRegularOutput(const char* szFname)
{
#if defined(_WIN32)
  return true;
#else
  struct stat st;
  if (stat(szFname, &st) != 0)
    return true;

  return S_ISREG(st.st_mode);
#endif
}

bool calcBytesPerLine(unsigned int width, unsigned int bitsPerSample,
                      unsigned int samples, unsigned int &bytesPerLine)
{
  if (!width || !bitsPerSample || !samples)
    return false;

  icUInt64Number bitsPerLine = static_cast<icUInt64Number>(width) *
                               bitsPerSample * samples;
  icUInt64Number bytes = bitsPerLine / 8;

  if (bitsPerLine % 8)
    bytes++;

  return checkedUInt32(bytes, bytesPerLine);
}

}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTiffImg::CTiffImg()
  : m_hTif(NULL),
    m_bRead(false),
    m_nWidth(0),
    m_nHeight(0),
    m_nBitsPerSample(0),
    m_nBytesPerSample(0),
    m_nPhoto(0),
    m_nSamples(0),
    m_nExtraSamples(0),
    m_nPlanar(0),
    m_nCompress(0),
    m_nSampleFormat(SAMPLEFORMAT_UINT),
    m_nOrientation(ORIENTATION_TOPLEFT),
    m_nResolutionUnit(RESUNIT_INCH),
    m_fXRes(0.0f),
    m_fYRes(0.0f),
    m_nBytesPerLine(0),
    m_nRowsPerStrip(0),
    m_nStripSize(0),
    m_nStripSamples(0),
    m_nStripsPerSample(0),
    m_nBytesPerStripLine(0),
    m_pStripBuf(NULL),
    m_nCurLine(0),
    m_nCurStrip(0),
    m_pProfile(NULL),
    m_nProfileLength(0)
{
}

CTiffImg::~CTiffImg()
{
  Close();
}

void CTiffImg::Close()
{
  // Close() is both the public reset and the destructor's cleanup (~CTiffImg
  // calls it), and Create()/Open() call it up front to recycle an instance.
  // Previously it released the two heap resources but only zeroed five of the
  // scalar members, leaving the rest (geometry, strip layout, cursors, photo,
  // resolution, profile fields) holding values from the prior image.  A reused
  // object therefore did not return to its freshly-constructed state, so a stale
  // member could leak into the next Open()/Create() if any path read it before
  // re-initializing it.  Align Close() with the constructor: free what is owned,
  // then reset EVERY member to the exact value CTiffImg::CTiffImg() initializes
  // it to, in the same order, so post-Close state is identical to post-ctor and
  // the object is safe to reuse (#1429).

  // Release owned resources first.  m_hTif is the libtiff handle; m_pStripBuf is
  // the only heap buffer this class allocates (in Open()/Create()).  Note
  // m_pProfile/m_nProfileLength are vestigial: GetIccProfile()/SetIccProfile()
  // route the ICC payload through libtiff's own TIFFTAG_ICCPROFILE storage and
  // never assign m_pProfile, so nulling it (below) frees nothing and cannot leak.
  if (m_hTif) {
    TIFFClose(m_hTif);
    m_hTif = NULL;
  }

  if (m_pStripBuf) {
    free(m_pStripBuf);
    m_pStripBuf = NULL;
  }

  // Reset all remaining members to their ctor-initialized values (ctor order).
  m_bRead = false;
  m_nWidth = 0;
  m_nHeight = 0;
  m_nBitsPerSample = 0;
  m_nBytesPerSample = 0;
  m_nPhoto = 0;
  m_nSamples = 0;
  m_nExtraSamples = 0;
  m_nPlanar = 0;
  m_nCompress = 0;
  m_nSampleFormat = SAMPLEFORMAT_UINT;
  m_nOrientation = ORIENTATION_TOPLEFT;
  m_nResolutionUnit = RESUNIT_INCH;
  m_fXRes = 0.0f;
  m_fYRes = 0.0f;
  m_nBytesPerLine = 0;
  m_nRowsPerStrip = 0;
  m_nStripSize = 0;
  m_nStripSamples = 0;
  m_nStripsPerSample = 0;
  m_nBytesPerStripLine = 0;
  m_nCurLine = 0;
  m_nCurStrip = 0;
  m_pProfile = NULL;
  m_nProfileLength = 0;
}

bool CTiffImg::Create(const char *szFname, unsigned int nWidth, unsigned int nHeight,
              unsigned int nBPS, unsigned int nPhoto, unsigned int nSamples, unsigned int nExtraSamples,
              float fXRes, float fYRes, bool bCompress, bool bSep,
              unsigned int nResolutionUnit)
{
  Close();
  m_bRead = false;

  if (nBPS % 8)
    return false;

  if (bCompress && nBPS != 8 && nBPS != 16 && nBPS != 32)
    return false;

  if (nSamples == 0 || nSamples > kMaxTiffSamples || nExtraSamples > nSamples)
    return false;

  // RESUNIT_NONE/INCH/CENTIMETER are the only units TIFF 6.0 defines, so anything else
  // cannot be written faithfully.  Refuse rather than write a file whose stated unit
  // disagrees with what the caller asked for -- silently dropping the request is the
  // very defect #2220 reports.  Neither tool caller can reach this in practice: their
  // unit comes back out of Open(), and libtiff rejects an out-of-range RESOLUTIONUNIT
  // while reading the directory ("Bad value 7 for \"ResolutionUnit\" tag") and leaves
  // the defaulted RESUNIT_INCH in place, so measured input can only yield a legal
  // value.  The guard is for direct callers of this public API.
  if (nResolutionUnit != RESUNIT_NONE && nResolutionUnit != RESUNIT_INCH &&
      nResolutionUnit != RESUNIT_CENTIMETER)
    return false;

  m_nWidth = nWidth;
  m_nHeight = nHeight;
  m_nBitsPerSample = (icUInt16Number)nBPS;
  m_nSamples = (icUInt16Number)nSamples;
  m_nExtraSamples = (icUInt16Number)nExtraSamples;
  m_nRowsPerStrip = 1;
  m_fXRes = fXRes;
  m_fYRes = fYRes;
  m_nPlanar = bSep ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG;
  m_nCompress = bCompress ? COMPRESSION_LZW : COMPRESSION_NONE;
  m_nSampleFormat = SAMPLEFORMAT_UINT;
  m_nResolutionUnit = (icUInt16Number)nResolutionUnit;
  
  // fix up some common errors from malformed TIFF files (which could cause errors down the line)
  if (m_fXRes <= 0.0)
    m_fXRes = 96.0;
  if (m_fYRes <= 0.0)
    m_fYRes = 96.0;

  switch(nPhoto) {
  case PHOTO_RGB:
    m_nPhoto = PHOTOMETRIC_RGB;
    break;

  case PHOTO_MINISBLACK:
    if (m_nSamples-m_nExtraSamples==3) 
      m_nPhoto = PHOTOMETRIC_RGB;
    else
      m_nPhoto = PHOTOMETRIC_MINISBLACK;
    break;

  case PHOTO_MINISWHITE:
    if (m_nSamples==4)
      m_nPhoto = PHOTOMETRIC_SEPARATED;
    else
      m_nPhoto = PHOTOMETRIC_MINISWHITE;
    break;

  case PHOTO_CIELAB:
    m_nPhoto = PHOTOMETRIC_CIELAB;
    break;

  case PHOTO_ICCLAB:
    m_nPhoto = PHOTOMETRIC_ICCLAB;
    break;
  }

  if (!canCreateRegularOutput(szFname)) {
    TIFFError(szFname,"Output image must be a regular file");
    return false;
  }

  m_hTif = TIFFOpen(szFname, "w");
  if (!m_hTif) {
    TIFFError(szFname,"Can not open output image");
    return false;
  }
  TIFFSetField(m_hTif, TIFFTAG_IMAGEWIDTH, (uint32_t) m_nWidth);
  TIFFSetField(m_hTif, TIFFTAG_IMAGELENGTH, (uint32_t) m_nHeight);
  TIFFSetField(m_hTif, TIFFTAG_PHOTOMETRIC, m_nPhoto);
  TIFFSetField(m_hTif, TIFFTAG_PLANARCONFIG, m_nPlanar);
  TIFFSetField(m_hTif, TIFFTAG_SAMPLESPERPIXEL, m_nSamples);
  if (m_nExtraSamples) {
    unsigned short* extrasamplevalues = static_cast<unsigned short*>(calloc(m_nExtraSamples, sizeof(unsigned short)));
    if (!extrasamplevalues) {
      Close();
      return false;
    }
    int extraStatus = TIFFSetField(m_hTif, TIFFTAG_EXTRASAMPLES, m_nExtraSamples, extrasamplevalues);
    free(extrasamplevalues);
    if (extraStatus != 1) {
      Close();
      return false;
    }
  }
  TIFFSetField(m_hTif, TIFFTAG_BITSPERSAMPLE, m_nBitsPerSample);
  if (m_nBitsPerSample >= 32) {
    m_nSampleFormat = SAMPLEFORMAT_IEEEFP;
    TIFFSetField(m_hTif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
  }
  TIFFSetField(m_hTif, TIFFTAG_ROWSPERSTRIP, m_nRowsPerStrip);
  TIFFSetField(m_hTif, TIFFTAG_COMPRESSION, m_nCompress);
  TIFFSetField(m_hTif, TIFFTAG_ORIENTATION, m_nOrientation);
  // Written unconditionally, next to the values it qualifies.  libtiff omits the tag
  // unless it is set and readers then fall back to their own default, so an output
  // converted from a centimetre-based source came out silently claiming inches and
  // its physical size shifted by 2.54x (#2220).  Stating the unit even when it is
  // RESUNIT_INCH costs one IFD entry and makes the file say what it means instead of
  // depending on the reader agreeing about the default.
  TIFFSetField(m_hTif, TIFFTAG_RESOLUTIONUNIT, m_nResolutionUnit);
  // m_fXRes/m_fYRes, not the raw parameters: a non-positive resolution was clamped to
  // 96 above, and writing the unclamped argument put the file at odds with the object
  // that produced it -- GetXRes() answered 96 while the file declared 0.  Harmless
  // while the unit was absent; now that an authoritative RESUNIT_INCH is written beside
  // them, the file would assert "0 pixels per inch".  Same class as #2220 itself, so it
  // is corrected here.  Neither tool caller is affected: iccSpecSepToTiff substitutes
  // its own value below 1, and iccApplyProfiles takes an already-clamped resolution out
  // of Open(), so for both m_fXRes == fXRes.
  TIFFSetField(m_hTif, TIFFTAG_XRESOLUTION, m_fXRes);
  TIFFSetField(m_hTif, TIFFTAG_YRESOLUTION, m_fYRes);
  if (bCompress) {
    if (m_nBitsPerSample >= 32) {
      TIFFSetField(m_hTif, TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT);
    }
    else {
      TIFFSetField(m_hTif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    }
  }

  m_nCurLine = 0;
  m_nCurStrip = 0;
  
  // we should always calculate this
  m_nBytesPerSample = m_nBitsPerSample / 8;

  if (bSep && m_nSamples>1) {
    m_nStripSamples = m_nSamples;
    if (m_nBitsPerSample % 8) {
      Close();
      return false;
    }

    // TIFFStripSize() returns a signed 64-bit tmsize_t, and the products below are
    // computed from a width that comes straight out of the source image header. Open()
    // already routes the identical arithmetic through calcBytesPerLine() (:471, :489)
    // and checkedUInt32(); Create() did the same sums raw, so the write path had no
    // guard where the read path had one.
    //
    // The wrap is reachable rather than theoretical. Create() runs before any pixel data
    // is read (iccApplyProfiles.cpp:508), so a source TIFF only has to *declare* a large
    // ImageWidth -- it needs no matching pixel data on disk. A width of 3e8 with a
    // 20-channel separated destination makes m_nBytesPerLine 6e9, which wraps to ~1.7e9,
    // while the strip allocation below asks for the true 6 GB and *succeeds* under Linux
    // overcommit -- so Create() returned true carrying a bytes-per-line far smaller than
    // a row. The caller then sizes its destination row buffer from exactly that value
    // (iccApplyProfiles.cpp:539) and writes a full row through it. The big allocation is
    // therefore not the accidental guard it looks like; measured, unfixed Create()
    // returns true for that shape.
    tmsize_t stripSize = TIFFStripSize(m_hTif);
    if (stripSize <= 0 ||
        !checkedUInt32(static_cast<icUInt64Number>(stripSize), m_nStripSize) ||
        !checkedUInt32Product(m_nWidth, m_nBytesPerSample, m_nBytesPerStripLine)) {
      Close();
      return false;
    }

    if (m_nStripSize!=m_nBytesPerStripLine) {
      Close();
      return false;
    }

    // calcBytesPerLine() rather than repeating the product: it is what Open() uses, and
    // with the nBPS % 8 rejection above it computes exactly m_nWidth * m_nBytesPerSample
    // * m_nSamples -- the same number, in 64 bits, refused when it will not fit.
    if (!calcBytesPerLine(m_nWidth, m_nBitsPerSample, m_nSamples, m_nBytesPerLine)) {
      Close();
      return false;
    }

    // Sized in 32 bits and rejected on overflow rather than left to malloc: relying on a
    // >4 GiB request failing is exactly what does not hold under overcommit.
    unsigned int nStripBufSize = 0;
    if (!checkedUInt32Product(m_nStripSize, m_nStripSamples, nStripBufSize)) {
      Close();
      return false;
    }

    m_pStripBuf = static_cast<unsigned char*>(malloc((size_t)nStripBufSize));

    if (m_nRowsPerStrip == 0 || !m_pStripBuf) {
      Close();
      return false;
    }
    m_nStripsPerSample = m_nHeight / m_nRowsPerStrip;
  }
  else {
    // Same narrowing as the separated branch above: tmsize_t is signed and 64-bit, so a
    // C cast can both truncate a large strip size and turn a libtiff error return into a
    // very large unsigned one. m_nBytesPerLine feeds the caller's row-buffer allocation,
    // so a wrong value here is the same class of defect on the contiguous path.
    tmsize_t stripSize = TIFFStripSize(m_hTif);
    if (stripSize <= 0 ||
        !checkedUInt32(static_cast<icUInt64Number>(stripSize), m_nStripSize)) {
      Close();
      return false;
    }
    m_nBytesPerLine = m_nStripSize;
    m_nStripSamples = 1;
  }

  return true;
}

bool CTiffImg::Open(const char *szFname)
{
  Close();
  m_bRead = true;

  m_hTif = TIFFOpen(szFname, "r");
  if (!m_hTif) {
    TIFFError(szFname,"Can not open input image");
    return false;
  }
  icUInt16Number *nSampleInfo=NULL;

  if (!TIFFGetField(m_hTif, TIFFTAG_IMAGEWIDTH, &m_nWidth) ||
      !TIFFGetField(m_hTif, TIFFTAG_IMAGELENGTH, &m_nHeight) ||
      !TIFFGetField(m_hTif, TIFFTAG_PHOTOMETRIC, &m_nPhoto) ||
      !TIFFGetField(m_hTif, TIFFTAG_BITSPERSAMPLE, &m_nBitsPerSample)) {
    Close();
    return false;
  }

  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_PLANARCONFIG, &m_nPlanar);
  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_SAMPLESPERPIXEL, &m_nSamples);
  TIFFGetField(m_hTif, TIFFTAG_EXTRASAMPLES, &m_nExtraSamples, &nSampleInfo);
  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_SAMPLEFORMAT, &m_nSampleFormat);
  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_ROWSPERSTRIP, &m_nRowsPerStrip);
  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_ORIENTATION, &m_nOrientation);
  // Defaulted rather than plain Get: an absent RESOLUTIONUNIT means inches per TIFF
  // 6.0, and libtiff supplies that, so this yields a usable unit for every input.
  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_RESOLUTIONUNIT, &m_nResolutionUnit);
  TIFFGetField(m_hTif, TIFFTAG_XRESOLUTION, &m_fXRes);
  TIFFGetField(m_hTif, TIFFTAG_YRESOLUTION, &m_fYRes);
  TIFFGetFieldDefaulted(m_hTif, TIFFTAG_COMPRESSION, &m_nCompress);
  
  if (m_nWidth == 0 || m_nHeight == 0 || m_nRowsPerStrip == 0 ||
      m_nSamples == 0 || m_nSamples > kMaxTiffSamples ||
      m_nExtraSamples > m_nSamples || m_nBitsPerSample == 0) {
    // Corrupt parameters - can't read the file
    // If the file is uncompressed, we might guess some of the values,
    // but it would take a bit of testing to get right.  Probably not worth it.
    Close();
    return false;
  }
  
  if (m_nRowsPerStrip > m_nHeight)
    m_nRowsPerStrip = m_nHeight;    // best guess, to limit memory allocated

  // Validate what we expect to work with:
  // 32 bit or greater is floating point
  // less than 32 bit is unsigned integer
  // this could be more general, but will require more code and testing
  if ((m_nBitsPerSample >= 32 && m_nSampleFormat != SAMPLEFORMAT_IEEEFP) ||
      (m_nBitsPerSample < 32 && m_nSampleFormat != SAMPLEFORMAT_UINT) ||
       m_nOrientation != ORIENTATION_TOPLEFT) {
    Close();
    return false;
  }
  
  // fix up some common errors from malformed TIFF files (which could cause errors down the line)
  if (m_fXRes <= 0.0)
    m_fXRes = 96.0;
  if (m_fYRes <= 0.0)
    m_fYRes = 96.0;
  
  m_nCurStrip=(unsigned int)-1;
  m_nCurLine = 0;

  tmsize_t stripSize = TIFFStripSize(m_hTif);
  if (stripSize <= 0 ||
      !checkedUInt32(static_cast<icUInt64Number>(stripSize), m_nStripSize)) {
    Close();
    return false;
  }

  if (m_nSamples>1 && m_nPlanar==PLANARCONFIG_SEPARATE) {
    m_nStripSamples = m_nSamples;
    //Only support bitspersample that fits on byte boundary
    if (m_nBitsPerSample%8) {
      Close();
      return false;
    }
    if (!calcBytesPerLine(m_nWidth, m_nBitsPerSample, m_nSamples, m_nBytesPerLine)) {
      Close();
      return false;
    }
    m_nBytesPerSample = m_nBitsPerSample / 8;
    if (m_nRowsPerStrip == 0 || !checkedUInt32Product(m_nWidth, m_nBytesPerSample, m_nBytesPerStripLine)) {
      Close();
      return false;
    }
    m_nStripsPerSample = m_nHeight / m_nRowsPerStrip;
    //Only support separations that evenly fit into strips
    if (m_nHeight % m_nRowsPerStrip) {
      Close();
      return false;
    }
  }
  else {
    m_nStripSamples = 1;
    if (!calcBytesPerLine(m_nWidth, m_nBitsPerSample, m_nSamples, m_nBytesPerLine)) {
      Close();
      return false;
    }
  }
  
  // Just in case we had to recalc the strip byte count,
  //   it is safer to have the buffer too large than too small.
  unsigned int minStripSize = 0;
  if (!checkedUInt32Product(m_nRowsPerStrip, m_nBytesPerLine, minStripSize)) {
    Close();
    return false;
  }
  m_nStripSize = std::max( m_nStripSize, minStripSize );
  
  unsigned int stripBufferSize = 0;
  if (!checkedUInt32Product(m_nStripSize, m_nStripSamples, stripBufferSize)) {
    Close();
    return false;
  }

  m_pStripBuf = static_cast<unsigned char*>(malloc((size_t)stripBufferSize));

  if (!m_pStripBuf) {
    Close();
    return false;
  }

  return true;
}


bool CTiffImg::ReadLine(unsigned char *pBuf)
{
  if (!m_bRead || m_nRowsPerStrip == 0 ||
      m_nSamples == 0 || m_nSamples > kMaxTiffSamples ||
      m_nStripSamples == 0 || m_nStripSamples > kMaxTiffSamples)
    return false;

  unsigned int nStrip = m_nCurLine / m_nRowsPerStrip;
  unsigned int nRowOffset = m_nCurLine % m_nRowsPerStrip;

  if (nStrip != m_nCurStrip) {
    m_nCurStrip = nStrip;

    if (m_nStripSamples>1) {
      unsigned int s;
      unsigned char *pos = m_pStripBuf;
      unsigned int nStripOffset = 0;
      for (s=0; s<m_nStripSamples; s++) {
        if (TIFFReadEncodedStrip(m_hTif, m_nCurStrip+nStripOffset, pos, m_nStripSize) < 0) {
          return false;
        }
        nStripOffset += m_nStripsPerSample;
        pos += m_nBytesPerStripLine;
      }
    }
    else if (TIFFReadEncodedStrip(m_hTif, m_nCurStrip, m_pStripBuf, m_nStripSize) < 0) {
      return false;
    }
  }

  if (m_nStripSamples>1) { //Sep to contig
    unsigned char *src, *dst;
    src = m_pStripBuf+nRowOffset*m_nBytesPerStripLine;
    dst = pBuf;
    unsigned int w, s;
    for (w=0; w<m_nWidth; w++) {
      unsigned char *pos = src;
      for (s=0; s<m_nSamples; s++) {
        memcpy(dst, pos, m_nBytesPerSample);
        dst += m_nBytesPerSample;
        pos += m_nStripSize;
      }
      src += m_nBytesPerSample;
    }
  }
  else {
    memcpy(pBuf, m_pStripBuf+nRowOffset*m_nBytesPerLine, m_nBytesPerLine);
  }
  m_nCurLine++;

  return true;
}

bool CTiffImg::WriteLine(unsigned char *pBuf)
{
  if (m_bRead ||
      m_nSamples == 0 || m_nSamples > kMaxTiffSamples ||
      m_nStripSamples == 0 || m_nStripSamples > kMaxTiffSamples)
    return false;

  if (m_nCurStrip < m_nHeight) { //Contig to Sep
    if (m_nStripSamples>1) {
      unsigned char *src, *dst;
      src = pBuf;
      dst = m_pStripBuf;
      unsigned int w, s, offset;
      for (w=0; w<m_nWidth; w++) {
        unsigned char *pos = dst;
        for (s=0; s<m_nSamples; s++) {
          memcpy(pos, src, m_nBytesPerSample);
          src += m_nBytesPerSample;
          pos += m_nStripSize;
        }
        dst += m_nBytesPerSample;
      }
      offset = 0;
      src = m_pStripBuf;
      for (s=0; s<m_nSamples; s++) {
        if (TIFFWriteEncodedStrip(m_hTif, m_nCurStrip+offset, src, m_nStripSize) < 0)
          return false;
        offset += m_nStripsPerSample;
        src += m_nStripSize;
      }
    }
    else if (TIFFWriteEncodedStrip(m_hTif, m_nCurStrip, pBuf, m_nBytesPerLine) < 0)
      return false;

    m_nCurStrip++;
  }

  return true;
}

unsigned int CTiffImg::GetPhoto()
{
  if (m_nPhoto == PHOTOMETRIC_RGB) {
    return PHOTO_RGB;
  }
  else if (m_nPhoto == PHOTOMETRIC_MINISBLACK) {
    return PHOTO_MINISBLACK;
  }
  else if (m_nPhoto == PHOTOMETRIC_MINISWHITE ||
           m_nPhoto==PHOTOMETRIC_SEPARATED) {
    return PHOTO_MINISWHITE;
  }
  else if (m_nPhoto==PHOTOMETRIC_CIELAB)
    return PHOTO_CIELAB;
  else if (m_nPhoto==PHOTOMETRIC_ICCLAB)
    return PHOTO_ICCLAB;
  else if (m_nPhoto==PHOTOMETRIC_PALETTE)
    // Palette TIFFs previously fell through to PHOTO_MINISWHITE and were silently
    // accepted; give them their own value so callers can reject them (#1381).
    return PHOTO_PALETTE;
  else
    // Report unrecognised photometrics as UNKNOWN rather than masking them as
    // PHOTO_MINISWHITE, so callers fail closed on unsupported input (#1380).
    return PHOTO_UNKNOWN;
}


bool CTiffImg::GetIccProfile(unsigned char *&pProfile, unsigned int &nLen)
{
  pProfile = NULL;
  nLen = 0;

  TIFFGetField(m_hTif, TIFFTAG_ICCPROFILE, &nLen, &pProfile);

  return pProfile!=NULL && nLen>0;
}

bool CTiffImg::SetIccProfile(unsigned char *pProfile, unsigned int nLen)
{
  return TIFFSetField(m_hTif, TIFFTAG_ICCPROFILE, nLen, pProfile) == 1;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
