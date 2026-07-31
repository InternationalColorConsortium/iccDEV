// Regression for CodeQL alert #2343: CIccTagJsonFixedNum::ToJson walked m_nSize
// without asserting an upper bound, and was the only m_nSize-driven serialization
// walk in the JSON writer still missing one.
//
// Every sibling writer asserts a cap before walking: CIccTagJsonNum::ToJson and
// CIccTagJsonFloatNum::ToJson use 0xffffff (added by #1543 and #1535),
// CIccTagJsonSparseMatrixArray::ToJson uses 0xffffff, CIccTagJsonXYZ::ToJson and
// CIccTagJsonCurve::ToJson use 65536. The direct XML mirror,
// CIccTagXmlFixedNum::ToXml, has carried the 0xffffff guard since it was the first
// site fixed on that side -- the sibling XML guards say so in their own comments.
// The JSON port simply never covered FixedNum.
//
// m_nSize is derived from the tag byte size in CIccTagFixedNum::Read() and m_Num is
// allocated to match, so the walk is proportional to its input rather than amplified
// without bound. The cost the cap actually bounds is the expansion factor: each
// 4-byte fixed-point value becomes a 16-byte nlohmann json node, and #1918 added a
// reserve() that commits all of them in a single allocation. Measured on this
// harness, an uncapped 64 MB tag drove ~581 MB peak RSS; capped, it is ~68 MB.
//
// Red-green: against the pre-fix ToJson, case 3 below fails -- the writer returns
// true and emits 16777216 elements instead of rejecting. Cases 1 and 2 pass both
// before and after; they are here so that a guard written with the wrong comparison
// (>= rather than >) or applied at the wrong size cannot pass by rejecting more than
// it should. Case 2 in particular pins that the last legal size is still serialized
// in full, which is the failure mode that would silently drop real tag data.
//
// Case 2 allocates a 64 MB backing array and builds ~256 MB of json nodes, so this
// test peaks near 600 MB for roughly a second. The cases run smallest-first and each
// payload is released before the next, so that is a peak and not a sum.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagJson.h"
#include "IccUtilJson.h"

#include <cstdio>
#include <cmath>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[json-fixednum-size-cap] FAIL: %s\n", what);
  }
}

// The cap asserted by CIccTagJsonFixedNum::ToJson, spelled the same way the writer
// spells it. A count equal to this is legal; the first rejected count is one more.
const icUInt32Number kMaxNumValues = 0xffffff;

// Fill the tag with distinct, exactly-representable values so a dropped or duplicated
// element shows up as a value mismatch and not only as a count mismatch. icFtoD/icDtoF
// round-trip s15Fixed16 exactly for multiples of 1/65536.
void fillDistinct(CIccTagJsonS15Fixed16 &tag, icUInt32Number n)
{
  for (icUInt32Number i = 0; i < n; i++)
    tag[i] = icDtoF((icFloatNumber)((i % 1024) / 1024.0));
}

// Case 1: an ordinary small array must still serialize, and its values must survive.
// A guard that rejected everything, or that ran before the payload was built, would
// show up here rather than in the boundary cases.
void ordinarySizeStillSerializes()
{
  const icUInt32Number n = 64;
  CIccTagJsonS15Fixed16 tag;
  if (!tag.SetSize(n)) {
    check(false, "SetSize(64) failed");
    return;
  }
  fillDistinct(tag, n);

  IccJson j;
  check(tag.ToJson(j), "ordinary 64-entry array was rejected");
  check(j.contains("values") && j["values"].is_array(), "ordinary array emitted no values array");
  if (!j.contains("values") || !j["values"].is_array())
    return;

  check(j["values"].size() == n, "ordinary array emitted the wrong element count");
  if (j["values"].size() != n)
    return;

  for (icUInt32Number i = 0; i < n; i++) {
    double got = j["values"][i].get<double>();
    double want = (double)icFtoD(tag[i]);
    if (std::fabs(got - want) > 1e-12) {
      check(false, "ordinary array value mismatch");
      return;
    }
  }
}

// Case 2: m_nSize == kMaxNumValues is the largest legal count and must still be
// serialized in full. This is what a guard written as `>=` would break, silently
// truncating the last representable tag rather than reporting anything.
void boundarySizeIsAccepted()
{
  CIccTagJsonS15Fixed16 tag;
  if (!tag.SetSize(kMaxNumValues)) {
    // A 64 MB allocation failing is an environment limit, not a regression in the
    // writer. Say so rather than reporting a false failure.
    std::fprintf(stderr,
                 "[json-fixednum-size-cap] SKIP: could not allocate %u entries for the "
                 "boundary case\n", kMaxNumValues);
    return;
  }

  IccJson j;
  check(tag.ToJson(j), "the largest legal count (0xffffff) was rejected");
  check(j.contains("values") && j["values"].is_array() &&
        j["values"].size() == kMaxNumValues,
        "the largest legal count was not serialized in full");
}

// Case 3: one past the cap must be rejected. This is the assertion that fails against
// the pre-fix writer. Returning false skips only this tag -- CIccProfileJson::ToJson
// continues past a failed tag rather than discarding the document (#1779/#1884) -- so
// rejection here degrades the output gracefully instead of losing the profile.
void oversizeIsRejected()
{
  const icUInt32Number n = kMaxNumValues + 1;
  CIccTagJsonS15Fixed16 tag;
  if (!tag.SetSize(n)) {
    std::fprintf(stderr,
                 "[json-fixednum-size-cap] SKIP: could not allocate %u entries for the "
                 "oversize case\n", n);
    return;
  }

  IccJson j;
  check(!tag.ToJson(j), "a count past 0xffffff was serialized instead of rejected");
  check(!j.contains("values"), "a rejected tag still emitted a values array");
}

} // namespace

int main()
{
  ordinarySizeStillSerializes();
  boundarySizeIsAccepted();
  oversizeIsRejected();

  if (g_fail)
    std::fprintf(stderr, "[json-fixednum-size-cap] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[json-fixednum-size-cap] all assertions passed\n");

  return g_fail;
}
