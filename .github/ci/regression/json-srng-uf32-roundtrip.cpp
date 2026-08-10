// Regression for #2085: the JSON tag factory did not register spectralRangeType
// ('srng') or u16Fixed16ArrayType ('uf32'), the two ICC.2 tag types that
// IccProfLib and the XML factory both already carry.
//
// Mechanism. CIccTagJsonFactory::CreateTag's default arm returns a
// CIccTagJsonUnknown rather than NULL, so once iccToJson/iccFromJson push the
// factory it answers for *every* signature and nothing falls through to
// CIccTagFactory. A missing case is therefore not a fall-back to the correct
// tag -- it is a silent downgrade to Unknown, which serializes the tag as an
// opaque byte blob and cannot parse one back. The XML side registers both
// (IccTagXmlFactory.cpp: icSigU16Fixed16ArrayType, icSigSpectralRangeType), so
// the same profile round-trips through XML and is lossy through JSON.
//
// 'uf32' had a second, independent defect underneath the missing case. The
// typedef at IccTagJson.h:363 read
//     typedef CIccTagFixedNum<icU16Fixed16Number, icSigU16Fixed16ArrayType> CIccTagJsonU16Fixed16;
// -- the IccProfLib base template, not CIccTagJsonFixedNum -- so the alias was a
// byte-for-byte copy of IccTagBasic.h:1070's CIccTagU16Fixed16 with only the name
// changed, carrying no ToJson/ParseJson and no GetExtension() at all. That the
// slip was unintended is settled by IccTagJson.cpp:1259-1260, where
// CIccTagJsonFixedNum<>::GetClassName already returns "CIccTagJsonU16Fixed16"
// for the non-S15 instantiation -- a branch that was dead code, because the only
// explicit instantiation was the S15 one.
//
// Fixing the typedef alone is not enough, and that is what the value assertions
// below exist to catch. CIccTagJsonFixedNum::ToJson/ParseJson hard-coded
// icFtoD/icDtoF, the *signed* s15Fixed16 helpers. The XML mirror
// CIccTagXmlFixedNum branches on Tsig in both directions and uses icUFtoD/icDtoUF
// for the unsigned type (IccTagXml.cpp:1723,1757). Without that branch a
// u16Fixed16 tag registers and then serializes through the wrong conversion, and
// the two directions fail at different thresholds -- measured, not inferred:
//   * writing:  values >= 32768 have the top bit of their raw word set, so
//               icFtoD reads them as negative. 32768.0 is emitted as -32768.0.
//   * reading:  icDtoF clamps its input at 32767.0 (IccUtil.cpp:569), so any
//               value above that is truncated. 32767.5 reads back as 32767.0.
// The reader therefore breaks lower than the writer does. A test that only
// checked values above 32768 would still have covered both, but would have
// misattributed the reader's boundary.
//
// Red-green, measured:
//   * against master a62b3568, all six cases fail at registration: the factory
//     yields CIccTagJsonUnknown for both signatures, so there is no typed tag to
//     round-trip. Exit status 6.
//   * with the typedef and the factory cases applied but the ToJson branch
//     reverted, 32768.0 is emitted as -32768.0 and round-trips to 0.0.
//   * with only the ParseJson branch reverted, 32767.5 round-trips to 32767.0,
//     and it fails in both the full-range and small-value cases.
// The small-value case is what stops a conversion fix applied in the wrong
// direction (icUFtoD everywhere, breaking S15, or a branch inverted) from passing
// by trading one half of the range for the other.
//
// srng's binary/library-level contract is already covered by
// extended-device-colorspace.cpp (#626); this test covers only the JSON layer,
// which that one explicitly leaves out.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagJson.h"
#include "IccTagJsonFactory.h"
#include "IccTagBasic.h"
#include "IccTagFactory.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[json-srng-uf32] FAIL: %s\n", what);
  }
}

