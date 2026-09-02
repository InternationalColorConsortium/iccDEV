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

    Two spellings reach the same block.  "select[0] == '['" is the ordinary one;
    "select[1] == '('" is a second entry with a defect of its own (it is a typo
    for select[0], which is why "in{C(0,3)}" silently ignores its index while
    tget/tput handle the same spelling correctly through
    CIccFuncTokenizer::GetIndex).  That typo is NOT changed here -- it alters
    which references are accepted, which is a maintainer ruling -- but it is
    covered, because a reference reaching the block that way overflowed too.

    The controls matter as much as the failures: an "unterminated index is
    refused" assertion is satisfied just as well by refusing every indexed
    reference, so the well-formed forms the corpus actually uses are pinned
    alongside.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"

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
// have somewhere real to point.  Only the function body and the optional macro
// block vary.
static std::string profileXml(const std::string& body, const std::string& macros = "")
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
    "            InputNames=\"C M Y K Lab[3]\" OutputNames=\"a b L\">\n"
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
static bool loadXml(const char* file, std::string& parseStr)
{
  CIccProfileXml profile;
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

  std::string parseStr;
  bool loaded = loadXml(file, parseStr);
  bool named = parseStr.find(needle) != std::string::npos;

  return check(!loaded && named, label);
}

static int expectParsed(const std::string& body, const char* file, const char* label,
                        const std::string& macros = "")
{
  if (!writeFile(file, profileXml(body, macros)))
    return check(false, label);

  std::string parseStr;
  return check(loadXml(file, parseStr), label);
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

  // The second way into the same block: select[1] == '(' rather than
  // select[0] == '['.  Reached with a reference whose selector starts ",(".
  failures += expectRefused("in{C,(" + filler(13) + " out{L}",
                            "unterminated-paren.xml",
                            "the select[1] == '(' entry into the block is refused too");

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

  return failures ? 1 : 0;
}
