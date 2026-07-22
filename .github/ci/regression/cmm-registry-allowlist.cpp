// Regression test for the CMM-signature validation allow-list catch-up landed
// for #1724 in IccProfLib/IccProfile.cpp (CIccProfile::CheckHeader).
//
// Background: PR #1473 reconciled the icCmmSignature enum and GetCmmSigName()
// name table with registry.color.org/cmm-signatures -- it gave names to the
// live-registry CMMs 'repr' (Reprointelligence) and 'iccd' (ICC).  But #1473 did
// NOT add those two to the CMM allow-list switch in CheckHeader(), so
// CIccProfile::Validate() still emitted
//   "<name>: Unregistered CMM signature."
// for a header whose preferred-CMM field was one of the two now-named sigs.  The
// #1724 change adds `case icSigReprointelligence:` and `case icSigICC:` to that
// switch (the enum value of icSigICC is 0x69636364 = ASCII 'iccd'), so the two
// registered CMMs validate cleanly, matching the names #1473 already gave them.
//
// This test drives CheckHeader() directly through a minimal subclass (it reads
// only m_Header, so no tags or profile body are needed) and asserts, per the
// live CMM registry as re-verified on 2026-07-21:
//   * 'repr' and 'iccd' -- registered -> must NOT warn (the #1724 fix)
//   * 'WCS ' and 'RIMX' -- registered and already allow-listed before #1724
//                          -> must NOT warn (guards against a regression that
//                          drops an existing allow-list entry)
//   * 'MSFT' -- NOT a registered CMM (it is the Microsoft *platform* signature;
//               Microsoft's registered CMM is 'WCS ') -> must STILL warn, so the
//               fix stayed narrow and did not blanket-accept a named-but-
//               unregistered signature
//   * 'ZZZZ' -- unregistered junk -> must STILL warn (negative control)
//
// Reverting the two-line IccProfile.cpp change flips the 'repr'/'iccd' cases from
// no-warn to warn and fails this test.
//
// Exit code 0 = pass, 1 = a case regressed.

#include "IccProfile.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <string>

static int g_failures = 0;
static bool check(bool cond, const char *msg) {
  if (cond) { std::printf("ok:   %s\n", msg); }
  else      { std::printf("FAIL: %s\n", msg); ++g_failures; }
  return cond;
}

// CheckHeader() is protected; expose it for the test.  A default-constructed
// CIccProfile zero-fills m_Header (cmmId 0 == icSigUnknownCmm, itself allow-
// listed), so setting cmmId is the only field the CMM branch of CheckHeader
// reads.  Other header fields left zero may raise unrelated warnings, but this
// test greps only for the CMM-specific message.
class HeaderProbe : public CIccProfile {
public:
  icValidateStatus RunCheckHeader(std::string &report) const {
    return CheckHeader(report);
  }
};

// True iff CheckHeader emits the "Unregistered CMM signature" warning for the
// given preferred-CMM signature value.
static bool cmmWarns(icUInt32Number cmm) {
  HeaderProbe p;
  p.m_Header.cmmId = cmm;
  std::string report;
  p.RunCheckHeader(report);
  return report.find("Unregistered CMM signature") != std::string::npos;
}

int main() {
  // The #1724 fix: two registered CMMs that #1473 named but left warning.
  check(!cmmWarns(icSigReprointelligence), "'repr' (registered) validates without CMM warning");
  check(!cmmWarns(icSigICC),               "'iccd' (registered) validates without CMM warning");

  // Already allow-listed before #1724 -- present here to catch a regression that
  // drops an existing accepted signature.
  check(!cmmWarns(icSigWindowsCMS),        "'WCS ' (registered) still validates without CMM warning");
  check(!cmmWarns(icSigRefIccMAX),         "'RIMX' (registered) still validates without CMM warning");

  // Must still warn: 'MSFT' is the Microsoft platform signature, not a
  // registered CMM, so the fix deliberately does not accept it.
  check(cmmWarns(icSigMicrosoftCMM),       "'MSFT' (platform sig, not a registered CMM) still warns");

  // Negative control: unregistered junk must still warn.
  check(cmmWarns(0x5A5A5A5A /* 'ZZZZ' */), "'ZZZZ' (unregistered) still warns");

  if (g_failures) { std::printf("\n%d check(s) FAILED\n", g_failures); return 1; }
  std::printf("\nall checks passed\n");
  return 0;
}
