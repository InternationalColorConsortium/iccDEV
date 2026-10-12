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
    File:       xml-curve-identity-size-one.cpp

    Contains:   CTest helper for the IdentitySize="1" form of a curveType in
                CIccTagXmlCurve (#2806).

    A one-entry curveType holds a gamma, not a table.  CIccTagCurve::IsIdentity()
    reports such a curve as the identity when its gamma is 1.0, and
    CIccTagXmlCurve::ToXml() writes every identity curve as

        <Curve IdentitySize="N"/>

    so a gamma 1.0 curve was written as IdentitySize="1" from the 2015 import
    on.  The reader rebuilt an identity curve as the ramp j/(N-1), which is 0/0
    for one entry, and #1095 stopped that division by refusing N <= 1.  From then
    on iccFromXml could not read what iccToXml wrote for a gamma 1.0 curve.  The
    grayTRCTag of the "Linear Gray" profile Apple embeds in HEIF images is such a
    curve, which is how #2806 was found.

    Both ends changed.  The reader takes IdentitySize="1" as one entry holding
    gamma 1.0, so XML already written loads.  Under the 16-bit encodings, which
    a curveType tag and a lutAToBType curve use, the writer spells a one-entry
    curve as its number, as it does every other gamma, so its output also loads
    in an iccFromXml built before the fix.  The 8-bit and float encodings cannot
    hold a u8Fixed8Number exactly, so there it keeps the shorthand.

    The reader has three copies of the branch, one for each encoding the caller
    names (8-bit, 16-bit and variable, float).  A curveType tag only ever reaches
    the 16-bit copy, so the test also calls ParseXml() with each encoding in
    turn; a fix applied to one copy only would otherwise pass.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <libxml/parser.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int check(bool condition, const std::string& label)
{
  if (condition) {
    std::fprintf(stdout, "xml-curve-identity-size-one: PASS  %s\n", label.c_str());
    return 0;
  }

  std::fprintf(stderr, "xml-curve-identity-size-one: FAIL  %s\n", label.c_str());
  return 1;
}

// The gamma of a one-entry curve as the u8Fixed8Number the profile stores.  The
// curve keeps it normalized by 65535, so 1.0 is 256 here.
static unsigned rawGamma(CIccTagCurve& curve)
{
  return curve.GetSize() == 1 ? icFtoU16(curve[0]) : 0xFFFFFFFFu;
}

// Hand one <Curve> element to CIccTagXmlCurve::ParseXml() under the encoding
// named, the way the curveType, lut8Type, lut16Type and lutAToBType readers do.
static bool parseCurve(const std::string& curveElem, icConvertType nType,
                       CIccTagXmlCurve& curve)
{
  std::string doc = "<curveType>" + curveElem + "</curveType>";
  xmlDoc* pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "curve.xml", NULL,
                               XML_PARSE_NONET);
  if (!pDoc)
    return false;

  xmlNode* pRoot = xmlDocGetRootElement(pDoc);
  std::string parseStr;
  bool rv = pRoot && curve.ParseXml(pRoot->children, nType, parseStr);

  xmlFreeDoc(pDoc);
  return rv;
}

