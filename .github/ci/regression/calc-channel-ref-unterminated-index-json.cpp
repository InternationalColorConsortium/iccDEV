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
    without any XML involved.  It is also reachable through MORE spellings than
    the XML copy, because its entry condition is the correct
    "select[0] == '('" where the XML one has a typo ("select[1]"): both
    "in{Lab[XXXX" and "in{Lab(XXXX" arrive here, and only the first arrives on
    the XML side.  Both are asserted below for exactly that reason.

    Registered separately from the XML helper rather than folded into it so that
    neither can mask the other: this one is skipped where IccJSON is not built,
    and the XML coverage does not disappear with it.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccProfileJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"

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
// is a three-wide group at offset 4.  Only mainFunction varies.
static std::string profileJson(const std::string& mainFunction)
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
    "                \"inputNames\": \"C M Y K Lab[3]\",\n"
    "                \"outputNames\": \"a b L\",\n"
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
static bool loadJson(const char* file, std::string& parseStr)
{
  CIccProfileJson profile;
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

  std::string parseStr;
  bool loaded = loadJson(file, parseStr);
  bool named = parseStr.find(needle) != std::string::npos;

  return check(!loaded && named, label);
}

static int expectParsed(const std::string& mainFunction, const char* file, const char* label)
{
  if (!writeFile(file, profileJson(mainFunction)))
    return check(false, label);

  std::string parseStr;
  return check(loadJson(file, parseStr), label);
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

  // The '(' spelling.  This one reaches the block here but NOT on the XML side,
  // where select[1] == '(' keeps it out -- so it is only covered by this file.
  failures += expectRefused("{\\nin{Lab(" + filler(14) + "\\nout{L}\\n}\\n",
                            "json-unterminated-paren.json",
                            "the '(' spelling, which the XML typo blocks, is refused here");

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
  // stack balance rather than on anything this fix touches.  That is the same
  // deferred defect as the select[1] typo -- an index quietly narrowed instead
  // of refused -- and pinning it here would assert today's wrong behaviour.
  failures += expectParsed("{\\nin{Lab(1)}\\nout{L}\\n}\\n",
                           "json-wellformed-paren.json",
                           "the well-formed '(' spelling still parses");

  return failures ? 1 : 0;
}
