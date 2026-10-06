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

/*
 * json-reader-strictness.cpp -- three places where the JSON profile reader
 * accepted a malformed document and built a different profile from it, each
 * refused by the XML reader's twin.
 *
 *   #2674  SpectralRange.steps (and BiSpectralRange) was parsed into an int
 *          and cast to uInt16Number, so "steps": 65567 was stored as 31 and
 *          the conversion reported success.  The XML reader uses a checked
 *          16-bit parse and fails with "Invalid SpectralRange Wavelengths
 *          steps".
 *   #2695  A Tags entry that is not a single-member object appended a
 *          "Warning" to parseStr and was skipped; iccFromJson prints parseStr
 *          only when the load fails, so the requested tag silently never
 *          reached the profile.
 *   #2697  A multiProcessElementType element that was empty, had no type, an
 *          unknown type, or a type without a JSON extension was dropped and
 *          the tag built without it, changing the transform; only an element
 *          whose own parser failed refused the profile.  The XML reader fails
 *          an unknown element type.
 *
 * Each case checks that the control document still loads and that the
 * mutated one is refused with the message the reader now gives.
 */

#include "IccProfileJson.h"
#include "IccTagJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccUtilJson.h"

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
    std::fprintf(stderr, "[json-reader-strictness] FAIL %s\n", what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

/* A v5 profile with one multiProcessElementType AToB1Tag holding a curve set,
   with hooks for the header spectral range, a second Tags-entry member, and
   the element list. */
std::string doc(const std::string &spectralRange, const std::string &extraTagMember,
                const std::string &elements)
{
  return
    "{\n"
    "  \"IccProfile\": {\n"
    "    \"Header\": {\n"
    "      \"ProfileVersion\": \"5.00\",\n"
    "      \"ProfileDeviceClass\": \"mntr\",\n"
    "      \"DataColourSpace\": \"RGB \",\n"
    "      \"PCS\": \"XYZ \",\n"
    "      \"CreationDateTime\": \"2026-10-06T00:00:00\",\n"
    "      \"RenderingIntent\": \"Perceptual\",\n"
    "      \"PCSIlluminant\": [0.9642, 1.0, 0.8249]" + spectralRange + "\n"
    "    },\n"
    "    \"Tags\": [\n"
    "      {\n"
    "        \"AToB1Tag\": {\n"
    "          \"data\": {\n"
    "            \"type\": \"multiProcessElementType\",\n"
    "            \"inputChannels\": 3,\n"
    "            \"outputChannels\": 3,\n"
    "            \"elements\": [" + elements + "]\n"
    "          }\n"
    "        }" + extraTagMember + "\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "}\n";
}

/* The shape iccToJson writes for a matrixElement (Testing/CalcTest/calcOverMem_tget.icc). */
const char *kMatrix =
  "{ \"type\": \"MatrixElement\", \"inputChannels\": 3, \"outputChannels\": 3,"
  " \"matrix\": [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0], \"constants\": [0.0, 0.0, 0.0] }";

bool load(const std::string &text, std::string &parseStr, CIccProfileJson &profile)
{
  parseStr.clear();
  IccJson j = IccJson::parse(text, nullptr, false);
  if (j.is_discarded()) {
    parseStr = "document is not JSON";
    return false;
  }
  return profile.ParseJson(j, parseStr);
}

void expectLoads(const std::string &text, const char *label)
{
  CIccProfileJson profile;
  std::string parseStr;
  bool ok = load(text, parseStr, profile);
  check(ok, label);
  if (!ok)
    std::fprintf(stderr, "  %s\n", parseStr.c_str());
}

void expectRefused(const std::string &text, const char *needle, const char *label)
{
  CIccProfileJson profile;
  std::string parseStr;
  bool ok = load(text, parseStr, profile);
  check(!ok, label);
  if (ok)
    return;
  std::string what = std::string(label) + ": the reason names the defect";
  check(has(parseStr, needle), what.c_str());
  if (!has(parseStr, needle))
    std::fprintf(stderr, "  got: %s\n", parseStr.c_str());
}

