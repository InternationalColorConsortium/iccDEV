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
 * json-reader-strictness.cpp -- JSON parser fields that previously
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
 *   Spectral viewing conditions ignored failed numeric reads for ObserverFuncs
 *          and IlluminantSPD ranges and narrowed Reserved to 16 bits. A bad
 *          IlluminantSPD.start was saved as zero while XML refused it.
 *   Spectral elements ignored failed numeric reads for wavelengths, saving a
 *          bad EmissionMatrixElement start as zero while XML refused it.
 *   NamedColor2 ignored a failed device-channel count read, saving a profile
 *          with no device coordinates after a wrong-type count.
 *   Fixed-size numeric arrays in seven tag readers and the profile header
 *          ignored failed conversions, so a string became a stored zero;
 *          oversized arrays also had their extra values silently dropped.
 *   Present required header strings of the wrong type were treated as absent,
 *          and ProfileFileSignature was replaced with "acsp" without reading
 *          it. NamedColor2 device coordinates outside 0..65535 were clamped.
 *
 * Each case checks that the control document still loads and that the
 * mutated one is refused with the message the reader now gives.
 */

#include "IccProfileJson.h"
#include "IccTagJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJson.h"
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
    "      \"ProfileFileSignature\": \"acsp\",\n"
    "      \"PrimaryPlatform\": \"\",\n"
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

void expectJsonRefused(const IccJson &json, const char *needle, const char *label)
{
  CIccProfileJson profile;
  std::string parseStr;
  bool ok = profile.ParseJson(json, parseStr);
  check(!ok, label);
  if (ok)
    return;
  std::string what = std::string(label) + ": the reason names the defect";
  check(has(parseStr, needle), what.c_str());
  if (!has(parseStr, needle))
    std::fprintf(stderr, "  got: %s\n", parseStr.c_str());
}

template <class TagType>
void expectTagLoads(const IccJson &json, const char *label)
{
  TagType tag;
  std::string parseStr;
  bool ok = tag.ParseJson(json, parseStr);
  check(ok, label);
  if (!ok)
    std::fprintf(stderr, "  %s\n", parseStr.c_str());
}

template <class TagType>
void expectTagRefused(const IccJson &json, const char *needle, const char *label)
{
  TagType tag;
  std::string parseStr;
  bool ok = tag.ParseJson(json, parseStr);
  check(!ok, label);
  if (ok)
    return;
  std::string what = std::string(label) + ": the reason names the defect";
  check(has(parseStr, needle), what.c_str());
  if (!has(parseStr, needle))
    std::fprintf(stderr, "  got: %s\n", parseStr.c_str());
}

