// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: two defects in the ACS multi-process elements (#2181).
//
// (1) CIccMpeAcs::Describe() chose its label from the ACS DATA SIGNATURE rather
//     than from the element type:
//
//       if (GetBAcsSig())
//         sDescription += "ELEM_bACS\n";
//       else
//         sDescription += "ELEM_eACS\n";
//
//     GetBAcsSig() returns m_signature on a bACS element and the base class's
//     icSigAcsZero on everything else, so the test conflates "is a bACS element"
//     with "has a non-zero signature". Those coincide for every bACS element
//     EXCEPT one carrying icSigAcsZero -- a legitimate, explicitly named value
//     that CIccMpeXmlBAcs::ToXml deliberately preserves (#1843) -- which then
//     described itself as ELEM_eACS.
//
//     The reported reproducer went through XML: a <BAcsElement Signature="">
//     dumped as ELEM_eACS. The written profile is not at fault, which is worth
//     recording because it narrows the fix. Reading the bytes of the file
//     iccFromXml produces, the element type signatures are 'bACS' and 'eACS' in
//     the right order and the right places; only Describe() disagrees. So this
//     test drives Describe() directly rather than shelling out to two tools --
//     it is the actual defect site, it is deterministic, and unlike a script
//     test it is registered on Windows.
//
// (2) CIccMpeAcs::AllocData() did not reset m_nDataSize when malloc failed:
//
//       free(m_pData);
//       if (size) {
//         m_pData = (icUInt8Number*)malloc(size);
//         if (m_pData)
//           m_nDataSize = size;      // <-- only on success
//       }
//
//     leaving m_pData NULL beside a stale non-zero m_nDataSize. That pair is
//     reachable as a NULL read, because all four copy paths guard the memcpy on
//     the DESTINATION pointer and the SOURCE size:
//
//       AllocData(elemAcs.m_nDataSize);
//       if (m_pData && elemAcs.m_nDataSize)
//         memcpy(m_pData, elemAcs.m_pData, m_nDataSize);
//
//     Copying an element in that state allocates fine from the stale size, sees
//     a non-zero source size, and memcpy()s from a NULL source. NewCopy() is one
//     of those four paths.
//
//     Forcing the failure needs no allocator interposition: a request of
//     SIZE_MAX/2 is refused outright by every malloc this builds against, which
//     makes the red/green deterministic rather than OOM-dependent. Case 4 then
//     performs the copy that would have dereferenced NULL, so this test is a
//     crash probe under ASan as well as a value assertion.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccMpeACS.h"
#include "IccMpeXml.h"
#include "IccTagMPE.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

static int g_failures = 0;

static void check(bool bOk, const char *szCase, const char *szDetail)
{
  printf("  %-46s %s%s%s\n", szCase, bOk ? "PASS" : "FAIL",
         bOk ? "" : "  -- ", bOk ? "" : szDetail);
  if (!bOk)
    g_failures++;
}

// Reported, never counted. Used for the one case whose premise is an allocator
// property rather than a property of this library: see case 4.
static void skip(const char *szCase, const char *szWhy)
{
  printf("  %-46s SKIP  -- %s\n", szCase, szWhy);
}

// Run ParseXml over a literal document, following the shape
// mpexml-unknown-reserved.cpp already uses for this.
static bool parseFragment(CIccMpeXmlBAcs &elem, const char *szDoc,
                          std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(szDoc, (int)strlen(szDoc), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc) {
    fprintf(stderr, "[mpe-acs-zero-signature] fixture is not valid XML\n");
    return false;
  }
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool rv = pRoot && elem.ParseXml(pRoot, parseStr);
  xmlFreeDoc(pDoc);
  return rv;
}

// Describe() emits the label first, so a prefix test is exact and does not
// depend on the "  Signature = ..." line that follows it.
static bool describesAs(CIccMultiProcessElement &elem, const char *szExpect)
{
  std::string sDesc;
  elem.Describe(sDesc, 100);
  return sDesc.compare(0, strlen(szExpect), szExpect) == 0;
}

