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


#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>
#include <memory>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccApplyBPC.h"
#include "TiffImg.h"

//===================================================

void Usage(const char *name)
{
  // remove path before command name
  const char *strippedName = strrchr( name, '/' );      // Unix/MacOS
  if (strippedName == NULL) {
    strippedName = strrchr( name, '\\' );    // Windows
    if (strippedName == NULL)
      strippedName = name;
  }
  else
    ++strippedName;

  printf("Usage: %s output compress sep infile_fmt start end incr {profile}\n", strippedName ); // argv[0]
  printf("Concatenates several spectral TIFF files into a single file, with optional embedded ICC profile.\n");
  
  printf("\toutput: path/name of the TIFF file to be created\n");                               // argv[1]
  printf("\tcompress: boolean (0 | 1), should the output be compressed\n");                     // argv[2]
  printf("\tsep: boolean (0 | 1), plane data be seperated in the output TIFF\n");               // argv[3]
  printf("\tinfile_fmt: printf format string for input files, example: \"spec_%%06d.tiff\"\n"); // argv[4]
  printf("\tstart: integer, first channel number to process\n");                                // argv[5]
  printf("\tend: integer, last channel number to process\n");                                   // argv[6]
  printf("\tincrement: integer, increment between channels\n");                                 // argv[7]
  printf("\tprofile: optional ICC profile to embed in the output TIFF\n");                      // argv[8]
  printf("\n");
}

//===================================================

int main(int argc, char* argv[]) {
  const int minargs = 8; // argc = 8 without profile, 9 with profile
  
  if (argc < minargs) {
    Usage(argv[0]);
    return -1;
  }

  bool bCompress = atoi(argv[2]) != 0;
  bool bSep = atoi(argv[3]) != 0;

  int start = atoi(argv[5]);
  int end = atoi(argv[6]);
  int step = atoi(argv[7]);

  if (step == 0) {
    printf("Error: increment cannot be zero.\n");
    return -1;  // Exit the program with an error code
  }

  // we do allow end < start, when step is negative
  if ( ((end < start) && (step > 0))
    || ((end > start) && (step < 0)) ) {
    printf("Bad steps values would overflow: %d, %d, %d\n", start, end, step );
    return -1;
  }

  int nSamples = std::abs(end - start) / step + 1;  // Safe to perform division now

  if (nSamples < 1) {  // just in case
    printf("Zero samples specified: %d, %d, %d\n", start, end, step );
    return -1;
  }

  // open ALL input files
  std::vector<CTiffImg> infile(nSamples);

  for (int i=0; i<nSamples; i++) {
    const int max_path_length = 510;
    char filename[ max_path_length ];
    
    int channelNum = i*step + start;
    snprintf(filename, max_path_length, argv[4], channelNum);
    if (!infile[i].Open(filename)) {
      printf("Cannot open input %s\n", filename);
      return -1;
    }

    if (infile[i].GetSamples() != 1) {
      printf("input %s does not have 1 sample per pixel\n", filename);
      return -1;
    }

    if (infile[i].GetPhoto() == PHOTOMETRIC_PALETTE) {
      printf("input %s is a palette based file\n", filename);
      return -1;
    }

    if (i && (infile[i].GetWidth() != infile[0].GetWidth() ||
      infile[i].GetHeight() != infile[0].GetHeight() ||
      infile[i].GetBitsPerSample() != infile[0].GetBitsPerSample() ||
      infile[i].GetPhoto() != infile[0].GetPhoto() ||
      infile[i].GetXRes() != infile[0].GetXRes() ||
      infile[i].GetYRes() != infile[0].GetYRes())) {
        printf("input %s does not have same format as other files\n", filename);
        return -1;
    }
  }
  
  // all inputs are open now
  
  // use the first input file for error checking and format info
  // since we made sure all inputs match basic format.
  CTiffImg *f = &infile[0];

  long bytePerLine = f->GetBytesPerLine();
  
  bool invert = false;
  if (f->GetPhoto()==PHOTO_MINISWHITE)
    invert = true;
  else if (f->GetPhoto()!=PHOTO_MINISBLACK) {
    printf("Input photometric interpretation must be MinIsWhite or MinIsBlack\n");
    return -1;
  }

  long bytesPerSample = f->GetBitsPerSample()/8;
  
  // use unique_ptr to automatically free the buffers
  std::unique_ptr<icUInt8Number> inbufffer( new icUInt8Number[ bytePerLine*nSamples ] );
  std::unique_ptr<icUInt8Number> outbuffer( new icUInt8Number[ f->GetWidth() * bytesPerSample * nSamples ] );
  icUInt8Number *inbuf = inbufffer.get();
  icUInt8Number *outbuf = outbuffer.get();

  float xRes = f->GetXRes();
  float yRes = f->GetYRes();

  if (xRes<1)
    xRes = 72;
  if (yRes<1)
    yRes = 72;

  CTiffImg outfile;
  if (!outfile.Create(argv[1], f->GetWidth(), f->GetHeight(), f->GetBitsPerSample(), PHOTO_MINISBLACK,
                     nSamples, 0, xRes, yRes, bCompress, bSep)) {
    printf("Unable to create %s\n", argv[1]);
    return -1;
  }

  // profile pointer lifetime needs to last until output file is written!
  std::unique_ptr<unsigned char> destProfile;
  if (argc>8) {
    CIccFileIO io;
    if (io.Open(argv[8], "rb")) {
      size_t length = io.GetLength();
      destProfile.reset( new unsigned char[length] );
      io.Read8( destProfile.get(), length );
      outfile.SetIccProfile( destProfile.get(), (unsigned int)length );
      io.Close();
    }
  }

  for (unsigned int i=0; i<f->GetHeight(); i++) {
    icUInt8Number *sptr, *tptr;
    for (int j=0; j<nSamples; j++) {
      sptr = inbuf + j*bytePerLine;
      if (!infile[j].ReadLine(sptr)) {
        printf("Error reading line %d of file %d\n", i, j);
        return -1;
      }
      if (invert) {     // NOTE - this will not work for floating point data, but that should never be inverted anyway
        for (long k=0; k<bytePerLine; k++) {
          sptr[k] ^= 0xff;
        }
      }
    }
    tptr = outbuf;
    for (unsigned int k=0; k<f->GetWidth(); k++) {
      for (int j=0; j<nSamples; j++) {
        sptr = inbuf + j*bytePerLine + k*bytesPerSample;
        memcpy(tptr, sptr, bytesPerSample);
        tptr += bytesPerSample;
      }
    }
    outfile.WriteLine(outbuf);
  }
  
  // We need to close output first, to use all pointer data before buffers are destructed.
  outfile.Close();

  printf("Image successfully written!\n");

  // buffers and input files closed by destructors
  return 0;
}