// The profile from #2806: the v2 "Linear Gray" monitor profile as iccToXml
// writes it.  Only the <Curve> element varies.
static std::string profileXml(const std::string& grayCurve)
{
  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <PreferredCMMType>appl</PreferredCMMType>\n"
    "    <ProfileVersion>2.10</ProfileVersion>\n"
    "    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n"
    "    <DataColourSpace>GRAY</DataColourSpace>\n"
    "    <PCS>XYZ </PCS>\n"
    "    <CreationDateTime>2016-01-01T00:00:00</CreationDateTime>\n"
    "    <RenderingIntent>Perceptual</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.964202881\" Y=\"1\" Z=\"0.824905396\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <profileDescriptionTag><textDescriptionType>"
    "<TextData><![CDATA[Linear Gray]]></TextData>"
    "</textDescriptionType></profileDescriptionTag>\n"
    "    <copyrightTag><textType>"
    "<TextData><![CDATA[none]]></TextData>"
    "</textType></copyrightTag>\n"
    "    <mediaWhitePointTag><XYZArrayType>"
    "<XYZNumber X=\"0.964202881\" Y=\"1\" Z=\"0.824905396\"/>"
    "</XYZArrayType></mediaWhitePointTag>\n"
    "    <grayTRCTag><curveType>\n      " + grayCurve + "\n    </curveType></grayTRCTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

// A lutBToAType whose B curves are one-entry curves, the other place a profile
// carries them.  Its curves are read and written under the variable encoding.
static std::string lutProfileXml(const std::string& curve)
{
  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <PreferredCMMType>ICCD</PreferredCMMType>\n"
    "    <ProfileVersion>4.40</ProfileVersion>\n"
    "    <ProfileDeviceClass>spac</ProfileDeviceClass>\n"
    "    <DataColourSpace>XYZ </DataColourSpace>\n"
    "    <PCS>XYZ </PCS>\n"
    "    <CreationDateTime>2026-10-12T00:00:00</CreationDateTime>\n"
    "    <RenderingIntent>Relative Colorimetric</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <BToA0Tag><lutBtoAType>\n"
    "      <Channels InputChannels=\"3\" OutputChannels=\"3\"/>\n"
    "      <BCurves>\n"
    "        " + curve + "\n"
    "        " + curve + "\n"
    "        " + curve + "\n"
    "      </BCurves>\n"
    "    </lutBtoAType></BToA0Tag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

// True when every B curve of the profile's BToA0Tag is one entry holding gamma
// 1.0.
static bool lutCurvesAreGammaOne(CIccProfile& profile)
{
  CIccMBB* pLut = dynamic_cast<CIccMBB*>(profile.FindTag(icSigBToA0Tag));
  if (!pLut || !pLut->GetCurvesB())
    return false;

  for (int i = 0; i < 3; i++) {
    CIccTagCurve* pCurve = dynamic_cast<CIccTagCurve*>(pLut->GetCurvesB()[i]);
    if (!pCurve || rawGamma(*pCurve) != 256)
      return false;
  }
  return true;
}

static bool writeFile(const std::string& path, const std::string& text)
{
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
  f << text;
  return f.good();
}

// The gamma a loaded profile's grayTRCTag holds, or a value no gamma can take
// when the tag is missing or is not a one-entry curve.
static unsigned grayGamma(CIccProfile& profile)
{
  CIccTagCurve* pCurve = dynamic_cast<CIccTagCurve*>(profile.FindTag(icSigGrayTRCTag));
  return pCurve ? rawGamma(*pCurve) : 0xFFFFFFFFu;
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
    return 1;
  }

  std::error_code ec;
  std::filesystem::create_directories(argv[1], ec);
  std::filesystem::current_path(argv[1], ec);
  if (ec) {
    std::fprintf(stderr, "xml-curve-identity-size-one: cannot use scratch dir '%s'\n", argv[1]);
    return 1;
  }

  // Same two factories iccFromXml pushes before LoadXml: without them
  // <curveType> does not resolve to CIccTagXmlCurve and every load fails.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  int failures = 0;

  const struct {
    icConvertType nType;
    const char* szName;
  } encodings[] = {
    { icConvert8Bit, "8-bit" },
    { icConvert16Bit, "16-bit" },
    { icConvertVariable, "variable" },
    { icConvertFloat, "float" },
  };

  for (const auto& enc : encodings) {
    const std::string name = enc.szName;

    // The defect: what the writer emits for a gamma 1.0 curve, the reader
    // refused.  Read back, it has to be the same one-entry gamma 1.0 curve,
    // not merely something that parses.
    {
      CIccTagXmlCurve written;
      std::string xml;
      bool wrote = written.SetGamma(1.0f) && written.ToXml(xml, enc.nType, "");

      CIccTagXmlCurve read;
      bool parsed = wrote && parseCurve(xml, enc.nType, read);
      failures += check(parsed, name + ": the writer's form of a gamma 1.0 curve is read back");
      failures += check(parsed && rawGamma(read) == 256,
                        name + ": and is read back as one entry holding gamma 1.0");

      // The stored sample itself, not only its 16-bit rounding: an 8-bit
      // sample reads back as 257/256 and an eight-decimal float one ulp high,
      // which IsIdentity() no longer accepts.
      failures += check(parsed && read.GetSize() == 1 && read[0] == written[0],
                        name + ": and holds the sample the written curve held");

      // The number is the form an iccFromXml built before the fix reads too,
      // and under the 16-bit encodings it is the u8Fixed8Number itself.  The
      // 8-bit and float encodings keep the shorthand; the check above is what
      // shows why.
      bool shorthand = xml.find("IdentitySize") != std::string::npos;
      if (enc.nType == icConvert16Bit || enc.nType == icConvertVariable)
        failures += check(wrote && !shorthand,
                          name + ": a gamma 1.0 curve is written as its number");
      else
        failures += check(wrote && shorthand,
                          name + ": a gamma 1.0 curve is still written as IdentitySize=\"1\"");
    }

    // A table that is the identity keeps the shorthand under every encoding.
    {
      CIccTagXmlCurve written;
      std::string xml;
      bool wrote = written.SetSize(256, icInitIdentity) && written.ToXml(xml, enc.nType, "");

      CIccTagXmlCurve read;
      failures += check(wrote && xml.find("IdentitySize=\"256\"") != std::string::npos &&
                        parseCurve(xml, enc.nType, read) && read.GetSize() == 256 &&
                        read.IsIdentity(),
                        name + ": a 256-entry identity table is still written as IdentitySize=\"256\"");
    }

    // The form every iccToXml since 2015 wrote, so XML already on disk loads
    // whatever the writer does in future.
    {
      CIccTagXmlCurve read;
      bool parsed = parseCurve("<Curve IdentitySize=\"1\"/>", enc.nType, read);
      failures += check(parsed && rawGamma(read) == 256,
                        name + ": IdentitySize=\"1\" is one entry holding gamma 1.0");
    }

    // The boundary below, so the fix cannot be "accept any size": the writer
    // spells an empty curve <Curve/>, never IdentitySize="0".
    {
      CIccTagXmlCurve read;
      failures += check(!parseCurve("<Curve IdentitySize=\"0\"/>", enc.nType, read),
                        name + ": IdentitySize=\"0\" is still refused");
    }

    // The boundary above: two entries is the smallest table, and still the ramp.
    {
      CIccTagXmlCurve read;
      bool parsed = parseCurve("<Curve IdentitySize=\"2\"/>", enc.nType, read);
      failures += check(parsed && read.GetSize() == 2 && read[0] == 0.0f && read[1] == 1.0f,
                        name + ": IdentitySize=\"2\" is still the two-entry ramp");
    }
  }

  // A one-entry curve that is not the identity never took the IdentitySize
  // path and must be unaffected: gamma 2.20703125 is 565/256.
  {
    CIccTagXmlCurve written;
    std::string xml;
    bool wrote = written.SetGamma(2.20703125f) && written.ToXml(xml, icConvert16Bit, "");

    CIccTagXmlCurve read;
    failures += check(wrote && xml.find("IdentitySize") == std::string::npos &&
                      parseCurve(xml, icConvert16Bit, read) && rawGamma(read) == 565,
                      "a gamma 2.20703125 curve is written as its number and read back");
  }

  // The tool path of #2806: the profile as iccToXml wrote it has to load, and
  // has to survive a second trip through XML with the gamma still 1.0.  The
  // second document carries the gamma as a number.
  if (!writeFile("linear-gray.xml", profileXml("<Curve IdentitySize=\"1\"/>"))) {
    std::fprintf(stderr, "xml-curve-identity-size-one: could not generate fixtures\n");
    return 1;
  }

  {
    CIccProfileXml profile;
    std::string parseStr;
    bool loaded = profile.LoadXml("linear-gray.xml", "", &parseStr);
    failures += check(loaded, "the #2806 profile loads");
    failures += check(loaded && grayGamma(profile) == 256,
                      "the #2806 profile's grayTRCTag is one entry holding gamma 1.0");

    std::string xml;
    bool rewritten = loaded && profile.ToXml(xml) && writeFile("linear-gray-2.xml", xml);
    failures += check(rewritten && xml.find("IdentitySize") == std::string::npos,
                      "the #2806 profile is written back with its gamma as a number");

    CIccProfileXml again;
    std::string parseStr2;
    bool reloaded = rewritten && again.LoadXml("linear-gray-2.xml", "", &parseStr2);
    failures += check(reloaded && grayGamma(again) == 256,
                      "the #2806 profile keeps gamma 1.0 through a second XML round trip");
  }

  // The same through a lutBToAType, whose curves take the variable encoding
  // rather than the tag's own.
  if (!writeFile("lut-gamma-one.xml", lutProfileXml("<Curve IdentitySize=\"1\"/>"))) {
    std::fprintf(stderr, "xml-curve-identity-size-one: could not generate fixtures\n");
    return 1;
  }

  {
    CIccProfileXml profile;
    std::string parseStr;
    bool loaded = profile.LoadXml("lut-gamma-one.xml", "", &parseStr);
    failures += check(loaded && lutCurvesAreGammaOne(profile),
                      "a lutBToAType's IdentitySize=\"1\" curves load as gamma 1.0");

    std::string xml;
    bool rewritten = loaded && profile.ToXml(xml) && writeFile("lut-gamma-one-2.xml", xml);
    failures += check(rewritten && xml.find("IdentitySize") == std::string::npos,
                      "and are written back with the gamma as a number");

    CIccProfileXml again;
    std::string parseStr2;
    bool reloaded = rewritten && again.LoadXml("lut-gamma-one-2.xml", "", &parseStr2);
    failures += check(reloaded && lutCurvesAreGammaOne(again),
                      "and keep gamma 1.0 through a second XML round trip");
  }

  return failures ? 1 : 0;
}
