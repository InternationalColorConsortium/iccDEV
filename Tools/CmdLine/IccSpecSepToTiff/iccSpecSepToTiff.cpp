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
#include <cstdarg>
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
#include "IccCmdLineUtil.h"
#include "IccProfLibVer.h"

//===================================================

static void Usage(FILE *stream, const char *name)
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

  // argv[0] is attacker-controlled in the same way every other argument is, so it
  // gets the same escaping the sibling TIFF tool applies before echoing a name.
  std::string safeName = icSanitizeConsoleText(strippedName);

  fprintf(stream, "Usage:\n");
  fprintf(stream, "  %s output compress sep infile_prefix start end incr [profile]\n", safeName.c_str());
  fprintf(stream, "  %s -h | --help\n", safeName.c_str());
  fprintf(stream, "  %s --version\n\n", safeName.c_str());
  fprintf(stream, "Combine single-channel spectral TIFF files into one multi-sample TIFF.\n\n");

  fprintf(stream, "Arguments:\n");
  fprintf(stream, "  output         TIFF file to create\n");                                     // argv[1]
  fprintf(stream, "  compress       0 for no compression, 1 for LZW\n");                         // argv[2]
  fprintf(stream, "  sep            0 for interleaved samples, 1 for separate planes\n");        // argv[3]
  fprintf(stream, "  infile_prefix  Literal prefix; the channel number is appended\n");          // argv[4]
  fprintf(stream, "  start          First channel number\n");                                    // argv[5]
  fprintf(stream, "  end            Last channel number, inclusive\n");                          // argv[6]
  fprintf(stream, "  incr           Nonzero channel increment\n");                               // argv[7]
  fprintf(stream, "  profile        Optional ICC profile to validate and embed\n\n");            // argv[8]

  fprintf(stream, "Rules:\n");
  fprintf(stream, "  compress and sep accept only 0 or 1.\n");
  fprintf(stream, "  start, end, and incr accept plain decimal integers.\n");
  fprintf(stream, "  The range must land exactly on end and may descend with a negative incr.\n");
  fprintf(stream, "  infile_prefix is not a printf format string.\n");
  fprintf(stream, "  Every input must be a matching single-channel grayscale TIFF.\n");
  fprintf(stream, "  An optional profile must be conformant and match SamplesPerPixel.\n\n");

  fprintf(stream, "Limits:\n");
  fprintf(stream, "  SamplesPerPixel is limited to 65535 channels by the TIFF field.\n");
  fprintf(stream, "  ICC profile data is limited to 4294967295 bytes.\n");
  fprintf(stream, "  TIFF width and height are limited to 32-bit unsigned values.\n");
  fprintf(stream, "  LZW output supports 8, 16, or 32 bits per sample.\n");
  fprintf(stream, "  Inputs stay open concurrently; the OS open-file limit may be lower.\n\n");
  fprintf(stream, "Built with IccProfLib version " ICCPROFLIBVER "\n");
}

//===================================================

static void PrintVersion()
{
  printf("iccSpecSepToTiff " ICCPROFLIBVER "\n");
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
    fprintf(stderr, "Profile validation detail: %s\n",
            icSanitizeConsoleText(line).c_str());
}