int main(void)
{
  printf("#2181 ACS element regression\n");

  // --- 1. the reported defect: a bACS element whose signature is icSigAcsZero.
  // Pre-fix this printed ELEM_eACS.
  {
    CIccMpeBAcs elem(3, icSigAcsZero);
    check(describesAs(elem, "ELEM_bACS"),
          "bACS with icSigAcsZero describes as bACS",
          "described itself as an eACS element");
  }

  // --- 2. the case that already worked, kept as the positive control. Without
  // it a Describe() hard-wired to "ELEM_bACS" would satisfy case 1 and prove
  // nothing.
  {
    CIccMpeEAcs elem(3, icSigAcsZero);
    check(describesAs(elem, "ELEM_eACS"),
          "eACS with icSigAcsZero describes as eACS",
          "described itself as a bACS element");
  }

  // --- 3. and the non-zero signatures, so the fix cannot have inverted the
  // discrimination it replaced. These are the two the reporter's XML used.
  {
    CIccMpeBAcs bElem(3, (icAcsSignature)0x62414353 /* 'bACS' */);
    CIccMpeEAcs eElem(3, (icAcsSignature)0x65414353 /* 'eACS' */);
    check(describesAs(bElem, "ELEM_bACS"),
          "bACS with a non-zero signature", "regressed");
    check(describesAs(eElem, "ELEM_eACS"),
          "eACS with a non-zero signature", "regressed");
  }

  // --- 4. the AllocData invariant, and the copy that the broken pair made
  // dereference NULL.
  {
    CIccMpeBAcs elem(3, icSigAcsZero);

    check(elem.AllocData(16) && elem.GetData() != NULL && elem.GetDataSize() == 16,
          "AllocData(16) succeeds and records the size",
          "a plain allocation did not take");

    // SIZE_MAX/2 is refused outright by every 64-bit allocator this builds
    // against, which is what makes the failure deterministic instead of
    // OOM-dependent. It is NOT a property of this library, though, so a host
    // that honours the request skips the two cases below rather than failing
    // them -- on a 32-bit build SIZE_MAX/2 is 2 GB and could succeed, and an
    // environment property must not read as a regression in iccDEV.
    const size_t nRefused = ((size_t)-1) / 2;

    if (elem.AllocData(nRefused)) {
      skip("a failed AllocData leaves the element empty",
           "this host honoured a SIZE_MAX/2 request");
      skip("copying after a failed AllocData stays empty",
           "this host honoured a SIZE_MAX/2 request");
    }
    else {
      // The invariant. Pre-fix: GetData() is NULL but GetDataSize() is still 16.
      check(elem.GetData() == NULL && elem.GetDataSize() == 0,
            "a failed AllocData leaves the element empty",
            "pointer and size disagree after a failed allocation");

      // The consequence. Pre-fix this memcpy()s from a NULL source and the
      // process dies here, so this case is a crash probe as well as an
      // assertion -- measured: reverting only the AllocData hunk segfaults.
      CIccMpeBAcs copy(elem);
      check(copy.GetData() == NULL && copy.GetDataSize() == 0,
            "copying after a failed AllocData stays empty",
            "the copy claims data it does not have");
    }
  }

  // --- 5. ParseXml() must fully define the element it parses. The overloads
  // used to call AllocData() only inside "if (nSize)", so re-parsing an element
  // that already held data, from a node carrying none, kept the old buffer.
  // Every in-tree caller builds a fresh element per node, so this is the change
  // in this commit with no mainline path -- which is exactly why it needs a test
  // of its own rather than riding on the two above.
  {
    CIccMpeXmlBAcs elem;
    std::string parseStr;

    const bool bLoaded = parseFragment(elem,
      "<BAcsElement InputChannels=\"3\" OutputChannels=\"3\" Signature=\"\">"
      "00 01 7f ff</BAcsElement>", parseStr);
    check(bLoaded && elem.GetDataSize() == 4 && elem.GetData() != NULL,
          "ParseXml loads hex payload", "the fixture did not parse");

    // Same object, second parse, no hex this time.
    const bool bCleared = parseFragment(elem,
      "<BAcsElement InputChannels=\"3\" OutputChannels=\"3\" Signature=\"\"/>",
      parseStr);
    check(bCleared && elem.GetData() == NULL && elem.GetDataSize() == 0,
          "re-parsing from a node with no hex clears it",
          "the previous payload survived the re-parse");

    // And the element still knows what it is.
    check(describesAs(elem, "ELEM_bACS"),
          "an XML-parsed bACS still describes as bACS",
          "the XML subclass lost its type");
  }

  xmlCleanupParser();

  printf("%s (%d failure(s))\n", g_failures ? "FAILED" : "OK", g_failures);
  return g_failures ? 1 : 0;
}