// Reach the JSON serializer the way iccToJson does: through the extension
// interface, not by naming a concrete Json class. Keeping the test to this API is
// what lets it compile unchanged against a tree that has no CIccTagJsonSpectralRange.
CIccTagJson *jsonExt(CIccTag *pTag)
{
  if (!pTag)
    return NULL;
  return dynamic_cast<CIccTagJson *>(pTag->GetExtension());
}

// ---------------------------------------------------------------------------
// u16Fixed16ArrayType ('uf32')
// ---------------------------------------------------------------------------

// Values chosen as exact multiples of 1/65536 that are ALSO exactly representable
// as float, so the round trip is lossless and any mismatch is a conversion defect
// rather than rounding. The float constraint is real and not merely belt-and-braces:
// icUFtoD returns icFloatNumber, which is float (IccDefs.h:88), so it carries 24
// mantissa bits where a u16Fixed16 word needs up to 32. Exactness therefore holds
// only below 256.0 -- measured, raw word 0x01000001 emits as 256.0 and reads back
// 0x01000000, while 0x00FF0001 survives. Values above 256.0 are used below anyway
// because they are what isolates the signed/unsigned defect, and they are chosen on
// 1/65536 boundaries coarse enough for float. The precision limit is shared with
// s15Fixed16 and the XML path and is noted in #2095; this test does not pin it. The
// last
// three sit above 32768, the region s15Fixed16 cannot hold -- they are the ones
// that separate a correct icUFtoD/icDtoUF path from the hard-coded signed one.
// Against the signed helpers these read back as -32768, a negative, and -1.0
// respectively, because their raw words all have the top bit set.
//
// The table stops at 65535.0 rather than the type's true ceiling of 65535.99998
// because icDtoUF (IccUtil.cpp:592) clamps its input at 65535.0, so the last
// 1/65536 of the range is unreachable through the library's own encoder. That is
// a separate question from this one -- filed as #2095 -- it predates #2085, and it
// applies equally to the XML path, so this test pins what the library can actually
// represent rather than asserting a bound it would have to change to meet.
const double kU16Values[] = { 0.0, 0.5, 1.0, 32767.5, 32768.0, 49152.25, 65535.0 };
const icUInt32Number kU16Count = (icUInt32Number)(sizeof(kU16Values) / sizeof(kU16Values[0]));

// Case 1: the factory must build a real u16Fixed16 tag carrying a JSON extension.
// Against master this is the assertion that fails first, and it fails because the
// switch has no case rather than because the type is unsupported by the library.
CIccTag *createU16Tag(const char *what)
{
  CIccTag *pTag = CIccTagCreator::CreateTag(icSigU16Fixed16ArrayType);
  check(pTag != NULL, "uf32: factory returned nothing");
  if (!pTag)
    return NULL;

  check(pTag->GetType() == icSigU16Fixed16ArrayType, "uf32: GetType() is not icSigU16Fixed16ArrayType");
  check(dynamic_cast<CIccTagU16Fixed16 *>(pTag) != NULL, what);
  check(jsonExt(pTag) != NULL, "uf32: tag carries no JSON extension");
  return pTag;
}

void u16IsRegistered()
{
  CIccTag *pTag = createU16Tag("uf32: factory tag is not a CIccTagU16Fixed16");
  if (pTag)
    delete pTag;
}

