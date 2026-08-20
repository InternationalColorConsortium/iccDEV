// Regression for #2119: CIccXformNDLut::Begin() checked the tag's output channel
// count against its colour space but never its input count, so Apply() read past
// the end of the caller's source pixel buffer.
//
// CIccXformNDLut::Apply() copies InputChannels() floats out of SrcPixel:
//
//   int nInput = (m_nNumInput > 16) ? 16 : m_nNumInput;
//   for (i=0; i<nInput; i++)
//     Pixel[i] = SrcPixel[i];            // IccCmm.cpp:7170
//
// The clamp there bounds the *destination* (Pixel is 16 wide); nothing bounded the
// source.  SrcPixel is sized from the profile's own colour space, not from the tag
// -- CIccApplyCmm::InitPixel() takes the max of GetNumSrcSamples()/GetNumDstSamples()
// across the chain, and the first xform in a chain is handed the application's own
// buffer -- so a tag declaring more input channels than its colour space has reads
// off the end of it.
//
// Begin() already rejected the mirror-image case on the output side:
//
//   icUInt16Number csOutChannels = icGetSpaceSamples( m_pTag->GetCsOutput() );
//   if (csOutChannels != m_pTag->OutputChannels())
//     return icCmmStatInvalidLut;
//
// with a comment saying it exists to "catch cases where the LUT does not match the
// colorspace given ... this avoids segfaults".  Only half of that was implemented.
//
// Only the ND path needs the check spelled out.  CIccXform3DLut::Begin() and
// CIccXform4DLut::Begin() pin InputChannels() to the 3 and 4 their Apply() reads,
// while the sites that construct an NDLut -- the default: arms of the switches on
// m_Header.colorSpace in both CIccXform::Create() overloads, plus
// CIccXformMpe::Create() -- serve every device space those two arms do not
// enumerate.  So the ND path is the one that takes a variable channel count from
// the tag, and it was the one not comparing that count to anything.
//
// The check compares InputChannels() against GetNumSrcSamples() rather than
// against the tag's own GetCsInput(), because GetNumSrcSamples() is what
// CIccApplyCmm::InitPixel() sizes the buffer from.  The two differ at the
// tag-explicit CIccXform::Create() overload, reached from the public
// CIccCmm::AddXform(CIccProfile*, CIccTag*, ...), where the caller supplies the
// tag and nothing ties its recorded colour spaces to the profile's header.
// testCallerSuppliedTagIsPinnedToTheProfile() below covers that path.
//
// Provenance, because the half-written check reads like an oversight and is not.
// 48a53197 ("Fix: SBO in CIccXform3DLut::Apply()", #655) added that output check
// to all three LUT xforms at once; 3DLut and 4DLut already pinned their input
// side in the guard above it, so the output half completed them, while NDLut's
// guard rejects only 3 and 4 and left the input side open.  Issue #523 reported a
// stack smash in this same CIccXformNDLut::Apply() in January, driven by
// nOutput=78 corrupting the local Pixel[16] -- the destination side -- and was
// closed invalid/wontfix/OOS as "caller pollutes the Library".  #655 landed six
// weeks later and, incidentally, closed that class: an AToBn tag's m_nOutput must
// now equal the PCS channel count.  #2119 is the remaining direction.
//
// This file is also, as far as a sweep of the tracked corpus can tell, the first
// coverage CIccXformNDLut::Begin() has ever had.  All tracked XML carries 9
// lut-type tags (7 lut16Type, 2 lut8Type) and none of them sits in an N-channel
// profile; the N-channel profiles that exist are MPE-based and route to
// CIccXformMpe instead, which bypasses the colour-space switch entirely.  So no
// corpus profile appears to reach this xform at all, which is both why a check
// could sit half-written since March and why the suite passing says nothing about
// this path -- the assertions below are the signal, not a green ctest run.
//
// The reported crash was an AFL mutation of a 13-channel profile: op:arith8 at
// pos:16 rewrote header byte 16, 'D' -> '7', turning 'DCLR' into '7CLR' while the
// A2B0 lut8 tag still declared 13 input channels.  ASAN put the read 0 bytes past a
// 28-byte region -- 28 bytes is exactly 7 floats -- i.e. at SrcPixel[7] of a buffer
// holding 0..6.  Nothing inside the tag was inconsistent; only the header and the
// tag disagreed, which is why no per-tag check caught it.
//
// The cases below build that shape directly rather than loading a fixture, so the
// test carries no corpus dependency and no oversized seed.  They assert Begin()
// rather than Apply(): a mismatched Apply() is the out-of-bounds read itself, which
// would report only under a sanitizer and would corrupt the run everywhere else.
// Begin() is the gate that is supposed to stop it, so that is what is pinned.
//
// Red-green: kMismatchOver and kMismatchUnder return icCmmStatOk without the fix and
// icCmmStatInvalidLut with it.  kConsistent returns icCmmStatOk either way and is the
// control -- it proves the fix rejects the header/tag disagreement rather than the
// N-channel path itself, which is the distinction a blunter guard would lose.
// Header + IccProfLib only.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTagLut.h"
#include "IccTagBasic.h"
#include "IccDefs.h"
#include "IccUtil.h"

