/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
    File:       tag-type-table.cpp

    Contains:   CTest helper for the permitted-type rows #3065 corrected in
                CIccProfile::IsTypeValid().

    - ICC.1 defines DToB3Tag and BToD3Tag with DToB0-2/BToD0-2
      (multiProcessElementsType). They sat in the v5-only MCS/BRDF row, so a
      v4.4 profile carrying them was NonCompliant.
    - ICC.1 preview0Tag permits lutAToBType as well as lut8, lut16 and
      lutBToA. It shared the BToA row and refused lutAToBType.
    - ICC.2 AToB0-3Tag and BToA0-3Tag do not list lut8Type or lut16Type,
      which were accepted at v5.
    - metadataTag (dictType), profileSequenceIdentifierTag
      (profileSequenceIdentifierType) and colorimetricIntentImageStateTag
      (signatureType) had no row, so they took the private-tag default and
      accepted any type.

    The table calls IsTypeValid() through a subclass, because it is protected;
    the last three rows go through Validate() to pin the report line.
*/

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagDict.h"
#include "IccTagProfSeqId.h"
#include "IccUtil.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const std::string &label, const std::string &what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[tag-type-table] FAIL %s: %s\n", label.c_str(), what.c_str());
  }
}

const icUInt32Number kV2  = 0x02100000;
const icUInt32Number kV40 = 0x04000000;
const icUInt32Number kV41 = 0x04100000;
const icUInt32Number kV42 = icVersionNumberV4_2;
const icUInt32Number kV44 = icVersionNumberV4_4;
const icUInt32Number kV5  = icVersionNumberV5;

class Probe : public CIccProfile
{
public:
  bool Valid(icUInt32Number version, icTagSignature tag, icTagTypeSignature type)
  {
    m_Header.version = version;
    return IsTypeValid(tag, type);
  }
};

struct Row {
  icUInt32Number version;
  icTagSignature tag;
  icTagTypeSignature type;
  bool valid;
};

const Row kRows[] = {
  // DToB3/BToD3 are ICC.1 tags, gated like DToB0-2.
  { kV44, icSigDToB3Tag, icSigMultiProcessElementType, true  },
  { kV44, icSigBToD3Tag, icSigMultiProcessElementType, true  },
  { kV44, icSigDToB0Tag, icSigMultiProcessElementType, true  },
  { kV41, icSigDToB3Tag, icSigMultiProcessElementType, false },
  { kV42, icSigDToB3Tag, icSigMultiProcessElementType, true  },
  { kV42, icSigBToD3Tag, icSigMultiProcessElementType, true  },
  { kV44, icSigDToB3Tag, icSigLut16Type,               false },
  { kV5,  icSigDToB3Tag, icSigMultiProcessElementType, true  },

  // preview0 takes lutAToBType from v4; preview1/2 and gamut do not.
  { kV44, icSigPreview0Tag, icSigLutAtoBType, true  },
  { kV44, icSigPreview0Tag, icSigLutBtoAType, true  },
  { kV44, icSigPreview0Tag, icSigLut16Type,   true  },
  { kV2,  icSigPreview0Tag, icSigLutAtoBType, false },
  { kV40, icSigPreview0Tag, icSigLutAtoBType, true  },
  { kV44, icSigPreview1Tag, icSigLutAtoBType, false },
  { kV44, icSigPreview2Tag, icSigLutAtoBType, false },
  { kV44, icSigGamutTag,    icSigLutAtoBType, false },

  // v5 AToB/BToA: no legacy LUTs.  v4 keeps them.
  { kV5,  icSigAToB0Tag, icSigLut16Type,               false },
  { kV5,  icSigAToB1Tag, icSigLut8Type,                false },
  { kV5,  icSigAToB3Tag, icSigLut16Type,               false },
  { kV5,  icSigBToA2Tag, icSigLut8Type,                false },
  { kV5,  icSigBToA3Tag, icSigLut16Type,               false },
  { kV5,  icSigAToB0Tag, icSigLutAtoBType,             true  },
  { kV5,  icSigAToB0Tag, icSigMultiProcessElementType, true  },
  { kV5,  icSigBToA0Tag, icSigLutBtoAType,             true  },
  { kV44, icSigAToB0Tag, icSigLut16Type,               true  },
  { kV44, icSigBToA1Tag, icSigLut8Type,                true  },
  { kV2,  icSigAToB0Tag, icSigLut8Type,                true  },
  // gamutTag and the preview tags are not ICC.2 tags; this PR leaves them.
  { kV5,  icSigGamutTag, icSigLut16Type,               true  },

  // One permitted type each.
  { kV44, icSigMetaDataTag,                     icSigDictType,            true  },
  { kV44, icSigMetaDataTag,                     icSigTextType,            false },
  { kV44, icSigProfileSequceIdTag,              icSigProfileSequceIdType, true  },
  { kV44, icSigProfileSequceIdTag,              icSigTextType,            false },
  { kV44, icSigColorimetricIntentImageStateTag, icSigSignatureType,       true  },
  { kV44, icSigColorimetricIntentImageStateTag, icSigTextType,            false },
  { kV5,  icSigColorimetricIntentImageStateTag, icSigSignatureType,       true  },
};