void testFixedNumericArrays()
{
  double fixed[3] = {0.0, 0.0, 0.0};
  check(jsonToArray(IccJson::array({1.0, 2.0, 3.0}), fixed, 3),
        "control: an exact numeric triplet loads");
  check(!jsonToArray(IccJson::array({1.0, 2.0}), fixed, 3),
        "a short fixed numeric array is refused");
  check(!jsonToArray(IccJson::array({1.0, 2.0, 3.0, 4.0}), fixed, 3),
        "a long fixed numeric array is refused rather than truncated");
  check(!jsonToArray(IccJson::object({{"x", 1.0}}), fixed, 3),
        "a fixed numeric array refuses a non-array value");
  check(!jsonToArray(IccJson::array({"wrong-type", 2.0, 3.0}), fixed, 3),
        "a mixed-type fixed numeric array is refused");

  const IccJson xyz = IccJson::parse("{\"XYZ\":[[0.1,0.2,0.3]]}", nullptr, false);
  expectTagLoads<CIccTagJsonXYZ>(xyz, "control: an XYZ triplet loads");
  IccJson bad = xyz;
  bad["XYZ"][0][0] = "wrong-type";
  expectTagRefused<CIccTagJsonXYZ>(bad, "Invalid XYZ array value",
                                   "XYZArrayType refuses a mixed-type triplet");
  bad = xyz;
  bad["XYZ"][0].push_back(0.4);
  expectTagRefused<CIccTagJsonXYZ>(bad, "Invalid XYZ array value",
                                   "XYZArrayType refuses a long triplet");

  const IccJson measurement = IccJson::parse(
    "{\"standardObserver\":1,\"backing\":[0.1,0.2,0.3],\"geometry\":1,"
    "\"flare\":\"Flare 0\",\"illuminant\":1}", nullptr, false);
  expectTagLoads<CIccTagJsonMeasurement>(measurement,
                                         "control: measurement backing loads");
  bad = measurement;
  bad["backing"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonMeasurement>(bad, "Invalid measurement backing",
                                           "measurementType refuses mixed-type backing");

  const IccJson viewing = IccJson::parse(
    "{\"illuminant\":[0.1,0.2,0.3],\"surround\":[0.4,0.5,0.6],"
    "\"illuminantType\":1}", nullptr, false);
  expectTagLoads<CIccTagJsonViewingConditions>(viewing,
                                               "control: viewing conditions load");
  bad = viewing;
  bad["illuminant"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonViewingConditions>(bad, "illuminant",
                                                 "viewingConditionsType refuses mixed-type illuminant");
  bad = viewing;
  bad["surround"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonViewingConditions>(bad, "surround",
                                                 "viewingConditionsType refuses mixed-type surround");

  const IccJson spectral = IccJson::parse(
    "{\"spectralColorSig\":\"65730191h\",\"spectralRange\":[380,780,401],"
    "\"biSpectralRange\":[300,780,49]}", nullptr, false);
  expectTagLoads<CIccTagJsonSpectralDataInfo>(spectral,
                                              "control: spectral data ranges load");
  IccJson withoutRange = spectral;
  withoutRange.erase("spectralRange");
  expectTagRefused<CIccTagJsonSpectralDataInfo>(withoutRange, "spectralRange",
    "spectralDataInfoType requires spectralRange");
  withoutRange = spectral;
  withoutRange.erase("biSpectralRange");
  expectTagLoads<CIccTagJsonSpectralDataInfo>(withoutRange,
    "compatibility control: biSpectralRange remains optional");
  bad = spectral;
  bad["spectralRange"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonSpectralDataInfo>(bad, "spectralRange",
                                                "spectralDataInfoType refuses a mixed-type range");
  bad = spectral;
  bad["biSpectralRange"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonSpectralDataInfo>(bad, "biSpectralRange",
                                                "spectralDataInfoType refuses a mixed-type bi-range");
  const char *rangeFields[] = {"spectralRange", "biSpectralRange"};
  const IccJson badSteps[] = {-1, 65536, 1.5, "wrong-type"};
  for (const char *field : rangeFields) {
    for (const IccJson &steps : badSteps) {
      bad = spectral;
      bad[field][2] = steps;
      std::string label = std::string("spectralDataInfoType refuses an invalid ") +
                          field + " step count";
      expectTagRefused<CIccTagJsonSpectralDataInfo>(bad, field,
                                                    label.c_str());
    }
  }
  IccJson rangeBoundaries = spectral;
  rangeBoundaries["spectralRange"][2] = 0;
  rangeBoundaries["biSpectralRange"][2] = 65535;
  expectTagLoads<CIccTagJsonSpectralDataInfo>(rangeBoundaries,
    "control: spectral data step-count boundaries load");

  const IccJson responseCurve = IccJson::parse(
    "{\"CountOfChannels\":1,\"ResponseCurves\":[{\"MeasurementUnit\":\"Status A\","
    "\"Channels\":[{\"MaxColorantXYZ\":[0.1,0.2,0.3],\"Measurements\":[]}]}]}",
    nullptr, false);
  expectTagLoads<CIccTagJsonResponseCurveSet16>(responseCurve,
    "control: response-curve maximum colorant triplet loads");
  IccJson withoutMaxColorant = responseCurve;
  withoutMaxColorant["ResponseCurves"][0]["Channels"][0].erase("MaxColorantXYZ");
  expectTagLoads<CIccTagJsonResponseCurveSet16>(withoutMaxColorant,
    "compatibility control: response-curve maximum colorant remains optional");
  bad = responseCurve;
  bad["ResponseCurves"][0]["Channels"][0]["MaxColorantXYZ"].push_back(0.4);
  expectTagRefused<CIccTagJsonResponseCurveSet16>(bad, "Invalid MaxColorantXYZ",
    "responseCurveSet16Type refuses a long maximum colorant triplet");
  bad = responseCurve;
  bad["ResponseCurves"][0]["Channels"][0]["MaxColorantXYZ"] = "wrong-type";
  expectTagRefused<CIccTagJsonResponseCurveSet16>(bad, "Invalid MaxColorantXYZ",
    "responseCurveSet16Type refuses a non-array maximum colorant triplet");
}

void testHeaderTypes()
{
  const IccJson control = IccJson::parse(doc("", "", kMatrix), nullptr, false);
  check(!control.is_discarded(), "header control is valid JSON");
  if (control.is_discarded())
    return;

  const char *stringFields[] = {
    "ProfileVersion", "ProfileDeviceClass", "DataColourSpace", "PCS",
    "CreationDateTime", "PrimaryPlatform", "RenderingIntent"
  };
  for (const char *field : stringFields) {
    IccJson bad = control;
    bad["IccProfile"]["Header"][field] = 123;
    std::string needle = std::string("Invalid Header.") + field;
    std::string label = std::string("Header refuses wrong-type ") + field;
    expectJsonRefused(bad, needle.c_str(), label.c_str());
  }

  IccJson bad = control;
  bad["IccProfile"]["Header"]["ProfileFileSignature"] = "nope";
  expectJsonRefused(bad, "Invalid Header.ProfileFileSignature",
                "Header refuses a non-acsp profile signature");
  bad = control;
  bad["IccProfile"]["Header"]["ProfileFileSignature"] = 123;
  expectJsonRefused(bad, "Invalid Header.ProfileFileSignature",
                "Header refuses a wrong-type profile signature");

  bad = control;
  bad["IccProfile"]["Header"]["PCSIlluminant"][0] = "wrong-type";
  expectJsonRefused(bad, "Invalid Header.PCSIlluminant",
                "Header refuses a mixed-type PCS illuminant");
  bad = control;
  bad["IccProfile"]["Header"]["PCSIlluminant"].push_back(1.0);
  expectJsonRefused(bad, "Invalid Header.PCSIlluminant",
                "Header refuses a long PCS illuminant");

  IccJson legacy = control;
  legacy["IccProfile"]["Header"].erase("ProfileFileSignature");
  legacy["IccProfile"]["Header"].erase("PrimaryPlatform");
  CIccProfileJson profile;
  std::string parseStr;
  check(profile.ParseJson(legacy, parseStr),
        "compatibility control: absent legacy header fields keep their defaults");
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
  expectRefused(doc(",\n      \"SpectralRange\": { \"start\": \"bogus\", \"end\": 780.0, \"steps\": 31 }", "", kMatrix),
                "Invalid SpectralRange Wavelengths start or end",
                "a wrong-type SpectralRange start is refused rather than stored as zero");
  expectRefused(doc(",\n      \"BiSpectralRange\": { \"start\": 380.0, \"end\": \"bogus\", \"steps\": 31 }", "", kMatrix),
                "Invalid BiSpectralRange Wavelengths start or end",
                "a wrong-type BiSpectralRange end is refused rather than stored as zero");
  expectLoads(doc(",\n      \"SpectralRange\": { \"start\": 380.0, \"end\": 780.0, \"steps\": 65535 }", "", kMatrix),
              "#2674: the largest storable step count still loads");
}

void testViewingConditionsNumbers()
{
  const IccJson control = IccJson::parse(
    "{\"StdObserver\":\"CIE 1931 (two degree) standard observer\","
    "\"IlluminantXYZ\":[0.9,1.0,1.1],"
    "\"ObserverFuncs\":{\"start\":400,\"end\":500,\"steps\":3,"
    "\"Reserved\":65535,\"data\":[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9]},"
    "\"StdIlluminant\":\"Illuminant D50\",\"ColorTemperature\":5000,"
    "\"IlluminantSPD\":{\"start\":400,\"end\":500,\"steps\":3,"
    "\"Reserved\":65535,\"data\":[1.0,2.0,3.0]},"
    "\"SurroundXYZ\":[0.1,0.2,0.3]}", nullptr, false);
  check(!control.is_discarded(), "viewing conditions control is valid JSON");
  if (control.is_discarded())
    return;

  CIccTagJsonSpectralViewingConditions tag;
  std::string parseStr;
  check(tag.ParseJson(control, parseStr),
        "control: viewing conditions ranges and largest Reserved load");

  auto expectBad = [&](const char *section, const char *field,
                       const IccJson &value, const char *reason,
                       const char *label) {
    IccJson bad = control;
    bad[section][field] = value;
    CIccTagJsonSpectralViewingConditions parsed;
    std::string error;
    check(!parsed.ParseJson(bad, error), label);
    std::string reasonLabel = std::string(label) + ": reason identifies field";
    check(has(error, reason), reasonLabel.c_str());
  };

  expectBad("IlluminantSPD", "start", "wrong-type", "Invalid IlluminantSPD range",
            "IlluminantSPD wrong-type start is refused");
  expectBad("ObserverFuncs", "end", "wrong-type", "Invalid ObserverFuncs range",
            "ObserverFuncs wrong-type end is refused");
  expectBad("IlluminantSPD", "steps", 3.5, "Invalid IlluminantSPD range",
            "IlluminantSPD fractional steps are refused");
  expectBad("ObserverFuncs", "steps", 3.5, "Invalid ObserverFuncs range",
            "ObserverFuncs fractional steps are refused");
  expectBad("IlluminantSPD", "Reserved", 65536, "Invalid IlluminantSPD Reserved",
            "IlluminantSPD over-wide Reserved is refused");
  expectBad("ObserverFuncs", "Reserved", 65536, "Invalid ObserverFuncs Reserved",
            "ObserverFuncs over-wide Reserved is refused");

  IccJson bad = control;
  bad["IlluminantXYZ"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonSpectralViewingConditions>(bad, "Invalid IlluminantXYZ",
    "spectralViewingConditionsType refuses mixed-type IlluminantXYZ");
  bad = control;
  bad["SurroundXYZ"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonSpectralViewingConditions>(bad, "Invalid SurroundXYZ",
    "spectralViewingConditionsType refuses mixed-type SurroundXYZ");
}

void testSpectralElementNumbers()
{
  const IccJson control = IccJson::parse(
    "{\"inputChannels\":3,\"outputChannels\":3,"
    "\"wavelengths\":{\"start\":380,\"end\":780,\"steps\":2},"
    "\"whiteData\":[1.0,1.0],"
    "\"matrixData\":[1.0,0.0,0.0,0.0,1.0,0.0]}", nullptr, false);
  check(!control.is_discarded(), "spectral matrix control is valid JSON");
  if (control.is_discarded())
    return;

  CIccMpeJsonEmissionMatrix matrix;
  std::string parseStr;
  check(matrix.ParseJson(control, parseStr),
        "control: a two-step spectral matrix loads");

  auto expectBad = [&](const char *field, const IccJson &value,
                       const char *label) {
    IccJson bad = control;
    bad["wavelengths"][field] = value;
    CIccMpeJsonEmissionMatrix parsed;
    std::string error;
    check(!parsed.ParseJson(bad, error), label);
    std::string reasonLabel = std::string(label) + ": reason names wavelengths";
    check(has(error, "spectral range wavelengths"), reasonLabel.c_str());
  };

  expectBad("start", "wrong-type", "spectral matrix wrong-type start is refused");
  expectBad("end", "wrong-type", "spectral matrix wrong-type end is refused");
  expectBad("steps", 2.5, "spectral matrix fractional steps are refused");
}

void testNamedColorDeviceCount()
{
  const IccJson control = IccJson::parse(
    "{\"countOfDeviceCoords\":4,\"vendorFlag\":0,\"colors\":["
    "{\"name\":\"Example\",\"pcsCoords\":[0.1,0.2,0.3],"
    "\"deviceCoords\":[1,2,3,4]}]}", nullptr, false);
  check(!control.is_discarded(), "named color control is valid JSON");
  if (control.is_discarded())
    return;

  CIccTagJsonNamedColor2 tag;
  std::string parseStr;
  check(tag.ParseJson(control, parseStr),
        "control: four named color device coordinates load");

  auto expectBad = [&](const IccJson &value, const char *label) {
    IccJson bad = control;
    bad["countOfDeviceCoords"] = value;
    CIccTagJsonNamedColor2 parsed;
    std::string error;
    check(!parsed.ParseJson(bad, error), label);
    std::string reasonLabel = std::string(label) + ": reason names count";
    check(has(error, "Invalid countOfDeviceCoords"), reasonLabel.c_str());
  };

  expectBad("wrong-type", "named color wrong-type device count is refused");
  expectBad(4.5, "named color fractional device count is refused");
  IccJson missing = control;
  missing.erase("countOfDeviceCoords");
  CIccTagJsonNamedColor2 parsed;
  std::string error;
  check(!parsed.ParseJson(missing, error), "named color missing device count is refused");
  check(has(error, "Invalid countOfDeviceCoords"),
        "named color missing count reason names count");

  IccJson bad = control;
  bad["colors"][0]["pcsCoords"][0] = "wrong-type";
  expectTagRefused<CIccTagJsonNamedColor2>(bad, "Invalid pcsCoords",
    "named color refuses mixed-type PCS coordinates");
  bad = control;
  bad["colors"][0]["pcsCoords"].push_back(0.4);
  expectTagRefused<CIccTagJsonNamedColor2>(bad, "Invalid pcsCoords",
    "named color refuses long PCS coordinates");

  const IccJson badDeviceValues[] = {-1, 65536, 1.5, "wrong-type"};
  const char *badDeviceLabels[] = {
    "negative", "over-wide", "fractional", "wrong-type"
  };
  for (size_t i = 0; i < sizeof(badDeviceValues) / sizeof(badDeviceValues[0]); i++) {
    bad = control;
    bad["colors"][0]["deviceCoords"][0] = badDeviceValues[i];
    std::string label = std::string("named color refuses ") + badDeviceLabels[i] +
                        " device coordinate";
    expectTagRefused<CIccTagJsonNamedColor2>(bad, "Invalid deviceCoords value",
                                             label.c_str());
  }
  bad = control;
  bad["colors"][0]["deviceCoords"].erase(3);
  expectTagRefused<CIccTagJsonNamedColor2>(bad, "Invalid deviceCoords count",
    "named color refuses too few device coordinates");
  bad = control;
  bad["colors"][0]["deviceCoords"].push_back(5);
  expectTagRefused<CIccTagJsonNamedColor2>(bad, "Invalid deviceCoords count",
    "named color refuses too many device coordinates");

  IccJson boundaries = control;
  boundaries["colors"][0]["deviceCoords"] = IccJson::array({0, 65535, 0, 65535});
  expectTagLoads<CIccTagJsonNamedColor2>(boundaries,
    "control: named color device coordinate boundaries load");
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

  testFixedNumericArrays();
  testHeaderTypes();
  testSpectralSteps();
  testViewingConditionsNumbers();
  testSpectralElementNumbers();
  testNamedColorDeviceCount();
  testTagsEntry();
  testMpeElements();

  if (g_fail) {
    std::fprintf(stderr, "[json-reader-strictness] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[json-reader-strictness] PASS\n");
  return 0;
}