/* #2674 */
void testSpectralSteps()
{
  CIccProfileJson profile;
  std::string parseStr;
  std::string control = doc(",\n      \"SpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": 31 }", "", kMatrix);
  check(load(control, parseStr, profile), "control: a 31-step spectral range loads");
  check(profile.m_Header.spectralRange.steps == 31, "control: steps is stored as 31");

  expectRefused(doc(",\n      \"SpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": 65567 }", "", kMatrix),
                "Invalid SpectralRange Wavelengths steps",
                "#2674: steps 65567 is refused rather than stored as 31");
  expectRefused(doc(",\n      \"SpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": -1 }", "", kMatrix),
                "Invalid SpectralRange Wavelengths steps",
                "#2674: a negative step count is refused");
  expectRefused(doc(",\n      \"SpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": 31.5 }", "", kMatrix),
                "Invalid SpectralRange Wavelengths steps",
                "#2674: a non-integer step count is refused");
  expectRefused(doc(",\n      \"BiSpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": 70000 }", "", kMatrix),
                "Invalid BiSpectralRange Wavelengths steps",
                "#2674: BiSpectralRange takes the same check");
  expectLoads(doc(",\n      \"SpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": 65535 }", "", kMatrix),
              "#2674: the largest storable step count still loads");
}

/* #2695 */
void testTagsEntry()
{
  expectLoads(doc("", "", kMatrix), "control: a single-member Tags entry loads");
  expectRefused(doc("", ",\n        \"unexpectedSecondMember\": { \"data\": { \"type\": \"signatureType\", \"Signature\": \"prmg\" } }", kMatrix),
                "Tags entry must be a single-member object",
                "#2695: a two-member Tags entry is refused rather than skipped");
  expectRefused("{ \"IccProfile\": { \"Header\": { \"ProfileVersion\": \"4.30\", \"ProfileDeviceClass\": \"mntr\","
                " \"DataColourSpace\": \"RGB \", \"PCS\": \"XYZ \" }, \"Tags\": [ {} ] } }",
                "Tags entry must be a single-member object",
                "#2695: an empty Tags entry is refused");
}

/* #2697 */
void testMpeElements()
{
  expectLoads(doc("", "", kMatrix), "control: a MatrixElement loads");
  std::string renamed = kMatrix;
  renamed.replace(renamed.find("\"type\""), 6, "\"ignoredType\"");
  expectRefused(doc("", "", renamed), "multiProcessElementType element has no type",
                "#2697: an element without a type is refused rather than dropped");
  std::string unknown = kMatrix;
  unknown.replace(unknown.find("MatrixElement"), 13, "NoSuchElement");
  expectRefused(doc("", "", unknown), "Unknown Element Type (NoSuchElement)",
                "#2697: an unknown element type is refused, as the XML reader does");
  expectLoads(doc("", "", std::string(kMatrix) + ", { \"type\": \"abcd\", \"inputChannels\": 3, \"outputChannels\": 3 }"),
              "control: a private element named by its four-character signature still loads");
  expectLoads(doc("", "", std::string(kMatrix) + ", { \"type\": \"UnknownElement\", \"inputChannels\": 3, \"outputChannels\": 3 }"),
              "control: the writer's own UnknownElement spelling still loads");
  expectRefused(doc("", "", std::string(kMatrix) + ", {}"), "must be a non-empty object",
                "#2697: an empty element object is refused");
  expectRefused(doc("", "", std::string(kMatrix) + ", 7"), "must be a non-empty object",
                "#2697: a non-object element is refused");
}

}  // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  testSpectralSteps();
  testTagsEntry();
  testMpeElements();

  if (g_fail) {
    std::fprintf(stderr, "[json-reader-strictness] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[json-reader-strictness] PASS\n");
  return 0;
}
