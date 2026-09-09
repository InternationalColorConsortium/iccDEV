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
#include <climits>   // LONG_MAX, for the IFD-offset bound in readStoredExtraSamplesCount()
#include "TiffImg.h"
#include "IccFileUtil.h"  // icSanitizeConsoleText - #2406
#include <cstdarg>

// #2406: libtiff routes every diagnostic through a handler that prints the
// "module" string and the formatted message straight to stderr.  Every
// TIFFError() call in this file passes the FILE PATH as that module string, and
// libtiff raises its own "TIFFOpen: <path>: No such file or directory." the same
// way -- so a path carrying ESC reproduced CSI/OSC payloads on the terminal.
// Sanitizing the argument at each call site would have left libtiff's internal
// message raw; the handler is the one place both meet.
//
// The format mirrors libtiff's own unix handlers exactly ("module: ", the
// message, then ".\n", with "Warning, " prefixed for warnings), so output is
// unchanged for the ASCII paths every existing test uses -- only control and
// non-ASCII bytes render differently, which is the defect.
//
// Installed for every consumer of this file rather than in one tool's main():
// iccApplyProfiles, iccSpecSepToTiff and iccTiffDump all compile TiffImg.cpp and
// all three leaked through these same sites.
static void icTiffSanitizedErrorHandler(const char* module, const char* fmt, va_list ap)
{
  char msg[1024];
  vsnprintf(msg, sizeof(msg), fmt, ap);
  if (module != NULL)
    fprintf(stderr, "%s: ", icSanitizeConsoleText(module).c_str());
  fprintf(stderr, "%s.\n", icSanitizeConsoleText(msg).c_str());
}

static void icTiffSanitizedWarningHandler(const char* module, const char* fmt, va_list ap)
{
  char msg[1024];
  vsnprintf(msg, sizeof(msg), fmt, ap);
  if (module != NULL)
    fprintf(stderr, "%s: ", icSanitizeConsoleText(module).c_str());
  fprintf(stderr, "Warning, %s.\n", icSanitizeConsoleText(msg).c_str());
}

namespace {
// A file-scope object so the handlers are in place before main() runs, whichever
// of the three tools is linking this file.  TIFFSetErrorHandler is a plain
// function-pointer assignment, so there is no static-initialisation-order
// dependency here.
struct IccTiffSanitizedHandlers {
  IccTiffSanitizedHandlers()
  {
    TIFFSetErrorHandler(icTiffSanitizedErrorHandler);
    TIFFSetWarningHandler(icTiffSanitizedWarningHandler);
  }
};
const IccTiffSanitizedHandlers g_icTiffSanitizedHandlers;
}

// The output-destination check below needs GetFileAttributesA() on Windows and
// lstat()/errno on POSIX (#2242).  <windows.h> is pulled in here, in the shared
// TU, rather than in either tool: iccSpecSepToTiff and iccApplyProfiles both
// reach the destination through this file's CTiffImg::Create(), so a tool-local
// copy would harden one caller and leave the other.
//
// NOMINMAX alone is NOT sufficient here, and getting this wrong breaks the
// build rather than the behaviour.  <windows.h> defines min()/max() as
// function-like macros, and this TU already used three names they capture --
// kMaxTiffSamples and checkedUInt32's std::numeric_limits<>::max() below, and
// the std::max() in Open()'s strip sizing.  The guard only suppresses them if
// it is seen before the FIRST inclusion of <windows.h>, and that can arrive
// transitively through "TiffImg.h" above, at which point the #define lands too
// late and the include guard makes our own #include a no-op.  MSVC then reports
// C4003 "not enough arguments for function-like macro invocation 'max'" plus
// C2589/C2059 at each of the three sites.  Undefining the macros after the
// include is what actually guarantees it -- the same belt-and-braces pairing
// used at IccXML/IccLibXML/IccUtilXml.cpp:75-88, which needs it for the same
// reason.  (IccProfLib/IccTagBasic.cpp:1669 solves the same collision the other
// way, by writing the call as "(std::numeric_limits<T>::max)()".)
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#else
#include <cerrno>
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