// Renders the accepted-profile line into a string instead of printing it, so
// main() can hold it back until the conversion has actually succeeded.  Kept as
// a printf-style formatter rather than a stream so the two call sites below use
// the exact format strings they printed before, and the attribute keeps the
// compiler checking them.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static void formatAcceptedSummary(std::string &out, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int needed = vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (needed < 0) {
    out.clear();
    return;
  }

  std::vector<char> buffer((size_t)needed + 1);
  va_start(args, format);
  vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);

  out.assign(buffer.data());
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
                                               const char *profilePath,
                                               std::string &acceptedSummary)
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
      fprintf(stderr, "Profile %s spectral PCS samples (%u, %s) do not match TIFF SamplesPerPixel (%zu).\n",
              icSanitizeConsoleText(profilePath).c_str(),
              (unsigned int)spectralSamples, spectralPcs, nSamples);
      return false;
    }

    if (!header.spectralRange.steps || header.spectralRange.steps != nSamples) {
      fprintf(stderr, "Profile %s spectral PCS range steps (%u) do not match TIFF SamplesPerPixel (%zu).\n",
              icSanitizeConsoleText(profilePath).c_str(),
              (unsigned int)header.spectralRange.steps, nSamples);
      return false;
    }

    formatAcceptedSummary(acceptedSummary,
                          "ICC profile accepted: %s, status=conformant, data=%s, PCS=%s, spectralPCS=%s, spectralRangeSteps=%u, TIFFSamples=%zu",
                          icSanitizeConsoleText(profilePath).c_str(), colorSpace, pcs, spectralPcs,
                          (unsigned int)header.spectralRange.steps, nSamples);
    return true;
  }

  icUInt32Number dataSamples = profile->GetSpaceSamples();
  if (!dataSamples || dataSamples != nSamples) {
    fprintf(stderr, "Profile %s data color-space samples (%u, %s) do not match TIFF SamplesPerPixel (%zu).\n",
            icSanitizeConsoleText(profilePath).c_str(),
            (unsigned int)dataSamples, colorSpace, nSamples);
    return false;
  }

  formatAcceptedSummary(acceptedSummary,
                        "ICC profile accepted: %s, status=conformant, data=%s, PCS=%s, TIFFSamples=%zu",
                        icSanitizeConsoleText(profilePath).c_str(), colorSpace, pcs, nSamples);
  return true;
}