std::string label(icUInt32Number version, icTagSignature tag, icTagTypeSignature type)
{
  char t[8], y[8], buf[64];
  std::snprintf(buf, sizeof(buf), "v%x.%x %s %s", (unsigned)(version >> 24),
                (unsigned)((version >> 20) & 0xf), icGetSigStr(t, sizeof(t), tag),
                icGetSigStr(y, sizeof(y), type));
  return buf;
}

// Validate() report lines naming the tag and "Invalid tag type".
bool reportsInvalid(icTagSignature tag, CIccTag *pTag)
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = kV44;
  prof.m_Header.deviceClass = icSigDisplayClass;
  prof.m_Header.colorSpace = icSigRgbData;
  prof.m_Header.pcs = icSigXYZData;
  prof.AttachTag(tag, pTag);

  std::string report;
  prof.Validate(report);
  CIccInfo info;
  std::string name = info.GetSigName(tag);
  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (line.find(name) != std::string::npos &&
        line.find("Invalid tag type") != std::string::npos)
      return true;
    pos = end + 1;
  }
  return false;
}

CIccTag *text()
{
  CIccTagText *p = new CIccTagText;
  p->SetText("not the permitted type");
  return p;
}

} // namespace

int main()
{
  Probe probe;
  for (const Row &r : kRows) {
    bool got = probe.Valid(r.version, r.tag, r.type);
    check(got == r.valid, label(r.version, r.tag, r.type),
          r.valid ? "refused, should be permitted" : "permitted, should be refused");
  }

  // End to end through Validate(): the right type is clean, textType is reported.
  CIccTagSignature *ciis = new CIccTagSignature;
  ciis->SetValue(icSigSceneColorimetryEstimates);
  check(!reportsInvalid(icSigColorimetricIntentImageStateTag, ciis), "Validate ciis sig",
        "signatureType reported as an invalid type");
  check(reportsInvalid(icSigColorimetricIntentImageStateTag, text()), "Validate ciis text",
        "textType not reported");
  check(reportsInvalid(icSigMetaDataTag, text()), "Validate meta text",
        "textType not reported");
  check(!reportsInvalid(icSigMetaDataTag, new CIccTagDict), "Validate meta dict",
        "dictType reported as an invalid type");
  check(reportsInvalid(icSigProfileSequceIdTag, text()), "Validate psid text",
        "textType not reported");
  check(!reportsInvalid(icSigProfileSequceIdTag, new CIccTagProfileSequenceId),
        "Validate psid psid", "profileSequenceIdentifierType reported as an invalid type");

  if (g_fail) {
    std::fprintf(stderr, "[tag-type-table] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[tag-type-table] all checks passed\n");
  return 0;
}