// Shared body for cases 2 and 3: write nValues of the table through ToJson, read
// them back into a second tag through ParseJson, and require exact equality.
// Comparing tag-to-tag rather than tag-to-literal keeps the assertion on the
// round trip itself; the emitted values are checked separately below.
void u16RoundTrip(icUInt32Number nValues, const char *label)
{
  CIccTag *pSrcTag = createU16Tag("uf32: factory tag is not a CIccTagU16Fixed16");
  if (!pSrcTag)
    return;

  CIccTagU16Fixed16 *pSrc = dynamic_cast<CIccTagU16Fixed16 *>(pSrcTag);
  CIccTagJson *pSrcJson = jsonExt(pSrcTag);
  if (!pSrc || !pSrcJson) {
    delete pSrcTag;
    return;
  }

  if (!pSrc->SetSize(nValues)) {
    check(false, "uf32: SetSize failed");
    delete pSrcTag;
    return;
  }
  for (icUInt32Number i = 0; i < nValues; i++)
    (*pSrc)[i] = icDtoUF((icFloatNumber)kU16Values[i]);

  IccJson j;
  check(pSrcJson->ToJson(j), "uf32: ToJson rejected the tag");
  check(j.contains("values") && j["values"].is_array(), "uf32: no values array emitted");

  if (j.contains("values") && j["values"].is_array()) {
    check(j["values"].size() == nValues, "uf32: wrong element count emitted");

    // The emitted numbers must be the values the tag holds. This is the assertion
    // that the signed helper fails on: icFtoD(65535.5's raw word) reads -0.5.
    if (j["values"].size() == nValues) {
      for (icUInt32Number i = 0; i < nValues; i++) {
        double got = j["values"][i].get<double>();
        if (std::fabs(got - kU16Values[i]) > 1e-9) {
          std::fprintf(stderr, "[json-srng-uf32]   %s: index %u emitted %.6f, expected %.6f\n",
                       label, (unsigned)i, got, kU16Values[i]);
          check(false, "uf32: emitted value does not match the tag");
          break;
        }
      }
    }
  }

  CIccTag *pDstTag = CIccTagCreator::CreateTag(icSigU16Fixed16ArrayType);
  CIccTagU16Fixed16 *pDst = dynamic_cast<CIccTagU16Fixed16 *>(pDstTag);
  CIccTagJson *pDstJson = jsonExt(pDstTag);
  if (!pDst || !pDstJson) {
    check(false, "uf32: could not create a destination tag");
    delete pSrcTag;
    delete pDstTag;
    return;
  }

  std::string parseStr;
  check(pDstJson->ParseJson(j, parseStr), "uf32: ParseJson rejected its own output");
  check(pDst->GetSize() == nValues, "uf32: round trip changed the element count");

  if (pDst->GetSize() == nValues) {
    for (icUInt32Number i = 0; i < nValues; i++) {
      // Raw-word equality: these are exact multiples of 1/65536, so a lossless
      // round trip reproduces the stored word bit for bit. icDtoF's clamp at the
      // s15Fixed16 ceiling shows up here as a changed word, not as a near miss.
      if ((*pDst)[i] != (*pSrc)[i]) {
        std::fprintf(stderr, "[json-srng-uf32]   %s: index %u round-tripped %.6f, expected %.6f\n",
                     label, (unsigned)i, (double)icUFtoD((*pDst)[i]), (double)icUFtoD((*pSrc)[i]));
        check(false, "uf32: round trip did not preserve the value");
        break;
      }
    }
  }

  delete pSrcTag;
  delete pDstTag;
}

// Case 2: the full table, including the values above 32768.
void u16FullRangeRoundTrips()
{
  u16RoundTrip(kU16Count, "full range");
}

// Case 3: the first four values only -- everything s15Fixed16 could also have
// carried. Three of them are genuinely below both defect thresholds and must stay
// green through any conversion change, which is what an inverted branch would
// break. The fourth, 32767.5, sits between the reader's clamp at 32767.0 and the
// writer's sign flip at 32768.0, so this case also isolates the reader half on its
// own: with only ParseJson reverted, it is the case that still fails.
void u16SmallValuesRoundTrip()
{
  u16RoundTrip(4, "small values");
}

// ---------------------------------------------------------------------------
// spectralRangeType ('srng')
// ---------------------------------------------------------------------------

// A 380..730nm visible range at 10nm, and a bi-spectral range that differs from it
// in all three fields so a serializer that emitted one where the other belongs
// cannot pass.
const float kSpecStart = 380.0f, kSpecEnd = 730.0f;
const icUInt16Number kSpecSteps = 36;
const float kBiStart = 400.0f, kBiEnd = 700.0f;
const icUInt16Number kBiSteps = 31;

