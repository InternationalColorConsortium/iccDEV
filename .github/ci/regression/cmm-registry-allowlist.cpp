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
// #2098 extension -- pinning enum VALUES, not just allow-list membership.
//
// The #1724 checks above all reach the allow-list through the enum *names*
// (icSigReprointelligence, icSigICC, ...).  That is deliberately blind to the
// defect class #2098 reported: an enum whose *value* does not match the
// registry.  `case icSigLogoSync:` allow-lists whatever number the header
// happens to assign, so a name-based check passes identically before and after
// a value correction -- it cannot go red.  Two enums were wrong:
//
//   * icSigLogoSync      was 0x44676F53 ('DgoS'); registry says 0x4C676F53 ('LgoS')
//   * icSigKonicaMinolta was 0x4D434D44 ('MCMD'); registry says 0x4D434D4C ('MCML')
//
// Effect of the defect, in both directions: a profile carrying the correctly
// registered CMM was reported "Unregistered CMM signature.", while a profile
// carrying the transposed non-registered value validated clean.  This is the
// same shape as the icSigRefIccMAX 'RICC'/'RIMX' correction #1473 already made
// (recorded as delta B3 in registry/PERMISSIVENESS_DELTAS.md), where the fix was
// likewise to change the value and keep the comment's registered string.
//
// So the checks below drive the allow-list with the literal registered hex and
// with the superseded values, which is what actually distinguishes fixed from
// broken.  Verified against registry.color.org/cmm-signatures on 2026-08-11.
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

// #2098: the header enum must carry the registered value.  These are compile-
// time so a reverted value fails the build rather than the run -- the enum is a
// public header constant, and a consumer that hardcodes the registered number
// would silently stop matching it.
static_assert(icSigLogoSync      == 0x4C676F53, "icSigLogoSync must be 'LgoS' per the CMM registry");
static_assert(icSigKonicaMinolta == 0x4D434D4C, "icSigKonicaMinolta must be 'MCML' per the CMM registry");

// icSigVivo's value was already correct; only its comment was wrong ('VIVO' for
// a value of 'vivo').  Pinned anyway because it is the likeliest of the three to
// be "corrected" in the wrong direction: the *manufacturer* registry lists Vivo
// as 'VIVO' 0x5649564F, and reconciling the CMM enum against that row instead of
// the CMM registry would reintroduce exactly the defect this change removes.
static_assert(icSigVivo          == 0x7669766F, "icSigVivo must be 'vivo' per the CMM registry, not the manufacturer registry's 'VIVO'");

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

  // #2098, red before the fix: driven by the literal registered value rather
  // than the enum name, so the allow-list has to actually contain the number
  // the registry publishes.
  check(!cmmWarns(0x4C676F53), "'LgoS' (registered, GretagMacbeth) validates without CMM warning");
  check(!cmmWarns(0x4D434D4C), "'MCML' (registered, Konica Minolta) validates without CMM warning");

  // #2098, the other direction: the superseded values were never registered, so
  // correcting the enum must stop accepting them.  Without these two, moving the
  // enum to the registered value while leaving the old number allow-listed
  // somewhere else would go unnoticed.
  check(cmmWarns(0x44676F53), "'DgoS' (superseded icSigLogoSync typo, unregistered) warns");
  check(cmmWarns(0x4D434D44), "'MCMD' (superseded icSigKonicaMinolta typo, unregistered) warns");

  // #2098: 'vivo' needed no value change, but pin it by literal too -- and pin
  // that the manufacturer registry's 'VIVO' is NOT a registered CMM, so the two
  // registries stay distinguishable at the allow-list.
  check(!cmmWarns(0x7669766F), "'vivo' (registered CMM, Vivo Mobile Communication) validates without CMM warning");
  check(cmmWarns(0x5649564F),  "'VIVO' (manufacturer-registry sig, not a registered CMM) warns");

  if (g_failures) { std::printf("\n%d check(s) FAILED\n", g_failures); return 1; }
  std::printf("\nall checks passed\n");
  return 0;
}
