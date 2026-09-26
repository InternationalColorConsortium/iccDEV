// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       encoding-inverse-lumamtx.cpp

    Contains:   CTest helper for the B2A matrix of a converted colour encoding
                profile (#1990).

    CIccDefaultEncProfileConverter::ConvertFromParams inverts the luma/chroma
    matrix for the B2A direction, then created a matrix element and attached it
    to BToA3 without sizing it or copying the inverse in.  The element reported
    0 input and 0 output channels with no data, so a converted YCbCr encoding
    profile had no usable reverse transform, and conversion still reported
    success.

    The sibling test for the A2B side (encoding-lumamtx-double-free.cpp, #1985)
    uses an identity matrix, which cannot tell an inverse from a copy.  This one
    uses the BT.709 luma/chroma matrix, and checks that BToA3 holds a matrix whose
    product with it is the identity: an element that was merely sized, or that
    held the forward matrix, both fail.
*/

#include "IccEncoding.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[encoding-inverse-lumamtx] FAIL %s: %s\n", label, what);
  }
}

// BT.709 R'G'B' to Y'CbCr, row-major.  Invertible and far from the identity.
const icFloatNumber kLumaMtx[9] = {
   0.2126f,  0.7152f,  0.0722f,
  -0.1146f, -0.3854f,  0.5000f,
   0.5000f, -0.4542f, -0.0458f
};

bool attachFloats(CIccTagStruct *pParams, icSignature sig,
                  const icFloatNumber *vals, icUInt32Number n)
{
  CIccTagFloat32 *pTag = (CIccTagFloat32 *)CIccTag::Create(icSigFloat32ArrayType);
  if (!pTag)
    return false;
  if (!pTag->SetSize(n)) {
    delete pTag;
    return false;
  }
  for (icUInt32Number i = 0; i < n; i++)
    (*pTag)[i] = vals[i];
  if (!pParams->AttachElem(sig, pTag)) {
    delete pTag;
    return false;
  }
  return true;
}

// White point, luma/chroma matrix and primaries: enough for the conversion to
// run to completion and build both directions.
CIccTagStruct *buildParams()
{
  CIccTagStruct *pParams = (CIccTagStruct *)CIccTag::Create(icSigTagStructType);
  if (!pParams)
    return NULL;
  if (!pParams->SetTagStructType(icSigColorEncodingParamsSruct)) {
    delete pParams;
    return NULL;
  }

  const icFloatNumber white[2] = {0.3127f, 0.3290f};
  const icFloatNumber red[2]   = {0.6400f, 0.3300f};
  const icFloatNumber green[2] = {0.3000f, 0.6000f};
  const icFloatNumber blue[2]  = {0.1500f, 0.0600f};

  bool ok =
    attachFloats(pParams, icSigCeptWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptMediumWhitePointChromaticityMbr, white, 2) &&
    attachFloats(pParams, icSigCeptLumaChromaMatrixMbr, kLumaMtx, 9) &&
    attachFloats(pParams, icSigCeptRedPrimaryXYZMbr, red, 2) &&
    attachFloats(pParams, icSigCeptGreenPrimaryXYZMbr, green, 2) &&
    attachFloats(pParams, icSigCeptBluePrimaryXYZMbr, blue, 2);
  if (!ok) {
    delete pParams;
    return NULL;
  }
  return pParams;
}

bool nearly(icFloatNumber a, icFloatNumber b) { return std::fabs(a - b) < 1e-4f; }

// True if a (3x3, row-major) equals the forward matrix.
bool isForward(const icFloatNumber *a)
{
  for (int i = 0; i < 9; i++)
    if (!nearly(a[i], kLumaMtx[i]))
      return false;
  return true;
}

// True if kLumaMtx * b is the identity, i.e. b is the forward matrix's inverse.
bool isInverse(const icFloatNumber *b)
{
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      icFloatNumber s = 0;
      for (int k = 0; k < 3; k++)
        s += kLumaMtx[r * 3 + k] * b[k * 3 + c];
      if (!nearly(s, r == c ? 1.0f : 0.0f))
        return false;
    }
  }
  return true;
}

// The matrix elements of an MPE tag, each classified.
struct MatrixScan {
  int nMatrices = 0;
  int nEmpty = 0;       // no channels or no data: the #1990 signature
  bool bForward = false;
  bool bInverse = false;
};

MatrixScan scan(CIccProfile *pIcc, icSignature sig)
{
  MatrixScan s;
  CIccTagMultiProcessElement *pTag =
    dynamic_cast<CIccTagMultiProcessElement *>(pIcc->FindTag(sig));
  if (!pTag)
    return s;
  for (icUInt32Number i = 0; i < pTag->NumElements(); i++) {
    CIccMpeMatrix *pMtx = dynamic_cast<CIccMpeMatrix *>(pTag->GetElement((int)i));
    if (!pMtx)
      continue;
    s.nMatrices++;
    if (!pMtx->NumInputChannels() || !pMtx->NumOutputChannels() || !pMtx->GetMatrix()) {
      s.nEmpty++;
      continue;
    }
    if (pMtx->NumInputChannels() == 3 && pMtx->NumOutputChannels() == 3) {
      if (isForward(pMtx->GetMatrix()))
        s.bForward = true;
      if (isInverse(pMtx->GetMatrix()))
        s.bInverse = true;
    }
  }
  return s;
}

} // namespace

int main()
{
  CIccTagStruct *pParams = buildParams();
  check(pParams != NULL, "setup", "could not build the colorEncodingParams struct");
  if (!pParams)
    return 1;

  icHeader hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.deviceClass = icSigColorSpaceClass;
  hdr.colorSpace = icSigRgbData;
  hdr.pcs = icSigXYZData;
  hdr.version = icVersionNumberV5;
  hdr.renderingIntent = icPerceptual;

  IIccEncProfileConverter *pConverter = IIccEncProfileConverter::GetHandler();
  check(pConverter != NULL, "setup", "no encoding converter handler");
  if (!pConverter) {
    delete pParams;
    return 1;
  }

  CIccProfilePtr pIcc = NULL;
  icStatusEncConvert stat = pConverter->ConvertFromParams(pIcc, pParams, &hdr);
  check(stat == icEncConvertOk && pIcc != NULL, "convert", "ConvertFromParams did not produce a profile");

  if (pIcc) {
    // Control: the A2B side has always carried the forward matrix.
    MatrixScan a2b = scan(pIcc, icSigAToB3Tag);
    check(a2b.bForward, "a2b control", "AToB3 does not carry the forward luma/chroma matrix");

    MatrixScan b2a = scan(pIcc, icSigBToA3Tag);
    check(b2a.nMatrices > 0, "b2a", "BToA3 has no matrix element at all");
    check(b2a.nEmpty == 0, "b2a empty element",
          "BToA3 carries a matrix element with no channels or no data");
    check(b2a.bInverse, "b2a inverse",
          "BToA3 carries no matrix whose product with the forward matrix is the identity");

    delete pIcc;
  }
  delete pParams;

  if (g_fail) {
    std::fprintf(stderr, "[encoding-inverse-lumamtx] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[encoding-inverse-lumamtx] all checks passed\n");
  return 0;
}
