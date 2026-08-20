// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: CIccTagChromaticity::Validate() read past the end of m_xy (#2094).
//
// The tag stores m_nChannels xy pairs. Validate() checked the count against the
// three that a colorant encoding is defined over:
//
//   if (m_nChannels!=3) {
//     sReport += " - Number of device channels must be three.\n";
//     rv = icMaxStatus(rv, icValidateCriticalError);
//   }
//
//   switch(m_nColorantType) {
//     case icColorantITU:
//       if ( (m_xy[0].x != ...) || (m_xy[1].x != ...) || (m_xy[2].x != ...) )
//
// -- and then walked into the comparison anyway. The count was reported, never
// acted on, so a one-channel tag was compared entry by entry against three
// fixed pairs: a 4-byte heap read one and two entries past an eight-byte
// allocation (CWE-125). Reproduced with a hand-written one-channel
// chromaticityTag through iccFromXml, ASan naming IccTagBasic.cpp:4563 reading
// 0 bytes after the 8-byte region allocated at IccTagBasic.cpp:4514.
//
// All three readers can produce that tag. Read() sizes the array from the tag's
// declared *size* -- nNum = (size - 12) / 8, with the declared channel count
// only required not to exceed it and then discarded by SetSize(nNum) -- so the
// 20-byte tag the repro above writes reads back as one channel;
// CIccTagXmlChromaticity::ParseXml() sizes it from the number of <Channel>
// elements; and CIccTagJsonChromaticity::ParseJson() from the length of the
// "channels" array, so {"colorantType":1,"channels":[[0.64,0.33]]} is the same
// shape again. No path had any coverage before this test: no chromaticityTag
// exists anywhere in the corpus -- not in the 208 tracked XML documents, the
// 105 tracked .icc files, or the 254 profiles CreateAllProfiles.sh generates
// under Testing/. The only in-tree mentions of the type are the name table in
// IccTagFactory.cpp and docs/icc-profile.schema.json.
//
// Two more defects in the same class fall out of the same reading, and are
// pinned below because they decide whether the switch above runs at all:
//
//   * the constructor clamped the channel *count* up to three but allocated the
//     caller's raw nSize, so CIccTagChromaticity(1) was a three-channel tag
//     over a one-entry array before a single byte was parsed;
//   * neither the copy constructor nor operator= copied m_nColorantType, and
//     the default constructor never initialised it, so a copied tag switched on
//     an indeterminate value (CWE-457) -- the #2000 defect, in another tag.
//
// What each level is worth, measured against the unfixed library rather than
// predicted. Clang 18 Debug, no sanitizers: levels 1, 2 and 7 fail (5 assertions
// -- both short tags report the encoding mismatch they could only have read off
// the end, and all three copy paths read the encoding back as 0, i.e. a copied
// ITU tag becomes a tag claiming no encoding and is never checked again -- that
// measurement is also why the fixture below is not 0). The same build with
// ASan aborts in level 1 instead, before any of that is reached, naming the
// 4-byte read 0 bytes past the 8-byte allocation.
//
// Levels 6 and 8 passed against the unfixed library in the uninstrumented
// build, and are recorded here as passing by luck rather than as coverage: an
// out-of-bounds *access* and an indeterminate read have no defined value to
// assert on, and this tag's storage happened to hold what was wanted. Level 6
// is real coverage under a sanitizer -- it is an out-of-bounds write, which
// ASan traps -- while level 8 is a statement of the intended contract that only
// a compiler-inserted check would catch. Neither is the whole case for its
// defect; the reasoning above is.
//
// Levels 9 to 13 cover the remaining #2106 punch-list item: Read() derived the
// channel count from the tag's *size* and discarded the count the tag declares.
// See the note above readHonoursTheDeclaredCount() for why that is this tag
// disagreeing with its sibling rather than a specification question. Measured
// the same way: against the unfixed library levels 9, 10 and 13 fail (5
// assertions -- both oversized elements report ten channels and re-emit ten on
// a Write() round trip, and the zero-channel element parses instead of being
// rejected), while 11 and 12 pass, so those two are controls rather than
// coverage. Level 13 is the only acceptance change in the set.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccTagBasic.h"
#include "IccIO.h"
#include "IccUtil.h"

