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
    File:       calc-channel-ref-unterminated-index-json.cpp

    Contains:   CTest helper for CIccMpeJsonCalculator::Flatten()'s in{}/out{}
                channel-reference index parsing (#2323).

    The JSON calculator carries a byte-for-byte copy of the defect fixed in
    CIccMpeXmlCalculator::Flatten(): it scans an index selector for its closing
    bracket and then steps one past it,

        for (p = select.c_str() + 1; *p && *p != ')' && *p != ']'; p++);
        select = p + 1;

    so an unterminated index leaves p on the NUL terminator and the assignment
    runs strlen from one past it.  The three length regimes are the same as on
    the XML side -- silently accepted at 14 characters or fewer, a
    1-byte-read-stack-buffer-overflow at exactly 15 where the selector fills the
    string's inline SSO buffer, a heap overflow at 16 or more.

    This is a SEPARATE reachable entry point, not a duplicate of the XML test:
    CIccMpeJsonCalculator is registered for icSigCalculatorElemType in
    IccMpeJsonFactory.cpp, so iccFromJson on a hand-authored document reaches it
    without any XML involved.  Both "in{Lab[XXXX" and "in{Lab(XXXX" arrive
    here, and both are asserted below.  (The XML copy once admitted only the
    first, through a select[1] typo in its entry test; that was corrected as the
    follow-up deferred from #2365, and the XML helper now pins the same two.)

    Registered separately from the XML helper rather than folded into it so that
    neither can mask the other: this one is skipped where IccJSON is not built,
    and the XML coverage does not disappear with it.

    It also covers the channel NAME map those references resolve against
    (#2829).  CIccMpeJsonCalculator::ParseChanMap() reads inputNames and
    outputNames, where "Lab[3]" declares a group three channels wide, and it
    carried the XML reader's bound,

        if (size < 0 || i + size > nChannels) return false;

    over an atoi() result, so a width near INT_MAX overflowed int before the
    comparison.  UBSan reports the addition; without a sanitizer the sum wraps
    negative, the test passes, and the document LOADS with a group 2147483647
    channels wide.  That second behaviour is what red/greens on every lane.  The
    cases use a function that addresses channels by number, so the refusal can
    only come from the name map.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccProfileJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccTagMPE.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "calc-channel-ref-unterminated-index-json: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "calc-channel-ref-unterminated-index-json: FAIL  %s\n", label);
  return 1;
}

// The same seven-in / three-out named calculator the XML helper uses, so "Lab"
// is a three-wide group at offset 4.  mainFunction varies, and the #2829 cases
// replace the two name lists as well.
static const char* const kInputNames = "C M Y K Lab[3]";
static const char* const kOutputNames = "a b L";

static std::string profileJson(const std::string& mainFunction,
                               const std::string& inputNames = kInputNames,
                               const std::string& outputNames = kOutputNames)
{
  return
    "{\n"
    "  \"IccProfile\": {\n"
    "    \"Header\": {\n"
    "      \"ProfileVersion\": \"5.00\",\n"
    "      \"ProfileDeviceClass\": \"spac\",\n"
    "      \"DataColourSpace\": \"7CLR\",\n"
    "      \"PCS\": \"XYZ \",\n"
    "      \"CreationDateTime\": \"2026-09-02T00:00:00\",\n"
    "      \"RenderingIntent\": \"Relative\",\n"
    "      \"PCSIlluminant\": [0.9642, 1.0, 0.8249]\n"
    "    },\n"
    "    \"Tags\": [\n"
    "      {\n"
    "        \"AToB1Tag\": {\n"
    "          \"data\": {\n"
    "            \"type\": \"multiProcessElementType\",\n"
    "            \"inputChannels\": 7,\n"
    "            \"outputChannels\": 3,\n"
    "            \"elements\": [\n"
    "              {\n"
    "                \"type\": \"CalculatorElement\",\n"
    "                \"inputChannels\": 7,\n"
    "                \"outputChannels\": 3,\n"
    "                \"inputNames\": \"" + inputNames + "\",\n"
    "                \"outputNames\": \"" + outputNames + "\",\n"
    "                \"mainFunction\": \"" + mainFunction + "\"\n"
    "              }\n"
    "            ]\n"
    "          }\n"
    "        }\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "}\n";
}

static bool writeFile(const std::string& path, const std::string& text)
{
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
  f << text;
  return f.good();
}

// Returns whether the document parsed.  A sanitizer abort inside here IS the
// defect on an instrumented lane: the caller never gets its result back.
static bool loadJson(CIccProfileJson& profile, const char* file, std::string& parseStr)
{
  return profile.LoadJson(file, &parseStr);
}

// n filler characters, none of which closes a bracket, so the scan runs to the
// terminator.  The selector is the bracket plus these, so pass n = length - 1.
static std::string filler(unsigned n)
{
  return std::string(n, 'X');
}

static int expectRefused(const std::string& mainFunction, const char* file, const char* label,
                         const char* needle = "Unterminated index")
{
  if (!writeFile(file, profileJson(mainFunction)))
    return check(false, label);

  CIccProfileJson profile;
  std::string parseStr;
  bool loaded = loadJson(profile, file, parseStr);
  bool named = parseStr.find(needle) != std::string::npos;

  return check(!loaded && named, label);
}

static int expectParsed(const std::string& mainFunction, const char* file, const char* label,
                        const std::string& inputNames = kInputNames)
{
  if (!writeFile(file, profileJson(mainFunction, inputNames)))
    return check(false, label);

  CIccProfileJson profile;
  std::string parseStr;
  return check(loadJson(profile, file, parseStr), label);
}

// A function that addresses channels by number and names none of them, so a
// document built from it is accepted or refused on its name lists alone.
static const char* const kNumberedFunction = "{\\nin[0]\\nout[2]\\n}\\n";

// Write one case with the given name lists and require ParseChanMap()'s refusal,
// which the caller reports against the key the list came from.
static int expectNamesRefused(const std::string& inputNames, const std::string& outputNames,
                              const char* file, const char* needle, const char* label)
{
  if (!writeFile(file, profileJson(kNumberedFunction, inputNames, outputNames)))
    return check(false, label);

  CIccProfileJson profile;
  std::string parseStr;
  bool loaded = loadJson(profile, file, parseStr);
  bool named = parseStr.find(needle) != std::string::npos;

  return check(!loaded && named, label);
}

// The flattened function, as Describe() prints it.  The XML helper's header
// calls a bare "it loaded" assertion worthless for this defect, and it is worth
// exactly as little here: the claim these two files jointly make is that the two
// parsers agree on WHICH CHANNEL a parenthesised index selects, and only the
// flattened function says that.  Without this the JSON side could regain the
// same slip -- select[1] for select[0] -- and stay green while reading channel 4.
static std::string flattenedFunction(const char* file)
{
  CIccProfileJson profile;
  std::string parseStr;
  if (!loadJson(profile, file, parseStr))
    return "<did not load: " + parseStr.substr(0, 120) + ">";

  CIccTagMultiProcessElement* mpe =
      dynamic_cast<CIccTagMultiProcessElement*>(profile.FindTag(icSigAToB1Tag));
  if (!mpe || mpe->NumElements() < 1)
    return "<no A2B1 multiProcessElement>";

  std::string text;
  mpe->GetElement(0)->Describe(text, 100);
  const std::string begin = "BEGIN_CALC_FUNCTION\n";
  size_t b = text.find(begin);
  size_t e = text.find("\nEND_CALC_FUNCTION", b);
  if (b == std::string::npos || e == std::string::npos)
    return "<no BEGIN_CALC_FUNCTION marker in Describe() output>";
  b += begin.size();
  return text.substr(b, e - b);
}

static int expectFlatten(const std::string& mainFunction, const char* file,
                         const char* expected, const char* label)
{
  if (!writeFile(file, profileJson(mainFunction)))
    return check(false, label);

  std::string flat = flattenedFunction(file);
  const bool ok = flat.find(expected) != std::string::npos;
  if (!ok)
    std::fprintf(stderr,
                 "calc-channel-ref-unterminated-index-json:       got '%s'  expected to contain '%s'\n",
                 flat.c_str(), expected);
  return check(ok, label);
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
    std::fprintf(stderr,
                 "calc-channel-ref-unterminated-index-json: cannot use scratch dir '%s'\n",
                 argv[1]);
    return 1;
  }

  // The factories iccFromJson pushes before LoadJson.  Without them no
  // CIccMpeJsonCalculator is ever constructed and every load fails for want of
  // an element handler, which would make the "is refused" assertions pass
  // without the code under test being entered.  The controls catch that.
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  int failures = 0;

  // The three length regimes.  The first is the one that red/greens without a
  // sanitizer: before the fix that document parsed.
  failures += expectRefused("{\\nin{Lab[" + filler(1) + "\\nout{L}\\n}\\n",
                            "json-unterminated-sso.json",
                            "selector inside the SSO buffer is refused, not silently accepted");

  failures += expectRefused("{\\nin{Lab[" + filler(14) + "\\nout{L}\\n}\\n",
                            "json-unterminated-stack.json",
                            "selector of exactly 15 (the stack overflow) is refused");

  failures += expectRefused("{\\nin{Lab[" + filler(39) + "\\nout{L}\\n}\\n",
                            "json-unterminated-heap.json",
                            "selector past the SSO buffer (heap overflow) is refused");

  // The '(' spelling.  Reaches the block here, and -- since the select[1] typo
  // was corrected -- on the XML side too; each helper pins its own parser.
  failures += expectRefused("{\\nin{Lab(" + filler(14) + "\\nout{L}\\n}\\n",
                            "json-unterminated-paren.json",
                            "the '(' spelling of an unterminated index is refused here too");

  failures += expectRefused("{\\nin{C}\\nout{a[" + filler(14) + "\\n}\\n",
                            "json-unterminated-out.json",
                            "out{} references are guarded by the same check");

  // The same int-overflow in this file's copy of the offset/size bound: the sum
  // wrapped negative and passed the test meant to refuse it.
  failures += expectRefused("{\\nin{Lab[2000000000],2000000000}\\nout{L}\\n}\\n",
                            "json-overflowing-offset-size.json",
                            "an offset/size pair that overflows the bound is refused by it",
                            "Invalid 'in' channel offset/size");

  // Controls, so that refusing every indexed reference cannot satisfy the above.
  failures += expectParsed("{\\nin{Lab[1],2}\\nout{a,2}\\n}\\n",
                           "json-wellformed-offset-size.json",
                           "in{Lab[1],2} still parses");

  failures += expectParsed("{\\nin{Lab[0]}\\nout{L}\\n}\\n",
                           "json-wellformed-offset.json",
                           "in{Lab[0]} still parses");

  // The '(' spelling in its well-formed form: this branch is live here, so it
  // must keep working rather than being refused along with the bad ones.
  //
  // Written as "(1)" and matched with a single-channel out{}, deliberately.
  // "in{Lab(1,2)}" would NOT be a control: the size is only read from a ",size"
  // that FOLLOWS the closing bracket, so the "2" inside the parentheses is
  // discarded, the operator flattens to in(5,1), and the function then fails on
  // stack balance rather than on anything this fix touches.  That is a deferred
  // defect of the same shape the select[1] typo had -- an index quietly narrowed
  // instead of refused -- and pinning it here would assert today's wrong behaviour.
  // Not expectParsed: this is the assertion the XML helper's parity claim rests
  // on, so it names the channel.  "Lab" is at offset 4, so index 1 is channel 5 --
  // the same in[5] the XML helper requires of both of its spellings.
  failures += expectFlatten("{\\nin{Lab(1)}\\nout{L}\\n}\\n",
                            "json-wellformed-paren.json",
                            "in[5]",
                            "in{Lab(1)} selects channel 5 here, as it now does in XML");

  failures += expectFlatten("{\\nin{Lab[1]}\\nout{L}\\n}\\n",
                            "json-wellformed-bracket.json",
                            "in[5]",
                            "in{Lab[1]} selects channel 5 here too");

  // #2829, the channel name map.  The control comes first because the cases
  // after it rest on it: with the stock name lists the numbered function loads,
  // so a refusal below is the name list's and not the function's.  It is also the
  // exact-fit case, "Lab[3]" with four channels before it out of seven, which a
  // bound tightened by one would refuse.
  failures += expectParsed(kNumberedFunction,
                           "json-names-wellformed.json",
                           "the numbered function loads with a group that reaches the last channel");

  // The reported shape: the last group's width is INT_MAX, so "4 + 2147483647"
  // overflowed before it was compared.  Before the fix this document loaded.
  failures += expectNamesRefused("C M Y K Lab[2147483647]", kOutputNames,
                                 "json-names-input-width-overflow.json",
                                 "Invalid name for inputChannels",
                                 "an input group whose width overflows the bound is refused");

  // The same width on a group that another name follows.  The wrapped comparison
  // let it through, and "i += size" at the following space then overflowed a
  // second time, leaving the next name at a negative channel index.
  failures += expectNamesRefused("C M[2147483647] Y", kOutputNames,
                                 "json-names-input-width-overflow-middle.json",
                                 "Invalid name for inputChannels",
                                 "an overflowing width on a group in mid-list is refused");

  // outputNames goes through the same function with the output count.
  failures += expectNamesRefused(kInputNames, "a b L[2147483647]",
                                 "json-names-output-width-overflow.json",
                                 "Invalid name for outputChannels",
                                 "an output group whose width overflows the bound is refused");

  // One channel past the end, where nothing overflows.  Refused before and after:
  // it pins that the rewritten comparison kept the bound where it was.
  failures += expectNamesRefused("C M Y K Lab[4]", kOutputNames,
                                 "json-names-input-width-one-over.json",
                                 "Invalid name for inputChannels",
                                 "a group one channel wider than the channels left is still refused");

  // A group in mid-list that is accepted, so the next name starts past it: "B"
  // begins at channel 3 and its four channels end exactly at the seventh.
  failures += expectParsed(kNumberedFunction,
                           "json-names-two-groups.json",
                           "two groups that fill the channels load",
                           "A[3] B[4]");

  return failures ? 1 : 0;
}
