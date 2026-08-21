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
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>
#include <memory>
#include <limits>
#include <new>
#include <string>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccApplyBPC.h"
#include "TiffImg.h"
#include "IccProfLibVer.h"

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

  printf("Usage: %s output compress sep infile_prefix start end incr {profile}\n", strippedName ); // argv[0]
  printf("Concatenates several spectral TIFF files into a single file, with optional embedded ICC profile.\n");
  
  printf("\toutput: path/name of the TIFF file to be created\n");                               // argv[1]
  printf("\tcompress: boolean (0 | 1), should the output be compressed\n");                     // argv[2]
  printf("\tsep: boolean (0 | 1), plane data are separated in the output TIFF\n");              // argv[3]
  printf("\tinfile_prefix: literal input filename prefix; channel numbers are appended, example: \"spec_\"\n"); // argv[4]
  printf("\tstart: integer, first channel number to process\n");                                // argv[5]
  printf("\tend: integer, last channel number to process\n");                                   // argv[6]
  printf("\tincrement: integer, increment between channels\n");                                 // argv[7]
  printf("\tprofile: optional ICC profile to validate and embed in the output TIFF\n");         // argv[8]
  printf("\n");
  printf("Notes:\n");
  printf("\t-h/--help are not option flags; run without arguments to print this usage.\n");
  printf("\tinfile_prefix is not a printf format string; \"spec_00\" with start=1 opens \"spec_001\".\n");
  printf("\tstart/end/increment must be plain decimal integers; whitespace, '+', NaN, and floats are rejected.\n");
  printf("\tEmbedded profiles must parse and validate as ICC profiles. If a spectral PCS is present,\n");
  printf("\tits channel count/range steps must match the generated TIFF SamplesPerPixel.\n");
  printf("\tWithout a spectral PCS, profile data color-space samples must match TIFF SamplesPerPixel.\n");
  printf("\tCI fixture images are intentionally tiny; use larger source TIFFs for visual display review.\n");
  printf("Built with IccProfLib version " ICCPROFLIBVER "\n");
  printf("\n");
}

//===================================================

static bool parseIntArg(const char *text, int &value)
{
  if (!text || !text[0])
    return false;

  const char *digits = text;
  if (*digits == '-') {
    ++digits;
    if (!*digits)
      return false;
  }

  for (const char *p = digits; *p; ++p) {
    if (*p < '0' || *p > '9')
      return false;
  }

  char *end = NULL;
  errno = 0;
  long parsed = strtol(text, &end, 10);

  if (errno || *end || parsed < INT_MIN || parsed > INT_MAX)
    return false;

  value = (int)parsed;
  return true;
}

static bool parseBoolArg(const char *text, bool &value)
{
  if (!strcmp(text, "0")) {
    value = false;
    return true;
  }

  if (!strcmp(text, "1")) {
    value = true;
    return true;
  }

  return false;
}

static bool checkedSizeProduct(size_t a, size_t b, size_t &result)
{
  if (a && b > std::numeric_limits<size_t>::max() / a)
    return false;

  result = a * b;
  return true;
}

static const char *validateStatusName(icValidateStatus status)
{
  switch (status) {
  case icValidateOK:
    return "OK";
  case icValidateWarning:
    return "Warning";
  case icValidateNonCompliant:
    return "NonCompliant";
  case icValidateCriticalError:
    return "CriticalError";
  default:
    return "Unknown";
  }
}

static void printFirstValidationLine(const std::string &report)
{
  if (report.empty())
    return;

  size_t end = report.find('\n');
  std::string line = report.substr(0, end == std::string::npos ? report.size() : end);
  if (!line.empty())
    printf("Profile validation detail: %s\n", line.c_str());
}

static void sigToText(icUInt32Number sig, char *buf, size_t bufSize)
{
  if (!buf || !bufSize)
    return;

  buf[0] = '\0';
  icGetColorSigStr(buf, bufSize, sig);
}

