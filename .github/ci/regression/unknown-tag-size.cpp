/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// CTest contract for CIccTagUnknown's empty state (#2706).
//
// The default constructor left m_nSize uninitialised. Read() and the XML/JSON
// parsers set it only when they find data, and CIccTagJsonUnknown::ParseJson()
// returns success without touching it when the document has no "unknownData"
// key, which is how iccFromJson reached Write() with a garbage count
// (MemorySanitizer, CIccTagUnknown::Write). The copy constructor and operator=
// copied an empty tag with memcpy() from a NULL source. Read() kept a stale or
// new count with no buffer on several failure paths, and ParseJson() set the
// new count before refusing an oversize payload.
//
// A plain build cannot see an uninitialised read, so the default-constructed
// objects are built with placement new over storage filled with 0xAB. On an
// unfixed library the "empty" size then reads 0xABABABAB.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagBasic.h"
#include "IccIO.h"
#include "IccTagJson.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "unknown-tag-size: FAIL  %s\n", label);
    g_failures++;
  }
}

const icTagTypeSignature kPrivateType = (icTagTypeSignature)0x57314230; // 'W1B0'

// Storage for one object, pre-filled so that any member the constructor
// forgets reads back as 0xAB bytes instead of whatever the stack held.
//
// The fill is read back through a volatile pointer before the object is
// constructed. Without that, a compiler may treat stores made before an
// object's lifetime begins as dead (GCC's -flifetime-dse does, once the
// constructor is inlined) and drop the memset, and the check would pass or fail
// on whatever the stack held. CMake also builds this test with
// -fno-lifetime-dse under GCC.
volatile unsigned char g_sink = 0;

template <typename T>
struct Poisoned {
  alignas(T) unsigned char bytes[sizeof(T)];
  Poisoned()
  {
    std::memset(bytes, 0xAB, sizeof(bytes));
    const volatile unsigned char *p = bytes;
    for (size_t i = 0; i < sizeof(bytes); i++)
      g_sink ^= p[i];
  }
};

// A tag read from a stream holding its type signature and nPayload bytes.
// Read() fails for an empty payload, but it has set m_nSize to 0 and left
// m_pData NULL by then, so nPayload == 0 builds an empty tag on any library.
void readTag(CIccTagUnknown &tag, const icUInt8Number *pPayload, icUInt32Number nPayload)
{
  icUInt8Number buf[64] = { 0x57, 0x31, 0x42, 0x30 };
  if (nPayload)
    std::memcpy(buf + 4, pPayload, nPayload);

  CIccMemIO io;
  io.Attach(buf, 4 + nPayload);
  tag.Read(4 + nPayload, &io);
}

void testDefaultConstructor()
{
  Poisoned<CIccTagUnknown> mem;
  CIccTagUnknown *pTag = new (mem.bytes) CIccTagUnknown();

  check(pTag->GetSize() == 0, "a default-constructed tag has size 0");
  check(pTag->GetData() == NULL, "a default-constructed tag has no data");

  // Write() emits the type signature and nothing else for an empty tag.
  CIccMemIO io;
  io.Alloc(64, true);
  check(pTag->Write(&io), "an empty tag writes");
  check(io.GetLength() == 4, "an empty tag writes only its type signature");

  pTag->~CIccTagUnknown();
}

// The reported path: iccFromJson on a private tag whose JSON has no
// "unknownData" key. ParseJson() accepts it and must leave an empty tag.
void testJsonWithoutUnknownData()
{
  Poisoned<CIccTagJsonUnknown> mem;
  CIccTagJsonUnknown *pTag = new (mem.bytes) CIccTagJsonUnknown(kPrivateType);

  IccJson j = IccJson::object();
  j["text"] = "not unknownData";
  std::string parseStr;
  check(pTag->ParseJson(j, parseStr), "ParseJson accepts a tag without unknownData");
  check(pTag->GetSize() == 0, "JSON without unknownData leaves size 0");
  check(pTag->GetData() == NULL, "JSON without unknownData leaves no data");

  CIccMemIO io;
  io.Alloc(64, true);
  check(pTag->Write(&io), "the JSON-parsed empty tag writes");
  check(io.GetLength() == 4, "the JSON-parsed empty tag writes only its type signature");

  pTag->~CIccTagJsonUnknown();
}