#if defined(_WIN32)
// Win32 resolves the reserved DOS device names (CON, PRN, AUX, NUL, COM1-9,
// LPT1-9) anywhere on a path, with any extension and with trailing spaces
// ignored -- "NUL", "NUL.tif" and "dir\\NUL " all open the null device rather
// than creating a file.  GetFileAttributesA() reports INVALID_FILE_ATTRIBUTES
// for them, which the caller below would otherwise read as "nothing there yet,
// go ahead", so the name has to be rejected before the attribute query.  This
// is the Windows counterpart of the POSIX S_ISREG() arm refusing /dev/null.
bool isWindowsDevicePath(const char *path)
{
  const char *name = path;

  // "C:NUL" is drive-*relative*: it names NUL in the current directory of drive
  // C:, and Win32 resolves it to the null device just as "NUL" does.  The
  // basename scan below stops at ':', so with the drive prefix left on, the
  // scan ends at length 1 and the device name is never compared -- the path
  // would be approved and written to the null device.  Strip the prefix first.
  if (name[0] && name[1] == ':' &&
      ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z')))
    name += 2;

  for (const char *p = name; *p; p++) {
    if (*p == '\\' || *p == '/')
      name = p + 1;
  }

  size_t length = 0;
  while (name[length] && name[length] != '.' && name[length] != ':')
    length++;
  while (length && name[length - 1] == ' ')
    length--;

  switch (length) {
  case 3:
    return (_strnicmp(name, "CON", 3) == 0 ||
            _strnicmp(name, "PRN", 3) == 0 ||
            _strnicmp(name, "AUX", 3) == 0 ||
            _strnicmp(name, "NUL", 3) == 0);
  case 4:
    return name[3] >= '1' && name[3] <= '9' &&
           (_strnicmp(name, "COM", 3) == 0 ||
            _strnicmp(name, "LPT", 3) == 0);
  // CONIN$/CONOUT$ are reserved console devices too, and match neither of the
  // length-3/4 forms above.
  case 6:
    return _strnicmp(name, "CONIN$", 6) == 0;
  case 7:
    return _strnicmp(name, "CONOUT$", 7) == 0;
  default:
    return false;
  }
}
#endif

// Decide whether TIFFOpen(szFname, "w") may be allowed to truncate szFname.
//
// The check already refused a directory, a FIFO and a device, but it used
// stat(), which follows symbolic links -- so a destination that was a symlink
// to a regular file was approved and the *link target* was truncated and
// rewritten (#2242).  lstat() inspects the link itself, so a symlink is no
// longer a regular file and is refused, whether or not its target exists.
//
// Any stat() failure previously meant "permit".  Only ENOENT means the path is
// genuinely free, so that is the only failure that still permits; anything else
// (EACCES, ENOTDIR, ELOOP on an intermediate component) is refused here instead
// of being handed to TIFFOpen().
//
// Scope, measured rather than assumed: relative to the previous behaviour this
// changes symlink destinations only.  Absent, regular, directory, FIFO and
// device destinations all resolve exactly as before.  A *hard* link is still
// accepted -- it is indistinguishable from the regular file it is a name for --
// and the lstat()/TIFFOpen() pair is still two syscalls, so a destination that
// is replaced between them is not covered.  Both limits are recorded in the
// tool Readmes; neither is a regression introduced here.
bool canCreateRegularOutput(const char* szFname)
{
  // TIFFOpen() survives a null path (libtiff reports "Bad address"), so this is
  // a diagnostic tidy-up rather than a crash fix: it lets the caller fail with
  // the destination message instead of a libtiff errno string.
  if (!szFname || !szFname[0])
    return false;

#if defined(_WIN32)
  if (isWindowsDevicePath(szFname))
    return false;

  DWORD attributes = GetFileAttributesA(szFname);
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
  }

  // Refusing every reparse point is too broad.  FILE_ATTRIBUTE_REPARSE_POINT is
  // also set on OneDrive Files On-Demand placeholders (IO_REPARSE_TAG_CLOUD*),
  // Windows dedup stubs (IO_REPARSE_TAG_DEDUP) and WOF-compressed files -- all
  // ordinary files that happen to be backed indirectly, and all of which the
  // tool wrote to happily before this change.  Only *name surrogates*
  // (IO_REPARSE_TAG_SYMLINK and IO_REPARSE_TAG_MOUNT_POINT) redirect the write
  // to another path, which is what #2242 is about, so discriminate on the
  // reparse tag rather than on the attribute bit.  The tag is only reachable
  // through FindFirstFileA's dwReserved0; szFname cannot contain a wildcard
  // here, because GetFileAttributesA above would have failed with
  // ERROR_INVALID_NAME and returned already.
  if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(szFname, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
      return false;

    DWORD reparseTag = findData.dwReserved0;
    FindClose(hFind);

    if (IsReparseTagNameSurrogate(reparseTag))
      return false;
  }

  return (attributes & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY)) == 0;
