/*
    File:       embedded-image-zero-byte.cpp

    Contains:   CTest helper for a zero-byte image in the embeddedHeightImageType
                and embeddedNormalImageType JSON reader/writer pairs and binary
                writers, and for the dataType and embedded image constructors
                given a size below 1 (#2572, from the #2571 post-merge report).

    Both binary Read() methods need at least one image byte after the fixed
    fields, so a zero-byte image has no binary form that loads.  #2571 made the
    XML reader and writer follow that.  The other paths did not:

      - ParseJson() took "ImageData" with no hex digits ("" or "zz") as "no
        data", skipped SetSize(), and returned true.  The constructor's one zero
        byte stayed in place, so iccFromJson exited 0 and saved a one-byte
        image the document never held (measured on master 5ce043f2).

      - ToJson() wrote a zero-byte image with no "ImageData", which ParseJson()
        refuses ("Cannot find ImageData").

      - Write() saved a zero-byte image as a tag of the fixed fields alone (24
        bytes for a height image, 16 for a normal image), which Read() refuses.

      - The constructors tested the clamp on the unsigned m_nSize and allocated
        the unclamped nSize.  nSize 0 recorded a size of 1 over a zero-byte
        allocation, which Write() read one byte past.  A negative nSize skipped
        the clamp, the allocation failed, and the tag was left empty.
        CIccTagData's constructor had the same lines and is fixed with them.

    Against the unfixed libraries, measured on master 5ce043f2, 11 cases fail on
    a plain build: the four JSON refusals, the two ToJson and two Write
    refusals (each observes true), and the three size -1 constructor cases
    (size 0 instead of 1).  The size 0 constructor cases pass there.  Their
    one-byte over-read shows only under valgrind ("0 bytes after a block of
    size 0").  AddressSanitizer does not see it, because it gives a zero-byte
    malloc one usable byte.

    Each refusal has a control that must still pass: a one-byte image parses
    with the byte it was given (0x8a, not the constructor's 0), writes, and
    reads back.  Without the controls, a reader or writer that refused every
    image would pass.  The writer refusals also assert that nothing was
    written, so a writer that refused only after emitting the fixed fields fails.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccTagJson.h"
#include "IccTagBasic.h"
#include "IccIO.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

static const char *kName = "embedded-image-zero-byte";
static int g_fail = 0;

static void check(bool ok, const std::string &label)
{
  if (ok) {
    std::fprintf(stdout, "%s: PASS  %s\n", kName, label.c_str());
  }
  else {
    ++g_fail;
    std::fprintf(stderr, "%s: FAIL  %s\n", kName, label.c_str());
  }
}

// An image tag's "data" object, as ToJson writes it.  pHex == NULL omits
// "ImageData".
static IccJson imageJson(const char *pHex)
{
  IccJson j = IccJson::object();
  j["SeamlessIndicator"] = 0;
  j["EncodingFormat"] = 0;
  if (pHex)
    j["ImageData"] = pHex;
  return j;
}

// Write pTag to memory and read it back into pOut with the length written.
static bool binaryRoundTrip(CIccTag *pTag, CIccTag *pOut, icUInt32Number &nWritten)
{
  CIccMemIO io;
  nWritten = 0;
  if (!io.Alloc(256, true) || !pTag->Write(&io))
    return false;
  nWritten = (icUInt32Number)io.Tell();
  return io.Seek(0, icSeekSet) == 0 && pOut->Read(nWritten, &io);
}

// JsonTag is the IccJSON class, BaseTag the IccProfLib class it derives from.
// nFixed is the tag's length with no image bytes.
template <class JsonTag, class BaseTag>
static void imageCases(const char *szTag, icUInt32Number nFixed)
{
  const std::string tag(szTag);

  // JSON reader: no hex digits is refused.
  for (const char *pHex : { "", "zz" }) {
    JsonTag t;
    std::string parseStr;
    check(!t.ParseJson(imageJson(pHex), parseStr),
          tag + " ParseJson refuses ImageData \"" + pHex + "\" (no hex digits)");
  }
  {
    JsonTag t;
    std::string parseStr;
    check(!t.ParseJson(imageJson(NULL), parseStr),
          tag + " ParseJson still refuses a missing ImageData");
  }

  // JSON reader control: one and two bytes load as given.
  {
    JsonTag t;
    std::string parseStr;
    bool ok = t.ParseJson(imageJson("8a"), parseStr);
    check(ok && t.GetSize() == 1 && *t.GetData(0) == 0x8a,
          tag + " ParseJson loads ImageData \"8a\" as one byte 0x8a");
  }
  {
    JsonTag t;
    std::string parseStr;
    bool ok = t.ParseJson(imageJson("00ff"), parseStr);
    check(ok && t.GetSize() == 2 && *t.GetData(0) == 0x00 && *t.GetData(1) == 0xff,
          tag + " ParseJson loads ImageData \"00ff\" as two bytes");
  }

  // JSON writer: a zero-byte image is refused and nothing is written.
  {
    JsonTag t;
    t.SetSize(0);
    IccJson j = IccJson::object();
    bool ok = t.ToJson(j);
    check(!ok && j.empty(),
          tag + " ToJson refuses a zero-byte image and writes no fields");
  }

  // JSON writer control: a one-byte image round-trips through the reader.
  {
    JsonTag t;
    std::string parseStr;
    IccJson j = IccJson::object();
    JsonTag back;
    bool ok = t.ParseJson(imageJson("8a"), parseStr) && t.ToJson(j) &&
              back.ParseJson(j, parseStr);
    check(ok && back.GetSize() == 1 && *back.GetData(0) == 0x8a,
          tag + " ToJson writes a one-byte image that ParseJson reads back");
  }

  // Binary writer: a zero-byte image is refused before any field is written.
  {
    BaseTag t;
    t.SetSize(0);
    CIccMemIO io;
    bool ok = io.Alloc(256, true) && t.Write(&io);
    check(!ok && io.Tell() == 0,
          tag + " Write refuses a zero-byte image and writes nothing");
  }

  // Binary writer control: a one-byte image writes nFixed + 1 bytes and reads.
  {
    BaseTag t;
    *t.GetData(0) = 0x8a;
    BaseTag back;
    icUInt32Number nWritten;
    bool ok = binaryRoundTrip(&t, &back, nWritten);
    check(ok && nWritten == nFixed + 1 && back.GetSize() == 1 &&
          *back.GetData(0) == 0x8a,
          tag + " Write saves a one-byte image that Read loads back");
  }

  // Constructor: a size below 1 gives one allocated zero byte that writes.
  for (int nSize : { 0, -1 }) {
    BaseTag t(nSize);
    BaseTag back;
    icUInt32Number nWritten;
    bool ok = t.GetSize() == 1 && binaryRoundTrip(&t, &back, nWritten);
    check(ok && nWritten == nFixed + 1 && *back.GetData(0) == 0,
          tag + " constructed with size " + std::to_string(nSize) +
          " holds one zero byte that writes and reads back");
  }
}

int main()
{
  imageCases<CIccTagJsonEmbeddedHeightImage, CIccTagEmbeddedHeightImage>(
    "embeddedHeightImageType", 24);
  imageCases<CIccTagJsonEmbeddedNormalImage, CIccTagEmbeddedNormalImage>(
    "embeddedNormalImageType", 16);

  // CIccTagData had the same constructor line.
  for (int nSize : { 0, -1 }) {
    CIccTagData t(nSize);
    CIccTagData back;
    icUInt32Number nWritten;
    bool ok = t.GetSize() == 1 && binaryRoundTrip(&t, &back, nWritten);
    check(ok && back.GetSize() == 1 && *back.GetData(0) == 0,
          "dataType constructed with size " + std::to_string(nSize) +
          " holds one zero byte that writes and reads back");
  }

  if (g_fail) {
    std::fprintf(stderr, "%s: %d case(s) failed\n", kName, g_fail);
    return 1;
  }

  std::fprintf(stdout, "%s: all cases passed\n", kName);
  return 0;
}