static bool validateProfileSampleCompatibility(const CIccProfile *profile,
                                               size_t nSamples,
                                               const char *profilePath)
{
  if (!profile)
    return false;

  const icHeader &header = profile->m_Header;
  char colorSpace[64];
  char pcs[64];
  char spectralPcs[64];

  sigToText(header.colorSpace, colorSpace, sizeof(colorSpace));
  sigToText(header.pcs, pcs, sizeof(pcs));
  sigToText(header.spectralPCS, spectralPcs, sizeof(spectralPcs));

  if (header.spectralPCS != icSigNoSpectralData) {
    icUInt32Number spectralSamples = icGetSpaceSamples((icColorSpaceSignature)header.spectralPCS);

    if (!spectralSamples || spectralSamples != nSamples) {
      printf("Profile %s spectral PCS samples (%u, %s) do not match TIFF SamplesPerPixel (%zu).\n",
             profilePath, (unsigned int)spectralSamples, spectralPcs, nSamples);
      return false;
    }

    if (!header.spectralRange.steps || header.spectralRange.steps != nSamples) {
      printf("Profile %s spectral PCS range steps (%u) do not match TIFF SamplesPerPixel (%zu).\n",
             profilePath, (unsigned int)header.spectralRange.steps, nSamples);
      return false;
    }

    printf("ICC profile accepted: %s, status=conformant, data=%s, PCS=%s, spectralPCS=%s, spectralRangeSteps=%u, TIFFSamples=%zu\n",
           profilePath, colorSpace, pcs, spectralPcs, (unsigned int)header.spectralRange.steps, nSamples);
    return true;
  }

  icUInt32Number dataSamples = profile->GetSpaceSamples();
  if (!dataSamples || dataSamples != nSamples) {
    printf("Profile %s data color-space samples (%u, %s) do not match TIFF SamplesPerPixel (%zu).\n",
           profilePath, (unsigned int)dataSamples, colorSpace, nSamples);
    return false;
  }

  printf("ICC profile accepted: %s, status=conformant, data=%s, PCS=%s, TIFFSamples=%zu\n",
         profilePath, colorSpace, pcs, nSamples);
  return true;
}

static bool readValidateProfile(const char *profilePath,
                                size_t nSamples,
                                std::unique_ptr<unsigned char[]> &destProfile,
                                size_t &destProfileLength)
{
  destProfile.reset();
  destProfileLength = 0;

  CIccFileIO io;
  if (!io.Open(profilePath, "rb")) {
    printf("Cannot open profile %s\n", profilePath);
    return false;
  }

  destProfileLength = io.GetLength();
  if (!destProfileLength) {
    io.Close();
    printf("Profile %s is empty; refusing to embed zero-length ICC data.\n", profilePath);
    return false;
  }

  if (destProfileLength > (size_t)std::numeric_limits<icUInt32Number>::max()) {
    io.Close();
    printf("Profile %s is too large to embed in TIFF ICCProfile tag: %zu bytes\n",
           profilePath, destProfileLength);
    return false;
  }

  destProfile.reset(new unsigned char[destProfileLength]);
  if (io.Read8(destProfile.get(), destProfileLength) != destProfileLength) {
    io.Close();
    printf("Cannot read complete profile %s\n", profilePath);
    return false;
  }
  io.Close();

  std::string validateReport;
  icValidateStatus validateStatus = icValidateOK;
  std::unique_ptr<CIccProfile> profile(ValidateIccProfile(destProfile.get(),
                                                         (icUInt32Number)destProfileLength,
                                                         validateReport,
                                                         validateStatus));
  if (!profile) {
    printf("Cannot parse profile %s as an ICC profile.\n", profilePath);
    printFirstValidationLine(validateReport);
    return false;
  }

  if (validateStatus >= icValidateNonCompliant) {
    printf("Profile %s failed ICC validation: %s.\n",
           profilePath, validateStatusName(validateStatus));
    printFirstValidationLine(validateReport);
    return false;
  }

  std::unique_ptr<CIccProfile> compatibilityProfile(OpenIccProfile(destProfile.get(),
                                                                  (icUInt32Number)destProfileLength,
                                                                  true));
  const CIccProfile *profileForSamples = profile.get();
  if (compatibilityProfile &&
      compatibilityProfile->m_Header.spectralPCS != icSigNoSpectralData) {
    profileForSamples = compatibilityProfile.get();
  }

  if (!validateProfileSampleCompatibility(profileForSamples, nSamples, profilePath))
    return false;

  return true;
}

