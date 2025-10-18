/*
    File:       iccSpecSepToTiff.cpp

    Contains:   Console app to concatenate several separated spectral tiff
                files into a single tiff file optionally including an
                embedded profile

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2013 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 12-7-2013
//
//////////////////////////////////////////////////////////////////////


#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccApplyBPC.h"
#include "TiffImg.h"

void Usage() 
{
  printf("Usage: SpecSep2Tiff output_file compress_flag sep_flag infile_fmt_file start_nm end_nm inc_nm {embedded_icc_profile_file}\n\n");
}

#ifndef _MAX_PATH
#define _MAX_PATH 510
#endif

//===================================================

int main(int argc, char* argv[]) {
    int minargs = 8; // Minimum number of arguments should account for the optional ICC profile
    if (argc < minargs) {
        std::cerr << "Usage: SpecSep2Tiff output_file compress_flag sep_flag infile_fmt_file start_nm end_nm inc_nm [embedded_icc_profile_file]\n";
        return -1;
    }

    CTiffImg infile[100], outfile;
    char filename[_MAX_PATH];
    int i, j, k;
    long bpl, bps;
    bool invert = false;
    int start, end, step, n;
    float xRes, yRes;

    bool bCompress = atoi(argv[2]) != 0;
    bool bSep = atoi(argv[3]) != 0;

    start = atoi(argv[5]);
    end = atoi(argv[6]);
    step = atoi(argv[7]);

    if (step == 0) {
        std::cerr << "Error: increment ('inc_nm') cannot be zero.\n";
        return -1;  // Exit the program with an error code
    }

    n = (end - start) / step + 1;  // Safe to perform division now

  for (i=0; i<n; i++) {
    sprintf(filename, argv[4], i*step + start);
    if (!infile[i].Open(filename)) {
      printf("Cannot open %s\n", filename);
      return -1;
    }

    if (infile[i].GetSamples() != 1) {
      printf("%s does not have 1 sampleperpixel\n", filename);
      return -1;
    }

    if (infile[i].GetPhoto() == PHOTOMETRIC_PALETTE) {
      printf("%s is a palette based file\n", filename);
      return -1;
    }

    if (i && (infile[i].GetWidth() != infile[0].GetWidth() ||
      infile[i].GetHeight() != infile[i-1].GetHeight() ||
      infile[i].GetBitsPerSample() != infile[i-1].GetBitsPerSample() ||
      infile[i].GetPhoto() != infile[i-1].GetPhoto() ||
      infile[i].GetXRes() != infile[i-1].GetXRes() ||
      infile[i].GetYRes() != infile[i-1].GetYRes())) {
        printf("%s doesn't have same format as other files\n", filename);
        return -1;
    }
  }
  bpl = infile[0].GetBytesPerLine();
  CTiffImg *f = &infile[0];

  if (f->GetPhoto()==PHOTO_MINISWHITE)
    invert = true;
  else if (f->GetPhoto()!=PHOTO_MINISBLACK) {
    printf("Photometric must be MinIsWhite or MinIsBlack\n");
    return -1;
  }

  bps = f->GetBitsPerSample()/8;
  
  icUInt8Number *inbuf = (icUInt8Number*)malloc(bpl*n);
  icUInt8Number *buf = (icUInt8Number*)malloc(f->GetWidth() * bps * n );
  icUInt8Number *sptr, *tptr;

  if (!inbuf || !buf) {
    printf("Memory allocation error!\n");
    goto cleanup;
  }

  xRes=f->GetXRes();
  yRes=f->GetYRes();

  if (xRes<1)
    xRes = 72;
  if (yRes<1)
    yRes = 72;

  if (outfile.Create(argv[1], f->GetWidth(), f->GetHeight(), f->GetBitsPerSample(), PHOTO_MINISBLACK, 
                     n, 0, xRes, yRes, bCompress, bSep)) {

    if (argc>8) {
      unsigned long length = 0;
      icUInt8Number *pDestProfile = NULL;

      CIccFileIO io;
      if (io.Open(argv[8], "r")) {
        length = io.GetLength();
        pDestProfile = (icUInt8Number *)malloc(length);
        if (pDestProfile) {
          io.Read8(pDestProfile, length);
          outfile.SetIccProfile(pDestProfile, length);
          free(pDestProfile);
        }
        io.Close();
      }
    }

    for (i=0; i<(int)f->GetHeight(); i++) {
      for (j=0; j<n; j++) {
        sptr = inbuf + j*bpl;
        if (!infile[j].ReadLine(sptr)) {
          printf("Error reading line %d of file %d\n", i, j);
          goto cleanup;
        }
        if (invert) {
          for (k=bpl; k>0; k--) {
            *sptr ^= 0xff;
            sptr++;
          }
        }
      }
      tptr = buf;
      for (k=0; k<(int)f->GetWidth(); k++) {
        for (j=0; j<n; j++) {
          sptr = inbuf + j*bpl + k*bps;
          memcpy(tptr, sptr, bps);
          tptr+=bps;
        }
      }
      outfile.WriteLine(buf);
    }
    printf("Image successfully written!\n");
  }
  else {
    printf("Unable to create %s\n", argv[1]);
  }

cleanup:
  if (inbuf)
    free(inbuf);

  if (buf)
    free(buf);

  for (i=0; i<n; i++)
    infile[i].Close();

  outfile.Close();

  return 0;
}
