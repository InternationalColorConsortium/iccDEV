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

class CTiffImg  
{
public:
  CTiffImg();
  virtual ~CTiffImg();

  void Close();

  bool Create(const char *szFname, unsigned int nWidth, unsigned int nHeight,
              unsigned int nBPS, unsigned int nPhoto, unsigned int nSamples, unsigned int nExtraSamples,
              float fXRes, float fYRes, bool bCompress=true, bool bSep=false);
  bool Open(const char *szFname);

  bool ReadLine(unsigned char *pBuf);
  bool WriteLine(unsigned char *pBuf);
  
  unsigned int GetWidth() { return m_nWidth;}
  unsigned int GetHeight() { return m_nHeight;}
  double GetWidthIn() { return (double)m_nWidth / m_fXRes; }
  double GetHeightIn() { return (double)m_nHeight / m_fYRes; }
  unsigned int GetBitsPerSample() { return m_nBitsPerSample;}
  unsigned int GetPhoto();
  unsigned int GetSamples() { return m_nSamples;}
  unsigned int GetExtraSamples() { return m_nExtraSamples; }
  unsigned int GetCompress() { return m_nCompress; }
  unsigned int GetPlanar() { return m_nPlanar; }
  float GetXRes() {return m_fXRes;}
  float GetYRes() {return m_fYRes;}

  unsigned int GetBytesPerLine() { return m_nBytesPerLine; }

  bool GetIccProfile(unsigned char *&pProfile, unsigned int  &nLen);
  bool SetIccProfile(unsigned char *pProfile, unsigned int  nLen);

protected:
  TIFF *m_hTif;
  bool m_bRead;

  unsigned int m_nWidth;
  unsigned int m_nHeight;
  icUInt16Number m_nBitsPerSample;
  icUInt16Number m_nBytesPerSample;
  icUInt16Number m_nPhoto;
  icUInt16Number m_nSamples;
  icUInt16Number m_nExtraSamples;
  icUInt16Number m_nPlanar;
  icUInt16Number m_nCompress;

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

#endif // !defined(_TIFFIMG_H)
