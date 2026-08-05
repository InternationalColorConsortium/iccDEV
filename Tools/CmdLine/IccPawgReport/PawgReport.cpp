/** @file
    File:       PawgReport.cpp

    Contains:   ICC PAWG profile assessment report support for IccPawgReport

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights
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

#include "PawgReport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "IccProfile.h"
#include "IccMpeCalc.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "IccQualityMetrics.h"
#include "IccSignatureRegistry.h"

namespace {

const char *kPawgUrl = "https://www.color.org/profiles/assessment/index.xalter";
const char *kIccTagRegistryUrl = "https://registry.color.org/tag-signatures/";

const double kRoundTripWarnAvg = 2.0;
const double kRoundTripWarnMax = 10.0;
const double kRoundTripFailAvg = 5.0;
const double kRoundTripFailMax = 20.0;
const double kCurveWarnMaxError = 0.01;
const double kSmoothWarnMaxStep = 15.0;
const double kSmoothWarnCurvature = 8.0;
const double kCharacterizationWarnAvg = 2.0;
const double kCharacterizationWarnMax = 10.0;
const double kCharacterizationFailAvg = 5.0;
const double kCharacterizationFailMax = 20.0;

enum class PawgVerdict {
  Ok,
  Warn,
  Fail,
  NotApplicable,
  Gap,
  NotRun
};

struct RawTag {
  icTagSignature sig = (icTagSignature)0;
  uint32_t offset = 0;
  uint32_t size = 0;
};

struct RawProfile {
  std::vector<unsigned char> data;
  std::vector<RawTag> tags;
  uint32_t declaredSize = 0;
  uint32_t version = 0;
  uint32_t profileClass = 0;
  uint32_t colorSpace = 0;
  uint32_t pcs = 0;
  uint32_t cmm = 0;
  uint32_t platform = 0;
  uint32_t manufacturer = 0;
  uint32_t creator = 0;
  bool opened = false;
  bool hasHeader = false;
  bool hasTagTable = false;
  std::string readError;
};

struct PawgItem {
  const char *id;
  const char *title;
  PawgVerdict verdict;
  std::string detail;
};

struct RuleTable {
  const icTagSignature *required;
  size_t requiredCount;
  const icTagSignature *alternative;
  size_t alternativeCount;
  const icTagSignature *optional;
  size_t optionalCount;
  const char *alternativeName;
  const char *specRef;
};

struct HeaderCheck {
  PawgVerdict verdict;
  std::string detail;
};

struct PrivateTagStatus {
  int total = 0;
  int registered = 0;
  int undocumented = 0;
  std::string detail;
};

uint32_t ReadU32BE(const unsigned char *p)
{
  return ((uint32_t)p[0] << 24) |
         ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

// Parse the ICC header fields and the tag table out of an already-populated
// raw.data.  Factored out of LoadRawProfile so the file loader and the in-memory
// loader (LoadRawProfileFromMemory, used by the #1775 assessment-only fuzz
// entries) apply byte-for-byte identical header/bounds handling -- the CLI and
// the overnight fuzzing harness must see the same view of a profile.
static bool ParseRawProfile(RawProfile &raw)
{
  raw.opened = true;
  raw.hasHeader = raw.data.size() >= 128;
  if (!raw.hasHeader) {
    raw.readError = "file is smaller than the 128-byte ICC header";
    return true;
  }

  raw.declaredSize = ReadU32BE(raw.data.data());
  raw.cmm = ReadU32BE(raw.data.data() + 4);
  raw.version = ReadU32BE(raw.data.data() + 8);
  raw.profileClass = ReadU32BE(raw.data.data() + 12);
  raw.colorSpace = ReadU32BE(raw.data.data() + 16);
  raw.pcs = ReadU32BE(raw.data.data() + 20);
  raw.platform = ReadU32BE(raw.data.data() + 40);
  raw.manufacturer = ReadU32BE(raw.data.data() + 48);
  raw.creator = ReadU32BE(raw.data.data() + 80);

  if (raw.data.size() < 132) {
    raw.readError = "file is too small for a tag count";
    return true;
  }

  uint32_t tagCount = ReadU32BE(raw.data.data() + 128);
  uint64_t tableBytes = 132ULL + (uint64_t)tagCount * 12ULL;
  if (tagCount > 100000 || tableBytes > raw.data.size()) {
    raw.readError = "tag table extends beyond the file";
    return true;
  }

  raw.hasTagTable = true;
  raw.tags.reserve(tagCount);
  for (uint32_t i = 0; i < tagCount; ++i) {
    const unsigned char *entry = raw.data.data() + 132 + (size_t)i * 12;
    RawTag tag;
    tag.sig = (icTagSignature)ReadU32BE(entry);
    tag.offset = ReadU32BE(entry + 4);
    tag.size = ReadU32BE(entry + 8);
    raw.tags.push_back(tag);
  }

  return true;
}

bool LoadRawProfile(const char *szFilename, RawProfile &raw)
{
  std::ifstream in(szFilename, std::ios::binary);
  if (!in) {
    raw.readError = "unable to open file";
    return false;
  }

  in.seekg(0, std::ios::end);
  std::streamoff end = in.tellg();
  // A directory (or other non-regular path) can be opened by std::ifstream
  // successfully, yet tellg() then reports a nonsensical length: libstdc++
  // returns LLONG_MAX (0x7fffffffffffffff) for a directory such as "/tmp".
  // That value flowed straight into raw.data.resize() below, requesting a ~9 EB
  // allocation that aborts under AddressSanitizer ("requested allocation size
  // 0x7fffffffffffffff", #1519).  An ICC profile carries its size in a uint32
  // header field, so any length that cannot fit a conformant profile is not one
  // we could ever load; reject it here exactly like an unreadable file so the
  // caller degrades to the same graceful "parse failed" report it already emits
  // for a missing path, instead of crashing.
  if (end < 0 || (uint64_t)end > 0xFFFFFFFFull) {
    raw.readError = "input is not a readable regular file";
    return false;
  }
  in.seekg(0, std::ios::beg);

  raw.data.resize((size_t)end);
  if (!raw.data.empty()) {
    in.read((char*)raw.data.data(), (std::streamsize)raw.data.size());
    if (!in) {
      raw.readError = "unable to read complete file";
      return false;
    }
  }

  return ParseRawProfile(raw);
}

// In-memory counterpart of LoadRawProfile, used by the assessment-only entries
// (AssessPawgFromMemory / PawgCompressionVerdict, #1775) so a fuzzer can drive
// the report straight from a byte buffer with no filesystem round-trip.  Applies
// the same uint32 profile-size ceiling as the file loader (#1519): a conformant
// profile carries its length in a uint32 header field, so a larger buffer cannot
// be one we could load.
bool LoadRawProfileFromMemory(const unsigned char *data, size_t size, RawProfile &raw)
{
  if (!data || (uint64_t)size > 0xFFFFFFFFull) {
    raw.readError = "input is not a readable profile buffer";
    return false;
  }
  raw.data.assign(data, data + size);
  return ParseRawProfile(raw);
}

std::string SigString(uint32_t sig)
{
  char buf[64];
  icGetSig(buf, sizeof(buf), sig, false);
  return std::string(buf);
}

bool StartsWith(const char *s, const char *prefix)
{
  return s && std::strncmp(s, prefix, std::strlen(prefix)) == 0;
}

template <typename T, size_t N>
size_t CountOf(const T (&)[N])
{
  return N;
}

// Attribute a private (non-spec) tag signature found in a profile body to the
// vendor that registered its signature range. Backed by the registered private
// tag-signature ranges from registry.color.org/tag-signatures (the non-ICC
// rows), encoded in the generated IccSignatureRegistry.h snapshot.
const IccTagSigRange *FindRegisteredPrivateTag(uint32_t sig)
{
  for (size_t i = 0; i < CountOf(kIccPrivateTagSigRanges); ++i) {
    const IccTagSigRange &range = kIccPrivateTagSigRanges[i];
    if (sig >= range.first && sig <= range.last) {
      return &range;
    }
  }
  return NULL;
}

// Look up an exact manufacturer signature in the Manufacturer Signatures
// registry snapshot. Returns the registrant's company name, or NULL if the
// signature is not registered.
const char *FindRegisteredManufacturer(uint32_t sig)
{
  for (size_t i = 0; i < CountOf(kIccManufacturerSignatures); ++i) {
    if (kIccManufacturerSignatures[i].sig == sig) {
      return kIccManufacturerSignatures[i].company;
    }
  }
  return NULL;
}

bool IsSpecTag(icTagSignature sig)
{
  CIccInfo info;
  const char *name = info.GetTagSigName(sig);
  return name && !StartsWith(name, "Unknown");
}

bool IsZeroOrPrintable(uint32_t sig)
{
  if (sig == 0) {
    return true;
  }
  for (int i = 0; i < 4; ++i) {
    unsigned char ch = (unsigned char)((sig >> (24 - i * 8)) & 0xff);
    if (ch < 0x20 || ch > 0x7e) {
      return false;
    }
  }
  return true;
}

// Per ICC.1:2022-05 section 7.2.17, the profile header 'manufacturer' and
// 'creator' fields shall correspond to a signature in the device manufacturer
// section of the ICC signature registry (the Manufacturer Signatures registry),
// or be zero. The previous implementation validated these header fields against
// the private *tag* signature ranges instead -- the wrong registry -- which
// caused false 'unregistered' warnings for legitimately registered
// manufacturers (e.g. 'KODA' for Kodak, which is in the Manufacturer Signatures
// registry but not in any private tag-signature range).
bool IsRegisteredManufacturerOrZero(uint32_t sig)
{
  return sig == 0 || FindRegisteredManufacturer(sig) != NULL;
}

// Some signatures are pervasive conventions that are not (yet) in the
// Manufacturer Signatures registry and are not the zero signature, so they still
// fail the strict ICC.1:2022-05 section 7.2.17 check and warrant a WARN -- but
// they are well-understood placeholders rather than malformed/unknown 4CCs, so
// S3 reports them with softened wording and they are tracked for ICC registry
// submission (issue #1459 Segment A) rather than presented as suspect data:
//   'none' (0x6E6F6E65) -- explicit "no manufacturer" placeholder
//   'ICC ' (0x49434320) -- the International Color Consortium's own reference
//                          profiles (also used by the iccDEV regression fixtures)
const char *KnownUnregisteredConventionNote(uint32_t sig)
{
  switch (sig) {
  case 0x6E6F6E65:  // 'none'
    return "known 'no manufacturer' placeholder";
  case 0x49434320:  // 'ICC '
    return "International Color Consortium reference-profile signature";
  default:
    return NULL;
  }
}

// Append the per-field "(<sig> ...)" explanation for an unregistered manufacturer
// or creator signature, softening the wording for the known conventions above.
void AppendUnregisteredSigDetail(std::ostringstream &detail, uint32_t sig)
{
  if (!IsZeroOrPrintable(sig)) {
    return;
  }
  const char *note = KnownUnregisteredConventionNote(sig);
  detail << " (" << SigString(sig) << " ";
  if (note) {
    detail << note << ", not yet in";
  }
  else {
    detail << "not in";
  }
  detail << " Manufacturer Signatures registry snapshot "
         << ICCPAWG_REGISTRY_SNAPSHOT_DATE << ")";
}

std::string HeaderSignatureDetail(const RawProfile &raw,
                                  bool cmmOk,
                                  bool platformOk,
                                  bool manufacturerOk,
                                  bool creatorOk)
{
  std::ostringstream detail;
  detail << "CMM " << (cmmOk ? "registered/zero" : "unregistered")
         << ", platform " << (platformOk ? "registered/zero" : "unregistered")
         << ", manufacturer " << (manufacturerOk ? "registered/zero" : "unregistered")
         << ", creator " << (creatorOk ? "registered/zero" : "unregistered");
  if (!manufacturerOk) {
    AppendUnregisteredSigDetail(detail, raw.manufacturer);
  }
  if (!creatorOk) {
    AppendUnregisteredSigDetail(detail, raw.creator);
  }
  return detail.str();
}

bool IsD50HeaderIlluminant(const RawProfile &raw)
{
  if (!raw.hasHeader) {
    return false;
  }
  const double x = (int32_t)ReadU32BE(raw.data.data() + 68) / 65536.0;
  const double y = (int32_t)ReadU32BE(raw.data.data() + 72) / 65536.0;
  const double z = (int32_t)ReadU32BE(raw.data.data() + 76) / 65536.0;
  return std::fabs(x - 0.9642) <= 0.0001 &&
         std::fabs(y - 1.0000) <= 0.0001 &&
         std::fabs(z - 0.8249) <= 0.0001;
}

bool IsD50ProfileIlluminant(CIccProfile *pIcc)
{
  if (!pIcc) {
    return false;
  }
  return std::fabs(icFtoD(pIcc->m_Header.illuminant.X) - 0.9642) <= 0.0001 &&
         std::fabs(icFtoD(pIcc->m_Header.illuminant.Y) - 1.0000) <= 0.0001 &&
         std::fabs(icFtoD(pIcc->m_Header.illuminant.Z) - 0.8249) <= 0.0001;
}

uint64_t Round4(uint64_t v)
{
  return (v + 3ULL) & ~3ULL;
}

bool TagInBounds(const RawTag &tag, uint64_t fileSize)
{
  uint64_t end = (uint64_t)tag.offset + (uint64_t)tag.size;
  return tag.offset >= 132 && end >= tag.offset && end <= fileSize;
}

std::vector<RawTag> SortedUniqueTagRegions(const RawProfile &raw)
{
  std::vector<RawTag> tags = raw.tags;
  std::sort(tags.begin(), tags.end(), [](const RawTag &a, const RawTag &b) {
    if (a.offset != b.offset) {
      return a.offset < b.offset;
    }
    return a.size < b.size;
  });

  std::vector<RawTag> unique;
  for (const RawTag &tag : tags) {
    if (!unique.empty() && unique.back().offset == tag.offset) {
      if (tag.size > unique.back().size) {
        unique.back().size = tag.size;
      }
      continue;
    }
    unique.push_back(tag);
  }
  return unique;
}

int CountPrivateTags(const RawProfile &raw)
{
  int count = 0;
  for (const RawTag &tag : raw.tags) {
    if (!IsSpecTag(tag.sig)) {
      ++count;
    }
  }
  return count;
}

PrivateTagStatus AssessPrivateTags(const RawProfile &raw)
{
  PrivateTagStatus status;
  std::ostringstream registered;
  std::ostringstream undocumented;

  for (const RawTag &tag : raw.tags) {
    if (IsSpecTag(tag.sig)) {
      continue;
    }

    ++status.total;
    const IccTagSigRange *entry = FindRegisteredPrivateTag(tag.sig);
    if (entry) {
      if (status.registered) {
        registered << ", ";
      }
      registered << SigString(tag.sig) << "=" << entry->owner;
      ++status.registered;
    }
    else {
      if (status.undocumented) {
        undocumented << ", ";
      }
      undocumented << SigString(tag.sig);
      ++status.undocumented;
    }
  }

  if (status.total == 0) {
    status.detail = "no private tags present";
  }
  else {
    std::ostringstream detail;
    detail << status.total << " private tag(s): "
           << status.registered << " registered in " << kIccTagRegistryUrl
           << ", " << status.undocumented << " undocumented";
    if (status.registered) {
      detail << "; registered: " << registered.str();
    }
    if (status.undocumented) {
      detail << "; undocumented: " << undocumented.str();
    }
    status.detail = detail.str();
  }

  return status;
}

PawgVerdict ValidationStatusVerdict(icValidateStatus status)
{
  if (status == icValidateOK) {
    return PawgVerdict::Ok;
  }
  if (status == icValidateWarning) {
    return PawgVerdict::Warn;
  }
  return PawgVerdict::Fail;
}

std::string FirstReportLine(const std::string &report)
{
  std::istringstream in(report);
  std::string line;
  while (std::getline(in, line)) {
    while (!line.empty() && (line[0] == ' ' || line[0] == '\t' || line[0] == '\r')) {
      line.erase(line.begin());
    }
    if (!line.empty()) {
      if (line.size() > 240) {
        line.resize(240);
        line += "...";
      }
      return line;
    }
  }
  return "validation reported an issue without detail";
}

typedef bool (*SigValidator)(const RawProfile &raw, size_t offset, size_t end);

bool ValidatePe(const RawProfile &raw, size_t offset, size_t end)
{
  if (offset > end || end - offset < 64) {
    return false;
  }

  uint32_t peOff = (uint32_t)raw.data[offset + 60] |
                   ((uint32_t)raw.data[offset + 61] << 8) |
                   ((uint32_t)raw.data[offset + 62] << 16) |
                   ((uint32_t)raw.data[offset + 63] << 24);
  return peOff < 1024 && peOff <= end - offset - 4 &&
         raw.data[offset + peOff] == 'P' && raw.data[offset + peOff + 1] == 'E';
}

bool ValidateGzipHeader(const RawProfile &raw, size_t offset, size_t end)
{
  const size_t kGzipHeaderLen = 10;
  if (offset > end || end - offset < kGzipHeaderLen) {
    return false;
  }

  const unsigned char cm = raw.data[offset + 2];
  const unsigned char flg = raw.data[offset + 3];
  const unsigned char xfl = raw.data[offset + 8];
  const unsigned char os = raw.data[offset + 9];
  if (cm != 0x08 || (flg & 0xe0) != 0 || !(xfl == 0 || xfl == 2 || xfl == 4) ||
      !(os <= 13 || os == 255)) {
    return false;
  }

  size_t pos = offset + kGzipHeaderLen;
  if (flg & 0x04) {
    if (end - pos < 2) {
      return false;
    }
    const size_t extraLen = (size_t)raw.data[pos] | ((size_t)raw.data[pos + 1] << 8);
    pos += 2;
    if (extraLen > end - pos) {
      return false;
    }
    pos += extraLen;
  }
  if (flg & 0x08) {
    while (pos < end && raw.data[pos] != 0) {
      ++pos;
    }
    if (pos >= end) {
      return false;
    }
    ++pos;
  }
  if (flg & 0x10) {
    while (pos < end && raw.data[pos] != 0) {
      ++pos;
    }
    if (pos >= end) {
      return false;
    }
    ++pos;
  }
  if (flg & 0x02) {
    if (end - pos < 2) {
      return false;
    }
  }

  return true;
}

bool ValidateElf(const RawProfile &raw, size_t offset, size_t end)
{
  // Corroborate a "\x7fELF" hit against the ELF identification fields (e_ident)
  // plus e_type/e_version, so a coincidental 4-byte match in high-entropy CLUT
  // data does not fire S8. Need e_ident(16) + e_type(2) + e_machine(2) +
  // e_version(4) = 24 bytes.
  if (offset > end || end - offset < 24) {
    return false;
  }

  const unsigned char eiClass = raw.data[offset + 4];    // 1=32-bit, 2=64-bit
  const unsigned char eiData = raw.data[offset + 5];     // 1=little, 2=big endian
  const unsigned char eiVersion = raw.data[offset + 6];  // EV_CURRENT == 1
  if (!(eiClass == 1 || eiClass == 2) || !(eiData == 1 || eiData == 2) ||
      eiVersion != 1) {
    return false;
  }

  const bool be = (eiData == 2);
  const unsigned char *p = raw.data.data() + offset;
  const uint16_t eType = be ? (uint16_t)((p[16] << 8) | p[17])
                            : (uint16_t)(p[16] | (p[17] << 8));
  const uint32_t eVersion = be
    ? ((uint32_t)p[20] << 24 | (uint32_t)p[21] << 16 |
       (uint32_t)p[22] << 8 | (uint32_t)p[23])
    : ((uint32_t)p[20] | (uint32_t)p[21] << 8 |
       (uint32_t)p[22] << 16 | (uint32_t)p[23] << 24);

  // e_type: NONE/REL/EXEC/DYN/CORE (0-4), or the OS/processor-specific reserved
  // ranges (0xfe00-0xffff). e_version must be EV_CURRENT.
  const bool typeOk = eType <= 4 || (eType >= 0xfe00);  // && eType <= 0xffff implied by data type
  return typeOk && eVersion == 1;
}

bool ValidateShebang(const RawProfile &raw, size_t offset, size_t end)
{
  // The magic is "#!/" (3 bytes). A real shebang continues with a printable
  // interpreter path terminated by a newline; a coincidental "#!/" inside binary
  // CLUT data is followed by non-printable bytes well before any newline.
  const size_t kMaxLine = 256;
  size_t limit = end;
  if (limit - offset > kMaxLine) {
    limit = offset + kMaxLine;
  }
  for (size_t pos = offset + 3; pos < limit; ++pos) {
    const unsigned char c = raw.data[pos];
    if (c == '\n') {
      return true;  // a fully-printable interpreter line
    }
    if (c == '\t' || c == '\r') {
      continue;
    }
    if (c < 0x20 || c > 0x7e) {
      return false;  // control/binary byte => not a script line
    }
  }
  return false;  // no newline within a plausible interpreter-line length
}

bool HasSignature(const RawProfile &raw, const unsigned char *sig, size_t sigLen,
                  SigValidator validate, size_t begin, size_t end,
                  std::string &detail)
{
  if (sigLen == 0 || begin >= end || end > raw.data.size()) {
    return false;
  }

  for (size_t i = begin; i + sigLen <= end; ++i) {
    if (std::memcmp(raw.data.data() + i, sig, sigLen) != 0) {
      continue;
    }

    if (validate && !validate(raw, i, end)) {
      continue;
    }

    char msg[96];
    std::snprintf(msg, sizeof(msg), "signature at offset 0x%zx", i);
    detail = msg;
    return true;
  }
  return false;
}

bool HasAsciiSignatureCaseInsensitive(const RawProfile &raw, const char *sig,
                                      size_t begin, size_t end, std::string &detail)
{
  const size_t sigLen = std::strlen(sig);
  if (sigLen == 0 || begin >= end || end > raw.data.size()) {
    return false;
  }

  for (size_t i = begin; i + sigLen <= end; ++i) {
    bool matched = true;
    for (size_t j = 0; j < sigLen; ++j) {
      const unsigned char a = raw.data[i + j];
      const unsigned char b = static_cast<unsigned char>(sig[j]);
      if (std::tolower(a) != std::tolower(b)) {
        matched = false;
        break;
      }
    }
    if (matched) {
      // Corroborate: these signatures are dangerous only as text, so a genuine
      // hit sits inside printable content (embedded script/markup/command line).
      // A coincidental case-insensitive match inside binary CLUT data has little
      // or no printable context -- require adjacent printable bytes before/after.
      const size_t kMinContext = 16;
      size_t printableBefore = 0;
      for (size_t k = i; k > begin && printableBefore < kMinContext; --k) {
        const unsigned char c = raw.data[k - 1];
        if (c != '\t' && c != '\n' && c != '\r' && (c < 0x20 || c > 0x7e)) {
          break;
        }
        ++printableBefore;
      }
      size_t printableAfter = 0;
      for (size_t k = i + sigLen; k < end && printableAfter < kMinContext; ++k) {
        const unsigned char c = raw.data[k];
        if (c != '\t' && c != '\n' && c != '\r' && (c < 0x20 || c > 0x7e)) {
          break;
        }
        ++printableAfter;
      }
      if (printableBefore + printableAfter < kMinContext) {
        continue;
      }

      char msg[96];
      std::snprintf(msg, sizeof(msg), "signature at offset 0x%zx", i);
      detail = msg;
      return true;
    }
  }
  return false;
}

bool ScanMalwareRange(const RawProfile &raw, size_t begin, size_t end,
                      std::string &detail)
{
  static const unsigned char elf[] = {0x7f, 'E', 'L', 'F'};
  static const unsigned char mz[] = {'M', 'Z'};
  static const unsigned char macho64[] = {0xcf, 0xfa, 0xed, 0xfe};
  static const unsigned char macho32[] = {0xce, 0xfa, 0xed, 0xfe};
  static const unsigned char shebang[] = {'#', '!', '/'};
  static const unsigned char pdf[] = {'%', 'P', 'D', 'F', '-'};
  static const unsigned char zip[] = {'P', 'K', 0x03, 0x04};
  static const unsigned char rar[] = {'R', 'a', 'r', '!', 0x1a, 0x07};
  static const unsigned char sevenZip[] = {'7', 'z', 0xbc, 0xaf, 0x27, 0x1c};
  static const unsigned char gzip[] = {0x1f, 0x8b, 0x08};

  struct Sig {
    const unsigned char *bytes;
    size_t len;
    const char *name;
    SigValidator validate;
  };
  static const Sig sigs[] = {
    {elf, sizeof(elf), "ELF executable", ValidateElf},
    {mz, sizeof(mz), "PE executable", ValidatePe},
    {macho64, sizeof(macho64), "Mach-O executable", nullptr},
    {macho32, sizeof(macho32), "Mach-O executable", nullptr},
    {shebang, sizeof(shebang), "script shebang", ValidateShebang},
    {pdf, sizeof(pdf), "embedded PDF", nullptr},
    {zip, sizeof(zip), "embedded ZIP archive", nullptr},
    {rar, sizeof(rar), "embedded RAR archive", nullptr},
    {sevenZip, sizeof(sevenZip), "embedded 7z archive", nullptr},
    {gzip, sizeof(gzip), "embedded gzip stream", ValidateGzipHeader}
  };

  const size_t kMaxScan = (size_t)10 * 1024 * 1024;
  size_t scanEnd = std::min(end, begin + kMaxScan);
  for (const Sig &sig : sigs) {
    std::string offset;
    if (HasSignature(raw, sig.bytes, sig.len, sig.validate, begin, scanEnd, offset)) {
      detail = std::string(sig.name) + " " + offset;
      return true;
    }
  }

  struct TextSig {
    const char *text;
    const char *name;
  };
  static const TextSig textSigs[] = {
    {"<script", "embedded script markup"},
    {"<!doctype html", "embedded HTML document"},
    {"<html", "embedded HTML document"},
    {"<?php", "embedded PHP script"},
    {"javascript:", "embedded JavaScript URL"},
    {"vbscript:", "embedded VBScript URL"},
    {"invoke-expression", "PowerShell expression invocation"},
    {"iex(", "PowerShell expression invocation"},
    {"cmd.exe", "Windows command interpreter reference"},
    {"/bin/sh", "Unix shell reference"}
  };
  for (const TextSig &sig : textSigs) {
    std::string offset;
    if (HasAsciiSignatureCaseInsensitive(raw, sig.text, begin, scanEnd, offset)) {
      detail = std::string(sig.name) + " " + offset;
      return true;
    }
  }
  return false;
}

bool HasRepeatedPattern(const RawProfile &raw, size_t begin, size_t end,
                        const unsigned char *pattern, size_t patternLen,
                        size_t minBytes, std::string &detail,
                        const char *name)
{
  if (patternLen == 0 || minBytes < patternLen || begin >= end || end > raw.data.size()) {
    return false;
  }

  size_t runBytes = 0;
  size_t runStart = begin;
  for (size_t i = begin; i + patternLen <= end;) {
    if (std::memcmp(raw.data.data() + i, pattern, patternLen) == 0) {
      if (runBytes == 0) {
        runStart = i;
      }
      runBytes += patternLen;
      if (runBytes >= minBytes) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "%s sled at offset 0x%zx", name, runStart);
        detail = msg;
        return true;
      }
      i += patternLen;
    }
    else {
      runBytes = 0;
      ++i;
    }
  }
  return false;
}

bool ScanPrivateTagsForMalware(const RawProfile &raw, std::string &detail)
{
  for (const RawTag &tag : raw.tags) {
    if (IsSpecTag(tag.sig) || !TagInBounds(tag, raw.data.size())) {
      continue;
    }
    std::string local;
    if (ScanMalwareRange(raw, tag.offset, (size_t)tag.offset + tag.size, local)) {
      detail = "private tag " + SigString(tag.sig) + ": " + local;
      return true;
    }
  }
  detail = CountPrivateTags(raw) == 0 ? "no private tags present" : "private tag bodies scanned";
  return false;
}

bool ScanPrivateTagsForNops(const RawProfile &raw, std::string &detail)
{
  for (const RawTag &tag : raw.tags) {
    if (IsSpecTag(tag.sig) || !TagInBounds(tag, raw.data.size())) {
      continue;
    }
    const size_t begin = tag.offset;
    const size_t end = begin + tag.size;
    int run = 0;
    for (size_t i = begin; i < end; ++i) {
      if (raw.data[i] == 0x90) {
        ++run;
        if (run >= 16) {
          detail = "x86 NOP sled in private tag " + SigString(tag.sig);
          return true;
        }
      }
      else {
        run = 0;
      }
    }

    static const unsigned char x86TwoByteNop[] = {0x66, 0x90};
    static const unsigned char x86ThreeByteNop[] = {0x0f, 0x1f, 0x00};
    static const unsigned char armNopLe[] = {0x00, 0xf0, 0x20, 0xe3};
    static const unsigned char armNopBe[] = {0xe3, 0x20, 0xf0, 0x00};
    static const unsigned char aarch64NopLe[] = {0x1f, 0x20, 0x03, 0xd5};
    static const unsigned char aarch64NopBe[] = {0xd5, 0x03, 0x20, 0x1f};
    std::string local;
    if (HasRepeatedPattern(raw, begin, end, x86TwoByteNop, sizeof(x86TwoByteNop), 16, local, "x86 two-byte NOP") ||
        HasRepeatedPattern(raw, begin, end, x86ThreeByteNop, sizeof(x86ThreeByteNop), 18, local, "x86 multi-byte NOP") ||
        HasRepeatedPattern(raw, begin, end, armNopLe, sizeof(armNopLe), 16, local, "ARM NOP") ||
        HasRepeatedPattern(raw, begin, end, armNopBe, sizeof(armNopBe), 16, local, "ARM NOP") ||
        HasRepeatedPattern(raw, begin, end, aarch64NopLe, sizeof(aarch64NopLe), 16, local, "AArch64 NOP") ||
        HasRepeatedPattern(raw, begin, end, aarch64NopBe, sizeof(aarch64NopBe), 16, local, "AArch64 NOP")) {
      detail = "private tag " + SigString(tag.sig) + ": " + local;
      return true;
    }
  }
  detail = CountPrivateTags(raw) == 0 ? "no private tags present" : "no private-tag NOP sleds found";
  return false;
}

bool HasTag(CIccProfile *pIcc, icTagSignature sig)
{
  return pIcc && pIcc->IsTagPresent(sig);
}

bool HasAnyTag(CIccProfile *pIcc, const std::vector<icTagSignature> &sigs)
{
  for (icTagSignature sig : sigs) {
    if (HasTag(pIcc, sig)) {
      return true;
    }
  }
  return false;
}

PawgVerdict ChannelCountVerdict(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }
  if (pIcc->m_Header.deviceClass == icSigNamedColorClass) {
    detail = "N/A: NamedColor profiles do not encode transform input/output channel counts";
    return PawgVerdict::NotApplicable;
  }

  const icUInt32Number dataChannels = icGetSpaceSamples((icColorSpaceSignature)pIcc->m_Header.colorSpace);
  const icUInt32Number pcsChannels = icGetSpaceSamples((icColorSpaceSignature)pIcc->m_Header.pcs);
  int checked = 0;

  auto check_mbb = [&](icTagSignature sig, icUInt32Number expectedIn, icUInt32Number expectedOut) -> bool {
    CIccMBB *mbb = dynamic_cast<CIccMBB*>(pIcc->FindTag(sig));
    if (!mbb) {
      return true;
    }
    ++checked;
    return mbb->InputChannels() == expectedIn && mbb->OutputChannels() == expectedOut;
  };
  auto check_mpe = [&](icTagSignature sig, icUInt32Number expectedIn, icUInt32Number expectedOut) -> bool {
    CIccTagMultiProcessElement *mpe = dynamic_cast<CIccTagMultiProcessElement*>(pIcc->FindTag(sig));
    if (!mpe) {
      return true;
    }
    ++checked;
    return mpe->NumInputChannels() == expectedIn && mpe->NumOutputChannels() == expectedOut;
  };

  bool ok = true;
  ok = check_mbb(icSigAToB0Tag, dataChannels, pcsChannels) && ok;
  ok = check_mbb(icSigAToB1Tag, dataChannels, pcsChannels) && ok;
  ok = check_mbb(icSigAToB2Tag, dataChannels, pcsChannels) && ok;
  ok = check_mbb(icSigBToA0Tag, pcsChannels, dataChannels) && ok;
  ok = check_mbb(icSigBToA1Tag, pcsChannels, dataChannels) && ok;
  ok = check_mbb(icSigBToA2Tag, pcsChannels, dataChannels) && ok;
  ok = check_mpe(icSigDToB0Tag, dataChannels, pcsChannels) && ok;
  ok = check_mpe(icSigDToB1Tag, dataChannels, pcsChannels) && ok;
  ok = check_mpe(icSigDToB2Tag, dataChannels, pcsChannels) && ok;
  ok = check_mpe(icSigBToD0Tag, pcsChannels, dataChannels) && ok;
  ok = check_mpe(icSigBToD1Tag, pcsChannels, dataChannels) && ok;
  ok = check_mpe(icSigBToD2Tag, pcsChannels, dataChannels) && ok;

  if (!ok) {
    detail = "one or more transform tags have channel counts inconsistent with data colour space or PCS";
    return PawgVerdict::Warn;
  }
  detail = checked > 0 ? std::to_string(checked) + " transform tag(s) checked"
                       : "no transform channel-count mismatches found";
  return PawgVerdict::Ok;
}

PawgVerdict CalculatorCostVerdict(CIccProfile *pIcc, const RawProfile &raw, std::string &detail)
{
  if (!raw.hasHeader || ((raw.version >> 24) < 5)) {
    detail = "not an iccMAX profile";
    return PawgVerdict::NotApplicable;
  }
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  int mpeTags = 0;
  uint64_t elements = 0;
  uint64_t calculatorElements = 0;
  uint64_t estimatedCost = 0;
  for (TagEntryList::iterator it = pIcc->m_Tags.begin(); it != pIcc->m_Tags.end(); ++it) {
    CIccTagMultiProcessElement *mpe = dynamic_cast<CIccTagMultiProcessElement*>(pIcc->FindTag(*it));
    if (!mpe) {
      continue;
    }
    ++mpeTags;
    const icUInt32Number elemCount = mpe->NumElements();
    elements += elemCount;
    for (icUInt32Number i = 0; i < elemCount; ++i) {
      CIccMultiProcessElement *elem = mpe->GetElement((int)i);
      CIccMpeCalculator *calc = dynamic_cast<CIccMpeCalculator*>(elem);
      if (calc) {
        ++calculatorElements;
        estimatedCost += (uint64_t)std::max<icUInt16Number>(calc->NumInputChannels(), 1) *
                         (uint64_t)std::max<icUInt16Number>(calc->NumOutputChannels(), 1) *
                         64ULL;
      }
      else {
        estimatedCost += 1;
      }
    }
  }

  std::ostringstream oss;
  oss << mpeTags << " MPE tag(s), " << elements << " process element(s), "
      << calculatorElements << " calculator element(s), estimated relative cost="
      << estimatedCost;
  detail = oss.str();
  return (elements > 4096 || calculatorElements > 256 || estimatedCost > 65536) ?
         PawgVerdict::Warn : PawgVerdict::Ok;
}

static const icTagSignature kCommonRequired[] = {
  icSigProfileDescriptionTag,
  icSigCopyrightTag,
  icSigMediaWhitePointTag
};

static const icTagSignature kMatrixTrcAlternative[] = {
  icSigAToB0Tag,
  icSigRedMatrixColumnTag,
  icSigGreenMatrixColumnTag,
  icSigBlueMatrixColumnTag,
  icSigRedTRCTag,
  icSigGreenTRCTag,
  icSigBlueTRCTag,
  icSigGrayTRCTag
};

static const icTagSignature kOutputRequired[] = {
  icSigProfileDescriptionTag,
  icSigCopyrightTag,
  icSigMediaWhitePointTag,
  icSigAToB0Tag,
  icSigBToA0Tag,
  icSigGamutTag
};

static const icTagSignature kA2B0B2A0Required[] = {
  icSigProfileDescriptionTag,
  icSigCopyrightTag,
  icSigMediaWhitePointTag,
  icSigAToB0Tag,
  icSigBToA0Tag
};

static const icTagSignature kLinkRequired[] = {
  icSigProfileDescriptionTag,
  icSigCopyrightTag,
  icSigAToB0Tag,
  icSigProfileSequenceDescTag
};

static const icTagSignature kA2B0Required[] = {
  icSigProfileDescriptionTag,
  icSigCopyrightTag,
  icSigMediaWhitePointTag,
  icSigAToB0Tag
};

static const icTagSignature kNamedColorRequired[] = {
  icSigProfileDescriptionTag,
  icSigCopyrightTag,
  icSigMediaWhitePointTag,
  icSigNamedColor2Tag
};

static const icTagSignature kCommonOptional[] = {
  icSigCalibrationDateTimeTag,
  icSigCharTargetTag,
  icSigChromaticAdaptationTag,
  icSigChromaticityTag,
  icSigColorantTableTag,
  icSigColorantTableOutTag,
  icSigDeviceMfgDescTag,
  icSigDeviceModelDescTag,
  icSigGamutTag,
  icSigLuminanceTag,
  icSigMeasurementTag,
  icSigMediaBlackPointTag,
  icSigMetaDataTag,
  icSigOutputResponseTag,
  icSigPerceptualRenderingIntentGamutTag,
  icSigProfileSequceIdTag,
  icSigSaturationRenderingIntentGamutTag,
  icSigTechnologyTag,
  icSigViewingConditionsTag,
  icSigAToB1Tag,
  icSigAToB2Tag,
  icSigBToA1Tag,
  icSigBToA2Tag,
  icSigDToB0Tag,
  icSigDToB1Tag,
  icSigDToB2Tag,
  icSigBToD0Tag,
  icSigBToD1Tag,
  icSigBToD2Tag
};

bool ContainsTag(const icTagSignature *tags, size_t count, icTagSignature sig)
{
  for (size_t i = 0; i < count; ++i) {
    if (tags[i] == sig) {
      return true;
    }
  }
  return false;
}

bool HasAnyRequiredAlternative(CIccProfile *pIcc, const RuleTable &rule)
{
  // CWE-400/CWE-834: the RuleTable entries are static, trusted definitions whose
  // alternative[] arrays hold only a handful of tags; clamp to an explicit upper
  // bound so the walk can never run unbounded if a count is ever miswired.
  const size_t nMaxRuleTags = 64;
  size_t nAlt = (rule.alternativeCount > nMaxRuleTags) ? nMaxRuleTags : rule.alternativeCount;
  for (size_t i = 0; i < nAlt; ++i) {
    if (HasTag(pIcc, rule.alternative[i])) {
      return true;
    }
  }
  return rule.alternativeCount == 0;
}

const RuleTable *GetRuleTable(icProfileClassSignature cls)
{
  static const RuleTable inputRule = {
    kCommonRequired, CountOf(kCommonRequired),
    kMatrixTrcAlternative, CountOf(kMatrixTrcAlternative),
    kCommonOptional, CountOf(kCommonOptional),
    "A2B0 or matrix/TRC transform",
    "ICC.1-2022-05 section 8.2"
  };
  static const RuleTable displayRule = {
    kCommonRequired, CountOf(kCommonRequired),
    kMatrixTrcAlternative, CountOf(kMatrixTrcAlternative),
    kCommonOptional, CountOf(kCommonOptional),
    "A2B0 or matrix/TRC transform",
    "ICC.1-2022-05 section 8.3"
  };
  static const RuleTable outputRule = {
    kOutputRequired, CountOf(kOutputRequired),
    NULL, 0,
    kCommonOptional, CountOf(kCommonOptional),
    NULL,
    "ICC.1-2022-05 section 8.4"
  };
  static const RuleTable linkRule = {
    kLinkRequired, CountOf(kLinkRequired),
    NULL, 0,
    kCommonOptional, CountOf(kCommonOptional),
    NULL,
    "ICC.1-2022-05 section 8.5"
  };
  static const RuleTable colorSpaceRule = {
    kA2B0B2A0Required, CountOf(kA2B0B2A0Required),
    NULL, 0,
    kCommonOptional, CountOf(kCommonOptional),
    NULL,
    "ICC.1-2022-05 section 8.6"
  };
  static const RuleTable abstractRule = {
    kA2B0Required, CountOf(kA2B0Required),
    NULL, 0,
    kCommonOptional, CountOf(kCommonOptional),
    NULL,
    "ICC.1-2022-05 section 8.7"
  };
  static const RuleTable namedColorRule = {
    kNamedColorRequired, CountOf(kNamedColorRequired),
    NULL, 0,
    kCommonOptional, CountOf(kCommonOptional),
    NULL,
    "ICC.1-2022-05 section 8.8"
  };

  switch (cls) {
    case icSigInputClass:
      return &inputRule;
    case icSigDisplayClass:
      return &displayRule;
    case icSigOutputClass:
      return &outputRule;
    case icSigLinkClass:
      return &linkRule;
    case icSigColorSpaceClass:
      return &colorSpaceRule;
    case icSigAbstractClass:
      return &abstractRule;
    case icSigNamedColorClass:
      return &namedColorRule;
    default:
      return NULL;
  }
}

bool IsRequiredForClass(icProfileClassSignature cls, icTagSignature sig)
{
  const RuleTable *rule = GetRuleTable(cls);
  if (!rule) {
    return false;
  }
  return ContainsTag(rule->required, rule->requiredCount, sig) ||
         ContainsTag(rule->alternative, rule->alternativeCount, sig);
}

bool IsAllowedForClass(icProfileClassSignature cls, icTagSignature sig)
{
  const RuleTable *rule = GetRuleTable(cls);
  if (!rule) {
    return false;
  }
  return IsRequiredForClass(cls, sig) ||
         ContainsTag(rule->optional, rule->optionalCount, sig);
}

bool HasRequiredTags(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return false;
  }

  std::vector<std::string> missing;
  icProfileClassSignature cls = (icProfileClassSignature)pIcc->m_Header.deviceClass;
  const RuleTable *rule = GetRuleTable(cls);
  if (!rule) {
    detail = "profile class has no local PAWG required-tag rule";
    return false;
  }

  auto requireTag = [&](icTagSignature sig, const char *name) {
    if (!HasTag(pIcc, sig)) {
      missing.push_back(name);
    }
  };

  // CWE-400/CWE-834: the RuleTable entries are static, trusted definitions whose
  // required[] arrays hold only a handful of tags; clamp to an explicit upper bound
  // so the walk can never run unbounded if a count is ever miswired.
  const size_t nMaxRuleTags = 64;
  size_t nReq = (rule->requiredCount > nMaxRuleTags) ? nMaxRuleTags : rule->requiredCount;
  for (size_t i = 0; i < nReq; ++i) {
    requireTag(rule->required[i], SigString(rule->required[i]).c_str());
  }

  if (!HasAnyRequiredAlternative(pIcc, *rule)) {
    missing.push_back(rule->alternativeName ? rule->alternativeName : "required transform alternative");
  }

  if (cls != icSigLinkClass && cls != icSigNamedColorClass &&
      !IsD50ProfileIlluminant(pIcc) &&
      !HasTag(pIcc, icSigChromaticAdaptationTag)) {
    missing.push_back("chad when PCS illuminant is not D50");
  }

  if (!missing.empty()) {
    std::ostringstream oss;
    for (size_t i = 0; i < missing.size(); ++i) {
      if (i) {
        oss << ", ";
      }
      oss << missing[i];
    }
    detail = "missing " + oss.str();
    return false;
  }

  detail = std::string("required tags present for profile class (") + rule->specRef + ")";
  return true;
}

PawgVerdict TagValueEncodingVerdict(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  icValidateStatus worst = icValidateOK;
  std::vector<std::string> issues;
  for (TagEntryList::iterator it = pIcc->m_Tags.begin(); it != pIcc->m_Tags.end(); ++it) {
    CIccTag *tag = pIcc->FindTag(*it);
    if (!tag) {
      worst = icValidateCriticalError;
      issues.push_back(SigString(it->TagInfo.sig) + ": tag failed to load");
      continue;
    }

    std::string report;
    const icValidateStatus status = tag->Validate(icGetSigPath(it->TagInfo.sig), report, pIcc);
    worst = icMaxStatus(worst, status);
    if (status != icValidateOK && issues.size() < 4) {
      issues.push_back(SigString(it->TagInfo.sig) + ": " + FirstReportLine(report));
    }
  }

  if (worst == icValidateOK) {
    detail = "tag structures, ranges, and encoded values pass library validation";
  }
  else {
    std::ostringstream oss;
    for (size_t i = 0; i < issues.size(); ++i) {
      if (i) {
        oss << "; ";
      }
      oss << issues[i];
    }
    detail = issues.empty() ? "tag value validation reported issues" : oss.str();
  }
  return ValidationStatusVerdict(worst);
}

PawgVerdict TagTypeAllowedVerdict(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  std::string report;
  const icValidateStatus status = pIcc->Validate(report);
  if (status == icValidateOK) {
    detail = "profile validation confirms tag signatures use allowed tag types";
  }
  else {
    detail = FirstReportLine(report);
  }
  return ValidationStatusVerdict(status);
}

// True for the three DEFLATE-backed tag TYPE signatures.  The compression
// measurement keys on the tag *type*, so it reads identically whether or not this
// build links zlib (issue #1775: "Compression On or Off, not gzip") -- detection
// never depends on being able to inflate.  Deliberately distinct from
// ValidateGzipHeader()'s gzip *malware* scan, which hunts a gzip magic inside
// private-tag payloads; here we recognise legitimate compressed ICC tag types.
bool IsCompressedTagTypeSig(uint32_t typeSig)
{
  return typeSig == (uint32_t)icSigZipUtf8TextType ||   // 'zut8'
         typeSig == (uint32_t)icSigZipXmlType ||         // 'zxml'
         typeSig == (uint32_t)icSigZipXMLType;           // 'ZXML' (X-Rite CxF)
}

// Measures the profile's compression surface for PAWG item S14.  Enumerates
// compressed tags from the RAW bytes -- so the measurement still runs when
// IccProfLib could not parse the profile (fuzz robustness): a tag's 4-byte TYPE
// signature lives at its data offset, and a 'data' tag additionally carries an
// icCompressedData bit in its flags word at data+8.  The measurement is
// build-independent; only how much can be *said* about the content depends on
// zlib, which is why a compressed tag on a no-zlib build is reported as a Gap
// (measured, but content not decodable here) rather than a silent pass.
PawgVerdict CompressionPathVerdict(const RawProfile &raw, CIccProfile *pIcc, std::string &detail)
{
  (void)pIcc;  // detection is raw-bytes based; pIcc reserved for future content checks
  const uint64_t fileSize = raw.data.size();
  std::vector<std::string> compressed;

  for (const RawTag &tag : raw.tags) {
    const uint64_t typeOff = tag.offset;
    if (typeOff + 4 > fileSize)
      continue;                       // truncated/out-of-bounds tag: S6/S7 report it
    const uint32_t typeSig = ReadU32BE(raw.data.data() + (size_t)typeOff);

    bool isCompressed = IsCompressedTagTypeSig(typeSig);
    if (!isCompressed && typeSig == (uint32_t)icSigDataType && typeOff + 12 <= fileSize) {
      // 'data' tag on disk is [type][reserved][flags]; icCompressedData -> compressed.
      const uint32_t flags = ReadU32BE(raw.data.data() + (size_t)typeOff + 8);
      if (flags & (uint32_t)icCompressedData)
        isCompressed = true;
    }
    if (isCompressed)
      compressed.push_back(SigString(tag.sig) + " (" + SigString(typeSig) + ")");
  }

  if (compressed.empty()) {
    detail = "no DEFLATE-compressed tags (zut8/zxml/compressed data) present";
    return PawgVerdict::Ok;
  }

  std::ostringstream oss;
  oss << compressed.size() << " compressed tag(s): ";
  for (size_t i = 0; i < compressed.size(); ++i) {
    if (i)
      oss << ", ";
    oss << compressed[i];
  }
#ifdef ICC_USE_ZLIB
  // zlib linked: Describe()/GetText() can inflate and inspect the content, so the
  // mere presence of compressed tags is not itself a finding.
  oss << "; zlib linked in this build - compressed content is assessable";
  detail = oss.str();
  return PawgVerdict::Ok;
#else
  // No zlib: the compressed bytes are retained losslessly (Read/Write are
  // passthrough) but cannot be decoded here, so the content is unassessed.  Flag
  // it as a Gap so a no-zlib assessment run never implies it examined content it
  // could not decompress.
  oss << "; no zlib in this build - compressed content not assessed (bytes retained losslessly)";
  detail = oss.str();
  return PawgVerdict::Gap;
#endif
}

bool IsKnownProfileClass(uint32_t cls)
{
  return cls == icSigInputClass ||
         cls == icSigDisplayClass ||
         cls == icSigOutputClass ||
         cls == icSigLinkClass ||
         cls == icSigColorSpaceClass ||
         cls == icSigAbstractClass ||
         cls == icSigNamedColorClass ||
         cls == icSigColorEncodingClass;
}

bool IsKnownColorSpace(uint32_t sig)
{
  return sig == icSigNoColorData ||
         sig == icSigNamedData ||
         sig == icSigXYZData ||
         sig == icSigLabData ||
         icGetSpaceSamples((icColorSpaceSignature)sig) > 0;
}

HeaderCheck ValidateHeaderContent(const RawProfile &raw)
{
  HeaderCheck check;
  check.verdict = PawgVerdict::Ok;

  if (!raw.hasHeader) {
    check.verdict = PawgVerdict::Fail;
    check.detail = raw.readError.empty() ? "missing 128-byte ICC header" : raw.readError;
    return check;
  }

  std::vector<std::string> failures;
  std::vector<std::string> warnings;

  auto addFailure = [&](const char *msg) {
    failures.push_back(msg);
  };
  auto addWarning = [&](const char *msg) {
    warnings.push_back(msg);
  };

  if (raw.declaredSize != raw.data.size()) {
    addFailure("declared profile size does not match file length");
  }
  if (std::memcmp(raw.data.data() + 36, "acsp", 4) != 0) {
    addFailure("missing acsp profile signature");
  }

  const unsigned char major = raw.data[8];
  if (!(major == 2 || major == 4 || major == 5)) {
    addWarning("profile major version is not one of v2, v4, or v5");
  }
  if (raw.data[10] != 0 || raw.data[11] != 0) {
    addWarning("reserved version bytes 10-11 are non-zero");
  }
  if (!IsKnownProfileClass(raw.profileClass)) {
    addFailure("unknown profile/device class signature");
  }
  if (!IsKnownColorSpace(raw.colorSpace)) {
    addFailure("unknown data colour-space signature");
  }
  if (raw.profileClass != icSigLinkClass &&
      raw.pcs != icSigLabData && raw.pcs != icSigXYZData) {
    addFailure("PCS is not Lab or XYZ for a non-DeviceLink profile");
  }
  if (raw.profileClass == icSigNamedColorClass && raw.colorSpace != icSigNamedData) {
    addWarning("NamedColor profile class does not use named colour data space");
  }
  if (raw.profileClass != icSigNamedColorClass &&
      raw.colorSpace == icSigNamedData) {
    addWarning("named colour data space appears outside a NamedColor profile");
  }

  uint32_t renderingIntent = ReadU32BE(raw.data.data() + 64);
  if (renderingIntent > 3) {
    addWarning("rendering intent is outside the ICC.1 range 0-3");
  }
  if (!IsD50HeaderIlluminant(raw)) {
    addWarning("PCS illuminant is not D50");
  }
  if (!IsZeroOrPrintable(raw.cmm) ||
      !IsZeroOrPrintable(raw.manufacturer) ||
      !IsZeroOrPrintable(raw.creator)) {
    addWarning("CMM/manufacturer/creator signature is not zero or printable");
  }

  if (!failures.empty()) {
    check.verdict = PawgVerdict::Fail;
  }
  else if (!warnings.empty()) {
    check.verdict = PawgVerdict::Warn;
  }

  std::ostringstream detail;
  if (check.verdict == PawgVerdict::Ok) {
    detail << "header fields conform to ICC.1-2022-05 section 7.2";
  }
  else {
    for (size_t i = 0; i < failures.size(); ++i) {
      if (i) {
        detail << "; ";
      }
      detail << failures[i];
    }
    if (!warnings.empty()) {
      if (!failures.empty()) {
        detail << "; ";
      }
      detail << "warnings: ";
      for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) {
          detail << ", ";
        }
        detail << warnings[i];
      }
    }
  }
  check.detail = detail.str();
  return check;
}

PawgVerdict TagsVersionVerdict(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  const uint32_t version = pIcc->m_Header.version;
  std::vector<std::string> issues;

  if (version >= icVersionNumberV4 &&
      HasAnyTag(pIcc, {
        icSigCrdInfoTag, icSigDataTag, icSigDateTimeTag,
        icSigDeviceSettingsTag, icSigPs2CRD0Tag, icSigPs2CRD1Tag,
        icSigPs2CRD2Tag, icSigPs2CRD3Tag, icSigPs2CSATag,
        icSigPs2RenderingIntentTag, icSigScreeningDescTag,
        icSigScreeningTag, icSigUcrBgTag
      })) {
    issues.push_back("profile contains tags removed for v4+ profiles");
  }

  if (version < 0x05000000 &&
      HasAnyTag(pIcc, {
        icSigAToB3Tag, icSigBToA3Tag, icSigDToB3Tag, icSigBToD3Tag,
        icSigBRDFMToB0Tag, icSigBRDFMToB1Tag, icSigBRDFMToB2Tag,
        icSigBRDFMToB3Tag, icSigBRDFAToB0Tag, icSigBRDFAToB1Tag,
        icSigBRDFAToB2Tag, icSigBRDFAToB3Tag
      })) {
    issues.push_back("iccMAX/v5-only transform tags appear in a v2/v4 profile");
  }

  if (issues.empty()) {
    detail = "tag signatures are compatible with profile version";
    return PawgVerdict::Ok;
  }

  std::ostringstream oss;
  for (size_t i = 0; i < issues.size(); ++i) {
    if (i) {
      oss << "; ";
    }
    oss << issues[i];
  }
  detail = oss.str();
  return PawgVerdict::Warn;
}

PawgVerdict TextTagEncodingVerdict(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  const bool isV4OrNewer = pIcc->m_Header.version >= icVersionNumberV4;
  std::vector<std::string> issues;
  int checked = 0;

  auto checkTag = [&](icTagSignature tagSig, icTagTypeSignature v2Type,
                      icTagTypeSignature v4Type, const char *name) {
    CIccTag *tag = pIcc->FindTag(tagSig);
    if (!tag) {
      return;
    }
    ++checked;
    const icTagTypeSignature actual = tag->GetType();
    const icTagTypeSignature expected = isV4OrNewer ? v4Type : v2Type;
    if (actual != expected) {
      std::ostringstream oss;
      oss << name << " uses " << SigString(actual)
          << ", expected " << SigString(expected)
          << (isV4OrNewer ? " for v4+" : " for v2");
      issues.push_back(oss.str());
    }
  };

  checkTag(icSigProfileDescriptionTag, icSigTextDescriptionType,
           icSigMultiLocalizedUnicodeType, "desc");
  checkTag(icSigCopyrightTag, icSigTextType,
           icSigMultiLocalizedUnicodeType, "cprt");

  if (issues.empty()) {
    detail = checked > 0 ? "desc/cprt text encodings match profile version"
                         : "desc/cprt tags not present; required-tag checks cover absence";
    return PawgVerdict::Ok;
  }

  std::ostringstream oss;
  for (size_t i = 0; i < issues.size(); ++i) {
    if (i) {
      oss << "; ";
    }
    oss << issues[i];
  }
  detail = oss.str();
  return PawgVerdict::Warn;
}

PawgVerdict MediaWhitePointVerdict(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  CIccTag *tag = pIcc->FindTag(icSigMediaWhitePointTag);
  if (!tag) {
    detail = "mediaWhitePointTag is not present";
    return PawgVerdict::Warn;
  }
  if (tag->GetType() != icSigXYZType) {
    detail = "mediaWhitePointTag uses " + SigString(tag->GetType()) + ", expected XYZType";
    return PawgVerdict::Fail;
  }

  CIccTagXYZ *xyzTag = dynamic_cast<CIccTagXYZ*>(tag);
  if (!xyzTag || xyzTag->GetSize() != 1) {
    detail = "mediaWhitePointTag must contain exactly one XYZ value";
    return PawgVerdict::Fail;
  }

  icXYZNumber *xyz = xyzTag->GetXYZ(0);
  if (!xyz) {
    detail = "mediaWhitePointTag XYZ payload is unavailable";
    return PawgVerdict::Fail;
  }

  const double x = icFtoD(xyz->X);
  const double y = icFtoD(xyz->Y);
  const double z = icFtoD(xyz->Z);
  std::ostringstream oss;
  oss << "wtpt XYZ=(" << x << ", " << y << ", " << z << ")";

  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      x < 0.0 || y <= 0.0 || z < 0.0) {
    detail = oss.str() + " is not a valid positive XYZ white point";
    return PawgVerdict::Fail;
  }

  const bool isV4Display = pIcc->m_Header.version >= icVersionNumberV4 &&
                           pIcc->m_Header.deviceClass == icSigDisplayClass;
  if (isV4Display) {
    const bool d50 = std::fabs(x - 0.9642) <= 0.0005 &&
                     std::fabs(y - 1.0000) <= 0.0005 &&
                     std::fabs(z - 0.8249) <= 0.0005;
    detail = d50 ? oss.str() + " is D50 for v4 display profile"
                 : oss.str() + " is not D50 for v4 display profile";
    return d50 ? PawgVerdict::Ok : PawgVerdict::Warn;
  }

  detail = oss.str() + " is a valid XYZ white point for profile class";
  return PawgVerdict::Ok;
}

const char *VerdictText(PawgVerdict verdict)
{
  switch (verdict) {
    case PawgVerdict::Ok:
      return "OK";
    case PawgVerdict::Warn:
      return "WARN";
    case PawgVerdict::Fail:
      return "FAIL";
    case PawgVerdict::NotApplicable:
      return "N/A";
    case PawgVerdict::Gap:
      return "GAP";
    case PawgVerdict::NotRun:
      return "--";
  }
  return "??";
}

void AddItem(std::vector<PawgItem> &items, const char *id, const char *title,
             PawgVerdict verdict, const std::string &detail)
{
  items.push_back(PawgItem{id, title, verdict, detail});
}

bool QualityMetricNotApplicable(const std::string &reason)
{
  return reason.find("requires bounded device channels and Lab/XYZ PCS") != std::string::npos ||
         reason.find("requires Lab or XYZ PCS") != std::string::npos ||
         reason.find("Invalid profile") != std::string::npos ||
         reason.find("Invalid profile transform") != std::string::npos ||
         reason.find("Only classic lut8/lut16 quality metrics are currently supported") != std::string::npos ||
         reason.find("No supported round-trip transform pair present") != std::string::npos ||
         reason.find("No supported forward transform tag present") != std::string::npos ||
         reason.find("No supported forward transform for smoothness analysis") != std::string::npos;
}

PawgVerdict QualityRoundTrip(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  iccquality::RoundTripMetrics metrics;
  std::string reason;
  if (!iccquality::measure_round_trip(pIcc, metrics, reason)) {
    detail = reason.empty() ? "no supported round-trip transform pair present" : reason;
    return QualityMetricNotApplicable(detail) ? PawgVerdict::NotApplicable : PawgVerdict::Gap;
  }

  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "model=%s samples=%d first(avg=%.4f max=%.4f) second(avg=%.4f max=%.4f); warn(avg>%.1f or max>%.1f), fail(avg>%.1f or max>%.1f)",
                metrics.model.c_str(), metrics.samples,
                metrics.avgFirstDe00, metrics.maxFirstDe00,
                metrics.avgSecondDe00, metrics.maxSecondDe00,
                kRoundTripWarnAvg, kRoundTripWarnMax,
                kRoundTripFailAvg, kRoundTripFailMax);
  detail = buf;
  double maxAvg = std::max(metrics.avgFirstDe00, metrics.avgSecondDe00);
  double maxDelta = std::max(metrics.maxFirstDe00, metrics.maxSecondDe00);
  if (maxAvg > kRoundTripFailAvg || maxDelta > kRoundTripFailMax) {
    return PawgVerdict::Fail;
  }
  if (maxAvg > kRoundTripWarnAvg || maxDelta > kRoundTripWarnMax) {
    return PawgVerdict::Warn;
  }
  return PawgVerdict::Ok;
}

PawgVerdict QualityCurveInvertibility(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  const auto metrics = iccquality::measure_curve_invertibility(pIcc);
  if (metrics.curves.empty()) {
    detail = "N/A: No supported curves found";
    return PawgVerdict::NotApplicable;
  }

  // Monotonicity / dead-curve defects are flagged for every curve, but the
  // round-trip inverse-error threshold applies to tone curves only: LUT A/B/M
  // shaper curves are not required to be individually invertible (e.g. a B2A
  // output curve's dark-end clamping plateau), so thresholding their inverse
  // error spuriously WARNed correct CMYK output profiles (#1458).  The error is
  // also in normalized 0-1 curve-input units, not CIEDE2000.
  int problemCount = 0;
  int toneCount = 0;
  int shaperCount = 0;
  double toneMaxErr = 0.0;
  for (const auto &curve : metrics.curves) {
    if (curve.isToneCurve) {
      ++toneCount;
      toneMaxErr = std::max(toneMaxErr, curve.maxError);
    } else {
      ++shaperCount;
    }
    if (!curve.monotonic || curve.flat) {
      ++problemCount;
    }
  }

  std::ostringstream oss;
  oss << metrics.curves.size() << " curve(s) checked (" << toneCount << " tone, "
      << shaperCount << " shaper)";
  if (toneCount > 0) {
    oss << ", max tone-curve inverse error=" << toneMaxErr << " (normalized curve units)";
  } else {
    oss << ", no tone curves to invert; shaper curves checked for monotonicity/flatness only";
  }
  oss << "; warn(non-monotonic, flat, or tone-curve inverse error>" << kCurveWarnMaxError << ")";
  detail = oss.str();
  return (problemCount == 0 && toneMaxErr <= kCurveWarnMaxError) ? PawgVerdict::Ok
                                                                 : PawgVerdict::Warn;
}

PawgVerdict QualitySmoothness(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  iccquality::SmoothnessMetrics metrics;
  std::string reason;
  if (!iccquality::measure_transform_smoothness(pIcc, metrics, reason)) {
    detail = reason.empty() ? "No supported forward transform for smoothness analysis" : reason;
    return QualityMetricNotApplicable(detail) ? PawgVerdict::NotApplicable : PawgVerdict::Gap;
  }

  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "model=%s samples=%d avgStep=%.4f maxStep=%.4f maxCurvature=%.4f discontinuities=%d; warn(discontinuities>0, maxStep>%.1f, or curvature>%.1f)",
                metrics.model.c_str(), metrics.samples,
                metrics.avgStepDe00, metrics.maxStepDe00,
                metrics.maxCurvatureDe00, metrics.discontinuities,
                kSmoothWarnMaxStep, kSmoothWarnCurvature);
  detail = buf;
  return (metrics.discontinuities == 0 &&
          metrics.maxStepDe00 <= kSmoothWarnMaxStep &&
          metrics.maxCurvatureDe00 <= kSmoothWarnCurvature) ? PawgVerdict::Ok
                                                            : PawgVerdict::Warn;
}

PawgVerdict QualityCharacterization(CIccProfile *pIcc, std::string &detail)
{
  if (!pIcc) {
    detail = "profile did not load";
    return PawgVerdict::NotRun;
  }

  iccquality::CharacterizationMetrics metrics;
  std::string reason;
  if (!iccquality::evaluate_characterization(pIcc, metrics, reason)) {
    if (reason.find("No characterization data") != std::string::npos) {
      detail = "N/A: " + reason;
      return PawgVerdict::NotApplicable;
    }
    detail = reason.empty() ? "Characterization data could not be evaluated" : reason;
    return PawgVerdict::Gap;
  }

  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "fields=%d rows=%d usableRows=%d avgDeltaE00=%.4f maxDeltaE00=%.4f; warn(avg>%.1f or max>%.1f), fail(avg>%.1f or max>%.1f)",
                metrics.fieldCount, metrics.rowCount, metrics.rowsUsed,
                metrics.avgDe00, metrics.maxDe00,
                kCharacterizationWarnAvg, kCharacterizationWarnMax,
                kCharacterizationFailAvg, kCharacterizationFailMax);
  detail = buf;
  if (metrics.avgDe00 > kCharacterizationFailAvg ||
      metrics.maxDe00 > kCharacterizationFailMax) {
    return PawgVerdict::Fail;
  }
  if (metrics.avgDe00 > kCharacterizationWarnAvg ||
      metrics.maxDe00 > kCharacterizationWarnMax) {
    return PawgVerdict::Warn;
  }
  return PawgVerdict::Ok;
}

std::vector<PawgItem> EvaluatePawg(const RawProfile &raw, CIccProfile *pIcc)
{
  std::vector<PawgItem> items;
  const uint64_t fileSize = raw.data.size();
  const int privateCount = CountPrivateTags(raw);
  const PrivateTagStatus privateStatus = AssessPrivateTags(raw);
  const HeaderCheck headerCheck = ValidateHeaderContent(raw);

  bool tagTableBoundsOk = raw.hasTagTable;
  bool tagAlignmentOk = raw.hasTagTable;
  bool tagLayoutOk = raw.hasTagTable;
  bool eofOk = raw.hasTagTable && raw.declaredSize == fileSize;
  std::string tagDetail = raw.hasTagTable ? "tag table parsed" : raw.readError;

  if (raw.hasTagTable) {
    uint64_t expected = 132ULL + raw.tags.size() * 12ULL;
    std::vector<RawTag> regions = SortedUniqueTagRegions(raw);
    uint64_t lastEnd = expected;
    for (const RawTag &tag : regions) {
      if (!TagInBounds(tag, fileSize)) {
        tagTableBoundsOk = false;
        tagLayoutOk = false;
      }
      if ((tag.offset % 4) != 0) {
        tagAlignmentOk = false;
      }
      uint64_t end = (uint64_t)tag.offset + tag.size;
      uint64_t roundEnd = Round4(end);
      if (roundEnd > fileSize) {
        tagTableBoundsOk = false;
        tagLayoutOk = false;
      }
      if (tag.offset < lastEnd) {
        tagLayoutOk = false;
      }
      if (tag.offset > lastEnd) {
        tagLayoutOk = false;
      }
      lastEnd = std::max(lastEnd, roundEnd);
    }
    eofOk = eofOk && lastEnd == fileSize;
    tagDetail = tagTableBoundsOk && tagLayoutOk ? "tag offsets, lengths, overlaps and gaps checked"
                                                : "tag bounds, overlap, or gap issue detected";
  }

  std::string channelDetail = tagDetail;
  AddItem(items, "S1",
          "Do channel counts in tags match the data colour space?",
          ChannelCountVerdict(pIcc, channelDetail),
          channelDetail);

  bool headerOk = raw.hasHeader && raw.declaredSize == fileSize &&
                  std::memcmp(raw.data.data() + 36, "acsp", 4) == 0;
  AddItem(items, "S2",
          "Is header 128 bytes and correctly encoded?",
          headerOk ? PawgVerdict::Ok : PawgVerdict::Fail,
          headerOk ? "header magic and declared size are consistent" : raw.readError);

  CIccInfo info;
  bool cmmOk = raw.hasHeader &&
               (raw.cmm == 0 || !StartsWith(info.GetCmmSigName((icCmmSignature)raw.cmm), "Unknown"));
  bool platformOk = raw.hasHeader &&
                    (raw.platform == 0 || !StartsWith(info.GetPlatformSigName((icPlatformSignature)raw.platform), "Unknown"));
  bool manufacturerOk = raw.hasHeader && IsRegisteredManufacturerOrZero(raw.manufacturer);
  bool creatorOk = raw.hasHeader && IsRegisteredManufacturerOrZero(raw.creator);
  bool sigsOk = cmmOk && platformOk && manufacturerOk && creatorOk;
  AddItem(items, "S3",
          "Do Platform, Creator, Manufacturer and CMM fields correspond to registered signatures or are zero?",
          sigsOk ? PawgVerdict::Ok : PawgVerdict::Warn,
          HeaderSignatureDetail(raw, cmmOk, platformOk, manufacturerOk, creatorOk));

  bool illumD50 = pIcc ? IsD50ProfileIlluminant(pIcc) : IsD50HeaderIlluminant(raw);
  AddItem(items, "S4",
          "Does illuminant correspond to D50?",
          illumD50 ? PawgVerdict::Ok : PawgVerdict::Warn,
          illumD50 ? "PCS illuminant is D50" : "PCS illuminant is not D50");

  bool pcsOk = raw.hasHeader &&
               (raw.profileClass == icSigLinkClass ||
                raw.pcs == icSigLabData ||
                raw.pcs == icSigXYZData);
  AddItem(items, "S5",
          "Is PCS Lab or XYZ (unless DeviceLink profile)?",
          pcsOk ? PawgVerdict::Ok : PawgVerdict::Fail,
          pcsOk ? "PCS/profile-class rule satisfied" : "PCS is not Lab or XYZ for a non-DeviceLink profile");

  AddItem(items, "S6",
          "Are tags correctly aligned - offset and length correspond to tag table, no overlapping tags or gaps between tags - and correctly encoded?",
          (tagTableBoundsOk && tagLayoutOk) ? PawgVerdict::Ok : PawgVerdict::Fail,
          tagDetail);

  AddItem(items, "S7",
          "Is the tag table correctly encoded?",
          tagTableBoundsOk ? PawgVerdict::Ok : PawgVerdict::Warn,
          raw.hasTagTable ? "tag count and tag records fit in file" : raw.readError);

  std::string malwareDetail;
  bool malware = raw.opened && ScanMalwareRange(raw, 128, raw.data.size(), malwareDetail);
  AddItem(items, "S8",
          "Is the profile free of known malware signatures?",
          malware ? PawgVerdict::Fail : PawgVerdict::Ok,
          malware ? malwareDetail : "no executable/archive/script signatures found in bounded scan");

  AddItem(items, "S9",
          "Does EOF follow last tag (including four-byte boundary), with no additional bytes before or after?",
          eofOk ? PawgVerdict::Ok : PawgVerdict::Fail,
          eofOk ? "last padded tag end matches EOF" : "declared size or last tag end does not match EOF");

  std::string calculatorDetail;
  PawgVerdict calculatorVerdict = CalculatorCostVerdict(pIcc, raw, calculatorDetail);
  AddItem(items, "S10",
          "[iccMAX profiles only] Are excessive calculator elements avoided (if possible provide an estimate of computation cost)",
          calculatorVerdict,
          calculatorDetail);

  AddItem(items, "S11",
          "Are private tags absent?",
          privateCount == 0 ? PawgVerdict::Ok : PawgVerdict::Warn,
          privateStatus.detail);

  std::string privateMalwareDetail;
  bool privateMalware = ScanPrivateTagsForMalware(raw, privateMalwareDetail);
  AddItem(items, "S12",
          "If present, are private tags free of malware?",
          privateMalware ? PawgVerdict::Fail : PawgVerdict::Ok,
          privateMalwareDetail);

  std::string nopDetail;
  bool nops = ScanPrivateTagsForNops(raw, nopDetail);
  AddItem(items, "S13",
          "If present, are private tags free of exploitable non-operation (NOP) instructions?",
          nops ? PawgVerdict::Fail : PawgVerdict::Ok,
          nopDetail);

  // S14 measures the DEFLATE compression surface (zut8/zxml/compressed data),
  // the gap #1774 flagged and #1775 asked to close: the report keyed off tag
  // signatures and never noticed a tag's *type* was compressed.  The measurement
  // is build-independent (compression On/Off, not gzip); a compressed tag on a
  // no-zlib build resolves to a Gap because its content cannot be decoded here.
  std::string compressionDetail;
  PawgVerdict compressionVerdict = CompressionPathVerdict(raw, pIcc, compressionDetail);
  AddItem(items, "S14",
          "Are DEFLATE-compressed tags (zut8/zxml/compressed data) measured, and is their content assessable in this build?",
          compressionVerdict,
          compressionDetail);

  std::string tagValueDetail;
  PawgVerdict tagValueVerdict = TagValueEncodingVerdict(pIcc, tagValueDetail);
  AddItem(items, "C1",
          "Are tag types correctly encoded (signature, structure, data types, ranges, encoded values)?",
          tagValueVerdict,
          tagValueDetail);

  std::string textEncodingDetail;
  PawgVerdict textEncodingVerdict = TextTagEncodingVerdict(pIcc, textEncodingDetail);
  AddItem(items, "C2",
          "Are cprt and desc tags encoded as Unicode or text according to specification version?",
          textEncodingVerdict,
          textEncodingDetail);

  std::string tagTypeDetail;
  PawgVerdict tagTypeVerdict = TagTypeAllowedVerdict(pIcc, tagTypeDetail);
  AddItem(items, "C3",
          "Do tags only use tag types allowed for the tag?",
          tagTypeVerdict,
          tagTypeDetail);

  std::string requiredDetail;
  bool requiredOk = HasRequiredTags(pIcc, requiredDetail);
  AddItem(items, "C4",
          "Are all required tags for profile class present?",
          requiredOk ? PawgVerdict::Ok : PawgVerdict::Warn,
          requiredDetail);

  bool extraTags = false;
  std::ostringstream extraTagDetail;
  if (pIcc) {
    icProfileClassSignature cls = (icProfileClassSignature)pIcc->m_Header.deviceClass;
    for (TagEntryList::iterator it = pIcc->m_Tags.begin(); it != pIcc->m_Tags.end(); ++it) {
      if (IsSpecTag(it->TagInfo.sig) && !IsAllowedForClass(cls, it->TagInfo.sig)) {
        if (extraTags) {
          extraTagDetail << ", ";
        }
        extraTagDetail << SigString(it->TagInfo.sig);
        extraTags = true;
      }
    }
  }
  AddItem(items, "C5",
          "Is the profile free of additional tags not required for profile class (other than allowed optional tags); if present are they flagged as private tags?",
          !pIcc ? PawgVerdict::NotRun : (extraTags ? PawgVerdict::Warn : PawgVerdict::Ok),
          !pIcc ? "profile did not load" :
                  (extraTags ? "standard tags outside the local class rule table: " + extraTagDetail.str()
                             : "all standard extra tags are allowed optional tags or private-tag checks cover them"));

  AddItem(items, "C6",
          "If present, do private tags have a registered signature?",
          privateCount == 0 ? PawgVerdict::Ok :
                              (privateStatus.undocumented == 0 ? PawgVerdict::Ok : PawgVerdict::Warn),
          privateStatus.detail);

  AddItem(items, "C7",
          "If present, is documentation for private tags available through the tag registry?",
          privateCount == 0 ? PawgVerdict::Ok :
                              (privateStatus.undocumented == 0 ? PawgVerdict::Ok : PawgVerdict::Warn),
          privateStatus.detail);

  AddItem(items, "C8",
          "Are any undocumented private tags identified?",
          privateStatus.undocumented == 0 ? PawgVerdict::Ok : PawgVerdict::Warn,
          privateStatus.detail);

  bool classSpaceOk = pIcc != nullptr;
  if (pIcc) {
    icProfileClassSignature cls = (icProfileClassSignature)pIcc->m_Header.deviceClass;
    icColorSpaceSignature cs = (icColorSpaceSignature)pIcc->m_Header.colorSpace;
    if (cls == icSigNamedColorClass) {
      classSpaceOk = cs == icSigNamedData;
    }
    else if (cls == icSigLinkClass) {
      classSpaceOk = cs != icSigNoColorData;
    }
    else {
      classSpaceOk = cs != icSigNoColorData && cs != icSigNamedData;
    }
  }
  AddItem(items, "C9",
          "Is the profile class consistent with the data colour space?",
          classSpaceOk ? PawgVerdict::Ok : PawgVerdict::Warn,
          classSpaceOk ? "profile class/data colour-space pairing is locally consistent"
                       : "profile class/data colour-space pairing needs review");

  AddItem(items, "C10",
          "Does the header content conform with the specification?",
          headerCheck.verdict,
          headerCheck.detail);

  std::string tagsVersionDetail;
  PawgVerdict tagsVersionVerdict = TagsVersionVerdict(pIcc, tagsVersionDetail);
  AddItem(items, "C11",
          "Do the tags present correspond to the profile version?",
          tagsVersionVerdict,
          tagsVersionDetail);

  std::string wtptDetail;
  PawgVerdict wtptVerdict = MediaWhitePointVerdict(pIcc, wtptDetail);
  AddItem(items, "C12",
          "Is the white point correctly encoded (D50 for v4 display; or valid value for other profile classes)?",
          wtptVerdict,
          wtptDetail);

  bool reservedZero = raw.hasHeader;
  if (reservedZero) {
    for (size_t i = 100; i < 128; ++i) {
      if (raw.data[i] != 0) {
        reservedZero = false;
        break;
      }
    }
  }
  AddItem(items, "C13",
          "Are reserved bytes zero?",
          reservedZero ? PawgVerdict::Ok : PawgVerdict::Warn,
          reservedZero ? "header reserved bytes 100-127 are zero" : "one or more header reserved bytes are non-zero");

  AddItem(items, "C14",
          "Do tags start and end on four-byte boundaries?",
          tagAlignmentOk ? PawgVerdict::Ok : PawgVerdict::Fail,
          tagAlignmentOk ? "tag data starts are four-byte aligned and padded ends stay within file"
                         : "one or more tag offsets or padded ends are misaligned");

  std::string q1Detail;
  PawgVerdict q1 = QualityRoundTrip(pIcc, q1Detail);
  AddItem(items, "Q1",
          "First and second round trip average and maximum differences in CIEDE2000",
          q1,
          q1Detail);

  std::string q2Detail;
  PawgVerdict q2 = QualityCurveInvertibility(pIcc, q2Detail);
  AddItem(items, "Q2",
          // The curve round-trip error is in normalized 0-1 curve-input units,
          // not CIEDE2000 (it compares an input value to its Find(Apply(x))), so
          // the item is labelled accordingly rather than "in CIEDE2000" (#1458).
          "Curve round trip differences in normalized curve units (i.e. can be inverted)",
          q2,
          q2Detail);

  std::string q3Detail;
  PawgVerdict q3 = QualitySmoothness(pIcc, q3Detail);
  AddItem(items, "Q3",
          "Smoothness metric values of overall transform",
          q3,
          q3Detail);

  std::string q4Detail;
  PawgVerdict q4 = QualityCharacterization(pIcc, q4Detail);
  AddItem(items, "Q4",
          "Round trip average and maximum differences of profile output in CIEDE2000 (if characterization data is present)",
          q4,
          q4Detail);

  return items;
}

int CountVerdict(const std::vector<PawgItem> &items, PawgVerdict verdict)
{
  int count = 0;
  for (const PawgItem &item : items) {
    if (item.verdict == verdict) {
      ++count;
    }
  }
  return count;
}

const char *SectionName(char prefix)
{
  switch (prefix) {
    case 'S':
      return "security";
    case 'C':
      return "conformance";
    case 'Q':
      return "quality";
    default:
      return "unknown";
  }
}

std::string JsonEscape(const std::string &s)
{
  std::ostringstream oss;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char ch = static_cast<unsigned char>(s[i]);
    switch (ch) {
      case '\\':
        oss << "\\\\";
        break;
      case '"':
        oss << "\\\"";
        break;
      case '\b':
        oss << "\\b";
        break;
      case '\f':
        oss << "\\f";
        break;
      case '\n':
        oss << "\\n";
        break;
      case '\r':
        oss << "\\r";
        break;
      case '\t':
        oss << "\\t";
        break;
      default:
        if (ch < 0x20 || ch > 0x7e) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
          oss << buf;
        }
        else {
          oss << (char)ch;
        }
        break;
    }
  }
  return oss.str();
}

void PrintJsonReport(const char *szFilename, const RawProfile &raw,
                     CIccProfile *pIcc, const std::vector<PawgItem> &items)
{
  printf("{\n");
  printf("  \"tool\": \"iccPawgReport\",\n");
  printf("  \"iccpProfileLibVersion\": \"" ICCPROFLIBVER "\",\n");
  printf("  \"profile\": \"%s\",\n",
         JsonEscape(szFilename ? szFilename : "(null)").c_str());
  printf("  \"sizeBytes\": %zu,\n", raw.data.size());
  printf("  \"load\": \"%s\",\n",
         pIcc ? "parsed by IccProfLib" : "raw checks only; IccProfLib parse failed");
  printf("  \"references\": [\n");
  printf("    \"%s\",\n", kPawgUrl);
  printf("    \"%s\",\n", kIccTagRegistryUrl);
  printf("    \"ICC.1-2022-05 section 7.2 and section 8 class tag tables\"\n");
  printf("  ],\n");
  printf("  \"summary\": {\n");
  printf("    \"total\": %zu,\n", items.size());
  printf("    \"pass\": %d,\n", CountVerdict(items, PawgVerdict::Ok));
  printf("    \"warn\": %d,\n", CountVerdict(items, PawgVerdict::Warn));
  printf("    \"fail\": %d,\n", CountVerdict(items, PawgVerdict::Fail));
  printf("    \"notApplicable\": %d,\n", CountVerdict(items, PawgVerdict::NotApplicable));
  printf("    \"gap\": %d,\n", CountVerdict(items, PawgVerdict::Gap));
  printf("    \"notRun\": %d\n", CountVerdict(items, PawgVerdict::NotRun));
  printf("  },\n");
  printf("  \"items\": [\n");
  for (size_t i = 0; i < items.size(); ++i) {
    const PawgItem &item = items[i];
    printf("    {\"id\":\"%s\",\"section\":\"%s\",\"verdict\":\"%s\",\"title\":\"%s\",\"detail\":\"%s\"}%s\n",
           item.id,
           SectionName(item.id[0]),
           VerdictText(item.verdict),
           JsonEscape(item.title).c_str(),
           JsonEscape(item.detail).c_str(),
           i + 1 == items.size() ? "" : ",");
  }
  printf("  ]\n");
  printf("}\n");
}

void PrintSection(const std::vector<PawgItem> &items, const char *section,
                  char prefix)
{
  printf("\n[ %s ]\n\n", section);
  for (const PawgItem &item : items) {
    if (item.id[0] != prefix) {
      continue;
    }
    printf("  [%-4s] %-3s %s\n", VerdictText(item.verdict), item.id, item.title);
    if (!item.detail.empty()) {
      printf("         %s\n", item.detail.c_str());
    }
  }
}

bool HasFail(const std::vector<PawgItem> &items)
{
  for (const PawgItem &item : items) {
    if (item.verdict == PawgVerdict::Fail) {
      return true;
    }
  }
  return false;
}

} // namespace

int DumpPawgReport(const char *szFilename, bool bJson)
{
  // The raw byte read is deliberately unconditional and comes first: it does not
  // depend on IccProfLib parsing the file at all, so a profile the library refuses
  // still receives the full raw-byte security assessment below, with the
  // parsed-profile items reported as NOT RUN. That graceful degradation is what
  // makes a strict-only load acceptable -- and is why the --read fallback that
  // used to sit further down was never needed (#1977).
  RawProfile raw;
  LoadRawProfile(szFilename, raw);

  // sReport/nStatus exist only to satisfy ValidateIccProfile's out-parameters:
  // this entry point reports the PAWG checklist, not the validation log, and the
  // checklist derives its own verdict from the profile.  TagTypeAllowedVerdict()
  // re-runs Validate() on the parsed profile and turns that status into the C-item
  // the report actually prints, so nothing downstream of here reads either local.
  std::string sReport;
  icValidateStatus nStatus = icValidateOK;
  CIccProfile *pIcc = ValidateIccProfile(szFilename, sReport, nStatus);

  std::vector<PawgItem> items = EvaluatePawg(raw, pIcc);

  if (bJson) {
    PrintJsonReport(szFilename, raw, pIcc, items);
    delete pIcc;
    return HasFail(items) ? 1 : 0;
  }

  printf("\n==============================================================================\n");
  printf("ICC PROFILE ASSESSMENT REPORT (PAWG)\n");
  printf("==============================================================================\n\n");
  printf("Reference:  ICC Profile Assessment Working Group\n");
  printf("            Goals for profile assessment\n");
  printf("URL:        %s\n", kPawgUrl);
  printf("Tool:       iccPawgReport (IccProfLib " ICCPROFLIBVER ")\n");
  printf("Profile:    %s\n", szFilename ? szFilename : "(null)");
  printf("Size:       %zu bytes\n", raw.data.size());
  printf("Load:       %s\n", pIcc ? "parsed by IccProfLib" : "raw checks only; IccProfLib parse failed");
  printf("\n");

  PrintSection(items, "SECURITY", 'S');
  PrintSection(items, "CONFORMANCE", 'C');
  PrintSection(items, "QUALITY", 'Q');

  printf("\n[ ASSESSMENT SUMMARY ]\n\n");
  printf("  Total checklist items:  %zu\n", items.size());
  printf("  PASS:                   %d\n", CountVerdict(items, PawgVerdict::Ok));
  printf("  WARN:                   %d\n", CountVerdict(items, PawgVerdict::Warn));
  printf("  FAIL:                   %d\n", CountVerdict(items, PawgVerdict::Fail));
  printf("  N/A:                    %d\n", CountVerdict(items, PawgVerdict::NotApplicable));
  printf("  GAP:                    %d\n", CountVerdict(items, PawgVerdict::Gap));
  printf("  NOT RUN:                %d\n", CountVerdict(items, PawgVerdict::NotRun));
  printf("\n");
  printf("  Overall:                %s\n",
         HasFail(items) ? "FAIL - one or more PAWG checks failed"
                        : "REPORT - no PAWG failures; review WARN/GAP/NOT RUN items");
  printf("\n");

  delete pIcc;

  return HasFail(items) ? 1 : 0;
}

// --- Assessment-only entry points (issue #1775) ----------------------------
// These run the PAWG evaluation "up to the point of Output, not for Output" so
// the overnight CI fuzzing harness can drive the report from an in-memory
// profile image with no filesystem round-trip and no report emission.

// The kPawg* codes the header exposes (so callers need not see the PawgVerdict
// enum type) must stay in lockstep with PawgVerdict's order.
static_assert((int)PawgVerdict::Ok == kPawgOk, "kPawgOk out of sync with PawgVerdict");
static_assert((int)PawgVerdict::Warn == kPawgWarn, "kPawgWarn out of sync with PawgVerdict");
static_assert((int)PawgVerdict::Fail == kPawgFail, "kPawgFail out of sync with PawgVerdict");
static_assert((int)PawgVerdict::NotApplicable == kPawgNotApplicable, "kPawgNotApplicable out of sync");
static_assert((int)PawgVerdict::Gap == kPawgGap, "kPawgGap out of sync with PawgVerdict");
static_assert((int)PawgVerdict::NotRun == kPawgNotRun, "kPawgNotRun out of sync with PawgVerdict");

int AssessPawgFromMemory(const unsigned char *data, size_t size)
{
  RawProfile raw;
  LoadRawProfileFromMemory(data, size, raw);

  // Parse a full CIccProfile from the same buffer when possible so the parsed-
  // profile checks (C1/C3, quality) also exercise; the buffer must outlive pIcc
  // and it does -- `data` is owned by the caller for the whole call.
  std::string sReport;
  icValidateStatus nStatus = icValidateOK;
  CIccProfile *pIcc = (data && size <= 0xFFFFFFFFull)
      ? ValidateIccProfile((const icUInt8Number *)data, (icUInt32Number)size, sReport, nStatus)
      : nullptr;

  std::vector<PawgItem> items = EvaluatePawg(raw, pIcc);
  const bool fail = HasFail(items);
  delete pIcc;
  return fail ? 1 : 0;
}

int PawgCompressionVerdict(const unsigned char *data, size_t size, std::string *outDetail)
{
  // Focused entry: runs ONLY the S14 compression measurement.  Detection is
  // raw-bytes based, so this needs no parsed profile -- cheap and fuzz-robust.
  RawProfile raw;
  LoadRawProfileFromMemory(data, size, raw);
  std::string detail;
  const PawgVerdict v = CompressionPathVerdict(raw, nullptr, detail);
  if (outDetail)
    *outDetail = detail;
  return (int)v;
}