#include <cstdio>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[ndlut-input-channels] FAIL: %s\n", what);
  }
}

// The PCS side is held fixed at 3-channel Lab across every case so the existing
// output-count check always passes; the only thing that varies is whether the tag's
// input count agrees with the device space.
const icUInt8Number kPcsChannels = 3;

// Build a curve of nEntries evenly-spaced points spanning 0..1.  CIccXformNDLut::Begin()
// rejects a NULL curve slot outright (the #1940 null-family hardening), so every slot
// the tag declares has to be filled or the control would fail for an unrelated reason.
CIccTagCurve *identityCurve(icUInt32Number nEntries)
{
  // Guards the division below; a single-entry curve is a gamma curve, not a ramp.
  if (nEntries < 2)
    return NULL;

  CIccTagCurve *pCurve = new CIccTagCurve();
  if (!pCurve)
    return NULL;

  // Free it here rather than leaking it to the caller's NULL check; the tag only
  // takes ownership of curve slots it is actually handed.
  if (!pCurve->SetSize(nEntries)) {
    delete pCurve;
    return NULL;
  }

  for (icUInt32Number i = 0; i < nEntries; i++)
    (*pCurve)[i] = (icFloatNumber)i / (icFloatNumber)(nEntries - 1);

  return pCurve;
}

// An input-class profile in colour space csDevice whose A2B0 lut8 tag declares
// nTagInput input channels.  When nTagInput disagrees with icGetSpaceSamples(csDevice)
// this is the malformed shape #2119 reports; when it agrees it is an ordinary
// N-channel profile.  csDevice is deliberately a signature that neither the 3DLut
// nor the 4DLut arm of CIccXform::Create() enumerates, so both cases route to
// CIccXformNDLut.
// csTagDeclaredInput overrides the input space stamped onto the tag; icSigUnknownData
// means "stamp csDevice", which is what CIccProfile::LoadTag() does for an AToBn tag.
// Passing anything else models a caller-supplied tag carrying a space of its own.
CIccProfile *buildNDLutProfile(icColorSpaceSignature csDevice, icUInt8Number nTagInput,
                               icColorSpaceSignature csTagDeclaredInput = icSigUnknownData)
{
  CIccProfile *pProfile = new CIccProfile();
  if (!pProfile)
    return NULL;

  pProfile->InitHeader();
  pProfile->m_Header.deviceClass = icSigInputClass;
  pProfile->m_Header.colorSpace = csDevice;
  pProfile->m_Header.pcs = icSigLabData;
  pProfile->m_Header.renderingIntent = icPerceptual;

  CIccTagLut8 *pLut = new CIccTagLut8();
  if (!pLut) {
    delete pProfile;
    return NULL;
  }

  pLut->Init(nTagInput, kPcsChannels);
  // Mirrors CIccProfile::LoadTag(), which calls
  // SetColorSpaces(m_Header.colorSpace, m_Header.pcs) for the AToBn tags -- this is
  // what populates the GetCsInput() the fixed Begin() compares against.
  pLut->SetColorSpaces(csTagDeclaredInput == icSigUnknownData ? csDevice : csTagDeclaredInput,
                       icSigLabData);

  // CIccMBB sizes its A and B curve arrays by IsInputB(), so ask the tag which side
  // carries how many rather than assuming the lut8 convention.
  LPIccCurve *pCurvesA = pLut->NewCurvesA();
  LPIccCurve *pCurvesB = pLut->NewCurvesB();
  if (!pCurvesA || !pCurvesB) {
    delete pLut;
    delete pProfile;
    return NULL;
  }

  const int nA = pLut->IsInputB() ? kPcsChannels : nTagInput;
  const int nB = pLut->IsInputB() ? nTagInput : kPcsChannels;
  for (int i = 0; i < nA; i++)
    pCurvesA[i] = identityCurve(2);
  for (int i = 0; i < nB; i++)
    pCurvesB[i] = identityCurve(2);

  // 2 grid points per input channel: the smallest table that still exercises the
  // interpolator, and it keeps the node count at 2^nTagInput rather than something
  // that would dominate the test's runtime at 13 channels.
  CIccCLUT *pCLUT = pLut->NewCLUT((icUInt8Number)2, pLut->GetPrecision());
  if (!pCLUT) {
    delete pLut;
    delete pProfile;
    return NULL;
  }

  // A flat mid-scale table: the control's Apply() only has to complete, not to
  // produce a particular colour, so a constant table keeps the assertion about
  // reaching the transform rather than about interpolation.
  for (icUInt32Number i = 0; i < pCLUT->NumPoints() * kPcsChannels; i++)
    (*pCLUT)[i] = 0.5f;

  if (!pProfile->AttachTag(icSigAToB0Tag, pLut)) {
    delete pLut;
    delete pProfile;
    return NULL;
  }

  return pProfile;
}