static bool readValidateProfile(const char *profilePath,
                                size_t nSamples,
                                std::unique_ptr<unsigned char[]> &destProfile,
                                size_t &destProfileLength,
                                std::string &acceptedSummary)
{
  destProfile.reset();
  destProfileLength = 0;
  acceptedSummary.clear();

  CIccFileIO io;
  if (!io.Open(profilePath, "rb")) {
    fprintf(stderr, "Cannot open profile %s\n",
            icSanitizeConsoleText(profilePath).c_str());
    return false;
  }

  destProfileLength = io.GetLength();
  if (!destProfileLength) {
    io.Close();
    fprintf(stderr, "Profile %s is empty; refusing to embed zero-length ICC data.\n",
            icSanitizeConsoleText(profilePath).c_str());
    return false;
  }

  if (destProfileLength > (size_t)std::numeric_limits<icUInt32Number>::max()) {
    io.Close();
    fprintf(stderr, "Profile %s is too large to embed in TIFF ICCProfile tag: %zu bytes\n",
            icSanitizeConsoleText(profilePath).c_str(), destProfileLength);
    return false;
  }

  destProfile.reset(new (std::nothrow) unsigned char[destProfileLength]);
  if (!destProfile) {
    io.Close();
    fprintf(stderr, "Unable to allocate %zu bytes for profile %s.\n",
            destProfileLength, icSanitizeConsoleText(profilePath).c_str());
    return false;
  }

  if (io.Read8(destProfile.get(), destProfileLength) != destProfileLength) {
    io.Close();
    fprintf(stderr, "Cannot read complete profile %s\n",
            icSanitizeConsoleText(profilePath).c_str());
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
    fprintf(stderr, "Cannot parse profile %s as an ICC profile.\n",
            icSanitizeConsoleText(profilePath).c_str());
    printFirstValidationLine(validateReport);
    return false;
  }

  if (validateStatus >= icValidateNonCompliant) {
    fprintf(stderr, "Profile %s failed ICC validation: %s.\n",
            icSanitizeConsoleText(profilePath).c_str(),
            validateStatusName(validateStatus));
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

  if (!validateProfileSampleCompatibility(profileForSamples, nSamples, profilePath,
                                          acceptedSummary))
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

  // An explicit help/version request is the one invocation that is *not* an
  // error, so it prints on stdout and exits 0, matching iccRoundTrip and
  // iccPawgReport.  Everything below keeps the #1514 contract: a malformed
  // invocation reported success to the caller even though nothing was produced,
  // so wrapper scripts/CI could not tell a no-op apart from a real conversion.
  if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
    Usage(stdout, argv[0]);
    return 0;
  }

  if (argc == 2 && !strcmp(argv[1], "--version")) {
    PrintVersion();
    return 0;
  }

  if (argc < minargs || argc > 9) {
    // Say which way the invocation is malformed before the syntax block: a bare
    // Usage() dump left the caller to diff their command against it.  The
    // diagnostic and the usage both go to stderr because this path is a failure;
    // -h/--help above is the only route to Usage() on stdout.  Fail like every
    // other error exit in main(), all of which return -1 (process status 255).
    if (argc < minargs)
      fprintf(stderr, "Missing arguments: expected 7 or 8, received %d.\n",
              argc > 0 ? argc - 1 : 0);
    else
      fprintf(stderr, "Unexpected extra argument: %s\n",
              icSanitizeConsoleText(argv[9]).c_str());

    // argc==0 is legal for execve, and Usage() runs strrchr() on the name before
    // anything else can reject it, so substitute the tool's own name there.
    Usage(stderr, argc > 0 ? argv[0] : "iccSpecSepToTiff");
    return -1;
  }

  bool bCompress = false;
  bool bSep = false;

  if (!parseBoolArg(argv[2], bCompress) || !parseBoolArg(argv[3], bSep)) {
    fprintf(stderr, "Invalid boolean value for compress or sep: %s, %s\n",
            icSanitizeConsoleText(argv[2]).c_str(),
            icSanitizeConsoleText(argv[3]).c_str());
    return -1;
  }

  int start = 0;
  int end = 0;
  int step = 0;

  if (!parseIntArg(argv[5], start) ||
      !parseIntArg(argv[6], end) ||
      !parseIntArg(argv[7], step)) {
    fprintf(stderr, "Invalid channel range: %s, %s, %s\n",
            icSanitizeConsoleText(argv[5]).c_str(),
            icSanitizeConsoleText(argv[6]).c_str(),
            icSanitizeConsoleText(argv[7]).c_str());
    return -1;
  }

  if (step == 0) {
    fprintf(stderr, "Error: increment cannot be zero.\n");
    return -1;  // Exit the program with an error code
  }

  // we do allow end < start, when step is negative
  long long range = (long long)end - (long long)start;

  if ( ((range < 0) && (step > 0))
    || ((range > 0) && (step < 0)) ) {
    fprintf(stderr, "Bad steps values would overflow: %d, %d, %d\n", start, end, step);
    return -1;
  }

  long long absRange = range < 0 ? -range : range;
  long long absStep = step < 0 ? -(long long)step : (long long)step;

  if (absRange % absStep) {
    fprintf(stderr, "Invalid channel range specified: increment does not land on end: %d, %d, %d\n",
            start, end, step);
    return -1;
  }

  size_t nSamples = (size_t)(absRange / absStep) + 1;

  if (nSamples < 1 ||
      nSamples > (size_t)std::numeric_limits<icUInt16Number>::max()) {
    fprintf(stderr, "Invalid sample count specified: %d, %d, %d\n", start, end, step);
    return -1;
  }

  // open ALL input files
  std::vector<CTiffImg> infile;
  try {
    infile.resize(nSamples);
  }
  catch (const std::bad_alloc &) {
    fprintf(stderr, "Unable to allocate input state for %zu channels.\n", nSamples);
    return -1;
  }

  for (size_t i=0; i<nSamples; i++) {
    long long channelNum = (long long)start + (long long)i * (long long)step;
    std::string filename = std::string(argv[4]) + std::to_string(channelNum);
    if (!infile[i].Open(filename.c_str())) {
      fprintf(stderr, "Cannot open input %s\n",
              icSanitizeConsoleText(filename).c_str());
      return -1;
    }

    if (infile[i].GetSamples() != 1) {
      fprintf(stderr, "input %s does not have 1 sample per pixel\n",
              icSanitizeConsoleText(filename).c_str());
      return -1;
    }

    // GetPhoto() returns CTiffImg's internal PHOTO_* enum, not a libtiff
    // PHOTOMETRIC_* constant, so the palette check must compare against
    // PHOTO_PALETTE.  Comparing against PHOTOMETRIC_PALETTE never matched, so
    // palette input was wrongly accepted and converted (#1381).
    if (infile[i].GetPhoto() == PHOTO_PALETTE) {
      fprintf(stderr, "input %s is a palette based file\n",
              icSanitizeConsoleText(filename).c_str());
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
        fprintf(stderr, "input %s does not have same format as other files\n",
                icSanitizeConsoleText(filename).c_str());
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
    fprintf(stderr, "Input photometric interpretation must be MinIsWhite or MinIsBlack\n");
    return -1;
  }

  // The MinIsWhite inversion below is a per-byte XOR with 0xff, which is only
  // meaningful for integer samples.  Applying it to IEEE floats rewrites the
  // sign and exponent instead of the sample value: 0.25 (0x3E800000) becomes
  // 0xC17FFFFF (-15.999999) and 0.0 becomes a NaN.  The tool nevertheless
  // reported success, so a float MinIsWhite input was silently converted to
  // garbage.  Refuse the combination rather than emit corrupt samples.
  if (invert && f->GetSampleFormat() == SAMPLEFORMAT_IEEEFP) {
    fprintf(stderr, "Floating point MinIsWhite input cannot be inverted: %s\n",
            icSanitizeConsoleText(argv[4]).c_str());
    return -1;
  }

  if (f->GetBitsPerSample() % 8) {
    fprintf(stderr, "Input bits per sample must be byte aligned: %u\n", f->GetBitsPerSample());
    return -1;
  }

  size_t bytesPerSample = f->GetBitsPerSample()/8;
  size_t inputSize = 0;
  size_t outSize = 0;
  size_t outWidthSize = 0;

  if (!checkedSizeProduct(bytePerLine, nSamples, inputSize) ||
      !checkedSizeProduct(f->GetWidth(), bytesPerSample, outWidthSize) ||
      !checkedSizeProduct(outWidthSize, nSamples, outSize)) {
    fprintf(stderr, "Image row size is too large\n");
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
    fprintf(stderr, "Cannot allocate image row buffers of %zu and %zu bytes\n", inputSize, outSize);
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
  // Held until the output is written: announcing the profile as accepted before
  // Create()/SetIccProfile()/WriteLine() have had their chance to fail left a
  // failed run with output on stdout, so "stdout empty" could not be read as
  // "nothing was produced" -- the very distinction #1514 asked for.
  std::string acceptedProfileSummary;
  if (argc>8) {
    if (!readValidateProfile(argv[8], nSamples, destProfile, destProfileLength,
                             acceptedProfileSummary))
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
  //
  // outputExisted alone is not sufficient once Create() refuses symlinks
  // (#2242).  A *dangling* symlink reads as "did not exist" here -- the probe
  // below opens the link's target, which is missing -- so the !outputExisted
  // arm on its own would remove() the link that Create() had just declined to
  // touch.  Measured: with the lstat() change but without WasOutputOpened(),
  // a dangling-symlink destination is correctly refused and then deleted.
  // Gating on "did TIFFOpen() actually run" is what keeps the refusal
  // non-destructive, so both halves of this guard are load-bearing.
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
    fprintf(stderr, "Unable to create %s\n", icSanitizeConsoleText(argv[1]).c_str());
    if (!outputExisted && outfile.WasOutputOpened())
      remove(argv[1]);
    return -1;
  }

  if (destProfile) {
    if (!outfile.SetIccProfile( destProfile.get(), (unsigned int)destProfileLength )) {
      fprintf(stderr, "Unable to embed ICC profile in %s\n",
              icSanitizeConsoleText(argv[1]).c_str());
      discardPartialOutput(outfile, argv[1]);
      return -1;
    }
  }

  for (unsigned int i=0; i<f->GetHeight(); i++) {
    icUInt8Number *sptr, *tptr;
    for (size_t j=0; j<nSamples; j++) {
      sptr = inbuf + j*bytePerLine;
      if (!infile[j].ReadLine(sptr)) {
        fprintf(stderr, "Error reading line %u of file %zu\n", i, j);
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
      fprintf(stderr, "Error writing line %u to %s\n", i,
              icSanitizeConsoleText(argv[1]).c_str());
      discardPartialOutput(outfile, argv[1]);
      return -1;
    }
  }
  
  // We need to close output first, to use all pointer data before buffers are destructed.
  outfile.Close();

  if (!acceptedProfileSummary.empty())
    printf("%s\n", acceptedProfileSummary.c_str());

  printf("Output:            %s\n", icSanitizeConsoleText(argv[1]).c_str());
  printf("Size:              %u x %u pixels\n", f->GetWidth(), f->GetHeight());
  printf("BitsPerSample:     %u\n", f->GetBitsPerSample());
  printf("SamplesPerPixel:   %zu\n", nSamples);
  printf("Planar:            %s\n", bSep ? "separate" : "interleaved");
  printf("Compression:       %s\n", bCompress ? "LZW" : "none");
  printf("Profile:           %s\n", destProfile ? "embedded" : "none");
  printf("Image successfully written!\n");

  // buffers and input files closed by destructors
  return 0;
}