//===================================================

// Once Create() has opened the output, every later failure path leaves a
// partially written TIFF behind.  Create() truncates any existing file, so a
// run that failed part way through also replaced a previously good output with
// an unreadable stub that still looked like a result.  Discard the incomplete
// file so a failed conversion leaves no output rather than a corrupt one.
static void discardPartialOutput(CTiffImg &outfile, const char *path)
{
  outfile.Close();
  remove(path);
}

//===================================================

int main(int argc, char* argv[]) {
  const int minargs = 8; // argc = 8 without profile, 9 with profile
  
  if (argc < minargs || argc > 9) {
    // Too few arguments is a usage *error*, not a successful help request: this
    // tool has no explicit -h/--help flag, so Usage() only ever fires here, on a
    // malformed invocation.  Returning 0 reported success to the caller even
    // though nothing was produced (#1514), so wrapper scripts/CI could not tell
    // a no-op apart from a real conversion.  Fail like every other error exit in
    // main(), all of which return -1 (process status 255).
    Usage(argv[0]);
    return -1;
  }

  bool bCompress = false;
  bool bSep = false;

  if (!parseBoolArg(argv[2], bCompress) || !parseBoolArg(argv[3], bSep)) {
    printf("Invalid boolean value for compress or sep: %s, %s\n", argv[2], argv[3]);
    return -1;
  }

  int start = 0;
  int end = 0;
  int step = 0;

  if (!parseIntArg(argv[5], start) ||
      !parseIntArg(argv[6], end) ||
      !parseIntArg(argv[7], step)) {
    printf("Invalid channel range: %s, %s, %s\n", argv[5], argv[6], argv[7]);
    return -1;
  }

  if (step == 0) {
    printf("Error: increment cannot be zero.\n");
    return -1;  // Exit the program with an error code
  }

  // we do allow end < start, when step is negative
  long long range = (long long)end - (long long)start;

  if ( ((range < 0) && (step > 0))
    || ((range > 0) && (step < 0)) ) {
    printf("Bad steps values would overflow: %d, %d, %d\n", start, end, step );
    return -1;
  }

  long long absRange = range < 0 ? -range : range;
  long long absStep = step < 0 ? -(long long)step : (long long)step;

  if (absRange % absStep) {
    printf("Invalid channel range specified: increment does not land on end: %d, %d, %d\n", start, end, step);
    return -1;
  }

  size_t nSamples = (size_t)(absRange / absStep) + 1;

  if (nSamples < 1 ||
      nSamples > (size_t)std::numeric_limits<icUInt16Number>::max()) {
    printf("Invalid sample count specified: %d, %d, %d\n", start, end, step );
    return -1;
  }

  // open ALL input files
  std::vector<CTiffImg> infile(nSamples);

  for (size_t i=0; i<nSamples; i++) {
    long long channelNum = (long long)start + (long long)i * (long long)step;
    std::string filename = std::string(argv[4]) + std::to_string(channelNum);
    if (!infile[i].Open(filename.c_str())) {
      printf("Cannot open input %s\n", filename.c_str());
      return -1;
    }

    if (infile[i].GetSamples() != 1) {
      printf("input %s does not have 1 sample per pixel\n", filename.c_str());
      return -1;
    }

    // GetPhoto() returns CTiffImg's internal PHOTO_* enum, not a libtiff
    // PHOTOMETRIC_* constant, so the palette check must compare against
    // PHOTO_PALETTE.  Comparing against PHOTOMETRIC_PALETTE never matched, so
    // palette input was wrongly accepted and converted (#1381).
    if (infile[i].GetPhoto() == PHOTO_PALETTE) {
      printf("input %s is a palette based file\n", filename.c_str());
      return -1;
    }

    // The resolution unit belongs in this check alongside the resolution values it
    // qualifies: 300 pixels/inch and 300 pixels/cm are the same two numbers describing
    // images of different physical size, so comparing only GetXRes()/GetYRes() accepted
    // a channel set that does not actually share a format (#2220).
    if (i && (infile[i].GetWidth() != infile[0].GetWidth() ||
      infile[i].GetHeight() != infile[0].GetHeight() ||
      infile[i].GetBitsPerSample() != infile[0].GetBitsPerSample() ||
      infile[i].GetPhoto() != infile[0].GetPhoto() ||
      infile[i].GetXRes() != infile[0].GetXRes() ||
      infile[i].GetYRes() != infile[0].GetYRes() ||
      infile[i].GetResolutionUnit() != infile[0].GetResolutionUnit())) {
        printf("input %s does not have same format as other files\n", filename.c_str());
        return -1;
    }
  }
  
  // all inputs are open now
  
  // use the first input file for error checking and format info
  // since we made sure all inputs match basic format.
  CTiffImg *f = &infile[0];

  size_t bytePerLine = f->GetBytesPerLine();
  
  bool invert = false;
  if (f->GetPhoto()==PHOTO_MINISWHITE)
    invert = true;
  else if (f->GetPhoto()!=PHOTO_MINISBLACK) {
    printf("Input photometric interpretation must be MinIsWhite or MinIsBlack\n");
    return -1;
  }

  // The MinIsWhite inversion below is a per-byte XOR with 0xff, which is only
  // meaningful for integer samples.  Applying it to IEEE floats rewrites the
  // sign and exponent instead of the sample value: 0.25 (0x3E800000) becomes
  // 0xC17FFFFF (-15.999999) and 0.0 becomes a NaN.  The tool nevertheless
  // reported success, so a float MinIsWhite input was silently converted to
  // garbage.  Refuse the combination rather than emit corrupt samples.
  if (invert && f->GetSampleFormat() == SAMPLEFORMAT_IEEEFP) {
    printf("Floating point MinIsWhite input cannot be inverted: %s\n", argv[4]);
    return -1;
  }

  if (f->GetBitsPerSample() % 8) {
    printf("Input bits per sample must be byte aligned: %u\n", f->GetBitsPerSample());
    return -1;
  }

  size_t bytesPerSample = f->GetBitsPerSample()/8;
  size_t inputSize = 0;
  size_t outSize = 0;
  size_t outWidthSize = 0;

  if (!checkedSizeProduct(bytePerLine, nSamples, inputSize) ||
      !checkedSizeProduct(f->GetWidth(), bytesPerSample, outWidthSize) ||
      !checkedSizeProduct(outWidthSize, nSamples, outSize)) {
    printf("Image row size is too large\n");
    return -1;
  }
  
  // use unique_ptr to automatically free the buffers
  // The row buffers are sized from the input geometry, so a small crafted TIFF
  // can request gigabytes: a 250-byte file declaring a 500,000,000-pixel wide
  // float row asks for 2GB here.  Throwing new turned that into an unhandled
  // std::bad_alloc and the tool died with SIGABRT (signal 6) instead of
  // reporting the problem, so allocate without throwing and fail like every
  // other input error.
  std::unique_ptr<icUInt8Number[]> inbufffer( new (std::nothrow) icUInt8Number[ inputSize ] );
  std::unique_ptr<icUInt8Number[]> outbuffer( new (std::nothrow) icUInt8Number[ outSize ] );
  if (!inbufffer || !outbuffer) {
    printf("Cannot allocate image row buffers of %zu and %zu bytes\n", inputSize, outSize);
    return -1;
  }
  icUInt8Number *inbuf = inbufffer.get();
  icUInt8Number *outbuf = outbuffer.get();

  float xRes = f->GetXRes();
  float yRes = f->GetYRes();

  if (xRes<1)
    xRes = 72;
  if (yRes<1)
    yRes = 72;

  // profile pointer lifetime needs to last until output file is written!
  std::unique_ptr<unsigned char[]> destProfile;
  size_t destProfileLength = 0;
  if (argc>8) {
    if (!readValidateProfile(argv[8], nSamples, destProfile, destProfileLength))
      return -1;
  }

  CTiffImg outfile;
  unsigned int nExtraSamples = nSamples > 1 ? (unsigned int)nSamples - 1 : 0;

  // Create() truncates the destination the moment its TIFFOpen(...,"w")
  // succeeds, but it also refuses some requests before opening anything (an
  // unsupported compression/sample-size pair, a destination that is not a
  // regular file).  Removing unconditionally on failure would therefore delete
  // a pre-existing file this run never touched, so only discard an output that
  // did not exist beforehand.
  bool outputExisted = false;
  {
    // Probed with CIccFileIO to match readValidateProfile() above; a bare
    // fopen() here would add another MSVC C4996 deprecation to the surface
    // #2171 is already tracking.
    CIccFileIO existing;
    outputExisted = existing.Open(argv[1], "rb");
    if (outputExisted)
      existing.Close();
  }

  // xRes/yRes above are copied straight from the input, so the input's unit has to be
  // carried across with them.  Leaving it to default meant a centimetre-based source
  // produced an output with no RESOLUTIONUNIT tag at all, which every reader takes as
  // inches -- the same numbers, a physical size 2.54x off (#2220).
  if (!outfile.Create(argv[1], f->GetWidth(), f->GetHeight(), f->GetBitsPerSample(), PHOTO_MINISBLACK,
                     (unsigned int)nSamples, nExtraSamples, xRes, yRes, bCompress, bSep,
                     f->GetResolutionUnit())) {
    printf("Unable to create %s\n", argv[1]);
    if (!outputExisted)
      remove(argv[1]);
    return -1;
  }

  if (destProfile) {
    if (!outfile.SetIccProfile( destProfile.get(), (unsigned int)destProfileLength )) {
      printf("Unable to embed ICC profile in %s\n", argv[1]);
      discardPartialOutput(outfile, argv[1]);
      return -1;
    }
  }

  for (unsigned int i=0; i<f->GetHeight(); i++) {
    icUInt8Number *sptr, *tptr;
    for (size_t j=0; j<nSamples; j++) {
      sptr = inbuf + j*bytePerLine;
      if (!infile[j].ReadLine(sptr)) {
        printf("Error reading line %u of file %zu\n", i, j);
        discardPartialOutput(outfile, argv[1]);
        return -1;
      }
      if (invert) {     // float samples are rejected above, so this XOR only ever sees integer data
        for (size_t k=0; k<bytePerLine; k++) {
          sptr[k] ^= 0xff;
        }
      }
    }
    tptr = outbuf;
    for (unsigned int k=0; k<f->GetWidth(); k++) {
      for (size_t j=0; j<nSamples; j++) {
        sptr = inbuf + j*bytePerLine + k*bytesPerSample;
        memcpy(tptr, sptr, bytesPerSample);
        tptr += bytesPerSample;
      }
    }
    if (!outfile.WriteLine(outbuf)) {
      printf("Error writing line %d\n", i);
      discardPartialOutput(outfile, argv[1]);
      return -1;
    }
  }
  
  // We need to close output first, to use all pointer data before buffers are destructed.
  outfile.Close();

  printf("Image successfully written!\n");

  // buffers and input files closed by destructors
  return 0;
}
