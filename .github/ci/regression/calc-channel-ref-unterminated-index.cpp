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
    File:       calc-channel-ref-unterminated-index.cpp

    Contains:   CTest helper for CIccMpeXmlCalculator::Flatten()'s in{}/out{}
                channel-reference index parsing (#2323).

    Flatten() splits a channel reference such as "Lab[1],2" into a root name and
    an index selector, then walks the selector looking for the closing bracket:

        for (ptr = select.c_str() + 1; *ptr && *ptr != ')' && *ptr != ']'; ptr++);
        select = ptr + 1;

    When the reference has no closing bracket the loop stops on the NUL
    terminator instead, so "ptr + 1" addresses one past it and the assignment
    runs strlen() from there.  How that presents depends only on how long the
    selector is, because it decides where the string keeps its bytes:

      <= 14 chars  no diagnostic at all.  The read stays inside libstdc++'s
                   16-byte inline SSO buffer, and the malformed reference is
                   ACCEPTED -- "in{Lab[XX" resolves quietly to in(4).
         15 chars  the selector exactly fills the SSO buffer, so the read leaves
                   the enclosing stack frame: AddressSanitizer reports a
                   1-byte-read-stack-buffer-overflow, scariness 27.  This is the
                   case the issue was filed on.
      >= 16 chars  the string has moved to the heap, so the same read is a
                   1-byte-read-heap-buffer-overflow, scariness 12.

    The reported case is therefore the narrowest of the three -- it needs the
    selector to be exactly one length -- and it is the only one a sanitizer lane
    can see.  The <= 14 case is the one that red/greens everywhere, sanitizer or
    not, because before the fix it PARSES: that is what keeps this test from
    being vacuous on the lanes that carry no instrumentation.

    Two spellings reach the block, "in{Lab[1]}" and "in{Lab(1)}".  The second
    only does so since the select[1] typo in its entry test was corrected (the
    follow-up deferred from #2365).  That one-character change moves FIVE input
    families, in both directions, and every one of them is pinned below --
    measured against a build of each side, 7-in/3-out, "Lab" at offset 4:

      document              before          after
      in{Lab(1)}            in[4]           in[5]     the intended fix
      in{Lab(XXXX (unterm)  LOADED in[4]    REFUSED   by the #2323 guard
      in{Lab(1),2}          REFUSED         in[5,2]   parity with "[1],2"
      in{C,(3)}             LOADED in[0]    REFUSED   acceptance regression
      in{C,(XXXX (unterm)   REFUSED here    REFUSED downstream

    The last two are the ",(" family: a selector whose first character is a
    comma, which is what the typo's second-character test used to admit.  Their
    refusal is no longer this guard's, so they are kept as cases rather than
    dropped along with the typo -- see the notes on each.

    Describe() text is the oracle wherever two spellings must agree, because a
    load that merely succeeds is exactly what the typo produced: both documents
    parsed either way, and only the channel differed.

    The controls matter as much as the failures: an "unterminated index is
    refused" assertion is satisfied just as well by refusing every indexed
    reference, so the well-formed forms the corpus actually uses are pinned
    alongside.

    It also covers the channel NAME map those references resolve against
    (#2829).  CIccMpeXmlCalculator::ParseChanMap() reads InputNames and
    OutputNames, where "Lab[3]" declares a group three channels wide, and it
    bounded the group with

        if (size<0 || i + size>nChannels)

    over an atoi() result, so a width near INT_MAX overflowed int before the
    comparison.  UBSan reports the addition; without a sanitizer the sum wraps
    negative, the test passes, and the document LOADS with a group 2147483647
    channels wide.  That second behaviour is what red/greens on every lane.  The
    cases use a function body that addresses channels by number, so the refusal
    can only come from the name map.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagMPE.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "calc-channel-ref-unterminated-index: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "calc-channel-ref-unterminated-index: FAIL  %s\n", label);
  return 1;
}

// A calculator element with seven named input channels and three named outputs,
// so that "Lab" resolves to a three-wide group at offset 4 and indexed forms
// have somewhere real to point.  The function body and the optional macro block
// vary, and the #2829 cases replace the two name lists as well.
static const char* const kInputNames = "C M Y K Lab[3]";
static const char* const kOutputNames = "a b L";

static std::string profileXml(const std::string& body, const std::string& macros = "",
                              const std::string& inputNames = kInputNames,
                              const std::string& outputNames = kOutputNames)
{
  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <ProfileVersion>5.0</ProfileVersion>\n"
    "    <ProfileDeviceClass>spac</ProfileDeviceClass>\n"
    "    <DataColourSpace>7CLR</DataColourSpace>\n"
    "    <PCS>XYZ </PCS>\n"
    "    <CreationDateTime>2026-09-02T00:00:00</CreationDateTime>\n"
    "    <RenderingIntent>Relative Colorimetric</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <multiProcessElementType>\n"
    "      <TagSignature>A2B1</TagSignature>\n"
    "      <MultiProcessElements InputChannels=\"7\" OutputChannels=\"3\">\n"
    "        <CalculatorElement InputChannels=\"7\" OutputChannels=\"3\"\n"
    "            InputNames=\"" + inputNames + "\" OutputNames=\"" + outputNames + "\">\n"
    + macros +
    "          <MainFunction>\n{\n" + body + "\n}\n          </MainFunction>\n"
    "        </CalculatorElement>\n"
    "      </MultiProcessElements>\n"
    "    </multiProcessElementType>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
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
static bool loadXml(CIccProfileXml& profile, const char* file, std::string& parseStr)
{
  return profile.LoadXml(file, "", &parseStr);
}

// n filler characters, none of which is a closing bracket, so the scan runs to
// the terminator.  The selector is "[" plus these, so pass n = length - 1.
static std::string filler(unsigned n)
{
  return std::string(n, 'X');
}

// Write one case and report whether it was refused with the guard's own message.
// Matching the text matters: "refused" on its own is also satisfied by an
// unrelated parse failure, and every case here is one character away from a
// document that parses.
static int expectRefused(const std::string& body, const char* file, const char* label,
                         const std::string& macros = "",
                         const char* needle = "Unterminated index")
{
  if (!writeFile(file, profileXml(body, macros)))
    return check(false, label);

  CIccProfileXml profile;
  std::string parseStr;
  bool loaded = loadXml(profile, file, parseStr);
  bool named = parseStr.find(needle) != std::string::npos;

  return check(!loaded && named, label);
}

static int expectParsed(const std::string& body, const char* file, const char* label,
                        const std::string& macros = "",
                        const std::string& inputNames = kInputNames)
{
  if (!writeFile(file, profileXml(body, macros, inputNames)))
    return check(false, label);

  CIccProfileXml profile;
  std::string parseStr;
  return check(loadXml(profile, file, parseStr), label);
}

// A function body that addresses channels by number and names none of them, so a
// document built from it is accepted or refused on its name lists alone.
static const char* const kNumberedBody = "in[0] out[2]";

// Write one case with the given name lists and require ParseChanMap()'s refusal,
// which the caller reports against the attribute the list came from.  It used to
// name InputChannels for both, so the needle is what tells the two calls apart.
static int expectNamesRefused(const std::string& inputNames, const std::string& outputNames,
                              const char* file, const char* needle, const char* label)
{
  if (!writeFile(file, profileXml(kNumberedBody, "", inputNames, outputNames)))
    return check(false, label);

  CIccProfileXml profile;
  std::string parseStr;
  bool loaded = loadXml(profile, file, parseStr);
  bool named = parseStr.find(needle) != std::string::npos;

  return check(!loaded && named, label);
}

// The flattened function the calculator element was built from, as Describe()
// prints it ("{ 0 0 in[5] out[0,3] }").  This is the only observable that
// separates "in{Lab(1)}" from "in{Lab[1]}": both LOAD, and under the typo both
// applied cleanly -- one of them on the wrong channel.
//
// Each way of not getting one is reported distinctly rather than collapsed into
// "": a fixture that stops loading for an unrelated reason, a tag lookup that
// breaks, and a Describe() marker that is reworded would otherwise all present
// as the same empty-string mismatch, and the diagnostic could not say which.
static std::string flattenedFunction(const char* file)
{
  CIccProfileXml profile;
  std::string parseStr;
  if (!loadXml(profile, file, parseStr))
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

// Write the same reference in both spellings and require the same function.
static int expectSameFlatten(const std::string& bracketBody, const std::string& parenBody,
                             const char* bracketFile, const char* parenFile,
                             const char* expected, const char* label)
{
  if (!writeFile(bracketFile, profileXml(bracketBody)) ||
      !writeFile(parenFile, profileXml(parenBody)))
    return check(false, label);

  std::string bracket = flattenedFunction(bracketFile);
  std::string paren = flattenedFunction(parenFile);
  const bool same = (bracket == paren) && bracket.find(expected) != std::string::npos;
  if (!same)
    std::fprintf(stderr,
                 "calc-channel-ref-unterminated-index:       bracket '%s'  paren '%s'  expected to contain '%s'\n",
                 bracket.c_str(), paren.c_str(), expected);
  return check(same, label);
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
                 "calc-channel-ref-unterminated-index: cannot use scratch dir '%s'\n",
                 argv[1]);
    return 1;
  }

  // The factories iccFromXml pushes before LoadXml.  Without them no
  // CIccMpeXmlCalculator is ever constructed, every load fails for want of an
  // element handler, and the "is refused" assertions below would all pass
  // without the code under test being entered.  The controls are what catch
  // that, which is why they are here and not merely for completeness.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  int failures = 0;

  // The three length regimes, smallest first.  The first is the one that
  // red/greens without a sanitizer: before the fix this document PARSED.
  failures += expectRefused("in{Lab[" + filler(1) + " out{L}",
                            "unterminated-sso.xml",
                            "selector inside the SSO buffer is refused, not silently accepted");

  failures += expectRefused("in{Lab[" + filler(14) + " out{L}",
                            "unterminated-stack.xml",
                            "selector of exactly 15 (the reported stack overflow) is refused");

  failures += expectRefused("in{Lab[" + filler(39) + " out{L}",
                            "unterminated-heap.xml",
                            "selector past the SSO buffer (heap overflow) is refused");

  // The '(' spelling of the same reference.  Before the select[1] typo was
  // corrected this document never entered the block at all -- the unterminated
  // index was ACCEPTED, as offset 0 -- so this case is red on the typo alone.
  failures += expectRefused("in{Lab(" + filler(14) + " out{L}",
                            "unterminated-paren.xml",
                            "the '(' spelling of an unterminated index is refused by the same guard");

  // The ",(" selector, which is where the typo used to send a reference whose
  // FIRST character is a comma.  Correcting the entry test moved this family out
  // of the block, so its refusal is no longer this guard's: the selector is
  // discarded, ",(XXXX" reaches the size branch as atoi("(XXXX") == 0, and the
  // flattened "in(0,0)" is refused by CIccFuncTokenizer::GetIndex, which rejects
  // a zero size.  Kept, rather than dropped with the typo that created it,
  // because the refusal now rests on a check nothing else here pins: were
  // GetIndex ever to accept in(n,0), this document would be silently accepted
  // with its malformed selector thrown away -- the #2323 class exactly.  The
  // needle is the downstream message, and the flattened text names WHERE.
  failures += expectRefused("in{C,(" + filler(13) + " out{L}",
                            "unterminated-comma-paren.xml",
                            "an unterminated ',(' index is still refused, now downstream",
                            "",
                            "Main Calculator Function from \"{ in(0,0)");

  // The same family, terminated.  This one CHANGED DIRECTION: it loaded before
  // the fix, as in(0,1) with the "(3)" silently discarded, and is refused now.
  // That is the narrowing standard #2323 applied, but it is an acceptance
  // regression for any hand-authored document using the spelling, so it is
  // stated here rather than left to be discovered.  No tracked XML uses it.
  failures += expectRefused("in{C,(3)} out{L}",
                            "terminated-comma-paren.xml",
                            "a terminated ',(' index, accepted before the fix, is refused now",
                            "",
                            "Main Calculator Function from \"{ in(0,0)");

  // Same guard from the out{} operator, which shares the code path and only
  // differs in which channel map it consults -- the message names the operator.
  failures += expectRefused("in{C} out{a[" + filler(14),
                            "unterminated-out.xml",
                            "out{} references are guarded by the same check");

  // The same bad reference inside a <Macro> body.  Flatten() recurses to expand a
  // macro and used to DISCARD that call's result, so every refusal raised inside a
  // macro -- this one included -- was written into parseStr and then dropped: the
  // offending operator vanished from the flattened function and LoadXml() still
  // returned true.  The guard above fired and the document was accepted anyway.
  // The JSON twin has always checked this return, so the XML side was the outlier.
  failures += expectRefused("#mm 0.5 out{L}",
                            "unterminated-macro.xml",
                            "a bad reference inside a macro body fails the load, not just parseStr",
                            "          <Macros>\n"
                            "            <Macro Name=\"mm\">in{Lab[" + filler(14) + "</Macro>\n"
                            "          </Macros>\n");

  // The offset/size bound two lines below the guard summed "first + offset + size"
  // as int over two atoi() results, so a large pair WRAPPED NEGATIVE and passed the
  // very test meant to refuse it -- 4 + 2000000000 + 2000000000 becomes -294967292.
  // UBSan reports it as signed integer overflow.  The value was refused downstream
  // by SetCalcFunc(), so this pins that it is now refused HERE, by its own guard,
  // and without the undefined behaviour on the way.
  failures += expectRefused("in{Lab[2000000000],2000000000} out{L}",
                            "overflowing-offset-size.xml",
                            "an offset/size pair that overflows the bound is refused by it",
                            "",
                            "Invalid 'in' operation channel offset or size");

  // Controls.  Every well-formed indexed spelling the tracked corpus uses must
  // still parse, or "refuse everything" would satisfy all five assertions above.
  failures += expectParsed("in{Lab[1],2} out{a,2}",
                           "wellformed-offset-size.xml",
                           "in{Lab[1],2} still parses");

  failures += expectParsed("in{Lab[0]} out{L}",
                           "wellformed-offset.xml",
                           "in{Lab[0]} still parses");

  // The control for the typo itself.  Both documents load before and after the
  // fix; what changed is WHICH channel the '(' form reads.  Lab sits at offset 4,
  // so index 1 is channel 5, and the bracket form is the reference the paren form
  // must agree with -- the same agreement the JSON parser has always had.
  failures += expectSameFlatten("in{Lab[1]} out{L}", "in{Lab(1)} out{L}",
                                "wellformed-bracket.xml", "wellformed-paren.xml",
                                "in[5]",
                                "in{Lab(1)} flattens to the same function as in{Lab[1]}, in[5]");

  // The offset+size pair, which is the second thing the corrected entry test
  // changed direction on: "in{Lab(1),2}" was REFUSED before the fix (the paren
  // form never entered the block, so the size stayed 1 and in(4,1) failed stack
  // balance against a two-channel out{}) and now flattens to in[5,2], the same
  // as the bracket spelling.  Asserted for the same reason as the bare form:
  // parity is the contract, and only a comparison of the two can state it.
  failures += expectSameFlatten("in{Lab[1],2} out{a,2}", "in{Lab(1),2} out{a,2}",
                                "wellformed-bracket-size.xml", "wellformed-paren-size.xml",
                                "in[5,2]",
                                "in{Lab(1),2} flattens to the same function as in{Lab[1],2}, in[5,2]");

  failures += expectParsed("in{Lab} out{a,2}",
                           "wellformed-sizeonly.xml",
                           "the size-only form out{a,2} still parses");

  // The control for the propagation change specifically: a macro whose body is
  // fine must still expand, or "return false on any macro" would satisfy the
  // assertion above.
  failures += expectParsed("#mm out{L}",
                           "wellformed-macro.xml",
                           "a macro whose body is well formed still expands",
                           "          <Macros>\n"
                           "            <Macro Name=\"mm\">in{Lab[0]}</Macro>\n"
                           "          </Macros>\n");

  // #2829, the channel name map.  The control comes first because the cases
  // after it rest on it: with the stock name lists the numbered body loads, so a
  // refusal below is the name list's and not the body's.  It is also the exact-fit
  // case, "Lab[3]" with four channels before it out of seven, which a bound
  // tightened by one would refuse.
  failures += expectParsed(kNumberedBody,
                           "names-wellformed.xml",
                           "the numbered body loads with a group that reaches the last channel");

  // The reported shape: the last group's width is INT_MAX, so "4 + 2147483647"
  // overflowed before it was compared.  Before the fix this document loaded.
  failures += expectNamesRefused("C M Y K Lab[2147483647]", kOutputNames,
                                 "names-input-width-overflow.xml",
                                 "Invalid name for InputChannels",
                                 "an input group whose width overflows the bound is refused");

  // The same width on a group that another name follows.  The wrapped comparison
  // let it through, and "i += size" at the following space then overflowed a
  // second time, leaving the next name at a negative channel index.
  failures += expectNamesRefused("C M[2147483647] Y", kOutputNames,
                                 "names-input-width-overflow-middle.xml",
                                 "Invalid name for InputChannels",
                                 "an overflowing width on a group in mid-list is refused");

  // OutputNames goes through the same function with the output count.
  failures += expectNamesRefused(kInputNames, "a b L[2147483647]",
                                 "names-output-width-overflow.xml",
                                 "Invalid name for OutputChannels",
                                 "an output group whose width overflows the bound is refused");

  // One channel past the end, where nothing overflows.  Refused before and after:
  // it pins that the rewritten comparison kept the bound where it was.
  failures += expectNamesRefused("C M Y K Lab[4]", kOutputNames,
                                 "names-input-width-one-over.xml",
                                 "Invalid name for InputChannels",
                                 "a group one channel wider than the channels left is still refused");

  // A group in mid-list that is accepted, so the next name starts past it: "B"
  // begins at channel 3 and its four channels end exactly at the seventh.
  failures += expectParsed(kNumberedBody,
                           "names-two-groups.xml",
                           "two groups that fill the channels load",
                           "",
                           "A[3] B[4]");

  return failures ? 1 : 0;
}
