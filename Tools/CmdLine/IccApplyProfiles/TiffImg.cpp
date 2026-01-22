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
#include "TiffImg.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTiffImg::CTiffImg()
{
  m_nWidth = 0;
  m_nHeight = 0;
  m_nBitsPerSample = 0;
  m_nSamples = 0;
  m_nExtraSamples = 0;

  m_hTif = NULL;
  m_pStripBuf = NULL;
}

CTiffImg::~CTiffImg()
{
  Close();
}

void CTiffImg::Close()
{
  m_nWidth = 0;
  m_nHeight = 0;
  m_nBitsPerSample = 0;
  m_nSamples = 0;
  m_nExtraSamples = 0;

  if (m_hTif) {
    TIFFClose(m_hTif);

    m_hTif = NULL;
  }

  if (m_pStripBuf) {
    free(m_pStripBuf);
    m_pStripBuf = NULL;
  }
}

bool CTiffImg::Create(const char *szFname, unsigned int nWidth, unsigned int nHeight,
              unsigned int nBPS, unsigned int nPhoto, unsigned int nSamples, unsigned int nExtraSamples,
              float fXRes, float fYRes, bool bCompress, bool bSep)
{
  Close();
  m_bRead = false;

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
    unsigned short* extrasamplevalues = (unsigned short*)calloc(m_nExtraSamples, sizeof(unsigned short));
    if (extrasamplevalues) {
      TIFFSetField(m_hTif, TIFFTAG_EXTRASAMPLES, m_nExtraSamples, extrasamplevalues);
      free(extrasamplevalues);
    }
  }
  TIFFSetField(m_hTif, TIFFTAG_BITSPERSAMPLE, m_nBitsPerSample);
  if (m_nBitsPerSample==32)
    TIFFSetField(m_hTif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
  TIFFSetField(m_hTif, TIFFTAG_ROWSPERSTRIP, m_nRowsPerStrip);
  TIFFSetField(m_hTif, TIFFTAG_COMPRESSION, m_nCompress);
  TIFFSetField(m_hTif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(m_hTif, TIFFTAG_XRESOLUTION, fXRes);
  TIFFSetField(m_hTif, TIFFTAG_YRESOLUTION, fYRes);
  if (bCompress) {
    if (m_nBitsPerSample==32) {
      TIFFSetField(m_hTif, TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT);
    }
    else {
      TIFFSetField(m_hTif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    }
  }

  m_nCurLine = 0;
  m_nCurStrip = 0;

  if (bSep && m_nSamples>1) {
    m_nStripSamples = m_nSamples;
    if (m_nBitsPerSample % 8) {
      Close();
      return false;
    }
    m_nBytesPerSample = m_nBitsPerSample / 8;

    m_nStripSize = (unsigned int)TIFFStripSize(m_hTif);
    m_nBytesPerStripLine = m_nWidth * m_nBytesPerSample;

    if (m_nStripSize!=m_nBytesPerStripLine) {
      Close();
      return false;
    }
    m_nBytesPerLine = m_nWidth * m_nBytesPerSample * m_nSamples;

    m_pStripBuf = (unsigned char*)malloc(m_nStripSize*m_nStripSamples);

    if (!m_pStripBuf) {
      Close();
      return false;
    }
    m_nStripsPerSample = m_nHeight / m_nRowsPerStrip;
  }
  else {
    m_nBytesPerLine = m_nStripSize = (unsigned int)TIFFStripSize(m_hTif);
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
  //icUInt16Number nPlanar=PLANARCONFIG_CONTIG;
  icUInt16Number nOrientation=ORIENTATION_TOPLEFT;
  icUInt16Number nSampleFormat=SAMPLEFORMAT_UINT;
  icUInt16Number *nSampleInfo=NULL;

  TIFFGetField(m_hTif, TIFFTAG_IMAGEWIDTH, &m_nWidth);
  TIFFGetField(m_hTif, TIFFTAG_IMAGELENGTH, &m_nHeight);
  TIFFGetField(m_hTif, TIFFTAG_PHOTOMETRIC, &m_nPhoto);
  TIFFGetField(m_hTif, TIFFTAG_PLANARCONFIG, &m_nPlanar);
  TIFFGetField(m_hTif, TIFFTAG_SAMPLESPERPIXEL, &m_nSamples);
  TIFFGetField(m_hTif, TIFFTAG_EXTRASAMPLES, &m_nExtraSamples, &nSampleInfo);
  TIFFGetField(m_hTif, TIFFTAG_BITSPERSAMPLE, &m_nBitsPerSample);
  TIFFGetField(m_hTif, TIFFTAG_SAMPLEFORMAT, &nSampleFormat);
  TIFFGetField(m_hTif, TIFFTAG_ROWSPERSTRIP, &m_nRowsPerStrip);
  TIFFGetField(m_hTif, TIFFTAG_ORIENTATION, &nOrientation);
  TIFFGetField(m_hTif, TIFFTAG_XRESOLUTION, &m_fXRes);
  TIFFGetField(m_hTif, TIFFTAG_YRESOLUTION, &m_fYRes);
  TIFFGetField(m_hTif, TIFFTAG_COMPRESSION, &m_nCompress);
  
  if (m_nRowsPerStrip == 0 || m_nSamples == 0 || m_nBitsPerSample == 0) {
    // Corrupt parameters - can't read the file
    // If the file is uncompressed, we might guess some of the values,
    // but it would take a bit of testing to get right.  Probably not worth it.
    Close();
    return false;
  }
  
  if (m_nRowsPerStrip > m_nHeight)
    m_nRowsPerStrip = m_nHeight;    // best guess, to limit memory allocated

  //Validate what we expect to work with
  if ((m_nBitsPerSample==32 && nSampleFormat!=SAMPLEFORMAT_IEEEFP) ||
      (m_nBitsPerSample!=32 && nSampleFormat!=SAMPLEFORMAT_UINT) ||
       nOrientation != ORIENTATION_TOPLEFT) {
    Close();
    return false;
  }
  
  m_nCurStrip=(unsigned int)-1;
  m_nCurLine = 0;

  m_nStripSize = (unsigned int)TIFFStripSize(m_hTif);

  if (m_nSamples>1 && m_nPlanar==PLANARCONFIG_SEPARATE) {
    m_nStripSamples = m_nSamples;
    m_nBytesPerLine = (m_nWidth * m_nBitsPerSample * m_nSamples + 7)>>3;
    m_nBytesPerSample = m_nBitsPerSample / 8;
    m_nBytesPerStripLine = m_nWidth * m_nBytesPerSample;
    //Only support bitspersample that fits on byte boundary
    if (m_nBitsPerSample%8) {
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
    m_nBytesPerLine = (m_nWidth * m_nBitsPerSample * m_nSamples + 7)>>3;
  }

  m_pStripBuf = (unsigned char*)malloc(m_nStripSize*m_nStripSamples);

  if (!m_pStripBuf) {
    Close();
    return false;
  }

  return true;
}


bool CTiffImg::ReadLine(unsigned char *pBuf)
{
  if (!m_bRead || m_nRowsPerStrip == 0)
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
  if (m_bRead)
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
  else
    return PHOTO_MINISWHITE;
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
  TIFFSetField(m_hTif, TIFFTAG_ICCPROFILE, nLen, pProfile);
  
  return true;
}
