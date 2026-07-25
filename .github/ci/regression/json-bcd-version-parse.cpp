// Regression for #1830: the JSON BCD version parser must reject a malformed version
// component instead of wrapping it through the header's shift arithmetic.
//
// CIccProfileJson::ParseBasic decodes "ProfileVersion" / "ProfileSubClassVersion"
// with icJsonParseBCDVersionStr, which used atoi() per component:
//
//     int v = atoi(s);
//     return (icUInt32Number)(((v / 10) % 10) * 16 + (v % 10));
//
// atoi() accepts a leading '-', so "-1" gave v == -1, and the BCD expression then
// evaluated to -1, which reinterpreted as icUInt32Number is 0xFFFFFFFF. That value
// wrapped both shifts downstream, which UBSan reported on the #1830 PoC as:
//
//   IccProfileJson.cpp:293: runtime error: left shift of 4294967295 by 8 places
//                           cannot be represented in type 'icUInt32Number'
//   IccProfileJson.cpp:308: runtime error: left shift of 4294967040 by 16 places
//                           cannot be represented in type 'icUInt32Number'
//
// atoi() is additionally undefined on an out-of-range input, which a digit-by-digit
// parse of at most two digits cannot reach.
//
// Each component is now required to be one or two decimal digits with no sign, so
// the encoded byte is always a valid BCD byte (<= 0x99) and the packed pair is at
// most 0x9999 -- the caller's further "<< 16" therefore cannot overflow. A malformed
// version parses as 0, which is what the zeroed header already carries.
//
// The test drives the public CIccProfileJson::ParseJson so it exercises the real
// header path rather than the file-static helpers. Red-green: restoring the atoi()
// form makes the "-1" cases fail (version becomes 0xFFFF0000 rather than 0) and, in
// a UBSan build, reintroduces the shift diagnostics above.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfileJson.h"
#include "IccTagJsonFactory.h"

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
    std::fprintf(stderr, "[json-bcd-version] FAIL: %s\n", what);
  }
}

// Build the smallest JSON profile ParseJson accepts (it requires both a Header and a
// Tags member) carrying the supplied version strings, parse it, and hand back the
// resulting packed header version.
icUInt32Number parseVersion(const char *szVersion, const char *szSubClassVersion)
{
  IccJson root;
  IccJson header;

  if (szVersion)
    header["ProfileVersion"] = szVersion;
  if (szSubClassVersion)
    header["ProfileSubClassVersion"] = szSubClassVersion;

  root["Header"] = header;
  root["Tags"] = IccJson::object();

  CIccProfileJson profile;
  std::string parseStr;
  profile.ParseJson(root, parseStr);

  return profile.m_Header.version;
}

void checkVersion(const char *szVersion, icUInt32Number expected, const char *what)
{
  icUInt32Number got = parseVersion(szVersion, NULL);
  if (got != expected) {
    ++g_fail;
    std::fprintf(stderr,
                 "[json-bcd-version] FAIL: %s -- \"%s\" gave 0x%08X, expected 0x%08X\n",
                 what, szVersion ? szVersion : "(none)",
                 (unsigned int)got, (unsigned int)expected);
  }
}

} // namespace

int main()
{
  // --- well-formed versions are unchanged --------------------------------------
  // CIccInfo::GetVersionName writes "%.2lf", so "4.30" is the canonical round-trip
  // form and must still encode as BCD 0x0430 in the high half of the header word.

  checkVersion("4.30", 0x04300000u, "\"4.30\" encodes as 0x0430");
  checkVersion("5.00", 0x05000000u, "\"5.00\" encodes as 0x0500");
  checkVersion("2.20", 0x02200000u, "\"2.20\" encodes as 0x0220");

  // A missing minor component keeps its previous meaning ("4" == "4.0"), and a
  // single-digit minor keeps its previous (quirky but unchanged) encoding: atoi("3")
  // was 3, giving 0x03 rather than 0x30. Pinned so this fix stays behaviour-neutral
  // for input that already parsed.
  checkVersion("4", 0x04000000u, "\"4\" encodes as 0x0400");
  checkVersion("4.3", 0x04030000u, "\"4.3\" encodes as 0x0403 (unchanged quirk)");

  // --- the #1830 cases ---------------------------------------------------------

  // The PoC input. Previously atoi("-1") == -1 -> 0xFFFFFFFF -> wrapped shifts.
  checkVersion("-1", 0u, "negative major version rejected (#1830)");
  checkVersion("-1.-1", 0u, "negative major and minor rejected (#1830)");
  checkVersion("4.-1", 0u, "negative minor rejected (#1830)");

  // A component wider than two digits cannot be a BCD byte.
  checkVersion("999.99", 0u, "three-digit major rejected");
  checkVersion("4.999", 0u, "three-digit minor rejected");

  // Non-numeric text. Note ToJson emits "Invalid BCD version 0x..." for a corrupt
  // header, so a re-parsed document can legitimately contain such a string.
  checkVersion("abc", 0u, "non-numeric version rejected");
  checkVersion("Invalid BCD version 0x00000000", 0u,
               "ToJson's invalid-version text re-parses as 0");
  checkVersion("", 0u, "empty version rejected");
  checkVersion(" 4.30", 0u, "leading space rejected (no whitespace skipping)");

  // --- the subclass path shares the same helper --------------------------------
  // It occupies the low half of the header word, so a wrapped value there would
  // corrupt the version half too.

  // ParseBasic ORs the subclass pair straight into the low 16 bits (no further
  // shift, unlike the main version's "<< 16"), so "1.20" lands as 0x0120.
  icUInt32Number v = parseVersion("4.30", "1.20");
  check(v == 0x04300120u, "subclass version encodes into the low half");

  v = parseVersion("4.30", "-1");
  check(v == 0x04300000u, "negative subclass version rejected, major preserved (#1830)");
  check((v & 0x0000ffffu) == 0u, "rejected subclass version leaves the low half zero");

  if (g_fail)
    std::fprintf(stderr, "[json-bcd-version] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[json-bcd-version] all assertions passed\n");

  return g_fail;
}