// Read() failures leave an empty tag. They used to keep the previous count on
// an early return, or the new count over a partly written buffer.
void testReadFailures()
{
  const icUInt8Number payload[3] = { 1, 2, 3 };

  // Declared size above the 256 MB cap, over a tag that held three bytes.
  CIccTagUnknown oversize;
  readTag(oversize, payload, 3);
  icUInt8Number sig[4] = { 0x57, 0x31, 0x42, 0x30 };
  CIccMemIO io;
  io.Attach(sig, 4);
  check(!oversize.Read(0x10000010, &io), "Read() refuses a size above its cap");
  check(oversize.GetSize() == 0 && oversize.GetData() == NULL,
        "a refused oversize Read() leaves an empty tag, not the old count");

  // Declared ten payload bytes, stream holds two.
  CIccTagUnknown shortRead;
  icUInt8Number buf[6] = { 0x57, 0x31, 0x42, 0x30, 9, 9 };
  CIccMemIO io2;
  io2.Attach(buf, 6);
  check(!shortRead.Read(14, &io2), "Read() refuses a short stream");
  check(shortRead.GetSize() == 0 && shortRead.GetData() == NULL,
        "a short Read() leaves an empty tag, not a partly written buffer");
}

// ParseJson() refuses unknownData above its 16 MB cap and leaves the tag as it
// was. It used to set the new count first, over the old three-byte buffer.
void testJsonCap()
{
  CIccTagJsonUnknown tag(kPrivateType);
  const icUInt8Number payload[3] = { 1, 2, 3 };
  readTag(tag, payload, 3);

  IccJson j = IccJson::object();
  j["unknownData"] = std::string(2 * (16u * 1024 * 1024 + 1), '0');
  std::string parseStr;
  check(!tag.ParseJson(j, parseStr), "ParseJson refuses unknownData above 16 MB");
  check(tag.GetSize() == 3 && tag.GetData() && std::memcmp(tag.GetData(), payload, 3) == 0,
        "a refused ParseJson leaves the previous payload and its size");
}

void testCopies()
{
  // Copy and assign from an empty tag: the result is empty, with no buffer.
  CIccTagUnknown empty;
  readTag(empty, NULL, 0);
  check(empty.GetSize() == 0 && empty.GetData() == NULL, "the empty source tag is empty");

  CIccTagUnknown copied(empty);
  check(copied.GetSize() == 0, "a copy of an empty tag has size 0");
  check(copied.GetData() == NULL, "a copy of an empty tag has no data");

  const icUInt8Number payload[3] = { 1, 2, 3 };
  CIccTagUnknown assigned;
  readTag(assigned, payload, 3);
  assigned = empty;
  check(assigned.GetSize() == 0, "assigning an empty tag leaves size 0");
  check(assigned.GetData() == NULL, "assigning an empty tag leaves no data");

  // Positive control: a tag with a payload copies and assigns it byte for byte.
  CIccTagUnknown full;
  readTag(full, payload, 3);
  check(full.GetSize() == 3, "the payload tag reads three bytes");

  CIccTagUnknown fullCopy(full);
  check(fullCopy.GetSize() == 3 && fullCopy.GetData() != full.GetData() &&
          std::memcmp(fullCopy.GetData(), payload, 3) == 0,
        "a copy of a payload tag owns an equal payload");

  CIccTagUnknown fullAssigned;
  fullAssigned = full;
  check(fullAssigned.GetSize() == 3 && fullAssigned.GetData() != full.GetData() &&
          std::memcmp(fullAssigned.GetData(), payload, 3) == 0,
        "an assigned payload tag owns an equal payload");
}

} // namespace

int main()
{
  testDefaultConstructor();
  testJsonWithoutUnknownData();
  testReadFailures();
  testJsonCap();
  testCopies();

  if (g_failures)
    std::fprintf(stderr, "unknown-tag-size: %d failure(s)\n", g_failures);
  else
    std::printf("unknown-tag-size: all checks passed\n");
  return g_failures;
}