// Drive one case to Begin() and report the status.  bApply additionally runs a pixel
// through, sized from the *colour space* exactly as a real caller sizes it -- which is
// the buffer the defect over-read.
icStatusCMM beginCase(icColorSpaceSignature csDevice, icUInt8Number nTagInput, bool bApply)
{
  CIccProfile *pProfile = buildNDLutProfile(csDevice, nTagInput);
  if (!pProfile)
    return icCmmStatAllocErr;

  CIccCmm cmm;

  // AddXform takes ownership of pProfile, including on its failure paths.
  icStatusCMM rv = cmm.AddXform(pProfile, icPerceptual, icInterpLinear, NULL,
                                icXformLutColor, false);
  if (rv != icCmmStatOk)
    return rv;

  rv = cmm.Begin();
  if (rv != icCmmStatOk || !bApply)
    return rv;

  const icUInt32Number nSrc = icGetSpaceSamples(csDevice);
  std::vector<icFloatNumber> src(nSrc, 0.5f);
  std::vector<icFloatNumber> dst(kPcsChannels, 0.0f);
  return cmm.Apply(&dst[0], &src[0]);
}

// The reported crash, reduced: header says 7 channels, tag says 13.  Without the fix
// Begin() returns icCmmStatOk and Apply() reads SrcPixel[7..12] off the end.
void testTagOverDeclaresInputChannels()
{
  const icStatusCMM rv = beginCase(icSig7colorData, 13, false);
  check(rv == icCmmStatInvalidLut,
        "7CLR profile with a 13-channel A2B0 must be rejected as an invalid LUT");
}

// The same disagreement the other way round: header says 13 channels, tag says 7.
// This one does not read out of bounds -- it silently consumes the wrong channels --
// but it is the same broken profile, and the output-side check has always rejected
// its mirror image.  Pinned so the two sides stay symmetric.
void testTagUnderDeclaresInputChannels()
{
  const icStatusCMM rv = beginCase(icSig13colorData, 7, false);
  check(rv == icCmmStatInvalidLut,
        "DCLR profile with a 7-channel A2B0 must be rejected as an invalid LUT");
}

// Control: the same 13-channel tag in the colour space it actually belongs to -- the
// profile the AFL input was mutated from.  This is the assertion that separates the
// fix from a blunt "reject N-channel LUTs", and it must hold both before and after.
void testConsistentProfileStillApplies()
{
  const icStatusCMM rv = beginCase(icSig13colorData, 13, true);
  check(rv == icCmmStatOk,
        "DCLR profile with a matching 13-channel A2B0 must still begin and apply");
}

// Drive a case through CIccCmm::AddXform(CIccProfile*, CIccTag*, ...), the public
// overload that hands CIccXform::Create() a tag of the caller's choosing.  The tag is
// still attached to the profile, so ownership is unchanged; what differs from
// beginCase() is only that Create() takes the tag from the caller rather than looking
// it up, and so never has cause to agree with m_Header.colorSpace.
icStatusCMM beginCallerSuppliedTagCase(icColorSpaceSignature csDevice,
                                       icUInt8Number nTagInput,
                                       icColorSpaceSignature csTagDeclaredInput)
{
  CIccProfile *pProfile = buildNDLutProfile(csDevice, nTagInput, csTagDeclaredInput);
  if (!pProfile)
    return icCmmStatAllocErr;

  CIccTag *pTag = pProfile->FindTag(icSigAToB0Tag);
  if (!pTag) {
    delete pProfile;
    return icCmmStatInvalidProfile;
  }

  CIccCmm cmm;

  // Takes ownership of pProfile on success and frees it on its failure paths, the
  // same contract as the profile-only overload.
  icStatusCMM rv = cmm.AddXform(pProfile, pTag, icPerceptual, icInterpLinear, NULL, false);
  if (rv != icCmmStatOk)
    return rv;

  return cmm.Begin();
}

// A 6CLR profile whose A2B0 tag is stamped as if it were a 10-channel B2A: the tag
// agrees with itself (GetCsInput() is ACLR, 10 samples, and InputChannels() is 10) and
// its output side agrees with the PCS, so every check that consults only the tag lets
// it through.  But SrcPixel is sized from the header's 6CLR, so Apply() would copy 10
// floats out of a 6-float buffer -- the #2119 over-read again, reached through the
// public API rather than through a malformed file.  This is the case that distinguishes
// comparing against GetNumSrcSamples() from comparing against the tag's own GetCsInput();
// only the former rejects it.
void testCallerSuppliedTagIsPinnedToTheProfile()
{
  const icStatusCMM rv = beginCallerSuppliedTagCase(icSig6colorData, 10, icSig10colorData);
  check(rv == icCmmStatInvalidLut,
        "6CLR profile with a caller-supplied 10-channel tag must be rejected as an invalid LUT");
}

}  // namespace

int main()
{
  testTagOverDeclaresInputChannels();
  testTagUnderDeclaresInputChannels();
  testConsistentProfileStillApplies();
  testCallerSuppliedTagIsPinnedToTheProfile();

  if (!g_fail)
    std::printf("[ndlut-input-channels] PASS\n");

  return g_fail;
}