#else
  struct stat st;
  if (lstat(szFname, &st) != 0)
    return errno == ENOENT;

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

// The ExtraSamples count as the FILE stores it, read straight out of the IFD.
//
// libtiff cannot answer this.  When the photometric model plus the stored
// ExtraSamples do not account for SamplesPerPixel it repairs the directory in memory
// and overwrites the count IN PLACE, leaving the field marked present, so every
// libtiff getter -- plain or Defaulted -- returns the repaired figure and the stored
// one is gone.  Measured on a 6-sample MinIsBlack file storing ExtraSamples [0]:
// tiffdump shows "ExtraSamples (338) SHORT (3) 1<0>" while tiffinfo and iccTiffDump
// both report 5, so a repaired file and an honest 5-extra file produced byte-identical
// dumps (#2386).
//
// Reading tag 338's own count field is the only way to tell them apart, and it needs
// nothing but the 12-byte IFD entry: for ExtraSamples the entry's count IS the number
// of extra samples.  Opened separately from libtiff's handle so nothing here can
// disturb the decoder's file position.
//
// Reporting only.  A false return means "could not determine" -- not "absent" -- and
// the caller must keep the two apart: BigTIFF (magic 43) lays its IFD out differently
// and is deliberately not parsed here rather than guessed at.
bool readStoredExtraSamplesCount(const char *szFname, icUInt16Number &nStored)
{
  if (!szFname)
    return false;

  FILE *f = fopen(szFname, "rb");
  if (!f)
    return false;

  unsigned char hdr[8];
  if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
    fclose(f);
    return false;
  }

  bool bLittle;
  if (hdr[0] == 'I' && hdr[1] == 'I')
    bLittle = true;
  else if (hdr[0] == 'M' && hdr[1] == 'M')
    bLittle = false;
  else {
    fclose(f);
    return false;
  }

  struct Get {
    bool little;
    icUInt32Number u16(const unsigned char *p) const {
      return little ? (icUInt32Number)(p[0] | (p[1] << 8))
                    : (icUInt32Number)(p[1] | (p[0] << 8));
    }
    icUInt32Number u32(const unsigned char *p) const {
      return little ? ((icUInt32Number)p[0] | ((icUInt32Number)p[1] << 8) |
                       ((icUInt32Number)p[2] << 16) | ((icUInt32Number)p[3] << 24))
                    : ((icUInt32Number)p[3] | ((icUInt32Number)p[2] << 8) |
                       ((icUInt32Number)p[1] << 16) | ((icUInt32Number)p[0] << 24));
    }
  } get{bLittle};

  // 42 is classic TIFF. 43 is BigTIFF, whose IFD carries 8-byte counts and 20-byte
  // entries; parsing it as classic would read garbage, so refuse instead.
  if (get.u16(hdr + 2) != 42) {
    fclose(f);
    return false;
  }

  // The offset is a TIFF uint32, but fseek() takes a long, which is 32-bit on LLP64 --
  // the MSVC lane.  A classic TIFF larger than 2 GiB may legally place its first IFD at
  // or above 0x80000000, which would cast to a negative offset there and fail, so the
  // same file would report its stored count on Linux and silently decline to on
  // Windows.  Refuse the out-of-range offset explicitly instead, so "could not
  // determine" means the same thing on every lane rather than depending on sizeof(long).
  icUInt32Number nIfdOffset = get.u32(hdr + 4);
  if (nIfdOffset < 8 || nIfdOffset > (icUInt32Number)LONG_MAX ||
      fseek(f, (long)nIfdOffset, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }

  unsigned char count[2];
  if (fread(count, 1, sizeof(count), f) != sizeof(count)) {
    fclose(f);
    return false;
  }

  icUInt32Number nEntries = get.u16(count);
  for (icUInt32Number i = 0; i < nEntries; i++) {
    unsigned char entry[12];
    if (fread(entry, 1, sizeof(entry), f) != sizeof(entry)) {
      fclose(f);
      return false;
    }
    if (get.u16(entry) == TIFFTAG_EXTRASAMPLES) {
      icUInt32Number nCount = get.u32(entry + 4);
      fclose(f);
      // The count is what is being reported, so it has to fit the field it is
      // compared against; a value this large is a malformed directory either way.
      if (nCount > kMaxTiffSamples)
        return false;
      nStored = (icUInt16Number)nCount;
      return true;
    }
  }

  fclose(f);
  return false;
}

}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTiffImg::CTiffImg()
  : m_hTif(NULL),
    m_bRead(false),
    m_bOutputOpened(false),
    m_nWidth(0),
    m_nHeight(0),
    m_nBitsPerSample(0),
    m_nBytesPerSample(0),
    m_nPhoto(0),
    m_nSamples(0),
    m_nExtraSamples(0),
    m_bExtraSamplesStored(false),
    m_nEffectiveExtraSamples(0),
    m_nStoredExtraSamplesCount(0),
    m_bStoredExtraSamplesKnown(false),
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
  //
  // m_bOutputOpened is the one deliberate exception to the #1429 "every member"
  // rule above, and it is an exception because Create() calls Close() itself:
  // both of Create()'s post-TIFFOpen failure arms (the EXTRASAMPLES calloc and
  // the TIFFSetField that follows it) close the handle and return false with a
  // truncated file already on disk.  Clearing the flag here would therefore
  // report "we never opened the output" to the caller in precisely the case
  // where a stub of ours exists to discard.  The flag records the outcome of
  // the last Create()/Open() call rather than the state of the handle, so it is
  // reset by those two entry points instead (#2242).
  m_bRead = false;
  m_nWidth = 0;
  m_nHeight = 0;
  m_nBitsPerSample = 0;
  m_nBytesPerSample = 0;
  m_nPhoto = 0;
  m_nSamples = 0;
  m_nExtraSamples = 0;
  m_bExtraSamplesStored = false;
  m_nEffectiveExtraSamples = 0;
  m_nStoredExtraSamplesCount = 0;
  m_bStoredExtraSamplesKnown = false;
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
  m_bOutputOpened = false;

  // Zero is rejected explicitly: it passes "% 8" and libtiff accepts
  // TIFFSetField(BITSPERSAMPLE, 0) with rc=1, so neither the modulus nor the write
  // status catches it.  The directory was therefore authored with BitsPerSample 0 and
  // only abandoned much further down, once TIFFStripSize() returned 0 ("Computed
  // scanline size is zero") and the "stripSize <= 0" test refused it -- on BOTH the
  // separated and contiguous branches, and note it is that test rather than
  // calcBytesPerLine(), which the contiguous branch never calls.  By then TIFFOpen had
  // created the destination, so a ~140-byte stub was left on disk.  Open() has always
  // refused m_nBitsPerSample == 0 on the way in; this makes Create() symmetric with it
  // and refuses before the destination is touched (#2386).  Neither tool caller can
  // reach it -- their depth comes back out of Open() -- so this guards the public API,
  // as the ResolutionUnit check below does.
  if (!nBPS || nBPS % 8)
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
    TIFFError(szFname,"Output image must be a regular non-symlink file");
    return false;
  }

  // Set before the call, not after it: TIFFOpen(..., "w") open()s with
  // O_CREAT|O_TRUNC and only then builds the TIFF structure, so a failure
  // inside libtiff (an allocation failure in TIFFClientOpen) returns NULL with
  // the destination already created and truncated.  Measured with libtiff's
  // internal allocation forced to fail: "TIFFClientOpenExt: Out of memory
  // (TIFF structure)", TIFFOpen NULL, and a 0-byte stub left on disk.  Setting
  // the flag after the null check below would report "we never opened the
  // output" for exactly that stub and leak it past the caller's cleanup.
  m_bOutputOpened = true;

  m_hTif = TIFFOpen(szFname, "w");
  if (!m_hTif) {
    TIFFError(szFname,"Can not open output image");
    return false;
  }
  // Required directory state, written through a helper that keeps the result.  The
  // EXTRASAMPLES arm below already closed and failed on a rejected write while every
  // other tag here discarded its status, so the writer could believe libtiff had
  // accepted a directory it had refused; this makes the whole block consistent (#2386).
  //
  // No input Create() permits can currently produce a rejection: measured against
  // libtiff 4.5.1 and 4.7.2, every tag below returns 1 for every value that gets past
  // the entry guards, and the one set-time failure available at all -- SamplesPerPixel
  // of 0, "Bad value 0" -- is already refused above.  Codec validity is NOT checked
  // here either way, because libtiff defers it to write time (COMPRESSION 9999 and an
  // unconfigured JBIG both return 1), so this guard is not a substitute for the
  // write-time diagnostics.  It exists so a stricter libtiff fails at the tag that
  // was rejected rather than somewhere downstream.
  //
  // Checked in two stages rather than once at the end: libtiff validates an
  // ExtraSamples count against SamplesPerPixel, so the EXTRASAMPLES arm below must
  // not run on a directory that did not accept SamplesPerPixel -- it would fail for
  // the second reason and report the wrong tag.  The remaining writes are collected
  // and checked together after the last of them.
  //
  // The accumulation is `&=`, never `&&=`/`&&`: it must NOT short-circuit.  Every
  // TIFFSetField below has to run whatever an earlier one returned, because the
  // directory being written is the product of all of them -- switching to a
  // short-circuiting operator would silently stop writing tags after the first
  // rejection and produce a file missing required state rather than an error.
  bool bFieldsSet = true;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_IMAGEWIDTH, (uint32_t) m_nWidth) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_IMAGELENGTH, (uint32_t) m_nHeight) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_PHOTOMETRIC, m_nPhoto) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_PLANARCONFIG, m_nPlanar) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_SAMPLESPERPIXEL, m_nSamples) == 1;
  if (!bFieldsSet) {
    TIFFError(szFname, "Can not set required TIFF directory fields");
    Close();
    return false;
  }
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
  // Keep the two ExtraSamples observers true for a written image as well as a read
  // one.  Close() resets them and only Open() set them, so a Create()d object
  // reported "tag 338 was not present" while having just written it, contradicting
  // the unconditional promise on HasStoredExtraSamples().  No caller reads them off
  // an output image today; setting them here keeps that from becoming a trap.  This
  // is the one place the write path knows the answer: the tag is written exactly
  // when m_nExtraSamples is nonzero, and nothing repairs a directory we author, so
  // the effective count is the requested one.
  m_bExtraSamplesStored = (m_nExtraSamples != 0);
  m_nEffectiveExtraSamples = m_nExtraSamples;
  // The stored-count pair belongs here for the same reason, and leaving it out would
  // have re-opened exactly the trap this block exists to close: a Create()d object
  // would answer HasStoredExtraSamplesCount() == false, which the header defines as
  // "could not determine -- unreadable, non-TIFF, or BigTIFF".  None of those is true
  // of a directory we just authored from a count we were handed.  Nothing repairs a
  // directory we write, so the stored count IS the requested one.
  m_bStoredExtraSamplesKnown = true;
  m_nStoredExtraSamplesCount = m_nExtraSamples;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_BITSPERSAMPLE, m_nBitsPerSample) == 1;
  if (m_nBitsPerSample >= 32) {
    m_nSampleFormat = SAMPLEFORMAT_IEEEFP;
    bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP) == 1;
  }
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_ROWSPERSTRIP, m_nRowsPerStrip) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_COMPRESSION, m_nCompress) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_ORIENTATION, m_nOrientation) == 1;
  // Written unconditionally, next to the values it qualifies.  libtiff omits the tag
  // unless it is set and readers then fall back to their own default, so an output
  // converted from a centimetre-based source came out silently claiming inches and
  // its physical size shifted by 2.54x (#2220).  Stating the unit even when it is
  // RESUNIT_INCH costs one IFD entry and makes the file say what it means instead of
  // depending on the reader agreeing about the default.
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_RESOLUTIONUNIT, m_nResolutionUnit) == 1;
  // m_fXRes/m_fYRes, not the raw parameters: a non-positive resolution was clamped to
  // 96 above, and writing the unclamped argument put the file at odds with the object
  // that produced it -- GetXRes() answered 96 while the file declared 0.  Harmless
  // while the unit was absent; now that an authoritative RESUNIT_INCH is written beside
  // them, the file would assert "0 pixels per inch".  Same class as #2220 itself, so it
  // is corrected here.  Neither tool caller is affected: iccSpecSepToTiff substitutes
  // its own value below 1, and iccApplyProfiles takes an already-clamped resolution out
  // of Open(), so for both m_fXRes == fXRes.
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_XRESOLUTION, m_fXRes) == 1;
  bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_YRESOLUTION, m_fYRes) == 1;
  if (bCompress) {
    if (m_nBitsPerSample >= 32) {
      bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT) == 1;
    }
    else {
      bFieldsSet &= TIFFSetField(m_hTif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL) == 1;
    }
  }

  if (!bFieldsSet) {
    TIFFError(szFname, "Can not set required TIFF directory fields");
    Close();
    return false;
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
  m_bOutputOpened = false;

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
  // Plain Get, and its return is now kept: unlike its neighbours this one must
  // report what the file STORES, because colour management sizes the image from
  // it (iccApplyProfiles.cpp:543 lets a profile match sn-sen instead of sn).  A
  // failed Get leaves the destination untouched, so m_nExtraSamples would silently
  // keep whatever it held; Close() zeroes it at the top of Open() so that was 0 by
  // luck rather than by contract.  Assign it explicitly and record whether tag 338
  // was there at all -- 0 otherwise means "absent", not "stored as zero" (#2386).
  m_bExtraSamplesStored =
    (TIFFGetField(m_hTif, TIFFTAG_EXTRASAMPLES, &m_nExtraSamples, &nSampleInfo) == 1);
  if (!m_bExtraSamplesStored)
    m_nExtraSamples = 0;

  // The reporting-only companion.  When the photometric model plus the stored
  // ExtraSamples do not add up to SamplesPerPixel, libtiff repairs the directory
  // in memory and warns ("Defining non-color channels as ExtraSamples"), but it
  // publishes the repaired count ONLY through the Defaulted getter -- the plain
  // Get above still fails.  Measured on libtiff 4.5.1 and 4.7.2 against the tracked
  // 81-band Testing/hybrid/Data/smCows380_5_780.tif: Get returns 0 and writes
  // nothing, Defaulted returns 1 with 80.  Keep that figure for iccTiffDump to
  // explain the layout the rest of libtiff is actually using, and keep it away from
  // m_nExtraSamples, which must stay the stored count (see the header).
  m_nEffectiveExtraSamples = m_nExtraSamples;
  {
    icUInt16Number nEffective = 0;
    icUInt16Number *pEffectiveInfo = NULL;
    // m_nSamples is already read above, so the count can be bounded here.  libtiff
    // derives it as SamplesPerPixel minus the photometric model's colour channels
    // and cannot exceed it, but this value is printed rather than validated, and the
    // check below rejects only m_nExtraSamples -- so clamp instead of trusting it.
    if (TIFFGetFieldDefaulted(m_hTif, TIFFTAG_EXTRASAMPLES, &nEffective, &pEffectiveInfo) == 1 &&
        nEffective <= m_nSamples)
      m_nEffectiveExtraSamples = nEffective;
  }

  // The count the file itself stores, which no libtiff getter can report once the
  // directory has been repaired in place.  Read from the IFD so that a repaired file
  // is distinguishable from an honest one: without it a 6-sample MinIsBlack image
  // storing ExtraSamples [0] and one honestly storing five produced identical dumps
  // (#2386).  Failure here means "could not determine", so the flag is what gates
  // reporting -- a 0 count is a legitimate stored value, not a sentinel.
  m_bStoredExtraSamplesKnown =
    readStoredExtraSamplesCount(szFname, m_nStoredExtraSamplesCount);
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
    // A TIFF plane may end with a short final strip.  Use overflow-free ceiling
    // division so TIFFComputeStrip()'s sample-plane stride includes that strip.
    // The old floor division was only correct when ImageLength was an exact
    // multiple of RowsPerStrip, and Open() rejected every other separated TIFF.
    m_nStripsPerSample = m_nHeight / m_nRowsPerStrip;
    if (m_nHeight % m_nRowsPerStrip)
      m_nStripsPerSample++;
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
  // For separated data a strip holds one plane, not a full interleaved row, and
  // the buffer below already multiplies by m_nStripSamples. Using the interleaved
  // width here counted the samples a second time, growing the allocation with the
  // square of SamplesPerPixel; the phantom product also overflowed the 32-bit
  // check and rejected large but valid spectral images (#2228).
  unsigned int minStripSize = 0;
  const unsigned int nMinBytesPerStripLine = m_nStripSamples > 1 ? m_nBytesPerStripLine
                                                                 : m_nBytesPerLine;
  if (!checkedUInt32Product(m_nRowsPerStrip, nMinBytesPerStripLine, minStripSize)) {
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
  if (!m_bRead || m_nCurLine >= m_nHeight || m_nRowsPerStrip == 0 ||
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
        // Each decoded plane occupies a whole m_nStripSize slot: the deplaning
        // loop below and WriteLine() both stride by m_nStripSize. Advancing by a
        // single row left every plane after the first at the wrong offset, so the
        // deplaned pixels came from unwritten buffer space (#2228).
        pos += m_nStripSize;
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