CIccTag *createSrngTag()
{
  CIccTag *pTag = CIccTagCreator::CreateTag(icSigSpectralRangeType);
  check(pTag != NULL, "srng: factory returned nothing");
  if (!pTag)
    return NULL;

  check(pTag->GetType() == icSigSpectralRangeType, "srng: GetType() is not icSigSpectralRangeType");
  check(dynamic_cast<CIccTagSpectralRange *>(pTag) != NULL, "srng: factory tag is not a CIccTagSpectralRange");
  check(jsonExt(pTag) != NULL, "srng: tag carries no JSON extension");
  return pTag;
}

// Case 4: registration. Same shape as case 1 -- against master the factory hands
// back CIccTagJsonUnknown even though CIccTagFactory has built srng since #626.
void srngIsRegistered()
{
  CIccTag *pTag = createSrngTag();
  if (pTag)
    delete pTag;
}

// Shared body for cases 5 and 6. biSteps == 0 exercises the absent bi-spectral
// range, which the XML writer omits entirely rather than emitting as zeros.
void srngRoundTrip(icUInt16Number biSteps, const char *label)
{
  CIccTag *pSrcTag = createSrngTag();
  if (!pSrcTag)
    return;

  CIccTagSpectralRange *pSrc = dynamic_cast<CIccTagSpectralRange *>(pSrcTag);
  CIccTagJson *pSrcJson = jsonExt(pSrcTag);
  if (!pSrc || !pSrcJson) {
    delete pSrcTag;
    return;
  }

  pSrc->m_spectralRange.start = icFtoF16(kSpecStart);
  pSrc->m_spectralRange.end   = icFtoF16(kSpecEnd);
  pSrc->m_spectralRange.steps = kSpecSteps;
  pSrc->m_biSpectralRange.start = icFtoF16(kBiStart);
  pSrc->m_biSpectralRange.end   = icFtoF16(kBiEnd);
  pSrc->m_biSpectralRange.steps = biSteps;

  IccJson j;
  check(pSrcJson->ToJson(j), "srng: ToJson rejected the tag");

  CIccTag *pDstTag = CIccTagCreator::CreateTag(icSigSpectralRangeType);
  CIccTagSpectralRange *pDst = dynamic_cast<CIccTagSpectralRange *>(pDstTag);
  CIccTagJson *pDstJson = jsonExt(pDstTag);
  if (!pDst || !pDstJson) {
    check(false, "srng: could not create a destination tag");
    delete pSrcTag;
    delete pDstTag;
    return;
  }

  std::string parseStr;
  check(pDstJson->ParseJson(j, parseStr), "srng: ParseJson rejected its own output");

  // icFloat16Number words compare exactly: the constants above are chosen to be
  // representable, so a half-float conversion applied twice or skipped shows up.
  check(pDst->m_spectralRange.start == pSrc->m_spectralRange.start, "srng: spectral start not preserved");
  check(pDst->m_spectralRange.end   == pSrc->m_spectralRange.end,   "srng: spectral end not preserved");
  check(pDst->m_spectralRange.steps == pSrc->m_spectralRange.steps, "srng: spectral steps not preserved");

  // All three bi-spectral fields must survive whatever the step count is. Write()
  // emits all three unconditionally and Read() validates none, so a tag carrying
  // start/end beside steps == 0 is readable-but-non-conformant, and the writer must
  // not quietly conform it by dropping them -- before this type had a JSON class it
  // fell to CIccTagJsonUnknown, whose hex blob preserved them exactly.
  check(pDst->m_biSpectralRange.start == pSrc->m_biSpectralRange.start, "srng: bi-spectral start not preserved");
  check(pDst->m_biSpectralRange.end   == pSrc->m_biSpectralRange.end,   "srng: bi-spectral end not preserved");
  check(pDst->m_biSpectralRange.steps == pSrc->m_biSpectralRange.steps, "srng: bi-spectral steps not preserved");

  std::fprintf(stdout, "[json-srng-uf32] srng %s round trip checked\n", label);

  delete pSrcTag;
  delete pDstTag;
}

// Case 5: both ranges populated.
void srngBothRangesRoundTrip()
{
  srngRoundTrip(kBiSteps, "with bi-spectral");
}