#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;

static const char *kCountMsg = "Number of device channels must be three.";
static const char *kDataMsg  = "Chromaticity data does not match specification.";

// The three ITU-R BT.709 pairs Validate() compares against, in the tag's own
// u16Fixed16 encoding, so a conforming three-channel tag can be built here.
static const double kItu[3][2] = { {0.640, 0.330}, {0.300, 0.600}, {0.150, 0.060} };

static bool contains(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

static void expect(const char *szCase, bool bOk, const char *szWhat)
{
  if (bOk) {
    printf("ok   [%s]: %s\n", szCase, szWhat);
    return;
  }

  printf("FAIL [%s]: %s\n", szCase, szWhat);
  g_failures++;
}

// Same, for the levels below that must stop rather than go on to dereference
// what the failed step was supposed to have produced.
static bool expectTrue(const char *szCase, bool bOk, const char *szWhat)
{
  expect(szCase, bOk, szWhat);
  return bOk;
}

// Same, for the copy levels: what a lost m_nColorantType reads back as is
// whatever the allocation held, so print it rather than leave a bare FAIL.
static void expectColorant(const char *szCase, icUInt16Number nGot,
                           icUInt16Number nWant, const char *szWhat)
{
  if (nGot == nWant) {
    printf("ok   [%s]: %s\n", szCase, szWhat);
    return;
  }

  printf("FAIL [%s]: %s -- got %u, expected %u\n", szCase, szWhat,
         (unsigned)nGot, (unsigned)nWant);
  g_failures++;
}

// Builds a tag of nChannels entries carrying the ITU values for as many of them
// as it has room for. Anything the tag is too short to hold is simply absent --
// that is the shape the defect walked off the end of.
static void fillItu(CIccTagChromaticity &tag, icUInt16Number nChannels,
                    icUInt16Number nColorantType)
{
  tag.SetSize(nChannels);
  tag.m_nColorantType = nColorantType;

  for (icUInt32Number i = 0; i < nChannels && i < 3; i++) {
    tag[i].x = icDtoUF((icFloatNumber)kItu[i][0]);
    tag[i].y = icDtoUF((icFloatNumber)kItu[i][1]);
  }
}

// Levels 1 and 2 -- the defect itself, at one and at two channels. One channel
// puts m_xy[1] and m_xy[2] out of bounds, two channels only m_xy[2], so the
// second case also shows the guard is a count check and not an is-it-empty
// check. The tag carries the correct ITU values for the entries it does have,
// which is what makes the assertion sharp: the only way the comparison can
// report a mismatch is by reading the entries the tag does not have.
static void shortTagStopsAtTheCountError(icUInt16Number nChannels)
{
  char szCase[64];
  snprintf(szCase, sizeof(szCase), "validate-%u-channel", (unsigned)nChannels);

  CIccTagChromaticity tag;
  fillItu(tag, nChannels, icColorantITU);

  std::string report;
  icValidateStatus rv = tag.Validate(icGetSigPath(icSigChromaticityTag), report, NULL);

  expect(szCase, contains(report, kCountMsg),
         "the channel count is reported as a critical error");
  expect(szCase, rv == icValidateCriticalError,
         "validation status is icValidateCriticalError");
  expect(szCase, !contains(report, kDataMsg),
         "the colorant-encoding comparison did not run past the end of the array");
}

// -- Levels 9 to 12: the declared channel count, on the binary read path. ------

static void put32(std::vector<icUInt8Number> &b, icUInt32Number v)
{
  b.push_back((icUInt8Number)(v >> 24));
  b.push_back((icUInt8Number)(v >> 16));
  b.push_back((icUInt8Number)(v >> 8));
  b.push_back((icUInt8Number)(v));
}

static void put16(std::vector<icUInt8Number> &b, icUInt16Number v)
{
  b.push_back((icUInt8Number)(v >> 8));
  b.push_back((icUInt8Number)(v));
}

// A chromaticityType tag declaring nDeclared channels while carrying nPairs xy
// pairs, plus nPad trailing bytes. The three are independent here precisely so
// the count, the payload and the padding can be made to disagree.
static std::vector<icUInt8Number> buildChrmTag(icUInt16Number nDeclared,
                                               icUInt16Number nColorantType,
                                               unsigned nPairs, unsigned nPad)
{
  std::vector<icUInt8Number> b;
  put32(b, 0x6368726DU);          // 'chrm'
  put32(b, 0);                    // reserved
  put16(b, nDeclared);
  put16(b, nColorantType);

  for (unsigned i = 0; i < nPairs; i++) {
    double x = i < 3 ? kItu[i][0] : 0.5;
    double y = i < 3 ? kItu[i][1] : 0.5;
    put32(b, icDtoUF((icFloatNumber)x));
    put32(b, icDtoUF((icFloatNumber)y));
  }

  for (unsigned i = 0; i < nPad; i++)
    b.push_back(0);

  return b;
}

// The count a Write() round trip puts back on the wire, read straight out of
// the serialised bytes at offset 8 rather than off the object, since the point
// of these levels is what a profile carries after iccDEV has handled it.
static bool writtenChannelCount(CIccTagChromaticity &tag, icUInt16Number &nOut)
{
  CIccMemIO out;
  if (!out.Alloc(12 + 8 * (size_t)tag.GetSize() + 16, true))
    return false;

  if (!tag.Write(&out))
    return false;

  icUInt8Number *p = out.GetData();
  nOut = (icUInt16Number)((p[8] << 8) | p[9]);
  return true;
}

// Levels 9 to 12 -- Read() must size the tag from the count the tag declares,
// not from its byte length.
//
// chromaticityType carries both a declared channel count (bytes 8-9) and, in
// the tag table, a size. Read() cross-checked them the way its sibling does --
//
//   icUInt32Number nNum = (size - 12) / sizeof(icChromaticityNumber);
//   if (nNum < nChannels)
//     return false;
//
// -- and then sized the tag from nNum, the *size-derived* figure, discarding
// the declared nChannels it had just validated against. CIccTagNamedColor2::Read
// (IccTagBasic.cpp:3330) faces the identical situation and resolves it the other
// way: it computes a size-derived nCount, uses it only as a capacity guard
// (`if (nCount < nNum) return false;`), then calls SetSize(nNum, nCoords) on the
// declared count and reads exactly that many entries. Chromaticity performed the
// same guard and then took the other branch.
//
// The consequence is not confined to memory. Write() emits m_nChannels, so a tag
// declaring three channels inside a 92-byte element was not merely reported as
// ten channels -- it was *re-emitted* as ten, and any tool that round-trips the
// profile persisted the rewrite. Where no colorant encoding is claimed,
// Validate() returns icValidateOK with an empty report (level 5 above), so the
// mutation was silent end to end.
//
// Levels 11 and 12 are the controls that make the fix a narrowing and not a
// change of acceptance: an element too short for its declared count is still
// rejected, and one carrying trailing padding still parses. Padding cannot be
// mistaken for a channel in either direction -- 12 + 8n is a multiple of four
// for every n, so a conforming element never needs padding, and the floor
// division absorbs any run shorter than a whole eight-byte pair regardless.
static void readHonoursTheDeclaredCount(const char *szCase,
                                        icUInt16Number nDeclared,
                                        icUInt16Number nColorantType,
                                        unsigned nPairs, unsigned nPad,
                                        bool bExpectRead,
                                        const char *szRejectWhat = NULL)
{
  std::vector<icUInt8Number> bytes =
    buildChrmTag(nDeclared, nColorantType, nPairs, nPad);

  CIccMemIO io;
  if (!io.Attach(&bytes[0], bytes.size())) {
    expect(szCase, false, "the byte stream could be attached for reading");
    return;
  }

  CIccTagChromaticity tag;
  bool bRead = tag.Read((icUInt32Number)bytes.size(), &io);

  if (!bExpectRead) {
    expect(szCase, !bRead, szRejectWhat ? szRejectWhat : "the element is rejected");
    return;
  }

  if (!expectTrue(szCase, bRead, "the element parses"))
    return;

  expectColorant(szCase, (icUInt16Number)tag.GetSize(), nDeclared,
                 "the tag reports the channel count it declared");

  icUInt16Number nWritten = 0;
  if (expectTrue(szCase, writtenChannelCount(tag, nWritten),
                 "the tag serialises back out"))
    expectColorant(szCase, nWritten, nDeclared,
                   "and a Write() round trip re-emits the declared count");
}

int main()
{
  shortTagStopsAtTheCountError(1);
  shortTagStopsAtTheCountError(2);

  // Level 3 -- control. A conforming three-channel ITU tag must still pass, or
  // the fix above would have "worked" by disabling the encoding check.
  {
    CIccTagChromaticity tag;
    fillItu(tag, 3, icColorantITU);

    std::string report;
    icValidateStatus rv = tag.Validate(icGetSigPath(icSigChromaticityTag), report, NULL);

    expect("validate-3-channel-conforming", rv == icValidateOK,
           "a conforming ITU tag validates OK");
    expect("validate-3-channel-conforming", report.empty(),
           "and reports nothing");
  }

  // Level 4 -- control. The encoding comparison must still bite when the tag is
  // long enough to be compared and the values are wrong. Without this, level 3
  // alone would be satisfied by a Validate() that reports nothing at all.
  {
    CIccTagChromaticity tag;
    fillItu(tag, 3, icColorantITU);
    tag[2].x = icDtoUF((icFloatNumber)0.999);   // not an ITU primary

    std::string report;
    icValidateStatus rv = tag.Validate(icGetSigPath(icSigChromaticityTag), report, NULL);

    expect("validate-3-channel-wrong-values", contains(report, kDataMsg),
           "wrong chromaticity data is still reported as non-compliant");
    expect("validate-3-channel-wrong-values", rv == icValidateNonCompliant,
           "and the status is icValidateNonCompliant");
  }

  // Level 5 -- control, and a note on scope. The whole block is guarded by
  // `if (m_nColorantType)`, so a short tag with no colorant encoding produces
  // no message at all. That is unchanged here: the encoding is what defines the
  // three primaries being compared, and widening the count check to tags that
  // claim no encoding is a specification question, not part of this fix.
  {
    CIccTagChromaticity tag;
    fillItu(tag, 1, icColorantUnknown);

    std::string report;
    icValidateStatus rv = tag.Validate(icGetSigPath(icSigChromaticityTag), report, NULL);

    expect("validate-1-channel-no-colorant", rv == icValidateOK,
           "a short tag claiming no colorant encoding is left alone");
    expect("validate-1-channel-no-colorant", !contains(report, kCountMsg),
           "and reports no channel-count error");
  }

  // Level 6 -- the constructor invariant. CIccTagChromaticity(1) used to report
  // three channels over a one-entry allocation, so every reader that walks m_xy
  // by GetSize() -- Describe, Write, the copy constructor -- was already out of
  // bounds. Writing and reading back every entry the tag says it has is the
  // access that trips ASan; see the note at the top of the file about what this
  // level is and is not worth on an uninstrumented build.
  {
    CIccTagChromaticity tag(1);

    expect("ctor-below-minimum", tag.GetSize() == 3,
           "a below-minimum request still reports three channels");

    for (icUInt32Number i = 0; i < tag.GetSize(); i++) {
      tag[i].x = icDtoUF((icFloatNumber)kItu[i][0]);
      tag[i].y = icDtoUF((icFloatNumber)kItu[i][1]);
    }

    bool bRoundTrip = true;
    for (icUInt32Number i = 0; i < tag.GetSize(); i++) {
      if (tag[i].x != icDtoUF((icFloatNumber)kItu[i][0]) ||
          tag[i].y != icDtoUF((icFloatNumber)kItu[i][1]))
        bRoundTrip = false;
    }

    expect("ctor-below-minimum", bRoundTrip,
           "and all three entries are backed by storage that round-trips");
  }

  // Level 7 -- the colorant encoding must survive a copy. It is the value the
  // switch dispatches on and the value Write() emits; NewCopy() is the copy
  // constructor, and CIccProfile's copy constructor clones every tag through
  // NewCopy(), so an uncopied member reaches anything that duplicates a
  // profile. EBU is used rather than ITU so the expected value is neither 0 nor
  // 1: an allocation that happened to read back zero, or a garbage low byte,
  // cannot masquerade as a correct copy.
  {
    CIccTagChromaticity src;
    fillItu(src, 3, icColorantEBU);

    CIccTagChromaticity copied(src);
    expectColorant("copy-construct", copied.m_nColorantType, icColorantEBU,
                   "the copy constructor carries the colorant encoding across");

    CIccTag *pCopy = src.NewCopy();
    CIccTagChromaticity *pChrm = dynamic_cast<CIccTagChromaticity *>(pCopy);
    if (!pChrm) {
      expect("newcopy", false, "NewCopy returned a CIccTagChromaticity");
    }
    else {
      expectColorant("newcopy", pChrm->m_nColorantType, icColorantEBU,
                     "NewCopy carries the colorant encoding across");
    }
    delete pCopy;

    CIccTagChromaticity assigned;
    assigned = src;
    expectColorant("assign", assigned.m_nColorantType, icColorantEBU,
                   "operator= carries the colorant encoding across");
  }

  // Level 8 -- a default-constructed tag must not switch on an indeterminate
  // encoding. The XML parser only assigns m_nColorantType when a <Colorant>
  // element is present, so this is the value a document without one leaves
  // behind. Zero means "no encoding claimed", which is the safe reading.
  {
    CIccTagChromaticity tag;
    expectColorant("default-ctor", tag.m_nColorantType, icColorantUnknown,
                   "the colorant encoding defaults to none rather than being left unset");
  }

  // Level 9 -- the defect. Three channels declared, ten pairs' worth of element.
  readHonoursTheDeclaredCount("read-oversized-itu", 3, icColorantITU, 10, 0, true);

  // Level 10 -- the same element with no colorant encoding claimed, which is
  // where the rewrite is silent: level 5 shows Validate() says nothing here, so
  // nothing between reading and writing the profile reports the changed count.
  readHonoursTheDeclaredCount("read-oversized-no-colorant", 3, icColorantUnknown,
                              10, 0, true);

  // Level 11 -- control. An element carrying fewer pairs than it declares must
  // still be rejected outright; that capacity guard is what keeps the read of
  // m_xy in bounds and it is not what these levels change.
  readHonoursTheDeclaredCount("read-undersized", 10, icColorantITU, 3, 0, false,
                              "an element too short for its declared count is rejected");

  // Level 12 -- control. A conforming element that carries trailing padding must
  // still parse, and at its declared count, or the change would be a narrowing
  // of what iccDEV accepts rather than a correction of what it reports.
  readHonoursTheDeclaredCount("read-padded", 3, icColorantITU, 3, 4, true);

  // Level 13 -- the one shape that is now rejected on purpose. An element
  // declaring no channels used to take however many pairs its length implied,
  // which is the fabrication these levels exist to stop; a chromaticityType
  // carrying no chromaticity leaves the count nothing to mean. This level is
  // the record that the rejection is intended, since it is the only acceptance
  // change in the set and it does not follow from the capacity guard.
  readHonoursTheDeclaredCount("read-zero-declared", 0, icColorantITU, 3, 0, false,
                              "an element declaring no channels is rejected");

  if (g_failures) {
    printf("%d case(s) regressed\n", g_failures);
    return 1;
  }

  printf("all cases passed\n");
  return 0;
}