// Case 6: zero step count but a populated start/end -- the non-conformant tag the
// writer must not normalise. Keying the BiSpectralRange omission on steps alone
// passes every other case here and fails only this one.
void srngNoBiSpectralRoundTrip()
{
  srngRoundTrip(0, "zero-step bi-spectral");
}

// Case 7: a genuinely empty bi-spectral range -- all three fields zero. This is the
// common non-bi-spectral profile, and it is the case that must still omit the key
// rather than emit zeros, so the fix for case 6 cannot be "always emit".
void srngEmptyBiSpectralIsOmitted()
{
  CIccTag *pTag = createSrngTag();
  if (!pTag)
    return;

  CIccTagSpectralRange *pSrc = dynamic_cast<CIccTagSpectralRange *>(pTag);
  CIccTagJson *pJson = jsonExt(pTag);
  if (!pSrc || !pJson) {
    delete pTag;
    return;
  }

  pSrc->m_spectralRange.start = icFtoF16(kSpecStart);
  pSrc->m_spectralRange.end   = icFtoF16(kSpecEnd);
  pSrc->m_spectralRange.steps = kSpecSteps;
  pSrc->m_biSpectralRange.start = 0;
  pSrc->m_biSpectralRange.end   = 0;
  pSrc->m_biSpectralRange.steps = 0;

  IccJson j;
  check(pJson->ToJson(j), "srng: ToJson rejected an empty bi-spectral range");
  check(j.contains("SpectralRange"), "srng: SpectralRange key missing");
  check(!j.contains("BiSpectralRange"), "srng: empty bi-spectral range was emitted anyway");

  delete pTag;
}

// Case 8: a malformed steps value must be reported, not silently accepted as an
// empty range. jGetValue leaves its out-param untouched and returns false when the
// field is present but unholdable -- a string, or an integer wider than int -- so a
// reader that discards that result sees the initialised 0, passes its own range
// guard, and writes a degenerate range into the profile. Each of these fails only
// if the jGetValue result is checked.
void srngMalformedFieldsAreRejected()
{
  struct Case { const char *json; const char *what; };
  const Case cases[] = {
    { "{\"SpectralRange\":{\"start\":380,\"end\":730,\"steps\":4294967295}}",
      "srng: an out-of-int-range steps was accepted" },
    { "{\"SpectralRange\":{\"start\":380,\"end\":730,\"steps\":\"36\"}}",
      "srng: a string steps was accepted" },
    { "{\"SpectralRange\":{\"start\":\"380\",\"end\":730,\"steps\":36}}",
      "srng: a string start was accepted" },
    { "{\"SpectralRange\":{\"start\":380,\"end\":null,\"steps\":36}}",
      "srng: a null end was accepted" },
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    CIccTag *pTag = createSrngTag();
    CIccTagJson *pJson = jsonExt(pTag);
    if (!pTag || !pJson) {
      delete pTag;
      return;
    }

    IccJson j = IccJson::parse(cases[i].json, NULL, false);
    std::string parseStr;
    check(!pJson->ParseJson(j, parseStr), cases[i].what);
    check(!parseStr.empty(), "srng: a rejected document produced no diagnostic");

    delete pTag;
  }
}

} // namespace

int main()
{
  // The JSON factory must be pushed before any CreateTag call, exactly as
  // iccToJson/iccFromJson do it -- without it CIccTagCreator builds plain
  // IccProfLib tags with no extension and every ToJson assertion below is
  // vacuous rather than failing.
  CIccTagCreator::PushFactory(new (std::nothrow) CIccTagJsonFactory());

  u16IsRegistered();
  u16FullRangeRoundTrips();
  u16SmallValuesRoundTrip();

  srngIsRegistered();
  srngBothRangesRoundTrip();
  srngNoBiSpectralRoundTrip();
  srngEmptyBiSpectralIsOmitted();
  srngMalformedFieldsAreRejected();

  if (g_fail)
    std::fprintf(stderr, "[json-srng-uf32] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[json-srng-uf32] all assertions passed\n");

  return g_fail;
}
